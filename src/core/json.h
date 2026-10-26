#pragma once
// Minimal, dependency-free JSON (parse + serialize). UTF-8 in, UTF-8 out.
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace aurora {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool v) : t_(Type::Bool), num_(v ? 1.0 : 0.0) {}
    Json(int v) : t_(Type::Number), num_(v) {}
    Json(long long v) : t_(Type::Number), num_((double)v) {}
    Json(size_t v) : t_(Type::Number), num_((double)v) {}
    Json(double v) : t_(Type::Number), num_(v) {}
    Json(const char* s) : t_(Type::String), str_(s ? s : "") {}
    Json(std::string s) : t_(Type::String), str_(std::move(s)) {}
    Json(const wchar_t* s);
    Json(const std::wstring& s);

    static Json array();
    static Json object();
    static Json array(std::initializer_list<Json> items);

    Type type() const { return t_; }
    bool isNull() const { return t_ == Type::Null; }
    bool isBool() const { return t_ == Type::Bool; }
    bool isNumber() const { return t_ == Type::Number; }
    bool isString() const { return t_ == Type::String; }
    bool isArray() const { return t_ == Type::Array; }
    bool isObject() const { return t_ == Type::Object; }

    bool asBool(bool def = false) const { return t_ == Type::Bool ? num_ != 0 : def; }
    double asNumber(double def = 0) const { return t_ == Type::Number ? num_ : def; }
    long long asInt(long long def = 0) const { return t_ == Type::Number ? (long long)num_ : def; }
    const std::string& asString() const;
    std::wstring asWString() const;
    std::string asString(const std::string& def) const { return t_ == Type::String ? str_ : def; }
    std::wstring asWString(const std::wstring& def) const { return t_ == Type::String ? asWString() : def; }

    const Array& items() const;
    Array& items();
    const Object& fields() const;
    Object& fields();

    size_t size() const;
    bool empty() const;

    bool has(const std::string& key) const;
    const Json& get(const std::string& key) const;
    Json& operator[](const std::string& key);          // inserts for objects
    const Json& operator[](const std::string& key) const;
    Json& operator[](size_t index);
    const Json& at(size_t index) const;

    void push(Json value);
    void set(const std::string& key, Json value);
    void remove(const std::string& key);

    std::string dump(int indent = 0) const;
    std::wstring dumpW() const;
    static bool parse(const std::string& text, Json& out, std::string* error = nullptr);

private:
    void dumpTo(std::string& out, int indent, int level) const;
    Type t_ = Type::Null;
    double num_ = 0;
    std::string str_;
    Array arr_;
    Object obj_;
};

} // namespace aurora
