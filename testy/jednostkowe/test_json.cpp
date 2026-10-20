// Parser i serializacja JSON (src/json.cpp).
#include <string>

#include "json.hpp"
#include "sprawdzian.hpp"

using json::Value;

TEST(json_proste_wartosci) {
    SPRAWDZ(json::parsuj("null").typ == Value::Typ::Null);
    SPRAWDZ(json::parsuj("true").b);
    SPRAWDZ(json::parsuj("false").typ == Value::Typ::Bool);
    SPRAWDZ(!json::parsuj("false").b);
    SPRAWDZ_ROWNE(0.0, json::parsuj("0").n);
    SPRAWDZ_ROWNE(-1.5, json::parsuj("-1.5").n);
    SPRAWDZ_ROWNE(1000.0, json::parsuj("1e3").n);
    SPRAWDZ_ROWNE(0.01, json::parsuj("1E-2").n);
    SPRAWDZ_ROWNE(std::string("abc"), json::parsuj("  \"abc\"\n").s);
}

TEST(json_sekwencje_ucieczki) {
    SPRAWDZ_ROWNE(std::string("a\"b\\c/d\n\t\r\b\f"), json::parsuj(R"("a\"b\\c\/d\n\t\r\b\f")").s);
    SPRAWDZ_ROWNE(std::string("ą"), json::parsuj(R"("ą")").s);
    SPRAWDZ_ROWNE(std::string("\xF0\x9F\x98\x80"), json::parsuj(R"("😀")").s);  // emoji z pary zastępczej
    SPRAWDZ_ROWNE(std::string("\xEF\xBF\xBD"), json::parsuj(R"("\ud83d")").s);            // samotna połówka -> U+FFFD
    SPRAWDZ_ROWNE(std::string("\xEF\xBF\xBD"), json::parsuj(R"("\ude00")").s);
}

TEST(json_obiekty_i_tablice) {
    Value v = json::parsuj(R"({"typ":"audio-start","dane":{"rate":22050,"kanaly":[1,2]},"pusty":{}})");
    SPRAWDZ(v.jest_obiektem());
    SPRAWDZ_ROWNE(std::string("audio-start"), v["typ"].tekst());
    SPRAWDZ_ROWNE(22050LL, v["dane"]["rate"].calk(0));
    SPRAWDZ(v["dane"]["kanaly"].jest_tablica());
    SPRAWDZ_ROWNE(size_t{2}, v["dane"]["kanaly"].tab.size());
    SPRAWDZ(v["pusty"].jest_obiektem());
    SPRAWDZ(json::parsuj("[]").jest_tablica());
}

TEST(json_brakujacy_klucz_daje_null) {
    Value v = json::parsuj(R"({"a":1})");
    SPRAWDZ(v["brak"].typ == Value::Typ::Null);
    SPRAWDZ(v["a"]["glebiej"].typ == Value::Typ::Null);  // [] na nie-obiekcie też nie rzuca
    SPRAWDZ_ROWNE(std::string("domyslny"), v["brak"].tekst("domyslny"));
    SPRAWDZ_ROWNE(7LL, v["brak"].calk(7));
}

TEST(json_calk_obcina_i_pilnuje_zakresu) {
    SPRAWDZ_ROWNE(42LL, json::parsuj("42.9").calk(0));
    SPRAWDZ_ROWNE(-3LL, json::parsuj("-3.7").calk(0));
    SPRAWDZ_ROWNE(5LL, json::parsuj("\"42\"").calk(5));  // tekst to nie liczba
    // Poza zakresem long long rzutowanie byłoby zachowaniem niezdefiniowanym - ma wrócić wartość domyślna.
    SPRAWDZ_ROWNE(5LL, json::parsuj("1e300").calk(5));
    SPRAWDZ_ROWNE(5LL, json::parsuj("-1e300").calk(5));
}

TEST(json_odrzuca_bledna_skladnie) {
    for (const char* zly : {"", "   ", "{", "[1,]", R"({"a":1,})", R"({"a" 1})", R"({a:1})", "tru", "nul",
                            R"("abc)", R"("\x")", R"("\u12")", R"("\u12G4")", "1 2", "[1 2]", "{}}"}) {
        SPRAWDZ_WYJATEK(json::parsuj(zly));
    }
}

TEST(json_odrzuca_bledne_liczby) {
    for (const char* zly : {"-", "1-2", "01", "-01", "1.", ".5", "1e", "1e+", "+1", "--1", "1.2.3", "0x10"}) {
        SPRAWDZ_WYJATEK(json::parsuj(zly));
    }
}

TEST(json_odrzuca_znaki_sterujace_w_tekscie) {
    SPRAWDZ_WYJATEK(json::parsuj("\"a\nb\""));
    SPRAWDZ_WYJATEK(json::parsuj(std::string("\"a\x01" "b\"")));
}

TEST(json_ogranicza_glebokosc) {
    std::string plytko = std::string(10, '[') + std::string(10, ']');
    SPRAWDZ(json::parsuj(plytko).jest_tablica());
    std::string gleboko = std::string(1000, '[') + std::string(1000, ']');
    SPRAWDZ_WYJATEK(json::parsuj(gleboko));  // zamiast przepełnienia stosu
}

TEST(json_cytuj_i_parsuj_daja_to_samo) {
    const std::string teksty[] = {"", "zwykły tekst", "cudzysłów \" i ukośnik \\", "linia\nnowa\ttab\r",
                                  std::string("sterujący \x01 i \x1f"), "zażółć gęślą jaźń 😀"};
    for (const std::string& t : teksty) SPRAWDZ_ROWNE(t, json::parsuj(json::cytuj(t)).s);
    SPRAWDZ_ROWNE(std::string(R"("\u0001")"), json::cytuj("\x01"));
}
