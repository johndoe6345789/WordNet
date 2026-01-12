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
#define MAX_TERMS 1024
#define MAX_GLOSS 512

struct concept {
    const char *name;
    const char *type;
    const char *seeds[16];
    char terms[MAX_CONCEPT_TERMS][MAX_TERM];
    int term_count;
    int score;
};

struct chat_context {
    char actions[MAX_LIST][MAX_TERM];
    int action_count;
    char entities[MAX_LIST][MAX_TERM];
    int entity_count;
    char qualifiers[MAX_LIST][MAX_TERM];
    int qualifier_count;
    char language[MAX_TERM];
    char platform[MAX_TERM];
    char framework[MAX_TERM];
    int language_score;
    int platform_score;
    int framework_score;
    struct {
        char term[MAX_TERM];
        int count;
    } terms[MAX_TERMS];
    int term_count;
    int turns;
};

static int is_noise_token(const char *word);

static const char *stopwords[] = {
    "a", "an", "and", "are", "as", "at", "be", "but", "by",
    "for", "from", "in", "is", "it", "of", "on", "or", "the",
    "to", "was", "were", "with", "me", "my", "your", "our",
    "should", "want", "need", "please", "make", "do", "write",
    NULL
};

static const char *generic_verbs[] = {
    "make", "do", "write", "build", "create", "implement",
    "develop", "add", "use", "target", "support", "provide", NULL
};

static void set_default_searchdir(void)
{
    const char *searchdir = getenv("WNSEARCHDIR");

    if (searchdir != NULL && searchdir[0] != '\0') {
        return;
    }
#ifdef _WIN32
    {
        char envbuf[512];
        snprintf(envbuf, sizeof(envbuf), "WNSEARCHDIR=%s", DEFAULTPATH);
        _putenv(envbuf);
    }
#else
    setenv("WNSEARCHDIR", DEFAULTPATH, 1);
#endif
}

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

static void add_term_count(struct chat_context *ctx, const char *term, int weight)
{
    int i;

    if (term == NULL || term[0] == '\0') {
        return;
    }
    for (i = 0; i < ctx->term_count; i++) {
        if (strcmp(ctx->terms[i].term, term) == 0) {
            ctx->terms[i].count += weight;
            return;
        }
    }
    if (ctx->term_count < MAX_TERMS) {
        snprintf(ctx->terms[ctx->term_count].term, MAX_TERM, "%s", term);
        ctx->terms[ctx->term_count].count = weight;
        ctx->term_count++;
    }
}

static void add_terms_from_text(struct chat_context *ctx, const char *text)
{
    char buf[MAX_GLOSS];
    size_t i = 0;
    size_t j = 0;

    if (text == NULL) {
        return;
    }
    snprintf(buf, sizeof(buf), "%s", text);

    while (buf[i] != '\0') {
        unsigned char c = (unsigned char)buf[i++];
        if (isalnum(c)) {
            if (j + 1 < sizeof(buf)) {
                buf[j++] = (char)tolower(c);
            }
        } else if (j > 0) {
            buf[j] = '\0';
            if (!is_noise_token(buf)) {
                add_term_count(ctx, buf, 1);
            }
            j = 0;
        }
    }
    if (j > 0) {
        buf[j] = '\0';
        if (!is_noise_token(buf)) {
            add_term_count(ctx, buf, 1);
        }
    }
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

static int is_stopword(const char *word)
{
    int i;

    for (i = 0; stopwords[i] != NULL; i++) {
        if (strcmp(word, stopwords[i]) == 0) {
            return 1;
        }
    }
    return 0;
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
    if (is_stopword(word)) {
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

static void collect_memory_from_synset(struct chat_context *ctx, SynsetPtr syn)
{
    int i;

    if (syn == NULL) {
        return;
    }
    for (i = 0; i < syn->wcount; i++) {
        char term[MAX_TERM];

        snprintf(term, sizeof(term), "%s", syn->words[i]);
        normalize_word(term);
        if (!is_noise_token(term)) {
            add_term_count(ctx, term, 2);
        }
    }
    if (syn->defn != NULL) {
        add_terms_from_text(ctx, syn->defn);
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

static int is_generic_verb(const char *word)
{
    int i;

    for (i = 0; generic_verbs[i] != NULL; i++) {
        if (strcmp(word, generic_verbs[i]) == 0) {
            return 1;
        }
    }
    return 0;
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

static int extract_match_score(char list[][MAX_TERM], int count, const char *options[], char *out, size_t out_size)
{
    int i;
    int score = 0;

    out[0] = '\0';
    for (i = 0; options[i] != NULL; i++) {
        if (list_contains(list, count, options[i])) {
            if (out[0] == '\0') {
                snprintf(out, out_size, "%s", options[i]);
            }
            score++;
        }
    }
    return score;
}

static int extract_language(char list[][MAX_TERM], int count, char *out, size_t out_size)
{
    const char *languages[] = {
        "c", "c++", "python", "javascript", "typescript", "go",
        "rust", "java", "c#", "ruby", "php", "swift", "kotlin", NULL
    };
    return extract_match_score(list, count, languages, out, out_size);
}

static int extract_platform(char list[][MAX_TERM], int count, char *out, size_t out_size)
{
    const char *platforms[] = {
        "cli", "command", "terminal", "web", "server", "service",
        "api", "mobile", "desktop", "library", "script", "gui",
        "linux", "windows", "mac", "macos", NULL
    };
    return extract_match_score(list, count, platforms, out, out_size);
}

static int extract_framework(char list[][MAX_TERM], int count, char *out, size_t out_size)
{
    const char *frameworks[] = {
        "sdl", "sdl2", "sdl3", "react", "vue", "django", "flask",
        "express", "spring", "qt", "gtk", "tk", NULL
    };
    return extract_match_score(list, count, frameworks, out, out_size);
}

static void merge_context(struct chat_context *ctx,
                          char actions[][MAX_TERM], int action_count,
                          char entities[][MAX_TERM], int entity_count,
                          char qualifiers[][MAX_TERM], int qualifier_count,
                          const char *language,
                          int language_score,
                          const char *platform,
                          int platform_score,
                          const char *framework,
                          int framework_score)
{
    int i;

    if (action_count > 0) {
        for (i = 0; i < action_count; i++) {
            add_unique(ctx->actions, &ctx->action_count, actions[i]);
        }
    }
    if (entity_count > 0) {
        for (i = 0; i < entity_count; i++) {
            add_unique(ctx->entities, &ctx->entity_count, entities[i]);
        }
    }
    if (qualifier_count > 0) {
        for (i = 0; i < qualifier_count; i++) {
            add_unique(ctx->qualifiers, &ctx->qualifier_count, qualifiers[i]);
        }
    }
    if (language[0] != '\0' && language_score >= ctx->language_score) {
        if (language_score > ctx->language_score || strcmp(ctx->language, language) == 0) {
            snprintf(ctx->language, sizeof(ctx->language), "%s", language);
            ctx->language_score = language_score;
        }
    }
    if (platform[0] != '\0' && platform_score >= ctx->platform_score) {
        if (platform_score > ctx->platform_score || strcmp(ctx->platform, platform) == 0) {
            snprintf(ctx->platform, sizeof(ctx->platform), "%s", platform);
            ctx->platform_score = platform_score;
        }
    }
    if (framework[0] != '\0' && framework_score >= ctx->framework_score) {
        if (framework_score > ctx->framework_score || strcmp(ctx->framework, framework) == 0) {
            snprintf(ctx->framework, sizeof(ctx->framework), "%s", framework);
            ctx->framework_score = framework_score;
        }
    }
}

static void analyze_input(const char *input, struct chat_context *ctx)
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
    char framework[MAX_TERM];
    int language_score = 0;
    int platform_score = 0;
    int framework_score = 0;
    char token[MAX_TERM];
    size_t i;
    size_t j = 0;

    init_concepts(concepts, &concept_count);
    ctx->turns++;

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
            int matched_pos = 0;

            token[j] = '\0';
            j = 0;
            snprintf(normalized, sizeof(normalized), "%s", token);
            normalize_word(normalized);

            if (is_noise_token(normalized)) {
                continue;
            }

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
                matched_pos = 1;
                snprintf(lemma, sizeof(lemma), "%s", normalized);

                if (pos == VERB) {
                    add_unique(actions, &action_count, lemma);
                } else if (pos == NOUN) {
                    add_unique(entities, &entity_count, lemma);
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
                        if (!is_noise_token(synword)) {
                            add_unique(entities, &entity_count, synword);
                        }
                    }
                    collect_memory_from_synset(ctx, syn);
                    free_synset(syn);
                }
                free_index(idx);
            }
            if (!matched_pos) {
                add_unique(entities, &entity_count, normalized);
            }
        }
    }

    if (action_count > 1) {
        int filtered = 0;
        for (i = 0; i < (size_t)action_count; i++) {
            if (!is_generic_verb(actions[i])) {
                snprintf(actions[filtered], MAX_TERM, "%s", actions[i]);
                filtered++;
            }
        }
        if (filtered > 0) {
            action_count = filtered;
        }
    }

    language_score = extract_language(entities, entity_count, language, sizeof(language));
    platform_score = extract_platform(entities, entity_count, platform, sizeof(platform));
    framework_score = extract_framework(entities, entity_count, framework, sizeof(framework));
    rank_concepts(concepts, concept_count, entities, entity_count);
    merge_context(ctx, actions, action_count, entities, entity_count,
                  qualifiers, qualifier_count,
                  language, language_score,
                  platform, platform_score,
                  framework, framework_score);

    printf("\nIntent sketch\n");
    printf("- actions: ");
    if (ctx->action_count == 0) {
        printf("(none)\n");
    } else {
        int i;
        for (i = 0; i < ctx->action_count && i < 6; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("%s", ctx->actions[i]);
        }
        printf("\n");
    }
    printf("- entities: ");
    if (ctx->entity_count == 0) {
        printf("(none)\n");
    } else {
        int i;
        for (i = 0; i < ctx->entity_count && i < 6; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("%s", ctx->entities[i]);
        }
        printf("\n");
    }
    if (ctx->qualifier_count > 0) {
        int i;
        printf("- qualifiers: ");
        for (i = 0; i < ctx->qualifier_count && i < 6; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("%s", ctx->qualifiers[i]);
        }
        printf("\n");
    }

    printf("\nSDLC focus\n");
    print_top_concepts(concepts, concept_count, "sdlc", 2);
    printf("\nDesign focus\n");
    print_top_concepts(concepts, concept_count, "design", 3);

    printf("\nLikely defaults\n");
    printf("- language: %s\n", ctx->language[0] ? ctx->language : "(unspecified)");
    if (ctx->language[0]) {
        printf("  confidence: %d\n", ctx->language_score);
    }
    printf("- platform: %s\n", ctx->platform[0] ? ctx->platform : "(unspecified)");
    if (ctx->platform[0]) {
        printf("  confidence: %d\n", ctx->platform_score);
    }
    printf("- framework: %s\n", ctx->framework[0] ? ctx->framework : "(unspecified)");
    if (ctx->framework[0]) {
        printf("  confidence: %d\n", ctx->framework_score);
    }

    printf("\nQuestions\n");
    if (ctx->action_count == 0) {
        printf("- What should the system do?\n");
    }
    if (ctx->entity_count == 0) {
        printf("- What should it operate on?\n");
    }
    if (ctx->language[0] == '\0') {
        printf("- Which language or runtime should I target?\n");
    }
    if (ctx->platform[0] == '\0') {
        printf("- Should this be a CLI, service, library, or UI?\n");
    }
}

static int compare_terms(const void *a, const void *b)
{
    const int *ia = (const int *)a;
    const int *ib = (const int *)b;

    if (*ia != *ib) {
        return (*ib - *ia);
    }
    return 0;
}

static void print_summary(struct chat_context *ctx)
{
    int i;
    int top = 12;
    int indices[MAX_TERMS];
    int count = ctx->term_count;

    if (count == 0) {
        printf("No memory yet.\n");
        return;
    }

    for (i = 0; i < count; i++) {
        indices[i] = i;
    }
    for (i = 0; i < count - 1; i++) {
        int j;
        for (j = i + 1; j < count; j++) {
            if (ctx->terms[indices[j]].count > ctx->terms[indices[i]].count) {
                int tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }

    printf("Memory summary (avg per turn)\n");
    for (i = 0; i < count && i < top; i++) {
        int idx = indices[i];
        double avg = 0.0;

        if (ctx->turns > 0) {
            avg = (double)ctx->terms[idx].count / (double)ctx->turns;
        }
        printf("- %s: %.2f\n", ctx->terms[idx].term, avg);
    }
}

static void print_context_check(struct chat_context *ctx)
{
    int i;
    int top = 5;
    int indices[MAX_TERMS];
    int count = ctx->term_count;

    printf("\nContext check\n");
    printf("- turns: %d\n", ctx->turns);
    printf("- actions: %d, entities: %d, qualifiers: %d\n",
           ctx->action_count, ctx->entity_count, ctx->qualifier_count);
    printf("- language: %s\n", ctx->language[0] ? ctx->language : "(unspecified)");
    if (ctx->language[0]) {
        printf("  confidence: %d\n", ctx->language_score);
    }
    printf("- platform: %s\n", ctx->platform[0] ? ctx->platform : "(unspecified)");
    if (ctx->platform[0]) {
        printf("  confidence: %d\n", ctx->platform_score);
    }
    printf("- framework: %s\n", ctx->framework[0] ? ctx->framework : "(unspecified)");
    if (ctx->framework[0]) {
        printf("  confidence: %d\n", ctx->framework_score);
    }

    if (count == 0) {
        printf("- memory: (empty)\n");
        return;
    }

    for (i = 0; i < count; i++) {
        indices[i] = i;
    }
    for (i = 0; i < count - 1; i++) {
        int j;
        for (j = i + 1; j < count; j++) {
            if (ctx->terms[indices[j]].count > ctx->terms[indices[i]].count) {
                int tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }

    printf("- top memory terms: ");
    for (i = 0; i < count && i < top; i++) {
        int idx = indices[i];
        double avg = 0.0;

        if (ctx->turns > 0) {
            avg = (double)ctx->terms[idx].count / (double)ctx->turns;
        }
        if (i > 0) {
            printf(", ");
        }
        printf("%s(%.2f)", ctx->terms[idx].term, avg);
    }
    printf("\n");
}

static void reset_context(struct chat_context *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

static void print_help(const char *prog)
{
    printf("DIY AI Chat (WordNet)\n");
    printf("usage: %s\n", prog);
    printf("\n");
    printf("commands:\n");
    printf("  /help        Show this help\n");
    printf("  /exit        Exit the chat\n");
    printf("  /summary     Show averaged memory summary\n");
    printf("  /reflect     Show context check\n");
    printf("  /reset       Clear memory and context\n");
    printf("\n");
    printf("examples:\n");
    printf("  build a CLI that parses log files\n");
    printf("  add retries and backoff for failed requests\n");
}

int main(void)
{
    char input[MAX_INPUT];
    struct chat_context ctx;

    memset(&ctx, 0, sizeof(ctx));
    set_default_searchdir();
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
        if (strcmp(input, "/summary") == 0) {
            print_summary(&ctx);
            continue;
        }
        if (strcmp(input, "/reflect") == 0) {
            print_context_check(&ctx);
            continue;
        }
        if (strcmp(input, "/reset") == 0) {
            reset_context(&ctx);
            printf("Memory reset.\n");
            continue;
        }
        analyze_input(input, &ctx);
        print_context_check(&ctx);
    }
    return 0;
}
