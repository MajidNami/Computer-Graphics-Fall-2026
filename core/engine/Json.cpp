#include "engine/Json.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace citylab {
namespace {

const Json& nullJson() {
    static const Json value;
    return value;
}

class Parser {
public:
    Parser(const std::string& text, const std::string& source) : text_(text), source_(source) {}

    Json parseDocument() {
        skipSpace();
        Json value = parseValue();
        skipSpace();
        if (pos_ != text_.size()) fail("unexpected text after the end of the document");
        return value;
    }

private:
    [[noreturn]] void fail(const std::string& message) const {
        int line = 1, column = 1;
        for (size_t i = 0; i < pos_ && i < text_.size(); ++i) {
            if (text_[i] == '\n') { ++line; column = 1; } else { ++column; }
        }
        throw JsonError(source_ + ":" + std::to_string(line) + ":" + std::to_string(column) +
                        ": " + message);
    }

    void skipSpace() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }

    bool consume(char c) {
        skipSpace();
        if (pos_ < text_.size() && text_[pos_] == c) { ++pos_; return true; }
        return false;
    }

    void expect(char c) {
        if (!consume(c)) fail(std::string("expected '") + c + "'");
    }

    Json parseValue() {
        skipSpace();
        if (pos_ >= text_.size()) fail("unexpected end of the document");
        const char c = text_[pos_];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return Json(parseString());
        if (c == 't') return literal("true", Json(true));
        if (c == 'f') return literal("false", Json(false));
        if (c == 'n') return literal("null", Json());
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        fail(std::string("unexpected character '") + c + "'");
    }

    Json literal(const char* word, Json value) {
        const std::string w(word);
        if (text_.compare(pos_, w.size(), w) != 0) fail("invalid literal");
        pos_ += w.size();
        return value;
    }

    Json parseNumber() {
        const size_t start = pos_;
        if (text_[pos_] == '-') ++pos_;
        while (pos_ < text_.size() && std::string("0123456789.eE+-").find(text_[pos_]) != std::string::npos)
            ++pos_;
        try {
            size_t used = 0;
            const std::string token = text_.substr(start, pos_ - start);
            const double value = std::stod(token, &used);
            if (used != token.size()) fail("invalid number");
            return Json(value);
        } catch (const std::logic_error&) {
            fail("invalid number");
        }
    }

    static void appendUtf8(std::string& out, unsigned code) {
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    unsigned parseHex4() {
        if (pos_ + 4 > text_.size()) fail("truncated \\u escape");
        unsigned value = 0;
        for (int k = 0; k < 4; ++k) {
            const char h = text_[pos_++];
            value <<= 4;
            if (h >= '0' && h <= '9') value |= static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') value |= static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') value |= static_cast<unsigned>(h - 'A' + 10);
            else fail("invalid \\u escape");
        }
        return value;
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (true) {
            if (pos_ >= text_.size()) fail("unterminated string");
            const char c = text_[pos_++];
            if (c == '"') break;
            if (c == '\\') {
                if (pos_ >= text_.size()) fail("unterminated escape");
                const char e = text_[pos_++];
                switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned code = parseHex4();
                    if (code >= 0xD800 && code <= 0xDBFF && text_.compare(pos_, 2, "\\u") == 0) {
                        pos_ += 2;
                        const unsigned low = parseHex4();
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    }
                    appendUtf8(out, code);
                    break;
                }
                default: fail("invalid escape sequence");
                }
            } else {
                out += c;
            }
        }
        return out;
    }

    Json parseArray() {
        expect('[');
        Json value = Json::array();
        if (consume(']')) return value;
        do {
            value.push(parseValue());
        } while (consume(','));
        expect(']');
        return value;
    }

    Json parseObject() {
        expect('{');
        Json value = Json::object();
        if (consume('}')) return value;
        do {
            skipSpace();
            const std::string key = parseString();
            expect(':');
            value.set(key, parseValue());
        } while (consume(','));
        expect('}');
        return value;
    }

    const std::string& text_;
    std::string source_;
    size_t pos_ = 0;
};

void escapeTo(std::string& out, const std::string& s) {
    out += '"';
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                out += buffer;
            } else {
                out += ch;
            }
        }
    }
    out += '"';
}

}  // namespace

Json Json::array() {
    Json j;
    j.type_ = Type::Array;
    j.array_ = std::make_shared<Array>();
    return j;
}

Json Json::object() {
    Json j;
    j.type_ = Type::Object;
    j.object_ = std::make_shared<Object>();
    return j;
}

Json Json::parse(const std::string& text, const std::string& sourceName) {
    return Parser(text, sourceName).parseDocument();
}

Json Json::parseFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw JsonError("cannot open " + path.string());
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string text = buffer.str();
    if (text.size() >= 3 && text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);  // BOM
    return parse(text, path.string());
}

bool Json::has(const std::string& key) const {
    if (type_ != Type::Object) return false;
    for (const auto& member : *object_)
        if (member.first == key) return true;
    return false;
}

const Json& Json::operator[](const std::string& key) const {
    if (type_ != Type::Object) return nullJson();
    for (const auto& member : *object_)
        if (member.first == key) return member.second;
    return nullJson();
}

const Json& Json::operator[](size_t index) const {
    if (type_ != Type::Array || index >= array_->size()) return nullJson();
    return (*array_)[index];
}

size_t Json::size() const {
    if (type_ == Type::Array) return array_->size();
    if (type_ == Type::Object) return object_->size();
    return 0;
}

const std::string& Json::str() const {
    static const std::string empty;
    return type_ == Type::String ? string_ : empty;
}

const Json::Array& Json::items() const {
    static const Array empty;
    return type_ == Type::Array ? *array_ : empty;
}

const Json::Object& Json::members() const {
    static const Object empty;
    return type_ == Type::Object ? *object_ : empty;
}

Json& Json::push(Json value) {
    if (type_ != Type::Array) *this = array();
    array_->push_back(std::move(value));
    return *this;
}

Json& Json::set(const std::string& key, Json value) {
    if (type_ != Type::Object) *this = object();
    for (auto& member : *object_) {
        if (member.first == key) {
            member.second = std::move(value);
            return *this;
        }
    }
    object_->emplace_back(key, std::move(value));
    return *this;
}

std::string Json::dump(int indent) const {
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

void Json::dumpTo(std::string& out, int indent, int depth) const {
    const auto newline = [&](int d) {
        if (indent < 0) return;
        out += '\n';
        out.append(static_cast<size_t>(indent * d), ' ');
    };
    switch (type_) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += bool_ ? "true" : "false"; break;
    case Type::Number: {
        if (std::isfinite(number_) && std::floor(number_) == number_ && std::fabs(number_) < 1e15) {
            out += std::to_string(static_cast<long long>(number_));
        } else {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%.9g", std::isfinite(number_) ? number_ : 0.0);
            out += buffer;
        }
        break;
    }
    case Type::String: escapeTo(out, string_); break;
    case Type::Array: {
        out += '[';
        for (size_t k = 0; k < array_->size(); ++k) {
            if (k) out += ',';
            newline(depth + 1);
            (*array_)[k].dumpTo(out, indent, depth + 1);
        }
        if (!array_->empty()) newline(depth);
        out += ']';
        break;
    }
    case Type::Object: {
        out += '{';
        for (size_t k = 0; k < object_->size(); ++k) {
            if (k) out += ',';
            newline(depth + 1);
            escapeTo(out, (*object_)[k].first);
            out += indent < 0 ? ":" : ": ";
            (*object_)[k].second.dumpTo(out, indent, depth + 1);
        }
        if (!object_->empty()) newline(depth);
        out += '}';
        break;
    }
    }
}

}  // namespace citylab
