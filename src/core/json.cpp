#include "json.h"
#include "util.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace aurora {

static const Json kNullJson;
static const std::string kEmptyString;
static const Json::Array kEmptyArray;
static const Json::Object kEmptyObject;

Json::Json(const wchar_t* s) : t_(Type::String), str_(s ? wideToUtf8(s) : std::string()) {}
Json::Json(const std::wstring& s) : t_(Type::String), str_(wideToUtf8(s)) {}
Json Json::array() { Json j; j.t_ = Type::Array; return j; }
Json Json::object() { Json j; j.t_ = Type::Object; return j; }
Json Json::array(std::initializer_list<Json> items) {
    Json j; j.t_ = Type::Array; j.arr_.assign(items); return j;
}

const std::string& Json::asString() const { return t_ == Type::String ? str_ : kEmptyString; }
std::wstring Json::asWString() const { return t_ == Type::String ? utf8ToWide(str_) : std::wstring(); }
const Json::Array& Json::items() const { return t_ == Type::Array ? arr_ : kEmptyArray; }
Json::Array& Json::items() { if (t_ != Type::Array) { t_ = Type::Array; arr_.clear(); } return arr_; }
const Json::Object& Json::fields() const { return t_ == Type::Object ? obj_ : kEmptyObject; }
Json::Object& Json::fields() { if (t_ != Type::Object) { t_ = Type::Object; obj_.clear(); } return obj_; }

size_t Json::size() const {
    if (t_ == Type::Array) return arr_.size();
    if (t_ == Type::Object) return obj_.size();
    return 0;
}

bool Json::empty() const { return size() == 0; }

bool Json::has(const std::string& key) const {
    if (t_ != Type::Object) return false;
    for (auto& kv : obj_) if (kv.first == key) return true;
    return false;
}

const Json& Json::get(const std::string& key) const {
    if (t_ != Type::Object) return kNullJson;
    for (auto& kv : obj_) if (kv.first == key) return kv.second;
    return kNullJson;
}

Json& Json::operator[](const std::string& key) {
    if (t_ != Type::Object) { t_ = Type::Object; obj_.clear(); }
    for (auto& kv : obj_) if (kv.first == key) return kv.second;
    obj_.emplace_back(key, Json());
    return obj_.back().second;
}

const Json& Json::operator[](const std::string& key) const { return get(key); }

Json& Json::operator[](size_t index) {
    if (t_ != Type::Array) { t_ = Type::Array; arr_.clear(); }
    if (index >= arr_.size()) arr_.resize(index + 1);
    return arr_[index];
}

const Json& Json::at(size_t index) const {
    if (t_ == Type::Array && index < arr_.size()) return arr_[index];
    return kNullJson;
}

void Json::push(Json value) {
    if (t_ != Type::Array) { t_ = Type::Array; arr_.clear(); }
    arr_.push_back(std::move(value));
}

void Json::set(const std::string& key, Json value) {
    if (t_ != Type::Object) { t_ = Type::Object; obj_.clear(); }
    for (auto& kv : obj_) {
        if (kv.first == key) { kv.second = std::move(value); return; }
    }
    obj_.emplace_back(key, std::move(value));
}

void Json::remove(const std::string& key) {
    if (t_ != Type::Object) return;
    for (auto it = obj_.begin(); it != obj_.end(); ++it) {
        if (it->first == key) { obj_.erase(it); return; }
    }
}

static void escapeString(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back((char)c);
                }
        }
    }
    out.push_back('"');
}

static void writeNumber(std::string& out, double v) {
    if (!std::isfinite(v)) { out += "0"; return; }
    if (v == std::floor(v) && std::fabs(v) < 9.0e15) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%lld", (long long)v);
        out += buf;
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", v);
        out += buf;
    }
}

void Json::dumpTo(std::string& out, int indent, int level) const {
    switch (t_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += num_ != 0 ? "true" : "false"; break;
        case Type::Number: writeNumber(out, num_); break;
        case Type::String: escapeString(out, str_); break;
        case Type::Array: {
            if (arr_.empty()) { out += "[]"; break; }
            out.push_back('[');
            for (size_t i = 0; i < arr_.size(); i++) {
                if (i) out.push_back(',');
                if (indent > 0) { out.push_back('\n'); out.append((size_t)(indent * (level + 1)), ' '); }
                arr_[i].dumpTo(out, indent, level + 1);
            }
            if (indent > 0) { out.push_back('\n'); out.append((size_t)(indent * level), ' '); }
            out.push_back(']');
            break;
        }
        case Type::Object: {
            if (obj_.empty()) { out += "{}"; break; }
            out.push_back('{');
            for (size_t i = 0; i < obj_.size(); i++) {
                if (i) out.push_back(',');
                if (indent > 0) { out.push_back('\n'); out.append((size_t)(indent * (level + 1)), ' '); }
                escapeString(out, obj_[i].first);
                out += indent > 0 ? ": " : ":";
                obj_[i].second.dumpTo(out, indent, level + 1);
            }
            if (indent > 0) { out.push_back('\n'); out.append((size_t)(indent * level), ' '); }
            out.push_back('}');
            break;
        }
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    out.reserve(256);
    dumpTo(out, indent, 0);
    return out;
}

std::wstring Json::dumpW() const { return utf8ToWide(dump()); }

// ---------------- parser ----------------
namespace {

struct Parser {
    const char* p;
    const char* end;
    std::string error;
    int line = 1;
    int depth = 0;
    bool failed = false;

    void fail(const std::string& msg) {
        if (!failed) {
            error = msg + " (ligne " + std::to_string(line) + ")";
            failed = true;
        }
    }

    void skipWs() {
        while (p < end) {
            char c = *p;
            if (c == '\n') { line++; p++; }
            else if (c == ' ' || c == '\t' || c == '\r') p++;
            else break;
        }
    }

    bool consume(const char* lit) {
        size_t n = strlen(lit);
        if ((size_t)(end - p) < n) return false;
        if (memcmp(p, lit, n) != 0) return false;
        p += n;
        return true;
    }

    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) out.push_back((char)cp);
        else if (cp <= 0x7FF) {
            out.push_back((char)(0xC0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp <= 0x10FFFF) {
            out.push_back((char)(0xF0 | (cp >> 18)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }

    bool parseHex4(uint32_t& value) {
        if (end - p < 4) return false;
        value = 0;
        for (int i = 0; i < 4; i++) {
            char c = p[i];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= (uint32_t)(c - '0');
            else if (c >= 'a' && c <= 'f') value |= (uint32_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= (uint32_t)(c - 'A' + 10);
            else return false;
        }
        p += 4;
        return true;
    }

    bool parseString(std::string& out) {
        if (p >= end || *p != '"') { fail("chaîne attendue"); return false; }
        p++;
        out.clear();
        while (p < end) {
            unsigned char c = (unsigned char)*p++;
            if (c == '"') return true;
            if (c == '\\') {
                if (p >= end) { fail("échappement incomplet"); return false; }
                char e = *p++;
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        uint32_t cp = 0;
                        if (!parseHex4(cp)) { fail("\\u invalide"); return false; }
                        if (cp >= 0xD800 && cp <= 0xDBFF && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                            p += 2;
                            uint32_t lo = 0;
                            if (!parseHex4(lo)) { fail("paire de substitution invalide"); return false; }
                            if (lo >= 0xDC00 && lo <= 0xDFFF)
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: fail("échappement inconnu"); return false;
                }
            } else if (c < 0x20) {
                fail("caractère de contrôle dans une chaîne");
                return false;
            } else {
                out.push_back((char)c);
            }
        }
        fail("chaîne non terminée");
        return false;
    }

    bool parseNumber(double& out) {
        const char* start = p;
        if (p < end && *p == '-') p++;
        while (p < end && *p >= '0' && *p <= '9') p++;
        if (p < end && *p == '.') {
            p++;
            while (p < end && *p >= '0' && *p <= '9') p++;
        }
        if (p < end && (*p == 'e' || *p == 'E')) {
            p++;
            if (p < end && (*p == '+' || *p == '-')) p++;
            while (p < end && *p >= '0' && *p <= '9') p++;
        }
        if (p == start) { fail("nombre invalide"); return false; }
        std::string tmp(start, p);
        char* endPtr = nullptr;
        out = strtod(tmp.c_str(), &endPtr);
        if (endPtr == tmp.c_str()) { fail("nombre invalide"); return false; }
        return true;
    }

    bool parseValue(Json& out) {
        skipWs();
        if (p >= end) { fail("valeur attendue"); return false; }
        if (++depth > 200) { fail("imbrication trop profonde"); return false; }
        char c = *p;
        bool ok = false;
        if (c == '{') {
            p++;
            out = Json::object();
            skipWs();
            if (p < end && *p == '}') { p++; depth--; return true; }
            while (!failed) {
                skipWs();
                std::string key;
                if (!parseString(key)) break;
                skipWs();
                if (p >= end || *p != ':') { fail("':' attendu"); break; }
                p++;
                Json value;
                if (!parseValue(value)) break;
                out.set(key, std::move(value));
                skipWs();
                if (p < end && *p == ',') { p++; continue; }
                if (p < end && *p == '}') { p++; ok = true; }
                else fail("'}' attendu");
                break;
            }
        } else if (c == '[') {
            p++;
            out = Json::array();
            skipWs();
            if (p < end && *p == ']') { p++; depth--; return true; }
            while (!failed) {
                Json value;
                if (!parseValue(value)) break;
                out.push(std::move(value));
                skipWs();
                if (p < end && *p == ',') { p++; continue; }
                if (p < end && *p == ']') { p++; ok = true; }
                else fail("']' attendu");
                break;
            }
        } else if (c == '"') {
            std::string s;
            if (parseString(s)) { out = Json(std::move(s)); ok = true; }
        } else if (consume("true")) { out = Json(true); ok = true; }
        else if (consume("false")) { out = Json(false); ok = true; }
        else if (consume("null")) { out = Json(); ok = true; }
        else {
            double v = 0;
            if (parseNumber(v)) { out = Json(v); ok = true; }
        }
        depth--;
        return ok && !failed;
    }
};

} // namespace

bool Json::parse(const std::string& text, Json& out, std::string* error) {
    Parser ps;
    const char* start = text.data();
    size_t size = text.size();
    if (size >= 3 && (unsigned char)start[0] == 0xEF && (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF) {
        start += 3;
        size -= 3;
    }
    ps.p = start;
    ps.end = start + size;
    ps.skipWs();
    if (!ps.parseValue(out)) {
        if (error) *error = ps.error.empty() ? "JSON invalide" : ps.error;
        return false;
    }
    ps.skipWs();
    if (ps.p != ps.end) {
        if (error) *error = "contenu après la valeur JSON (ligne " + std::to_string(ps.line) + ")";
        return false;
    }
    return true;
}

} // namespace aurora
