#include "bledy.hpp"

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

Blad::Blad(Rodzaj rodzaj, const std::string& komunikat, std::string szczegoly)
    : std::runtime_error(komunikat), rodzaj_(rodzaj), szczegoly_(std::move(szczegoly)) {}

namespace {

std::string z_wielkiej(std::string s) {
    // Pierwsze litery naszych nazw usług są ASCII ("usługa..."), więc wystarczy zwykłe toupper.
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = static_cast<char>(s[0] - 'a' + 'A');
    return s;
}

fs::path katalog_stanu() {
    if (const char* x = std::getenv("XDG_STATE_HOME"); x && *x) return fs::path(x) / "komunikator";
    if (const char* h = std::getenv("HOME"); h && *h) return fs::path(h) / ".local" / "state" / "komunikator";
    return {};
}

}  // namespace

std::string komunikat_uslugi(Rodzaj rodzaj, const std::string& usluga, const std::string& adres) {
    switch (rodzaj) {
        case Rodzaj::UslugaNiedostepna:
            return "Nie mogę połączyć się z: " + usluga + " (" + adres +
                   "). Sprawdź, czy serwer jest włączony i czy adres jest dobry: komunikator ustawienia.";
        case Rodzaj::UslugaNieOdpowiada:
            return z_wielkiej(usluga) + " (" + adres +
                   ") nie odpowiada. Spróbuj za chwilę; jeśli to się powtarza, sprawdź serwer.";
        case Rodzaj::UslugaPrzerwala:
            return z_wielkiej(usluga) + " przerwała rozmowę albo odpowiedziała niezrozumiale. Spróbuj jeszcze raz.";
        case Rodzaj::UslugaOdmowila:
            return z_wielkiej(usluga) + " nie mogła wykonać zadania. Spróbuj jeszcze raz albo krótszym tekstem.";
        case Rodzaj::ZlyAdres:
            return "Adres \"" + adres + "\" jest niepoprawny. Podaj go jako host:port, np. 192.168.1.10:10200.";
        default:
            return "Coś poszło nie tak z: " + usluga + ". Spróbuj jeszcze raz.";
    }
}

std::string sciezka_dziennika() {
    fs::path k = katalog_stanu();
    return k.empty() ? std::string() : (k / "dziennik.log").string();
}

void zapisz_w_dzienniku(const std::string& wpis) {
    if (wpis.empty()) return;
    fs::path k = katalog_stanu();
    if (k.empty()) return;
    std::error_code ec;
    fs::create_directories(k, ec);
    if (ec) return;
    fs::path plik = k / "dziennik.log";
    if (fs::exists(plik, ec) && fs::file_size(plik, ec) > (1u << 20)) fs::rename(plik, k / "dziennik.log.1", ec);

    char czas[32] = "?";
    std::time_t teraz = std::time(nullptr);
    std::tm lok{};
    if (localtime_r(&teraz, &lok)) std::strftime(czas, sizeof czas, "%Y-%m-%d %H:%M:%S", &lok);
    std::ofstream f(plik, std::ios::app);
    if (f) f << czas << "  " << wpis << "\n";
}
