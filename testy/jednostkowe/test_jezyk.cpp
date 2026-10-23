// Język komunikatów (src/jezyk.cpp) i komunikaty w obu językach.
#include <cstdlib>
#include <string>

#include "bledy.hpp"
#include "jezyk.hpp"
#include "sprawdzian.hpp"

namespace {

// Ustawia zmienne środowiska na czas jednego sprawdzenia i przywraca język testów (polski).
struct Srodowisko {
    Srodowisko(const char* lc_all, const char* lang) {
        unsetenv("KOMUNIKATOR_INTERFEJS");
        unsetenv("LC_MESSAGES");
        lc_all ? setenv("LC_ALL", lc_all, 1) : unsetenv("LC_ALL");
        lang ? setenv("LANG", lang, 1) : unsetenv("LANG");
    }
    ~Srodowisko() { ustaw_jezyk_interfejsu(Jezyk::Polski); }
};

}  // namespace

TEST(jezyk_z_locale_pomija_neutralne_C) {
    {
        Srodowisko s("C.UTF-8", "pl_PL.UTF-8");  // tak jest na serwerze: sortowanie C, język polski
        SPRAWDZ(jezyk_z_srodowiska() == Jezyk::Polski);
    }
    {
        Srodowisko s(nullptr, "en_GB.UTF-8");
        SPRAWDZ(jezyk_z_srodowiska() == Jezyk::Angielski);
    }
    {
        Srodowisko s("de_DE.UTF-8", "pl_PL.UTF-8");  // LC_ALL decyduje; nieobsługiwany = angielski
        SPRAWDZ(jezyk_z_srodowiska() == Jezyk::Angielski);
    }
    {
        Srodowisko s(nullptr, nullptr);
        SPRAWDZ(jezyk_z_srodowiska() == Jezyk::Angielski);
    }
}

TEST(jezyk_zmienna_komunikatora_ma_pierwszenstwo) {
    Srodowisko s(nullptr, "en_US.UTF-8");
    setenv("KOMUNIKATOR_INTERFEJS", "pl", 1);
    SPRAWDZ(jezyk_z_srodowiska() == Jezyk::Polski);
    unsetenv("KOMUNIKATOR_INTERFEJS");
}

TEST(jezyk_wstawki_w_calym_zdaniu) {
    ustaw_jezyk_interfejsu(Jezyk::Angielski);
    SPRAWDZ_ROWNE(std::string("saved dog (1.5 s)"), T("zapisano {} ({})", "saved {} ({})", {"dog", "1.5 s"}));
    SPRAWDZ_ROWNE(std::string("no {} here"), T("x", "no {} here"));  // brak wstawki - zostaje jak jest
    ustaw_jezyk_interfejsu(Jezyk::Polski);
    SPRAWDZ_ROWNE(std::string("zapisano pies (1.5 s)"), T("zapisano {} ({})", "saved {} ({})", {"pies", "1.5 s"}));
}

TEST(jezyk_komunikaty_uslug_w_obu_jezykach) {
    ustaw_jezyk_interfejsu(Jezyk::Angielski);
    std::string en = komunikat_uslugi(Rodzaj::UslugaNieOdpowiada, "speech service", "10.0.0.1:10200");
    ustaw_jezyk_interfejsu(Jezyk::Polski);
    std::string pl = komunikat_uslugi(Rodzaj::UslugaNieOdpowiada, "usługa mowy", "10.0.0.1:10200");
    SPRAWDZ(en.rfind("The speech service (10.0.0.1:10200) is not responding", 0) == 0);
    SPRAWDZ(pl.rfind("Usługa mowy (10.0.0.1:10200) nie odpowiada", 0) == 0);
}
