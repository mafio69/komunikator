// Moduł błędów (src/bledy.cpp): komunikaty i dziennik.
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "bledy.hpp"
#include "sprawdzian.hpp"

TEST(bledy_komunikaty_uslug_sa_zdaniami) {
    std::string t = komunikat_uslugi(Rodzaj::UslugaNieOdpowiada, "usługa mowy", "10.0.0.1:10200");
    SPRAWDZ(t.rfind("Usługa mowy", 0) == 0);  // z wielkiej litery
    SPRAWDZ(t.find("10.0.0.1:10200") != std::string::npos);
    SPRAWDZ(t.back() == '.');
}

TEST(bledy_dziennik_zapisuje_wpis) {
    namespace fs = std::filesystem;
    fs::path katalog = fs::temp_directory_path() / ("komunikator-test-" + std::to_string(::getpid()));
    fs::remove_all(katalog);
    setenv("XDG_STATE_HOME", katalog.c_str(), 1);
    zapisz_w_dzienniku("próbny wpis");
    std::ifstream f(sciezka_dziennika());
    std::string tresc((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    unsetenv("XDG_STATE_HOME");
    fs::remove_all(katalog);
    SPRAWDZ(tresc.find("próbny wpis") != std::string::npos);
}
