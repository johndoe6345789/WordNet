#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wn.h>

#define MAX_TERM 64
#define MAX_LIST 128
#define MAX_CONCEPT_TERMS 256
#define MAX_INPUT 1024
#define MAX_CONCEPTS 16

struct concept {
    const char *name;
    const char *type;
    const char *seeds[16];
    char terms[MAX_CONCEPT_TERMS][MAX_TERM];
    int term_count;
    int score;
};

static int add_unique(char list[][MAX_TERM], int *count, const char *value)
{
    int i;

    if (value == NULL || value[0] == '\0') {
        return 0;
    }
    for (i = 0; i < *count; i++) {
        if (strcmp(list[i], value) == 0) {
            return 0;
        }
    }
    if (*count >= MAX_CONCEPT_TERMS) {
        return 0;
    }
    snprintf(list[*count], MAX_TERM, "%s", value);
    (*count)++;
    return 1;
}

static void normalize_word(char *word)
{
    size_t i = 0;
    size_t j = 0;

    while (word[i] != '\0') {
        unsigned char c = (unsigned char)word[i++];
        if (isalnum(c) || c == '_' || c == '-') {
            word[j++] = (char)tolower(c);
        }
    }
    word[j] = '\0';
}

static int is_noise_token(const char *word)
{
    size_t len;

    if (word == NULL || word[0] == '\0') {
        return 1;
    }
    len = strlen(word);
    if (len < 3) {
        return 1;
    }
    return 0;
}

static int add_concept_term(struct concept *concept, const char *term)
{
    char buf[MAX_TERM];

    if (term == NULL || term[0] == '\0') {
        return 0;
    }
    snprintf(buf, sizeof(buf), "%s", term);
    normalize_word(buf);
    if (buf[0] == '\0') {
        return 0;
    }
    return add_unique(concept->terms, &concept->term_count, buf);
}

static void collect_from_synset(struct concept *concept, SynsetPtr syn)
{
    int i;

    if (syn == NULL) {
        return;
    }
    for (i = 0; i < syn->wcount; i++) {
        add_concept_term(concept, syn->words[i]);
    }
}

static void expand_seed_terms(struct concept *concept)
{
    int i;

    for (i = 0; concept->seeds[i] != NULL; i++) {
        int pos_list[] = { NOUN, VERB };
        int p;
        const char *seed = concept->seeds[i];
        char normalized[MAX_TERM];

        snprintf(normalized, sizeof(normalized), "%s", seed);
        normalize_word(normalized);
        add_concept_term(concept, normalized);

        for (p = 0; p < 2; p++) {
            int pos = pos_list[p];
            IndexPtr idx = getindex(normalized, pos);
            SynsetPtr syn;

            if (idx == NULL || idx->off_cnt == 0) {
                if (idx != NULL) {
                    free_index(idx);
                }
                continue;
            }
            syn = read_synset(pos, idx->offset[0], normalized);
            if (syn != NULL) {
                collect_from_synset(concept, syn);
                free_synset(syn);
            }
            free_index(idx);
        }
    }
}

static void init_concepts(struct concept *concepts, int *concept_count)
{
    struct concept base[] = {
        { "requirements", "sdlc", { "requirement", "specification", "story", "scope", NULL }, { { 0 } }, 0, 0 },
        { "design", "sdlc", { "design", "architecture", "model", "interface", NULL }, { { 0 } }, 0, 0 },
        { "implementation", "sdlc", { "implement", "build", "code", "develop", NULL }, { { 0 } }, 0, 0 },
        { "testing", "sdlc", { "test", "verify", "validate", "qa", NULL }, { { 0 } }, 0, 0 },
        { "deployment", "sdlc", { "deploy", "release", "ship", "deliver", NULL }, { { 0 } }, 0, 0 },
        { "maintenance", "sdlc", { "maintain", "operate", "support", "monitor", NULL }, { { 0 } }, 0, 0 },
        { "api", "design", { "api", "interface", "endpoint", "protocol", NULL }, { { 0 } }, 0, 0 },
        { "data", "design", { "data", "database", "storage", "schema", NULL }, { { 0 } }, 0, 0 },
        { "ui", "design", { "ui", "ux", "screen", "visual", NULL }, { { 0 } }, 0, 0 },
        { "performance", "design", { "performance", "latency", "throughput", "optimize", NULL }, { { 0 } }, 0, 0 },
        { "security", "design", { "security", "auth", "encrypt", "permission", NULL }, { { 0 } }, 0, 0 },
        { "reliability", "design", { "reliability", "retry", "failover", "resilience", NULL }, { { 0 } }, 0, 0 },
        { "observability", "design", { "log", "trace", "monitor", "metric", NULL }, { { 0 } }, 0, 0 }
    };
    int i;
    int count = (int)(sizeof(base) / sizeof(base[0]));

    for (i = 0; i < count; i++) {
        concepts[i] = base[i];
        expand_seed_terms(&concepts[i]);
    }
    *concept_count = count;
}

static int token_matches(const char *token, struct concept *concept)
{
    int i;

    for (i = 0; i < concept->term_count; i++) {
        if (strcmp(token, concept->terms[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static void rank_concepts(struct concept *concepts, int concept_count, char list[][MAX_TERM], int list_count)
{
    int i;
    int j;

    for (i = 0; i < concept_count; i++) {
        concepts[i].score = 0;
    }
    for (i = 0; i < list_count; i++) {
        for (j = 0; j < concept_count; j++) {
            if (token_matches(list[i], &concepts[j])) {
                concepts[j].score += 1;
            }
        }
    }
}

static void print_top_concepts(struct concept *concepts, int concept_count, const char *type, int max_count)
{
    int i;
    int printed = 0;

    for (i = 0; i < concept_count && printed < max_count; i++) {
        int j;
        int best = -1;

        for (j = 0; j < concept_count; j++) {
            if (strcmp(concepts[j].type, type) != 0) {
                continue;
            }
            if (best == -1 || concepts[j].score > concepts[best].score) {
                best = j;
            }
        }
        if (best == -1 || concepts[best].score <= 0) {
            break;
        }
        printf("  - %s\n", concepts[best].name);
        concepts[best].score = -1;
        printed++;
    }
    if (printed == 0) {
        printf("  - (unsure)\n");
    }
}

static int list_contains(char list[][MAX_TERM], int count, const char *value)
{
    int i;

    for (i = 0; i < count; i++) {
        if (strcmp(list[i], value) == 0) {
            return 1;
        }
    }
    return 0;
}

static void extract_language(char list[][MAX_TERM], int count, char *out, size_t out_size)
{
    const char *languages[] = {
        "c", "c++", "python", "javascript", "typescript", "go",
        "rust", "java", "c#", "ruby", "php", "swift", "kotlin", NULL
    };
    int i;

    out[0] = '\0';
    for (i = 0; languages[i] != NULL; i++) {
        if (list_contains(list, count, languages[i])) {
            snprintf(out, out_size, "%s", languages[i]);
            return;
        }
    }
}

static void extract_platform(char list[][MAX_TERM], int count, char *out, size_t out_size)
{
    const char *platforms[] = {
        "cli", "command", "terminal", "web", "server", "service",
        "api", "mobile", "desktop", "library", "script", NULL
    };
    int i;

    out[0] = '\0';
    for (i = 0; platforms[i] != NULL; i++) {
        if (list_contains(list, count, platforms[i])) {
            snprintf(out, out_size, "%s", platforms[i]);
            return;
        }
    }
}

static void analyze_input(const char *input)
{
    struct concept concepts[MAX_CONCEPTS];
    int concept_count = 0;
    char actions[MAX_LIST][MAX_TERM];
    char entities[MAX_LIST][MAX_TERM];
    char qualifiers[MAX_LIST][MAX_TERM];
    int action_count = 0;
    int entity_count = 0;
    int qualifier_count = 0;
    char language[MAX_TERM];
    char platform[MAX_TERM];
    char token[MAX_TERM];
    size_t i;
    size_t j = 0;

    init_concepts(concepts, &concept_count);

    for (i = 0; i <= strlen(input); i++) {
        unsigned char c = (unsigned char)input[i];
        if (isalnum(c) || c == '_' || c == '-') {
            if (j + 1 < sizeof(token)) {
                token[j++] = (char)c;
            }
        } else if (j > 0) {
            int pos_list[] = { NOUN, VERB, ADJ, ADV };
            int p;
            char normalized[MAX_TERM];

            token[j] = '\0';
            j = 0;
            snprintf(normalized, sizeof(normalized), "%s", token);
            normalize_word(normalized);

            if (is_noise_token(normalized)) {
                continue;
            }

            add_unique(entities, &entity_count, normalized);

            for (p = 0; p < 4; p++) {
                int pos = pos_list[p];
                IndexPtr idx = getindex(normalized, pos);
                SynsetPtr syn;
                char lemma[MAX_TERM];

                if (idx == NULL || idx->off_cnt == 0) {
                    if (idx != NULL) {
                        free_index(idx);
                    }
                    continue;
                }
                snprintf(lemma, sizeof(lemma), "%s", normalized);

                if (pos == VERB) {
                    add_unique(actions, &action_count, lemma);
                } else if (pos == ADJ || pos == ADV) {
                    add_unique(qualifiers, &qualifier_count, lemma);
                }
                syn = read_synset(pos, idx->offset[0], lemma);
                if (syn != NULL) {
                    int w;
                    for (w = 0; w < syn->wcount; w++) {
                        char synword[MAX_TERM];
                        snprintf(synword, sizeof(synword), "%s", syn->words[w]);
                        normalize_word(synword);
                        add_unique(entities, &entity_count, synword);
                    }
                    free_synset(syn);
                }
                free_index(idx);
            }
        }
    }

    extract_language(entities, entity_count, language, sizeof(language));
    extract_platform(entities, entity_count, platform, sizeof(platform));
    rank_concepts(concepts, concept_count, entities, entity_count);

    printf("\nIntent sketch\n");
    printf("- actions: ");
    if (action_count == 0) {
        printf("(none)\n");
    } else {
        int i;
        for (i = 0; i < action_count && i < 6; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("%s", actions[i]);
        }
        printf("\n");
    }
    printf("- entities: ");
    if (entity_count == 0) {
        printf("(none)\n");
    } else {
        int i;
        for (i = 0; i < entity_count && i < 6; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("%s", entities[i]);
        }
        printf("\n");
    }
    if (qualifier_count > 0) {
        int i;
        printf("- qualifiers: ");
        for (i = 0; i < qualifier_count && i < 6; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("%s", qualifiers[i]);
        }
        printf("\n");
    }

    printf("\nSDLC focus\n");
    print_top_concepts(concepts, concept_count, "sdlc", 2);
    printf("\nDesign focus\n");
    print_top_concepts(concepts, concept_count, "design", 3);

    printf("\nLikely defaults\n");
    printf("- language: %s\n", language[0] ? language : "(unspecified)");
    printf("- platform: %s\n", platform[0] ? platform : "(unspecified)");

    printf("\nQuestions\n");
    if (action_count == 0) {
        printf("- What should the system do?\n");
    }
    if (entity_count == 0) {
        printf("- What should it operate on?\n");
    }
    if (language[0] == '\0') {
        printf("- Which language or runtime should I target?\n");
    }
    if (platform[0] == '\0') {
        printf("- Should this be a CLI, service, library, or UI?\n");
    }
}

static void print_help(const char *prog)
{
    printf("DIY AI Chat (WordNet)\n");
    printf("usage: %s\n", prog);
    printf("\n");
    printf("commands:\n");
    printf("  /help        Show this help\n");
    printf("  /exit        Exit the chat\n");
    printf("\n");
    printf("examples:\n");
    printf("  build a CLI that parses log files\n");
    printf("  add retries and backoff for failed requests\n");
}

int main(void)
{
    char input[MAX_INPUT];

    if (wninit() != 0) {
        fprintf(stderr, "WordNet data files not found. Set WNHOME or WNSEARCHDIR.\n");
        return 1;
    }

    printf("DIY AI Chat (type /help for commands)\n");
    while (1) {
        printf("diy-ai> ");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        input[strcspn(input, "\n")] = '\0';
        if (input[0] == '\0') {
            continue;
        }
        if (strcmp(input, "/help") == 0) {
            print_help("wn-chat");
            continue;
        }
        if (strcmp(input, "/exit") == 0 || strcmp(input, "/quit") == 0) {
            break;
        }
        analyze_input(input);
    }
    return 0;
}
