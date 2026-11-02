#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace citylab {

class JsonError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A small JSON value: enough for city/*.json, team.json, settings.json and the
// --check reports, without a third-party dependency.
//
// Reading is forgiving on purpose: asking a missing key or the wrong type
// returns the fallback you pass, so configuration code stays short. Parsing,
// on the other hand, is strict and reports line and column.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;  // keeps file order

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool value) : type_(Type::Bool), bool_(value) {}
    Json(int value) : type_(Type::Number), number_(value) {}
    Json(double value) : type_(Type::Number), number_(value) {}
    Json(float value) : type_(Type::Number), number_(value) {}
    Json(const char* value) : type_(Type::String), string_(value) {}
    Json(std::string value) : type_(Type::String), string_(std::move(value)) {}

    static Json array();
    static Json object();
    static Json parse(const std::string& text, const std::string& sourceName = "json");
    static Json parseFile(const std::filesystem::path& path);

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isObject() const { return type_ == Type::Object; }
    bool isArray() const { return type_ == Type::Array; }
    bool isString() const { return type_ == Type::String; }
    bool isNumber() const { return type_ == Type::Number; }

    bool has(const std::string& key) const;
    const Json& operator[](const std::string& key) const;  // null if missing
    const Json& operator[](size_t index) const;            // null if out of range
    size_t size() const;

    double number(double fallback = 0.0) const { return type_ == Type::Number ? number_ : fallback; }
    float f(float fallback = 0.0f) const { return static_cast<float>(number(fallback)); }
    int i(int fallback = 0) const { return static_cast<int>(number(fallback)); }
    bool boolean(bool fallback = false) const { return type_ == Type::Bool ? bool_ : fallback; }
    const std::string& str() const;  // empty string if not a string
    std::string str(const std::string& fallback) const { return isString() ? string_ : fallback; }

    const Array& items() const;
    const Object& members() const;

    // Building values (used for the --check reports).
    Json& push(Json value);
    Json& set(const std::string& key, Json value);

    std::string dump(int indent = -1) const;

private:
    void dumpTo(std::string& out, int indent, int depth) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::shared_ptr<Array> array_;
    std::shared_ptr<Object> object_;
};

}  // namespace citylab
