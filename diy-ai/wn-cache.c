#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wn.h>

#define MAX_TERM 128
#define MAX_LIST 8
#define MAX_LINE 4096

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

static void json_escape(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;

    while (*p) {
        unsigned char c = *p++;
        switch (c) {
            case '\\': printf("\\\\"); break;
            case '"': printf("\\\""); break;
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            default:
                if (c < 32) {
                    printf("\\u%04x", c);
                } else {
                    putchar(c);
                }
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

static int add_unique(char list[][MAX_TERM], int *count, int limit, const char *value)
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
    if (*count >= limit) {
        return 0;
    }
    snprintf(list[*count], MAX_TERM, "%s", value);
    (*count)++;
    return 1;
}

static void shorten_gloss(const char *gloss, char *out, size_t out_size)
{
    size_t i = 0;
    size_t j = 0;

    if (gloss == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    while (gloss[i] != '\0' && j + 1 < out_size) {
        char c = gloss[i++];
        if (j == 0 && c == '(') {
            continue;
        }
        if (c == ';' || c == '.') {
            break;
        }
        out[j++] = c;
        if (j >= 160) {
            break;
        }
    }
    out[j] = '\0';
    while (j > 0 && isspace((unsigned char)out[j - 1])) {
        out[--j] = '\0';
    }
    if (j > 0 && out[j - 1] == ')') {
        out[j - 1] = '\0';
    }
}

static int parse_index_line(char *line, char **lemma_out, int *pos_out, long *offset_out)
{
    char *tokens[256];
    int count = 0;
    char *tok = strtok(line, " \t\r\n");

    while (tok != NULL && count < 256) {
        tokens[count++] = tok;
        tok = strtok(NULL, " \t\r\n");
    }
    if (count < 6) {
        return 0;
    }
    if (tokens[1][0] == '\0') {
        return 0;
    }
    {
        int p_cnt = atoi(tokens[3]);
        int offsets_start = 6 + p_cnt;
        if (offsets_start >= count) {
            return 0;
        }
        *lemma_out = tokens[0];
        switch (tokens[1][0]) {
            case 'n': *pos_out = NOUN; break;
            case 'v': *pos_out = VERB; break;
            case 'a': *pos_out = ADJ; break;
            case 'r': *pos_out = ADV; break;
            default: return 0;
        }
        *offset_out = atol(tokens[offsets_start]);
        return 1;
    }
}

static void emit_json_list(char list[][MAX_TERM], int count)
{
    int i;

    printf("[");
    for (i = 0; i < count; i++) {
        if (i > 0) {
            printf(", ");
        }
        printf("\"");
        json_escape(list[i]);
        printf("\"");
    }
    printf("]");
}

static void process_index_file(const char *path, int *emitted, int limit)
{
    FILE *fp = fopen(path, "r");
    char line[MAX_LINE];

    if (fp == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char buf[MAX_LINE];
        char *lemma = NULL;
        int pos = 0;
        long offset = 0;
        SynsetPtr syn;
        char gloss[MAX_TERM * 2];
        char synonyms[MAX_LIST][MAX_TERM];
        char hypernyms[MAX_LIST][MAX_TERM];
        int syn_count = 0;
        int hyper_count = 0;
        int i;

        if (line[0] == ' ' || line[0] == '\n') {
            continue;
        }
        snprintf(buf, sizeof(buf), "%s", line);
        if (!parse_index_line(buf, &lemma, &pos, &offset)) {
            continue;
        }
        syn = read_synset(pos, offset, lemma);
        if (syn == NULL) {
            continue;
        }
        for (i = 0; i < syn->wcount; i++) {
            char term[MAX_TERM];
            snprintf(term, sizeof(term), "%s", syn->words[i]);
            normalize_word(term);
            add_unique(synonyms, &syn_count, MAX_LIST, term);
        }
        for (i = 0; i < syn->ptrcount; i++) {
            if (syn->ptrtyp[i] != HYPERPTR) {
                continue;
            }
            if (syn->ppos[i] == 0) {
                continue;
            }
            SynsetPtr hyper = read_synset(syn->ppos[i], syn->ptroff[i], lemma);
            if (hyper != NULL) {
                int w;
                for (w = 0; w < hyper->wcount; w++) {
                    char term[MAX_TERM];
                    snprintf(term, sizeof(term), "%s", hyper->words[w]);
                    normalize_word(term);
                    add_unique(hypernyms, &hyper_count, MAX_LIST, term);
                }
                free_synset(hyper);
            }
        }
        gloss[0] = '\0';
        if (syn->defn != NULL) {
            shorten_gloss(syn->defn, gloss, sizeof(gloss));
        }
        if (*emitted > 0) {
            printf(",\n");
        }
        printf("  {\n");
        printf("    \"lemma\": \"");
        json_escape(lemma);
        printf("\",\n");
        printf("    \"pos\": %d,\n", pos);
        printf("    \"gloss\": \"");
        json_escape(gloss);
        printf("\",\n");
        printf("    \"synonyms\": ");
        emit_json_list(synonyms, syn_count);
        printf(",\n");
        printf("    \"hypernyms\": ");
        emit_json_list(hypernyms, hyper_count);
        printf("\n  }");
        free_synset(syn);

        (*emitted)++;
        if (limit > 0 && *emitted >= limit) {
            break;
        }
    }
    fclose(fp);
}

int main(int argc, char **argv)
{
    const char *out_path = "word_cache.json";
    int limit = 0;
    int i;
    FILE *out;
    const char *searchdir;
    char pathbuf[512];
    int emitted = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
            limit = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("wn-cache: build a WordNet cache\n");
            printf("usage: %s [--out FILE] [--limit N]\n", argv[0]);
            return 0;
        }
    }

    set_default_searchdir();
    if (wninit() != 0) {
        fprintf(stderr, "WordNet data files not found. Set WNHOME or WNSEARCHDIR.\n");
        return 1;
    }
    searchdir = getenv("WNSEARCHDIR");
    if (searchdir == NULL || searchdir[0] == '\0') {
        fprintf(stderr, "WNSEARCHDIR not set.\n");
        return 1;
    }

    out = freopen(out_path, "w", stdout);
    if (out == NULL) {
        fprintf(stderr, "Failed to open %s\n", out_path);
        return 1;
    }

    printf("{\n  \"entries\": [\n");
    snprintf(pathbuf, sizeof(pathbuf), "%s/index.noun", searchdir);
    process_index_file(pathbuf, &emitted, limit);
    if (limit <= 0 || emitted < limit) {
        snprintf(pathbuf, sizeof(pathbuf), "%s/index.verb", searchdir);
        process_index_file(pathbuf, &emitted, limit);
    }
    if (limit <= 0 || emitted < limit) {
        snprintf(pathbuf, sizeof(pathbuf), "%s/index.adj", searchdir);
        process_index_file(pathbuf, &emitted, limit);
    }
    if (limit <= 0 || emitted < limit) {
        snprintf(pathbuf, sizeof(pathbuf), "%s/index.adv", searchdir);
        process_index_file(pathbuf, &emitted, limit);
    }
    printf("\n  ],\n  \"count\": %d\n}\n", emitted);

    fclose(out);
    return 0;
}
