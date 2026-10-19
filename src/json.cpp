#include "json.hpp"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace json {

const Value& Value::operator[](const std::string& klucz) const {
    static const Value pusty;
    if (typ != Typ::Obiekt) return pusty;
    auto it = obj.find(klucz);
    return it == obj.end() ? pusty : it->second;
}

std::string Value::tekst(const std::string& domyslny) const {
    return typ == Typ::Tekst ? s : domyslny;
}

long long Value::calk(long long domyslna) const {
    return typ == Typ::Liczba ? static_cast<long long>(n) : domyslna;
}

namespace {

void dopisz_utf8(std::string& w, uint32_t cp) {
    if (cp < 0x80) {
        w += static_cast<char>(cp);
    } else if (cp < 0x800) {
        w += static_cast<char>(0xC0 | (cp >> 6));
        w += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        w += static_cast<char>(0xE0 | (cp >> 12));
        w += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        w += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        w += static_cast<char>(0xF0 | (cp >> 18));
        w += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        w += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        w += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

struct Parser {
    const std::string& s;
    size_t i = 0;

    [[noreturn]] void blad(const char* co) const {
        throw std::runtime_error(std::string("JSON: ") + co + " (pozycja " + std::to_string(i) + ")");
    }

    void spacje() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    }

    bool slowo(const char* w, size_t dl) {
        if (s.compare(i, dl, w) != 0) return false;
        i += dl;
        return true;
    }

    Value wartosc(int glebokosc) {
        if (glebokosc > 64) blad("za głęboko zagnieżdżone");
        spacje();
        if (i >= s.size()) blad("nieoczekiwany koniec");
        Value v;
        char c = s[i];
        if (c == '{') return obiekt(glebokosc);
        if (c == '[') return tablica(glebokosc);
        if (c == '"') {
            v.typ = Value::Typ::Tekst;
            v.s = tekst();
            return v;
        }
        if (slowo("true", 4)) { v.typ = Value::Typ::Bool; v.b = true; return v; }
        if (slowo("false", 5)) { v.typ = Value::Typ::Bool; return v; }
        if (slowo("null", 4)) return v;
        if (c == '-' || (c >= '0' && c <= '9')) return liczba();
        blad("nieznany znak");
    }

    Value obiekt(int glebokosc) {
        Value v;
        v.typ = Value::Typ::Obiekt;
        ++i;
        spacje();
        if (i < s.size() && s[i] == '}') { ++i; return v; }
        for (;;) {
            spacje();
            if (i >= s.size() || s[i] != '"') blad("oczekiwano klucza");
            std::string k = tekst();
            spacje();
            if (i >= s.size() || s[i] != ':') blad("oczekiwano ':'");
            ++i;
            v.obj[k] = wartosc(glebokosc + 1);
            spacje();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == '}') { ++i; return v; }
            blad("oczekiwano ',' lub '}'");
        }
    }

    Value tablica(int glebokosc) {
        Value v;
        v.typ = Value::Typ::Tablica;
        ++i;
        spacje();
        if (i < s.size() && s[i] == ']') { ++i; return v; }
        for (;;) {
            v.tab.push_back(wartosc(glebokosc + 1));
            spacje();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == ']') { ++i; return v; }
            blad("oczekiwano ',' lub ']'");
        }
    }

    uint32_t hex4() {
        if (i + 4 > s.size()) blad("ucięte \\u");
        uint32_t w = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s[i++];
            w <<= 4;
            if (c >= '0' && c <= '9') w |= c - '0';
            else if (c >= 'a' && c <= 'f') w |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') w |= c - 'A' + 10;
            else blad("zła cyfra w \\u");
        }
        return w;
    }

    std::string tekst() {
        ++i;  // otwierający cudzysłów
        std::string w;
        for (;;) {
            if (i >= s.size()) blad("niezamknięty tekst");
            char c = s[i++];
            if (c == '"') return w;
            if (c != '\\') { w += c; continue; }
            if (i >= s.size()) blad("ucięta sekwencja");
            char e = s[i++];
            switch (e) {
                case '"': w += '"'; break;
                case '\\': w += '\\'; break;
                case '/': w += '/'; break;
                case 'b': w += '\b'; break;
                case 'f': w += '\f'; break;
                case 'n': w += '\n'; break;
                case 'r': w += '\r'; break;
                case 't': w += '\t'; break;
                case 'u': {
                    uint32_t cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        // para zastępcza (np. emoji)
                        if (s.compare(i, 2, "\\u") == 0) {
                            i += 2;
                            uint32_t lo = hex4();
                            cp = (lo >= 0xDC00 && lo <= 0xDFFF) ? 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00)
                                                                : 0xFFFD;
                        } else {
                            cp = 0xFFFD;
                        }
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        cp = 0xFFFD;
                    }
                    dopisz_utf8(w, cp);
                    break;
                }
                default: blad("nieznana sekwencja ucieczki");
            }
        }
    }

    Value liczba() {
        size_t start = i;
        if (s[i] == '-') ++i;
        while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' || s[i] == 'E' ||
                                s[i] == '+' || s[i] == '-'))
            ++i;
        Value v;
        v.typ = Value::Typ::Liczba;
        v.n = std::strtod(s.substr(start, i - start).c_str(), nullptr);
        return v;
    }
};

}  // namespace

Value parsuj(const std::string& wejscie) {
    Parser p{wejscie};
    Value v = p.wartosc(0);
    p.spacje();
    if (p.i != wejscie.size()) p.blad("nadmiarowe znaki po końcu");
    return v;
}

std::string cytuj(const std::string& tekst) {
    static const char* hex = "0123456789abcdef";
    std::string w = "\"";
    for (unsigned char c : tekst) {
        switch (c) {
            case '"': w += "\\\""; break;
            case '\\': w += "\\\\"; break;
            case '\n': w += "\\n"; break;
            case '\r': w += "\\r"; break;
            case '\t': w += "\\t"; break;
            default:
                if (c < 0x20) {
                    w += "\\u00";
                    w += hex[c >> 4];
                    w += hex[c & 0xF];
                } else {
                    w += static_cast<char>(c);
                }
        }
    }
    return w + "\"";
}

}  // namespace json
