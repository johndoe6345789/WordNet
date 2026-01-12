#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wn.h>

#define MAX_WORDS 256
#define MAX_TERM 64
#define MAX_TERMS 512
#define MAX_GLOSS 512
#define DEFAULT_TOP 12

struct term_count {
    char term[MAX_TERM];
    int count;
};

static int is_stopword(const char *word)
{
    static const char *words[] = {
        "a", "an", "and", "are", "as", "at", "be", "but", "by",
        "for", "from", "in", "is", "it", "of", "on", "or", "the",
        "to", "was", "were", "with"
    };
    size_t i;

    for (i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        if (strcmp(word, words[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

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

static int is_noise_token(const char *word)
{
    if (word == NULL || word[0] == '\0') {
        return 1;
    }
    if (strlen(word) < 3) {
        return 1;
    }
    return is_stopword(word);
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

static void add_term(struct term_count *terms, int *term_count, const char *term, int weight)
{
    int i;

    if (term == NULL || term[0] == '\0') {
        return;
    }
    for (i = 0; i < *term_count; i++) {
        if (strcmp(terms[i].term, term) == 0) {
            terms[i].count += weight;
            return;
        }
    }
    if (*term_count < MAX_TERMS) {
        snprintf(terms[*term_count].term, sizeof(terms[*term_count].term), "%s", term);
        terms[*term_count].count = weight;
        (*term_count)++;
    }
}

static void add_terms_from_text(struct term_count *terms, int *term_count, const char *text)
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
            if (!is_stopword(buf)) {
                add_term(terms, term_count, buf, 1);
            }
            j = 0;
        }
    }
    if (j > 0) {
        buf[j] = '\0';
        if (!is_stopword(buf)) {
            add_term(terms, term_count, buf, 1);
        }
    }
}

static int compare_terms(const void *a, const void *b)
{
    const struct term_count *ta = (const struct term_count *)a;
    const struct term_count *tb = (const struct term_count *)b;

    if (ta->count != tb->count) {
        return tb->count - ta->count;
    }
    return strcmp(ta->term, tb->term);
}

static void collect_from_synset(struct term_count *terms, int *term_count, SynsetPtr syn)
{
    int i;

    if (syn == NULL) {
        return;
    }
    for (i = 0; i < syn->wcount; i++) {
        add_term(terms, term_count, syn->words[i], 2);
        add_terms_from_text(terms, term_count, syn->words[i]);
    }
    if (syn->defn != NULL) {
        add_terms_from_text(terms, term_count, syn->defn);
    }
}

static void collect_hypernyms(struct term_count *terms, int *term_count, SynsetPtr syn, int pos)
{
    int i;

    if (syn == NULL) {
        return;
    }
    if (pos != NOUN && pos != VERB) {
        return;
    }
    for (i = 0; i < syn->ptrcount; i++) {
        SynsetPtr hyper;

        if (syn->ptrtyp[i] != HYPERPTR) {
            continue;
        }
        hyper = read_synset(pos, syn->ptroff[i], "");
        if (hyper == NULL) {
            continue;
        }
        collect_from_synset(terms, term_count, hyper);
        free_synset(hyper);
    }
}

static IndexPtr lookup_index(const char *word, int pos, char *resolved, size_t resolved_size)
{
    IndexPtr idx;
    char *lemma;

    snprintf(resolved, resolved_size, "%s", word);
    idx = getindex(resolved, pos);
    if (idx != NULL) {
        return idx;
    }
    lemma = morphstr(resolved, pos);
    if (lemma != NULL && lemma[0] != '\0') {
        snprintf(resolved, resolved_size, "%s", lemma);
        idx = getindex(resolved, pos);
    }
    return idx;
}

static void explain_word(const char *word, int show_gloss)
{
    int pos_list[] = { NOUN, VERB, ADJ, ADV };
    int i;

    if (!show_gloss) {
        return;
    }
    printf("\nWord: %s\n", word);
    for (i = 0; i < 4; i++) {
        int pos = pos_list[i];
        char resolved[MAX_TERM];
        IndexPtr idx = lookup_index(word, pos, resolved, sizeof(resolved));
        SynsetPtr syn;

        if (idx == NULL || idx->off_cnt == 0) {
            if (idx != NULL) {
                free_index(idx);
            }
            continue;
        }

        syn = read_synset(pos, idx->offset[0], resolved);
        if (syn != NULL) {
            const char *pos_name = partnames[pos];
            const char *gloss = syn->defn ? syn->defn : "(no gloss)";

            printf("  %s: %s\n", pos_name, gloss);
            free_synset(syn);
        }
        free_index(idx);
    }
}

static void print_help(const char *prog)
{
    printf("DIY AI Meaning Sketch (WordNet)\n");
    printf("usage: %s [options] \"text to interpret\"\n", prog);
    printf("\n");
    printf("options:\n");
    printf("  -h, --help           Show this help\n");
    printf("  --top N              Show top N meaning hints (default %d)\n", DEFAULT_TOP);
    printf("  --no-gloss           Skip per-word gloss output\n");
    printf("  --no-hypernyms       Skip hypernym expansion\n");
    printf("\n");
    printf("examples:\n");
    printf("  %s \"add caching to reduce latency\"\n", prog);
    printf("  %s --top 8 --no-gloss \"retry failed requests\"\n", prog);
}

static int parse_int(const char *text, int *value)
{
    char *end = NULL;
    long parsed;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed <= 0) {
        return 0;
    }
    *value = (int)parsed;
    return 1;
}

int main(int argc, char **argv)
{
    struct term_count terms[MAX_TERMS];
    int term_count = 0;
    char input[1024];
    char word[MAX_TERM];
    int word_count = 0;
    int show_gloss = 1;
    int use_hypernyms = 1;
    int top_n = DEFAULT_TOP;
    size_t i;
    size_t j = 0;

    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    for (i = 1; i < (size_t)argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--no-gloss") == 0) {
            show_gloss = 0;
            continue;
        }
        if (strcmp(argv[i], "--no-hypernyms") == 0) {
            use_hypernyms = 0;
            continue;
        }
        if (strcmp(argv[i], "--top") == 0) {
            if (i + 1 >= (size_t)argc || !parse_int(argv[i + 1], &top_n)) {
                fprintf(stderr, "Invalid value for --top\n");
                return 1;
            }
            i++;
            continue;
        }
    }

    set_default_searchdir();
    if (wninit() != 0) {
        const char *env_home = getenv("WNHOME");
        const char *env_search = getenv("WNSEARCHDIR");

        fprintf(stderr, "WordNet data files not found.\n");
        fprintf(stderr, "DEFAULTPATH=%s\n", DEFAULTPATH);
        fprintf(stderr, "WNHOME=%s\n", env_home ? env_home : "(unset)");
        fprintf(stderr, "WNSEARCHDIR=%s\n", env_search ? env_search : "(unset)");
        return 1;
    }

    input[0] = '\0';
    for (i = 1; i < (size_t)argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            if (strcmp(argv[i], "--top") == 0) {
                i++;
            }
            continue;
        }
        strncat(input, argv[i], sizeof(input) - strlen(input) - 1);
        if (i + 1 < (size_t)argc) {
            strncat(input, " ", sizeof(input) - strlen(input) - 1);
        }
    }

    if (input[0] == '\0') {
        fprintf(stderr, "No input text provided.\n");
        return 1;
    }

    printf("Input: %s\n", input);

    for (i = 0; i <= strlen(input); i++) {
        unsigned char c = (unsigned char)input[i];
        if (isalnum(c) || c == '_' || c == '-') {
            if (j + 1 < sizeof(word)) {
                word[j++] = (char)c;
            }
        } else if (j > 0) {
            word[j] = '\0';
            normalize_word(word);
            j = 0;

            if (is_noise_token(word)) {
                continue;
            }
            if (word_count++ < MAX_WORDS) {
                explain_word(word, show_gloss);
            }

            {
                int pos_list[] = { NOUN, VERB, ADJ, ADV };
                int p;
                for (p = 0; p < 4; p++) {
                    int pos = pos_list[p];
                    char resolved[MAX_TERM];
                    IndexPtr idx = lookup_index(word, pos, resolved, sizeof(resolved));
                    SynsetPtr syn;

                    if (idx == NULL || idx->off_cnt == 0) {
                        if (idx != NULL) {
                            free_index(idx);
                        }
                        continue;
                    }
                    syn = read_synset(pos, idx->offset[0], resolved);
                    if (syn != NULL) {
                        collect_from_synset(terms, &term_count, syn);
                        if (use_hypernyms) {
                            collect_hypernyms(terms, &term_count, syn, pos);
                        }
                        free_synset(syn);
                    }
                    free_index(idx);
                }
            }
        }
    }

    qsort(terms, term_count, sizeof(terms[0]), compare_terms);

    printf("\nMeaning hints (top terms):\n");
    for (i = 0; i < (size_t)term_count && i < (size_t)top_n; i++) {
        printf("  %s (%d)\n", terms[i].term, terms[i].count);
    }

    return 0;
}
