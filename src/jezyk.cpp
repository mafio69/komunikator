#include "jezyk.hpp"

#include <cstdlib>

namespace {

Jezyk& biezacy() {
    static Jezyk jezyk = jezyk_z_srodowiska();
    return jezyk;
}

bool neutralny(const std::string& locale) {
    return locale == "C" || locale == "POSIX" || locale.rfind("C.", 0) == 0;
}

}  // namespace

Jezyk jezyk_interfejsu() { return biezacy(); }

void ustaw_jezyk_interfejsu(Jezyk jezyk) { biezacy() = jezyk; }

bool jezyk_z_nazwy(const std::string& nazwa, Jezyk& wynik) {
    std::string poczatek = nazwa.substr(0, 2);
    if (poczatek == "pl") { wynik = Jezyk::Polski; return true; }
    if (poczatek == "en") { wynik = Jezyk::Angielski; return true; }
    return false;
}

Jezyk jezyk_z_srodowiska() {
    Jezyk j = Jezyk::Angielski;
    if (const char* v = std::getenv("KOMUNIKATOR_INTERFEJS"); v && jezyk_z_nazwy(v, j)) return j;
    for (const char* zmienna : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* v = std::getenv(zmienna);
        if (!v || !*v || neutralny(v)) continue;
        return jezyk_z_nazwy(v, j) ? j : Jezyk::Angielski;  // pierwsza nie-neutralna decyduje
    }
    return Jezyk::Angielski;
}

std::string T(const char* pl, const char* en, std::initializer_list<std::string> wstawki) {
    std::string w = jezyk_interfejsu() == Jezyk::Polski ? pl : en;
    size_t poz = 0;
    for (const std::string& x : wstawki) {
        poz = w.find("{}", poz);
        if (poz == std::string::npos) break;
        w.replace(poz, 2, x);
        poz += x.size();
    }
    return w;
}
