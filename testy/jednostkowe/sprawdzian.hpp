// Minimalny zestaw testów jednostkowych. Bez zewnętrznych bibliotek - zasada całego projektu.
//
//   TEST(nazwa) { SPRAWDZ(warunek); SPRAWDZ_ROWNE(a, b); SPRAWDZ_WYJATEK(wyrazenie); }
//
// Każdy test to osobna funkcja rejestrowana przy starcie programu; pierwsza niespełniona
// asercja kończy dany test, pozostałe testy biegną dalej. Kod wyjścia 1 = coś nie przeszło.
#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace sprawdzian {

struct Test {
    const char* nazwa;
    std::function<void()> cialo;
};

inline std::vector<Test>& rejestr() {
    static std::vector<Test> testy;
    return testy;
}

struct Rejestracja {
    Rejestracja(const char* nazwa, std::function<void()> cialo) { rejestr().push_back({nazwa, std::move(cialo)}); }
};

// Rzucana przez asercje; celowo nie dziedziczy po std::exception, żeby SPRAWDZ_WYJATEK jej nie połknął.
struct Porazka {
    std::string opis;
};

inline std::string miejsce(const char* plik, int linia) { return std::string(plik) + ":" + std::to_string(linia); }

template <typename T>
std::string opisz(const T& wartosc) {
    std::ostringstream w;
    w << wartosc;
    return w.str();
}

inline std::string opisz(const std::string& tekst) { return "\"" + tekst + "\""; }

inline int uruchom_wszystkie() {
    int dobrze = 0, zle = 0;
    for (const Test& t : rejestr()) {
        try {
            t.cialo();
            ++dobrze;
            std::cout << "  ✔ " << t.nazwa << "\n";
        } catch (const Porazka& p) {
            ++zle;
            std::cout << "  ✘ " << t.nazwa << "\n      " << p.opis << "\n";
        } catch (const std::exception& e) {
            ++zle;
            std::cout << "  ✘ " << t.nazwa << "\n      nieoczekiwany wyjątek: " << e.what() << "\n";
        }
    }
    std::cout << "== testy jednostkowe: dobrze " << dobrze << ", źle " << zle << " ==\n";
    return zle == 0 ? 0 : 1;
}

}  // namespace sprawdzian

#define TEST(nazwa)                                                          \
    static void nazwa();                                                     \
    static const ::sprawdzian::Rejestracja rejestracja_##nazwa(#nazwa, nazwa); \
    static void nazwa()

#define SPRAWDZ(warunek)                                                                              \
    do {                                                                                              \
        if (!(warunek))                                                                               \
            throw ::sprawdzian::Porazka{::sprawdzian::miejsce(__FILE__, __LINE__) + ": " #warunek}; \
    } while (0)

#define SPRAWDZ_ROWNE(oczekiwane, faktyczne)                                                            \
    do {                                                                                                \
        const auto& o_ = (oczekiwane);                                                                  \
        const auto& f_ = (faktyczne);                                                                   \
        if (!(o_ == f_))                                                                                \
            throw ::sprawdzian::Porazka{::sprawdzian::miejsce(__FILE__, __LINE__) + ": " #faktyczne " = " + \
                                        ::sprawdzian::opisz(f_) + ", oczekiwano " + ::sprawdzian::opisz(o_)}; \
    } while (0)

#define SPRAWDZ_WYJATEK(wyrazenie)                                                                     \
    do {                                                                                               \
        bool rzucil_ = false;                                                                          \
        try {                                                                                          \
            static_cast<void>(wyrazenie);                                                              \
        } catch (const std::exception&) {                                                              \
            rzucil_ = true;                                                                            \
        }                                                                                              \
        if (!rzucil_)                                                                                  \
            throw ::sprawdzian::Porazka{::sprawdzian::miejsce(__FILE__, __LINE__) + ": brak wyjątku: " #wyrazenie}; \
    } while (0)
