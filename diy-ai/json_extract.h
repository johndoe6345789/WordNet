#ifndef DIY_AI_JSON_EXTRACT_H
#define DIY_AI_JSON_EXTRACT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int extract_strings_from_json(const char *json, char *out, size_t out_size);
int load_chat_strings(const char *path);
const char *get_chat_string(const char *key);

#ifdef __cplusplus
}
#endif

#endif
