#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wn.h>

#include "json_extract.h"

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
    char alt_languages[MAX_LIST][MAX_TERM];
    int alt_language_count;
    char alt_platforms[MAX_LIST][MAX_TERM];
    int alt_platform_count;
    char alt_frameworks[MAX_LIST][MAX_TERM];
    int alt_framework_count;
    struct {
        char term[MAX_TERM];
        int count;
    } terms[MAX_TERMS];
    int term_count;
    int turns;
    char last_entity[MAX_TERM];
    char last_action[MAX_TERM];
    int last_variant;
};

struct related_term {
    char term[MAX_TERM];
    char gloss[MAX_GLOSS];
    char synonyms[MAX_LIST][MAX_TERM];
    int synonym_count;
    char hypernyms[MAX_LIST][MAX_TERM];
    int hypernym_count;
};

struct analysis_result {
    char actions[MAX_LIST][MAX_TERM];
    int action_count;
    char entities[MAX_LIST][MAX_TERM];
    int entity_count;
    char qualifiers[MAX_LIST][MAX_TERM];
    int qualifier_count;
    struct related_term related[MAX_LIST];
    int related_count;
    char sdlc_focus[2][MAX_TERM];
    int sdlc_focus_count;
    char design_focus[3][MAX_TERM];
    int design_focus_count;
    int is_question;
    int is_preference_question;
    int domain_score;
    int has_greeting;
};

static int is_noise_token(const char *word);

static const char *stopwords[] = {
    "a", "an", "and", "are", "as", "at", "be", "but", "by",
    "for", "from", "in", "is", "it", "of", "on", "or", "the",
    "to", "was", "were", "with", "me", "my", "your", "our",
    "should", "want", "need", "please", "make", "do", "write", "like",
    "love", "yeah", "yea", "nice", "today", "would", "could", "can",
    NULL
};

static const char *generic_verbs[] = {
    "make", "do", "write", "build", "create", "implement",
    "develop", "add", "use", "target", "support", "provide",
    "like", "love", "want", "need", "think", NULL
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

static void add_alt(char list[][MAX_TERM], int *count, const char *value)
{
    add_unique(list, count, MAX_LIST, value);
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
    return add_unique(concept->terms, &concept->term_count, MAX_CONCEPT_TERMS, buf);
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

static int is_allowed_action(const char *word)
{
    const char *allowed[] = {
        "build", "make", "create", "implement", "design", "plan",
        "draft", "write", "develop", "test", "deploy", "outline",
        "prototype", "code", NULL
    };
    int i;

    if (word == NULL || word[0] == '\0') {
        return 0;
    }
    for (i = 0; allowed[i] != NULL; i++) {
        if (strcmp(word, allowed[i]) == 0) {
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

static int term_frequency(struct chat_context *ctx, const char *term)
{
    int i;

    if (term == NULL || term[0] == '\0') {
        return 0;
    }
    for (i = 0; i < ctx->term_count; i++) {
        if (strcmp(ctx->terms[i].term, term) == 0) {
            return ctx->terms[i].count;
        }
    }
    return 0;
}

static int related_match_score(struct analysis_result *analysis, const char *term)
{
    int i;
    int j;

    for (i = 0; i < analysis->related_count; i++) {
        struct related_term *rel = &analysis->related[i];
        for (j = 0; j < rel->synonym_count; j++) {
            if (strcmp(rel->synonyms[j], term) == 0) {
                return 2;
            }
        }
        for (j = 0; j < rel->hypernym_count; j++) {
            if (strcmp(rel->hypernyms[j], term) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static double score_entity(struct chat_context *ctx, struct analysis_result *analysis, const char *term)
{
    int freq = term_frequency(ctx, term);
    int rel = related_match_score(analysis, term);
    double score = 1.0;

    if (freq > 0) {
        score += (double)freq * 0.5;
    }
    if (rel > 0) {
        score += (double)rel * 0.75;
    }
    return score;
}

static const char *top_scored_entity(struct chat_context *ctx,
                                     struct analysis_result *analysis,
                                     double *out_prob)
{
    int i;
    double best = 0.0;
    double total = 0.0;
    const char *best_term = NULL;

    for (i = 0; i < analysis->entity_count; i++) {
        double score = score_entity(ctx, analysis, analysis->entities[i]);
        total += score;
        if (score > best) {
            best = score;
            best_term = analysis->entities[i];
        }
    }
    if (out_prob != NULL) {
        if (total > 0.0 && best_term != NULL) {
            *out_prob = best / total;
        } else {
            *out_prob = 0.0;
        }
    }
    return best_term;
}

static const char *top_action(struct analysis_result *analysis)
{
    if (analysis->action_count > 0) {
        return analysis->actions[0];
    }
    return NULL;
}

static void append_defaults_for(char *buf, size_t buf_size,
                                const char *language,
                                const char *platform,
                                const char *framework)
{
    size_t used = strlen(buf);

    if (language != NULL && language[0] != '\0') {
        snprintf(buf + used, buf_size - used, " in %s", language);
        used = strlen(buf);
    }
    if (platform != NULL && platform[0] != '\0') {
        snprintf(buf + used, buf_size - used, " on %s", platform);
        used = strlen(buf);
    }
    if (framework != NULL && framework[0] != '\0') {
        snprintf(buf + used, buf_size - used, " with %s", framework);
    }
}

static void normalize_spaces(char *text)
{
    size_t i = 0;
    size_t j = 0;
    int last_space = 1;

    while (text[i] != '\0') {
        char c = text[i++];
        if (isspace((unsigned char)c)) {
            if (!last_space) {
                text[j++] = ' ';
                last_space = 1;
            }
        } else {
            text[j++] = c;
            last_space = 0;
        }
    }
    if (j > 0 && text[j - 1] == ' ') {
        j--;
    }
    text[j] = '\0';
}

static void capitalize_sentence(char *text)
{
    size_t i;

    for (i = 0; text[i] != '\0'; i++) {
        if (isalpha((unsigned char)text[i])) {
            text[i] = (char)toupper((unsigned char)text[i]);
            break;
        }
    }
}

static int is_response_empty(const char *text)
{
    size_t i;

    for (i = 0; text[i] != '\0'; i++) {
        if (!isspace((unsigned char)text[i])) {
            return 0;
        }
    }
    return 1;
}

static int sentence_has_verb(const char *text)
{
    char token[MAX_TERM];
    size_t i = 0;
    size_t j = 0;

    while (text[i] != '\0') {
        unsigned char c = (unsigned char)text[i++];
        if (isalnum(c) || c == '_' || c == '-') {
            if (j + 1 < sizeof(token)) {
                token[j++] = (char)tolower(c);
            }
        } else if (j > 0) {
            IndexPtr idx;

            token[j] = '\0';
            j = 0;
            idx = getindex(token, VERB);
            if (idx != NULL && idx->off_cnt > 0) {
                free_index(idx);
                return 1;
            }
            if (idx != NULL) {
                free_index(idx);
            }
        }
    }
    if (j > 0) {
        IndexPtr idx;

        token[j] = '\0';
        idx = getindex(token, VERB);
        if (idx != NULL && idx->off_cnt > 0) {
            free_index(idx);
            return 1;
        }
        if (idx != NULL) {
            free_index(idx);
        }
    }
    return 0;
}

static int validate_response(const char *text)
{
    size_t len;

    if (is_response_empty(text)) {
        return 0;
    }
    len = strlen(text);
    if (len < 6 || len > 420) {
        return 0;
    }
    if (!sentence_has_verb(text)) {
        return 0;
    }
    if (strstr(text, "??") != NULL || strstr(text, "!!") != NULL) {
        return 0;
    }
    return 1;
}

static int apply_guardrails(char *text, size_t text_size)
{
    size_t len;

    if (text_size == 0) {
        return 0;
    }
    normalize_spaces(text);
    if (is_response_empty(text)) {
        snprintf(text, text_size, "I can help with that, but I need a little more detail");
        return 1;
    }
    if (!sentence_has_verb(text)) {
        char prefix[64];
        char combined[256];

        snprintf(prefix, sizeof(prefix), "I can help with that.");
        snprintf(combined, sizeof(combined), "%s %s", prefix, text);
        snprintf(text, text_size, "%s", combined);
    }
    capitalize_sentence(text);
    len = strlen(text);
    if (len > 0 && text[len - 1] != '.' && text[len - 1] != '!' && text[len - 1] != '?') {
        if (len + 1 < text_size) {
            text[len] = '.';
            text[len + 1] = '\0';
        }
    }
    return validate_response(text);
}

static const char *top_memory_term(struct chat_context *ctx)
{
    int i;
    int best = -1;

    for (i = 0; i < ctx->term_count; i++) {
        if (best == -1 || ctx->terms[i].count > ctx->terms[best].count) {
            best = i;
        }
    }
    if (best == -1) {
        return NULL;
    }
    return ctx->terms[best].term;
}

static const char *pick_related_term(struct analysis_result *analysis,
                                     const char *primary,
                                     const char *kind,
                                     int *out_count)
{
    int i;

    if (out_count != NULL) {
        *out_count = 0;
    }
    if (primary == NULL) {
        return NULL;
    }
    for (i = 0; i < analysis->related_count; i++) {
        if (strcmp(analysis->related[i].term, primary) == 0) {
            if (strcmp(kind, "synonym") == 0 && analysis->related[i].synonym_count > 0) {
                if (out_count != NULL) {
                    *out_count = analysis->related[i].synonym_count;
                }
                return analysis->related[i].synonyms[0];
            }
            if (strcmp(kind, "hypernym") == 0 && analysis->related[i].hypernym_count > 0) {
                if (out_count != NULL) {
                    *out_count = analysis->related[i].hypernym_count;
                }
                return analysis->related[i].hypernyms[0];
            }
        }
    }
    return NULL;
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
        if (j >= 140) {
            break;
        }
    }
    out[j] = '\0';
    normalize_spaces(out);
    if (j > 0 && out[j - 1] == ')') {
        out[j - 1] = '\0';
    }
}

static const char *persona_prefix(int turn, int variant)
{
    if (turn == 1) {
        if ((turn + variant) % 3 == 0) {
            const char *val = get_chat_string("persona_hey");
            return val ? val : "Hey, I'm WN-Guide.";
        }
        if ((turn + variant) % 3 == 1) {
            const char *val = get_chat_string("persona_alright");
            return val ? val : "Alright, I'm WN-Guide.";
        }
        {
            const char *val = get_chat_string("persona_hi");
            return val ? val : "Hi, I'm WN-Guide.";
        }
    }
    {
        const char *val = get_chat_string("persona_empty");
        return val ? val : "";
    }
}

static void synthesize_response(struct chat_context *ctx, struct analysis_result *analysis,
                                char *out, size_t out_size, double *out_prob)
{
    const char *primary_entity = top_scored_entity(ctx, analysis, out_prob);
    const char *primary_action = top_action(analysis);
    const char *fallback_action = NULL;
    int language_only = 0;
    int platform_only = 0;
    const char *secondary_entity = NULL;
    const char *memory_term = top_memory_term(ctx);
    const char *synonym = NULL;
    const char *hypernym = NULL;
    const char *gloss = NULL;
    char gloss_short[160];
    char sentence[256];
    char candidate[512];
    size_t used = 0;
    int synonym_count = 0;
    int hypernym_count = 0;
    int i;
    int variant;
    int ok = 0;

    if (out_size == 0) {
        return;
    }

    if (analysis->has_greeting) {
        const char *prefix = persona_prefix(ctx->turns, 0);
        if (prefix[0] != '\0') {
            const char *val = get_chat_string("greet_with_persona");
            if (val != NULL) {
                snprintf(out, out_size, "%s %s", prefix, val);
            } else {
                snprintf(out, out_size, "%s Great to see you. What are you working on?", prefix);
            }
        } else {
            const char *val = get_chat_string("greet_plain");
            if (val != NULL) {
                snprintf(out, out_size, "%s", val);
            } else {
                snprintf(out, out_size, "Great to see you. What are you working on?");
            }
        }
        apply_guardrails(out, out_size);
        return;
    }

    if (analysis->is_question) {
        primary_action = NULL;
    }
    if (!is_allowed_action(primary_action)) {
        primary_action = NULL;
    }
    if (ctx->language[0] != '\0' && primary_entity != NULL &&
        strcmp(primary_entity, ctx->language) == 0 &&
        analysis->entity_count <= 1) {
        language_only = 1;
        primary_entity = NULL;
    }
    if (!language_only && ctx->platform[0] != '\0' && primary_entity != NULL &&
        strcmp(primary_entity, ctx->platform) == 0 &&
        analysis->entity_count <= 1 && primary_action == NULL) {
        platform_only = 1;
        primary_entity = NULL;
    }
    if (primary_action == NULL && analysis->domain_score > 0 && !analysis->is_question) {
        fallback_action = "build";
        if (primary_entity != NULL) {
            if (strcmp(primary_entity, "plan") == 0 || strcmp(primary_entity, "outline") == 0) {
                fallback_action = "outline";
            } else if (strcmp(primary_entity, "test") == 0 || strcmp(primary_entity, "testing") == 0) {
                fallback_action = "test";
            }
        }
        primary_action = fallback_action;
    }

    if (primary_entity != NULL) {
        synonym = pick_related_term(analysis, primary_entity, "synonym", &synonym_count);
        hypernym = pick_related_term(analysis, primary_entity, "hypernym", &hypernym_count);
    }
    if (analysis->entity_count > 1) {
        int e;
        for (e = 0; e < analysis->entity_count; e++) {
            if (primary_entity == NULL || strcmp(analysis->entities[e], primary_entity) != 0) {
                secondary_entity = analysis->entities[e];
                break;
            }
        }
    }
    for (i = 0; i < analysis->related_count; i++) {
        if (analysis->related[i].gloss[0] != '\0') {
            gloss = analysis->related[i].gloss;
            break;
        }
    }
    gloss_short[0] = '\0';
    if (gloss != NULL) {
        shorten_gloss(gloss, gloss_short, sizeof(gloss_short));
    }

    for (variant = 0; variant < 4 && !ok; variant++) {
        int pick = (ctx->last_variant + 1 + variant) % 4;
        candidate[0] = '\0';
        used = 0;

        if (persona_prefix(ctx->turns, pick)[0] != '\0') {
            snprintf(candidate + used, sizeof(candidate) - used, "%s ", persona_prefix(ctx->turns, pick));
            used = strlen(candidate);
        }

        if (analysis->is_preference_question && primary_entity != NULL) {
            const char *val = get_chat_string("pref_answer");
            if (val != NULL) {
                snprintf(sentence, sizeof(sentence), "%s %s", val, primary_entity);
            } else {
                snprintf(sentence, sizeof(sentence), "I don't have preferences, but I can help with %s", primary_entity);
            }
        } else if (language_only) {
            const char *val = get_chat_string("set_language");
            if (val != NULL) {
                snprintf(sentence, sizeof(sentence), "%s %s", val, ctx->language);
            } else {
                snprintf(sentence, sizeof(sentence), "Got it. We'll use %s", ctx->language);
            }
        } else if (platform_only) {
            const char *val = get_chat_string("set_platform");
            if (val != NULL) {
                snprintf(sentence, sizeof(sentence), "%s %s", val, ctx->platform);
            } else {
                snprintf(sentence, sizeof(sentence), "Got it. We'll target %s", ctx->platform);
            }
        } else if (primary_action != NULL && primary_entity != NULL) {
            if (pick == 0) {
                const char *val = get_chat_string("action_got_it");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s %s %s", val, primary_action, primary_entity);
                } else {
                    snprintf(sentence, sizeof(sentence), "Got it. You want to %s %s", primary_action, primary_entity);
                }
            } else if (pick == 1) {
                const char *val = get_chat_string("action_sounds_like");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s %s %s", val, primary_action, primary_entity);
                } else {
                    snprintf(sentence, sizeof(sentence), "Sounds like you want to %s %s", primary_action, primary_entity);
                }
            } else if (pick == 2) {
                const char *val = get_chat_string("action_okay");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s %s %s", val, primary_action, primary_entity);
                } else {
                    snprintf(sentence, sizeof(sentence), "Okay, let's %s %s", primary_action, primary_entity);
                }
            } else {
                const char *val = get_chat_string("action_all_right");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s %s %s", val, primary_action, primary_entity);
                } else {
                    snprintf(sentence, sizeof(sentence), "All right. We'll %s %s", primary_action, primary_entity);
                }
            }
        } else if (primary_entity != NULL) {
            if (pick == 2) {
                if (analysis->is_question) {
                    const char *val = get_chat_string("focus_asking");
                    if (val != NULL) {
                        snprintf(sentence, sizeof(sentence), "%s %s", val, primary_entity);
                    } else {
                        snprintf(sentence, sizeof(sentence), "You're asking about %s", primary_entity);
                    }
                } else {
                    const char *val = get_chat_string("focus_circling");
                    if (val != NULL) {
                        snprintf(sentence, sizeof(sentence), "%s %s", val, primary_entity);
                    } else {
                        snprintf(sentence, sizeof(sentence), "You're circling around %s", primary_entity);
                    }
                }
            } else if (pick == 3) {
                const char *val = get_chat_string("focus_mix");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s %s", val, primary_entity);
                } else {
                    snprintf(sentence, sizeof(sentence), "I hear %s in the mix", primary_entity);
                }
            } else {
                if (analysis->is_question) {
                    const char *val = get_chat_string("focus_curious");
                    if (val != NULL) {
                        snprintf(sentence, sizeof(sentence), "%s %s", val, primary_entity);
                    } else {
                        snprintf(sentence, sizeof(sentence), "Sounds like you're curious about %s", primary_entity);
                    }
                } else {
                    const char *val = get_chat_string("focus_picking");
                    if (val != NULL) {
                        snprintf(sentence, sizeof(sentence), "%s %s", val, primary_entity);
                    } else {
                        snprintf(sentence, sizeof(sentence), "I'm picking up a focus on %s", primary_entity);
                    }
                }
            }
        } else {
            if (pick == 3) {
                const char *val = get_chat_string("fallback_help_alt");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s", val);
                } else {
                    snprintf(sentence, sizeof(sentence), "I'm here to help");
                }
            } else {
                const char *val = get_chat_string("fallback_help");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s", val);
                } else {
                    snprintf(sentence, sizeof(sentence), "I can help with that");
                }
            }
        }
        if (!language_only && !platform_only) {
            const char *lang = ctx->language[0] ? ctx->language : NULL;
            const char *plat = ctx->platform[0] ? ctx->platform : NULL;
            const char *fw = ctx->framework[0] ? ctx->framework : NULL;

            if (primary_entity != NULL) {
                if (lang != NULL && strcmp(primary_entity, lang) == 0) {
                    lang = NULL;
                }
                if (plat != NULL && strcmp(primary_entity, plat) == 0) {
                    plat = NULL;
                }
                if (fw != NULL && strcmp(primary_entity, fw) == 0) {
                    fw = NULL;
                }
            }
            append_defaults_for(sentence, sizeof(sentence), lang, plat, fw);
        }
        snprintf(candidate + used, sizeof(candidate) - used, "%s. ", sentence);
        used = strlen(candidate);

        if (!analysis->is_preference_question &&
            gloss_short[0] != '\0' && primary_entity != NULL &&
            analysis->domain_score > 0 && *out_prob >= 0.35) {
            if (variant == 1) {
                const char *val = get_chat_string("gloss_quick");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s %s %s", val, primary_entity, gloss_short);
                } else {
                    snprintf(sentence, sizeof(sentence), "Quick meaning: %s means %s", primary_entity, gloss_short);
                }
            } else {
                const char *val = get_chat_string("gloss_plain");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s %s %s", val, primary_entity, gloss_short);
                } else {
                    snprintf(sentence, sizeof(sentence), "In plain terms, %s means %s", primary_entity, gloss_short);
                }
            }
            snprintf(candidate + used, sizeof(candidate) - used, "%s. ", sentence);
            used = strlen(candidate);
        } else if (secondary_entity != NULL && analysis->domain_score > 0 && pick == 3) {
            const char *val = get_chat_string("gloss_secondary");
            if (val != NULL) {
                snprintf(sentence, sizeof(sentence), "%s %s", val, secondary_entity);
            } else {
                snprintf(sentence, sizeof(sentence), "Also heard %s", secondary_entity);
            }
            snprintf(candidate + used, sizeof(candidate) - used, "%s. ", sentence);
            used = strlen(candidate);
        } else if (hypernym != NULL) {
            const char *val = get_chat_string("gloss_hypernym");
            if (val != NULL) {
                snprintf(sentence, sizeof(sentence), "%s %s", val, hypernym);
            } else {
                snprintf(sentence, sizeof(sentence), "That sounds like a kind of %s", hypernym);
            }
            snprintf(candidate + used, sizeof(candidate) - used, "%s. ", sentence);
            used = strlen(candidate);
        } else if (synonym != NULL && strcmp(synonym, primary_entity) != 0) {
            const char *val = get_chat_string("gloss_synonym");
            if (val != NULL) {
                snprintf(sentence, sizeof(sentence), "%s %s", val, synonym);
            } else {
                snprintf(sentence, sizeof(sentence), "You might also mean %s", synonym);
            }
            snprintf(candidate + used, sizeof(candidate) - used, "%s. ", sentence);
            used = strlen(candidate);
        }

        if (memory_term != NULL && primary_entity != NULL && strcmp(memory_term, primary_entity) != 0 && variant == 2) {
            snprintf(sentence, sizeof(sentence), "We've been circling around %s too", memory_term);
            snprintf(candidate + used, sizeof(candidate) - used, "%s. ", sentence);
            used = strlen(candidate);
        }

        if (analysis->domain_score > 0) {
            if (ctx->language[0] == '\0' || ctx->platform[0] == '\0') {
                if (ctx->language[0] == '\0' && ctx->platform[0] == '\0') {
                    const char *val = get_chat_string("ask_language_platform");
                    if (val != NULL) {
                        snprintf(sentence, sizeof(sentence), "%s", val);
                    } else {
                        snprintf(sentence, sizeof(sentence), "Any preference for language or platform");
                    }
                } else if (ctx->language[0] == '\0') {
                    const char *val = get_chat_string("ask_language");
                    if (val != NULL) {
                        snprintf(sentence, sizeof(sentence), "%s", val);
                    } else {
                        snprintf(sentence, sizeof(sentence), "Which language should I use");
                    }
                } else {
                    const char *val = get_chat_string("ask_platform");
                    if (val != NULL) {
                        snprintf(sentence, sizeof(sentence), "%s", val);
                    } else {
                        snprintf(sentence, sizeof(sentence), "Should this be a CLI, service, library, or UI");
                    }
                }
            } else {
                const char *val = get_chat_string("ask_plan_or_example");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s", val);
                } else {
                    snprintf(sentence, sizeof(sentence), "Want me to draft a quick plan or jump into an example");
                }
            }
            snprintf(candidate + used, sizeof(candidate) - used, "%s? ", sentence);
        } else {
            if (analysis->is_question) {
                const char *val = get_chat_string("ask_software_focus");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s", val);
                } else {
                    snprintf(sentence, sizeof(sentence), "I focus on software projects. What would you like to build");
                }
            } else {
                const char *val = get_chat_string("ask_next");
                if (val != NULL) {
                    snprintf(sentence, sizeof(sentence), "%s", val);
                } else {
                    snprintf(sentence, sizeof(sentence), "What would you like to do next");
                }
            }
            snprintf(candidate + used, sizeof(candidate) - used, "%s? ", sentence);
        }

        ok = apply_guardrails(candidate, sizeof(candidate));
        if (ok) {
            snprintf(out, out_size, "%s", candidate);
            ctx->last_variant = pick;
            snprintf(ctx->last_entity, sizeof(ctx->last_entity), "%s", primary_entity ? primary_entity : "");
            snprintf(ctx->last_action, sizeof(ctx->last_action), "%s", primary_action ? primary_action : "");
            return;
        }
    }

    snprintf(out, out_size, "Hi, I'm WN-Guide. Tell me what you want to build and I will help.");
    apply_guardrails(out, out_size);
}

static void rank_concepts(struct concept *concepts, int concept_count,
                          char list[][MAX_TERM], int list_count,
                          struct chat_context *ctx,
                          struct analysis_result *analysis)
{
    int i;
    int j;

    for (i = 0; i < concept_count; i++) {
        concepts[i].score = 0;
    }
    for (i = 0; i < list_count; i++) {
        for (j = 0; j < concept_count; j++) {
            if (token_matches(list[i], &concepts[j])) {
                int freq = term_frequency(ctx, list[i]);
                int boost = freq > 4 ? 2 : (freq > 0 ? 1 : 0);
                concepts[j].score += 2 + boost;
            }
        }
    }
    for (j = 0; j < concept_count; j++) {
        int k;
        for (k = 0; k < concepts[j].term_count; k++) {
            int rel_score = related_match_score(analysis, concepts[j].terms[k]);
            if (rel_score > 0) {
                concepts[j].score += rel_score;
            }
        }
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
            add_unique(ctx->actions, &ctx->action_count, MAX_LIST, actions[i]);
        }
    }
    if (entity_count > 0) {
        for (i = 0; i < entity_count; i++) {
            add_unique(ctx->entities, &ctx->entity_count, MAX_LIST, entities[i]);
        }
    }
    if (qualifier_count > 0) {
        for (i = 0; i < qualifier_count; i++) {
            add_unique(ctx->qualifiers, &ctx->qualifier_count, MAX_LIST, qualifiers[i]);
        }
    }
    if (language[0] != '\0') {
        if (ctx->language[0] == '\0' || language_score > ctx->language_score) {
            snprintf(ctx->language, sizeof(ctx->language), "%s", language);
            ctx->language_score = language_score;
        } else if (strcmp(ctx->language, language) != 0) {
            add_alt(ctx->alt_languages, &ctx->alt_language_count, language);
        }
    }
    if (platform[0] != '\0') {
        if (ctx->platform[0] == '\0' || platform_score > ctx->platform_score) {
            snprintf(ctx->platform, sizeof(ctx->platform), "%s", platform);
            ctx->platform_score = platform_score;
        } else if (strcmp(ctx->platform, platform) != 0) {
            add_alt(ctx->alt_platforms, &ctx->alt_platform_count, platform);
        }
    }
    if (framework[0] != '\0') {
        if (ctx->framework[0] == '\0' || framework_score > ctx->framework_score) {
            snprintf(ctx->framework, sizeof(ctx->framework), "%s", framework);
            ctx->framework_score = framework_score;
        } else if (strcmp(ctx->framework, framework) != 0) {
            add_alt(ctx->alt_frameworks, &ctx->alt_framework_count, framework);
        }
    }
}

static void init_analysis(struct analysis_result *analysis)
{
    memset(analysis, 0, sizeof(*analysis));
}

static struct related_term *find_or_add_related(struct analysis_result *analysis, const char *term)
{
    int i;

    for (i = 0; i < analysis->related_count; i++) {
        if (strcmp(analysis->related[i].term, term) == 0) {
            return &analysis->related[i];
        }
    }
    if (analysis->related_count >= MAX_LIST) {
        return NULL;
    }
    snprintf(analysis->related[analysis->related_count].term, MAX_TERM, "%s", term);
    analysis->related[analysis->related_count].gloss[0] = '\0';
    return &analysis->related[analysis->related_count++];
}

static void fetch_related_terms(const char *term, int pos, SynsetPtr syn,
                                struct analysis_result *analysis,
                                struct chat_context *ctx)
{
    int i;
    struct related_term *related = find_or_add_related(analysis, term);

    if (related == NULL || syn == NULL) {
        return;
    }
    for (i = 0; i < syn->wcount; i++) {
        char synword[MAX_TERM];
        snprintf(synword, sizeof(synword), "%s", syn->words[i]);
        normalize_word(synword);
        if (!is_noise_token(synword)) {
            add_unique(related->synonyms, &related->synonym_count, MAX_LIST, synword);
        }
    }
    if (syn->defn != NULL && related->gloss[0] == '\0') {
        snprintf(related->gloss, sizeof(related->gloss), "%s", syn->defn);
    }
    for (i = 0; i < syn->ptrcount; i++) {
        if (syn->ptrtyp[i] != HYPERPTR) {
            continue;
        }
        if (syn->ppos[i] == 0) {
            continue;
        }
        SynsetPtr hyper = read_synset(syn->ppos[i], syn->ptroff[i], term);
        if (hyper != NULL) {
            int w;
            for (w = 0; w < hyper->wcount; w++) {
                char hyperword[MAX_TERM];
                snprintf(hyperword, sizeof(hyperword), "%s", hyper->words[w]);
                normalize_word(hyperword);
                if (!is_noise_token(hyperword)) {
                    add_unique(related->hypernyms, &related->hypernym_count, MAX_LIST, hyperword);
                    add_term_count(ctx, hyperword, 1);
                }
            }
            free_synset(hyper);
        }
    }
    (void)pos;
}

static void capture_top_concepts(struct concept *concepts, int concept_count,
                                 const char *type, char list[][MAX_TERM], int *out_count, int max_count)
{
    int i;
    int printed = 0;

    *out_count = 0;
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
        snprintf(list[printed], MAX_TERM, "%s", concepts[best].name);
        concepts[best].score = -1;
        printed++;
    }
    *out_count = printed;
}

static void analyze_input(const char *input, struct chat_context *ctx, struct analysis_result *analysis)
{
    struct concept concepts[MAX_CONCEPTS];
    int concept_count = 0;
    char language[MAX_TERM];
    char platform[MAX_TERM];
    char framework[MAX_TERM];
    int language_score = 0;
    int platform_score = 0;
    int framework_score = 0;
    char token[MAX_TERM];
    size_t i;
    size_t j = 0;

    init_analysis(analysis);
    init_concepts(concepts, &concept_count);
    ctx->turns++;

    if (strchr(input, '?') != NULL) {
        analysis->is_question = 1;
    }

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
            if (analysis->is_question &&
                (strcmp(normalized, "like") == 0 || strcmp(normalized, "enjoy") == 0)) {
                analysis->is_preference_question = 1;
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
                    add_unique(analysis->actions, &analysis->action_count, MAX_LIST, lemma);
                } else if (pos == NOUN) {
                    add_unique(analysis->entities, &analysis->entity_count, MAX_LIST, lemma);
                } else if (pos == ADJ || pos == ADV) {
                    add_unique(analysis->qualifiers, &analysis->qualifier_count, MAX_LIST, lemma);
                }
                syn = read_synset(pos, idx->offset[0], lemma);
                if (syn != NULL) {
                    int w;
                    for (w = 0; w < syn->wcount; w++) {
                        char synword[MAX_TERM];
                        snprintf(synword, sizeof(synword), "%s", syn->words[w]);
                        normalize_word(synword);
                        if (!is_noise_token(synword)) {
                            if (pos == NOUN) {
                                add_unique(analysis->entities, &analysis->entity_count, MAX_LIST, synword);
                            } else if (pos == ADJ || pos == ADV) {
                                add_unique(analysis->qualifiers, &analysis->qualifier_count, MAX_LIST, synword);
                            } else if (pos == VERB) {
                                add_unique(analysis->actions, &analysis->action_count, MAX_LIST, synword);
                            }
                        }
                    }
                    fetch_related_terms(normalized, pos, syn, analysis, ctx);
                    collect_memory_from_synset(ctx, syn);
                    free_synset(syn);
                }
                free_index(idx);
            }
            if (!matched_pos) {
                add_unique(analysis->entities, &analysis->entity_count, MAX_LIST, normalized);
            }
        }
    }

    if (analysis->action_count > 0) {
        int filtered = 0;
        for (i = 0; i < (size_t)analysis->action_count; i++) {
            if (!is_generic_verb(analysis->actions[i])) {
                snprintf(analysis->actions[filtered], MAX_TERM, "%s", analysis->actions[i]);
                filtered++;
            }
        }
        analysis->action_count = filtered;
    }

    language_score = extract_language(analysis->entities, analysis->entity_count, language, sizeof(language));
    platform_score = extract_platform(analysis->entities, analysis->entity_count, platform, sizeof(platform));
    framework_score = extract_framework(analysis->entities, analysis->entity_count, framework, sizeof(framework));
    rank_concepts(concepts, concept_count, analysis->entities, analysis->entity_count, ctx, analysis);
    merge_context(ctx, analysis->actions, analysis->action_count, analysis->entities, analysis->entity_count,
                  analysis->qualifiers, analysis->qualifier_count,
                  language, language_score,
                  platform, platform_score,
                  framework, framework_score);
    capture_top_concepts(concepts, concept_count, "sdlc",
                         analysis->sdlc_focus, &analysis->sdlc_focus_count, 2);
    capture_top_concepts(concepts, concept_count, "design",
                         analysis->design_focus, &analysis->design_focus_count, 3);

    if (language_score > 0 || platform_score > 0 || framework_score > 0) {
        analysis->domain_score += 1;
    }
    if (analysis->sdlc_focus_count > 0 || analysis->design_focus_count > 0) {
        analysis->domain_score += 2;
    }
    for (i = 0; i < (size_t)analysis->entity_count; i++) {
        const char *term = analysis->entities[i];
        if (strcmp(term, "code") == 0 || strcmp(term, "software") == 0 ||
            strcmp(term, "app") == 0 || strcmp(term, "application") == 0 ||
            strcmp(term, "program") == 0 || strcmp(term, "game") == 0 ||
            strcmp(term, "cli") == 0 ||
            strcmp(term, "api") == 0 || strcmp(term, "server") == 0 ||
            strcmp(term, "service") == 0 || strcmp(term, "ui") == 0 ||
            strcmp(term, "gui") == 0 || strcmp(term, "database") == 0 ||
            strcmp(term, "library") == 0) {
            analysis->domain_score += 1;
        }
        if (strcmp(term, "hello") == 0 || strcmp(term, "hi") == 0 ||
            strcmp(term, "hey") == 0 || strcmp(term, "greeting") == 0) {
            analysis->has_greeting = 1;
        }
    }
    if (analysis->action_count > 0 && !analysis->is_question) {
        analysis->domain_score += 1;
    }
    if (!analysis->has_greeting) {
        for (i = 0; i < analysis->related_count; i++) {
            int h;
            for (h = 0; h < analysis->related[i].hypernym_count; h++) {
                if (strcmp(analysis->related[i].hypernyms[h], "greeting") == 0 ||
                    strcmp(analysis->related[i].hypernyms[h], "salutation") == 0) {
                    analysis->has_greeting = 1;
                    break;
                }
            }
            if (analysis->has_greeting) {
                break;
            }
        }
    }
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
    if (ctx->alt_language_count > 0) {
        int i;
        printf("  alternatives: ");
        for (i = 0; i < ctx->alt_language_count; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("%s", ctx->alt_languages[i]);
        }
        printf("\n");
    }
    printf("- platform: %s\n", ctx->platform[0] ? ctx->platform : "(unspecified)");
    if (ctx->platform[0]) {
        printf("  confidence: %d\n", ctx->platform_score);
    }
    if (ctx->alt_platform_count > 0) {
        int i;
        printf("  alternatives: ");
        for (i = 0; i < ctx->alt_platform_count; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("%s", ctx->alt_platforms[i]);
        }
        printf("\n");
    }
    printf("- framework: %s\n", ctx->framework[0] ? ctx->framework : "(unspecified)");
    if (ctx->framework[0]) {
        printf("  confidence: %d\n", ctx->framework_score);
    }
    if (ctx->alt_framework_count > 0) {
        int i;
        printf("  alternatives: ");
        for (i = 0; i < ctx->alt_framework_count; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("%s", ctx->alt_frameworks[i]);
        }
        printf("\n");
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

static void print_related_terms(struct analysis_result *analysis)
{
    int i;
    int shown = 0;

    for (i = 0; i < analysis->related_count && shown < 3; i++) {
        struct related_term *rel = &analysis->related[i];
        if (rel->term[0] == '\0') {
            continue;
        }
        printf("- %s", rel->term);
        if (rel->gloss[0] != '\0') {
            printf(": %s", rel->gloss);
        }
        printf("\n");
        if (rel->synonym_count > 0) {
            int s;
            printf("  synonyms: ");
            for (s = 0; s < rel->synonym_count && s < 5; s++) {
                if (s > 0) {
                    printf(", ");
                }
                printf("%s", rel->synonyms[s]);
            }
            printf("\n");
        }
        if (rel->hypernym_count > 0) {
            int h;
            printf("  hypernyms: ");
            for (h = 0; h < rel->hypernym_count && h < 4; h++) {
                if (h > 0) {
                    printf(", ");
                }
                printf("%s", rel->hypernyms[h]);
            }
            printf("\n");
        }
        shown++;
    }
}

static void generate_response(struct chat_context *ctx, struct analysis_result *analysis)
{
    double top_prob = 0.0;
    char reply[512];

    printf("\n");
    synthesize_response(ctx, analysis, reply, sizeof(reply), &top_prob);
    printf("%s\n", reply);
    if (top_prob > 0.0) {
        printf("I'm about %.0f%% confident.\n", top_prob * 100.0);
    }
    if (ctx->alt_language_count > 0 || ctx->alt_platform_count > 0 || ctx->alt_framework_count > 0) {
        printf("I also saw hints about ");
        if (ctx->alt_language_count > 0) {
            printf("languages %s", ctx->alt_languages[0]);
        }
        if (ctx->alt_platform_count > 0) {
            if (ctx->alt_language_count > 0) {
                printf(", ");
            }
            printf("platforms %s", ctx->alt_platforms[0]);
        }
        if (ctx->alt_framework_count > 0) {
            if (ctx->alt_language_count > 0 || ctx->alt_platform_count > 0) {
                printf(", ");
            }
            printf("frameworks %s", ctx->alt_frameworks[0]);
        }
        printf(".\n");
    }
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
    printf("  /json        Print current context as JSON\n");
    printf("  /reset       Clear memory and context\n");
    printf("\n");
    printf("examples:\n");
    printf("  build a CLI that parses log files\n");
    printf("  add retries and backoff for failed requests\n");
}

static void print_json_context(struct chat_context *ctx)
{
    int i;

    printf("{\n");
    printf("  \"turns\": %d,\n", ctx->turns);
    printf("  \"actions\": [");
    for (i = 0; i < ctx->action_count; i++) {
        if (i > 0) {
            printf(", ");
        }
        printf("\"%s\"", ctx->actions[i]);
    }
    printf("],\n");
    printf("  \"entities\": [");
    for (i = 0; i < ctx->entity_count; i++) {
        if (i > 0) {
            printf(", ");
        }
        printf("\"%s\"", ctx->entities[i]);
    }
    printf("],\n");
    printf("  \"qualifiers\": [");
    for (i = 0; i < ctx->qualifier_count; i++) {
        if (i > 0) {
            printf(", ");
        }
        printf("\"%s\"", ctx->qualifiers[i]);
    }
    printf("],\n");
    printf("  \"language\": {\"value\": \"%s\", \"score\": %d, \"alternatives\": [",
           ctx->language[0] ? ctx->language : "", ctx->language_score);
    for (i = 0; i < ctx->alt_language_count; i++) {
        if (i > 0) {
            printf(", ");
        }
        printf("\"%s\"", ctx->alt_languages[i]);
    }
    printf("]},\n");
    printf("  \"platform\": {\"value\": \"%s\", \"score\": %d, \"alternatives\": [",
           ctx->platform[0] ? ctx->platform : "", ctx->platform_score);
    for (i = 0; i < ctx->alt_platform_count; i++) {
        if (i > 0) {
            printf(", ");
        }
        printf("\"%s\"", ctx->alt_platforms[i]);
    }
    printf("]},\n");
    printf("  \"framework\": {\"value\": \"%s\", \"score\": %d, \"alternatives\": [",
           ctx->framework[0] ? ctx->framework : "", ctx->framework_score);
    for (i = 0; i < ctx->alt_framework_count; i++) {
        if (i > 0) {
            printf(", ");
        }
        printf("\"%s\"", ctx->alt_frameworks[i]);
    }
    printf("]}\n");
    printf("}\n");
}

int main(int argc, char **argv)
{
    char input[MAX_INPUT];
    char extracted[MAX_INPUT];
    struct chat_context ctx;
    struct analysis_result analysis;
    int quiet = 0;

    memset(&ctx, 0, sizeof(ctx));
    set_default_searchdir();
    if (wninit() != 0) {
        fprintf(stderr, "WordNet data files not found. Set WNHOME or WNSEARCHDIR.\n");
        return 1;
    }

    if (!load_chat_strings("chat_strings.json")) {
        load_chat_strings("diy-ai/chat_strings.json");
    }

    if (argc > 1 && strcmp(argv[1], "--quiet") == 0) {
        quiet = 1;
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
        if (strcmp(input, "/json") == 0) {
            print_json_context(&ctx);
            continue;
        }
        if (strcmp(input, "/reset") == 0) {
            reset_context(&ctx);
            printf("Memory reset.\n");
            continue;
        }
        if (input[0] == '{' || input[0] == '[') {
            if (extract_strings_from_json(input, extracted, sizeof(extracted))) {
                if (extracted[0] != '\0') {
                    snprintf(input, sizeof(input), "%s", extracted);
                }
            }
        }
        analyze_input(input, &ctx, &analysis);
        if (!quiet) {
            generate_response(&ctx, &analysis);
        }
    }
    return 0;
}
