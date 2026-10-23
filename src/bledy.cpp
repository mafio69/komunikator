#include "bledy.hpp"

#include "jezyk.hpp"

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
    // Pierwsze litery nazw usług są ASCII ("usługa...", "speech..."), więc wystarczy zwykła zamiana.
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
    const std::string Usluga = z_wielkiej(usluga);
    switch (rodzaj) {
        case Rodzaj::UslugaNiedostepna:
            return T("Nie mogę połączyć się z: {} ({}). Sprawdź, czy serwer jest włączony i czy adres jest dobry: "
                     "komunikator ustawienia.",
                     "I can't reach the {} ({}). Check that the server is running and the address is right: "
                     "komunikator settings.",
                     {usluga, adres});
        case Rodzaj::UslugaNieOdpowiada:
            return T("{} ({}) nie odpowiada. Spróbuj za chwilę; jeśli to się powtarza, sprawdź serwer.",
                     "The {} ({}) is not responding. Try again in a moment; if it keeps happening, check the server.",
                     {jezyk_interfejsu() == Jezyk::Polski ? Usluga : usluga, adres});
        case Rodzaj::UslugaPrzerwala:
            return T("{} przerwała rozmowę albo odpowiedziała niezrozumiale. Spróbuj jeszcze raz.",
                     "The {} broke off the conversation or answered with something unreadable. Please try again.",
                     {jezyk_interfejsu() == Jezyk::Polski ? Usluga : usluga});
        case Rodzaj::UslugaOdmowila:
            return T("{} nie mogła wykonać zadania. Spróbuj jeszcze raz albo krótszym tekstem.",
                     "The {} could not do the job. Please try again, perhaps with a shorter text.",
                     {jezyk_interfejsu() == Jezyk::Polski ? Usluga : usluga});
        case Rodzaj::ZlyAdres:
            return T("Adres \"{}\" jest niepoprawny. Podaj go jako host:port, np. 192.168.1.10:10200.",
                     "The address \"{}\" is not valid. Give it as host:port, e.g. 192.168.1.10:10200.", {adres});
        default:
            return T("Coś poszło nie tak z: {}. Spróbuj jeszcze raz.", "Something went wrong with the {}. Please try again.",
                     {usluga});
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
