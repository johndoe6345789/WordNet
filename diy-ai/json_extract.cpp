#include "json_extract.h"

#include <rapidjson/document.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace {
std::unordered_map<std::string, std::string> g_strings;

const rapidjson::Value *find_member(const rapidjson::Value &obj, const char *key) {
    if (!obj.IsObject()) {
        return nullptr;
    }
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd()) {
        return nullptr;
    }
    return &it->value;
}

void load_string_map(const rapidjson::Value &obj) {
    if (!obj.IsObject()) {
        return;
    }
    for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it) {
        if (!it->name.IsString()) {
            continue;
        }
        if (it->value.IsString()) {
            g_strings[it->name.GetString()] = it->value.GetString();
        } else if (it->value.IsObject()) {
            load_string_map(it->value);
        }
    }
}
}  // namespace

static void append_token(char *out, size_t out_size, const char *token)
{
    size_t used;
    size_t len;

    if (out == NULL || token == NULL || out_size == 0) {
        return;
    }
    used = strlen(out);
    len = strlen(token);
    if (len == 0) {
        return;
    }
    if (used > 0 && used + 1 < out_size) {
        out[used++] = ' ';
        out[used] = '\0';
    }
    if (used + len + 1 > out_size) {
        len = out_size - used - 1;
    }
    if (len > 0) {
        memcpy(out + used, token, len);
        out[used + len] = '\0';
    }
}

static void collect_strings(const rapidjson::Value &value, char *out, size_t out_size)
{
    if (value.IsString()) {
        append_token(out, out_size, value.GetString());
        return;
    }
    if (value.IsArray()) {
        for (rapidjson::SizeType i = 0; i < value.Size(); i++) {
            collect_strings(value[i], out, out_size);
        }
        return;
    }
    if (value.IsObject()) {
        for (rapidjson::Value::ConstMemberIterator it = value.MemberBegin();
             it != value.MemberEnd();
             ++it) {
            collect_strings(it->value, out, out_size);
        }
    }
}

int extract_strings_from_json(const char *json, char *out, size_t out_size)
{
    rapidjson::Document doc;

    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (json == NULL || json[0] == '\0') {
        return 0;
    }
    doc.Parse(json);
    if (doc.HasParseError()) {
        return 0;
    }
    collect_strings(doc, out, out_size);
    return 1;
}

int load_chat_strings(const char *path)
{
    FILE *fp;
    long size;
    std::string buffer;
    rapidjson::Document doc;

    if (path == nullptr || path[0] == '\0') {
        return 0;
    }
    fp = fopen(path, "rb");
    if (fp == nullptr) {
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        return 0;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 0;
    }
    buffer.resize(static_cast<size_t>(size));
    if (buffer.empty()) {
        fclose(fp);
        return 0;
    }
    if (fread(&buffer[0], 1, buffer.size(), fp) != buffer.size()) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    doc.Parse(buffer.c_str());
    if (doc.HasParseError()) {
        return 0;
    }

    g_strings.clear();
    if (doc.IsObject()) {
        const rapidjson::Value *strings = find_member(doc, "strings");
        const rapidjson::Value *templates = find_member(doc, "templates");
        if (strings != nullptr && strings->IsObject()) {
            load_string_map(*strings);
        }
        if (templates != nullptr && templates->IsObject()) {
            load_string_map(*templates);
        }
    }
    return g_strings.empty() ? 0 : 1;
}

const char *get_chat_string(const char *key)
{
    if (key == nullptr || key[0] == '\0') {
        return nullptr;
    }
    auto it = g_strings.find(key);
    if (it == g_strings.end()) {
        return nullptr;
    }
    return it->second.c_str();
}
