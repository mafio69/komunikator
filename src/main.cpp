// komunikator - mówienie i rozpoznawanie mowy z linii poleceń, dla ludzi, skryptów i innych programów.
// Podstawa bez zależności: łączy się wprost z usługami mowy (Wyoming: piper, whisper).
// Instrukcja: INSTRUKCJA.md, instalacja: INSTALACJA.md.
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bledy.hpp"
#include "gemini.hpp"
#include "json.hpp"
#include "proces.hpp"
#include "wav.hpp"
#include "wyoming.hpp"

extern char** environ;

namespace fs = std::filesystem;

namespace {

constexpr const char* WERSJA = "0.5.0";

// Kody wyjścia - stałe, żeby inne programy mogły na nich polegać.
constexpr int OK = 0;
constexpr int BLAD = 1;         // coś nie wyszło (usługa, plik, odtwarzacz)
constexpr int ZLE_UZYCIE = 2;   // złe polecenie lub argumenty
constexpr int PRZERWANO = 130;  // zatrzymane na życzenie (Ctrl+C, przycisk Zatrzymaj)

struct Ustawienia {
    std::string tts = "127.0.0.1:10200";
    std::string stt = "127.0.0.1:10300";
    std::string glos;              // pusty = domyślny głos serwera
    std::string jezyk = "pl";
    std::string rozpoznawanie = "whisper";            // whisper (lokalnie) albo gemini (chmura, z zapasem na whisper)
    std::string gemini_model = "gemini-flash-latest";  // stały alias Google na najnowszy model Flash
    std::string odtwarzacz = "aplay -q";
    std::string mikrofon;  // urządzenie ALSA; puste = jeszcze nie wybrany (program poprosi o wybór)
    std::string glosnik;   // jak wyżej, dla odtwarzania
    fs::path dane;                 // katalog z komunikatami
    fs::path plik;                 // skąd wczytano ustawienia
};

// Zatrzymanie (Ctrl+C albo SIGTERM od okna): kończy też aplay/arecord, żeby nic nie grało dalej "samo".
// Nagrywanie po przerwaniu zapisuje to, co zdążyło się nagrać.
volatile sig_atomic_t przerwano = 0;
volatile pid_t dziecko = 0;

void na_przerwanie(int) {
    przerwano = 1;
    pid_t d = dziecko;
    if (d > 0) kill(d, SIGTERM);
    else _exit(130);  // nic nie gra ani nie nagrywa - po prostu kończymy
}

struct ZleUzycie : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::string przytnij(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

fs::path katalog_xdg(const char* zmienna, const char* domyslny_w_home) {
    const char* v = std::getenv(zmienna);
    if (v && *v) return fs::path(v);
    const char* home = std::getenv("HOME");
    if (!home || !*home) throw std::runtime_error("brak zmiennej HOME");
    return fs::path(home) / domyslny_w_home;
}

Ustawienia wczytaj_ustawienia() {
    Ustawienia u;
    u.plik = katalog_xdg("XDG_CONFIG_HOME", ".config") / "komunikator" / "ustawienia";
    u.dane = katalog_xdg("XDG_DATA_HOME", ".local/share") / "komunikator";

    std::ifstream f(u.plik);
    std::string linia;
    int nr = 0;
    while (std::getline(f, linia)) {
        ++nr;
        linia = przytnij(linia);
        if (linia.empty() || linia[0] == '#') continue;
        auto rowna = linia.find('=');
        if (rowna == std::string::npos) {
            std::cerr << "komunikator: " << u.plik.string() << ":" << nr << ": brak '=' - pomijam\n";
            continue;
        }
        std::string k = przytnij(linia.substr(0, rowna)), v = przytnij(linia.substr(rowna + 1));
        if (k == "tts") u.tts = v;
        else if (k == "stt") u.stt = v;
        else if (k == "glos") u.glos = v;
        else if (k == "jezyk") u.jezyk = v;
        else if (k == "rozpoznawanie") u.rozpoznawanie = v;
        else if (k == "gemini_model") u.gemini_model = v;
        else if (k == "odtwarzacz") u.odtwarzacz = v;
        else if (k == "mikrofon") u.mikrofon = v;
        else if (k == "glosnik") u.glosnik = v;
        else if (k == "dane") u.dane = v;
        else std::cerr << "komunikator: " << u.plik.string() << ":" << nr << ": nieznane ustawienie \"" << k << "\"\n";
    }

    // Zmienne środowiska mają pierwszeństwo przed plikiem - wygodne dla innych programów.
    auto nadpisz = [](const char* zmienna, std::string& cel) {
        if (const char* v = std::getenv(zmienna)) cel = v;
    };
    nadpisz("KOMUNIKATOR_TTS", u.tts);
    nadpisz("KOMUNIKATOR_STT", u.stt);
    nadpisz("KOMUNIKATOR_GLOS", u.glos);
    nadpisz("KOMUNIKATOR_JEZYK", u.jezyk);
    nadpisz("KOMUNIKATOR_ROZPOZNAWANIE", u.rozpoznawanie);
    nadpisz("KOMUNIKATOR_ODTWARZACZ", u.odtwarzacz);
    nadpisz("KOMUNIKATOR_MIKROFON", u.mikrofon);
    nadpisz("KOMUNIKATOR_GLOSNIK", u.glosnik);
    if (const char* v = std::getenv("KOMUNIKATOR_DANE")) u.dane = v;
    return u;
}

fs::path katalog_komunikatow(const Ustawienia& u) { return u.dane / "komunikaty"; }

// Nazwa komunikatu staje się nazwą pliku - nie może wyjść poza katalog ani zawierać dziwnych znaków.
void sprawdz_nazwe(const std::string& n) {
    if (n.empty() || n.size() > 64) throw ZleUzycie("nazwa komunikatu musi mieć od 1 do 64 znaków");
    if (n[0] == '.' || n[0] == '-') throw ZleUzycie("nazwa nie może zaczynać się od '.' ani '-'");
    if (n.front() == ' ' || n.back() == ' ') throw ZleUzycie("nazwa nie może zaczynać się ani kończyć spacją");
    for (unsigned char c : n) {
        bool dozwolony = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                         c == '_' || c == '.' || c == ' ' || c >= 0x80;  // >= 0x80: litery UTF-8, np. ą, ż
        if (!dozwolony)
            throw ZleUzycie("niedozwolony znak w nazwie \"" + n + "\" (dozwolone: litery, cyfry, spacja, - _ .)");
    }
}

// Zapis przez plik tymczasowy + rename: przerwany zapis nie zostawia połówki pliku.
void zapisz_plik(const fs::path& cel, const std::string& tresc) {
    fs::create_directories(cel.parent_path());
    fs::path tmp = cel;
    tmp += ".tmp" + std::to_string(getpid());
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) throw std::runtime_error("nie mogę pisać do " + tmp.string());
        f.write(tresc.data(), static_cast<std::streamsize>(tresc.size()));
        if (!f) throw std::runtime_error("błąd zapisu " + tmp.string());
    }
    fs::rename(tmp, cel);
}

std::string sekundy(const Audio& a) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f s", a.sekundy());
    return b;
}

std::string czytaj_wszystko(std::istream& in) {
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string czytaj_plik(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("nie mogę otworzyć " + p.string());
    return czytaj_wszystko(f);
}

std::vector<std::string> podziel_spacjami(const std::string& s) {
    std::istringstream in(s);
    std::vector<std::string> w;
    for (std::string x; in >> x;) w.push_back(x);
    return w;
}

// Wybór głośnika dotyczy aplay; własny odtwarzacz z ustawień sam wie, gdzie grać.
bool uzywa_aplay(const Ustawienia& u) {
    auto a = podziel_spacjami(u.odtwarzacz);
    return !a.empty() && fs::path(a[0]).filename() == "aplay";
}

// Odtwarza plik zewnętrznym odtwarzaczem (domyślnie aplay). To nie jest zależność do budowy:
// bez odtwarzacza program dalej działa, tylko wypisuje ścieżkę do pliku.
int odtworz_plik(const Ustawienia& u, const fs::path& plik) {
    std::vector<std::string> args = podziel_spacjami(u.odtwarzacz);
    if (args.empty()) {
        std::cout << plik.string() << "\n";
        return OK;
    }
    if (uzywa_aplay(u)) {
        args.push_back("-D");
        args.push_back(u.glosnik);
    }
    args.push_back(plik.string());
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);

    pid_t pid;
    if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ) != 0) {
        std::cerr << "komunikator: nie znalazłem odtwarzacza \"" << args[0] << "\"; dźwięk jest w pliku "
                  << plik.string() << "\n";
        return BLAD;
    }
    dziecko = pid;
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    dziecko = 0;
    if (przerwano) return PRZERWANO;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return OK;
    std::cerr << "komunikator: odtwarzacz \"" << args[0] << "\" zgłosił błąd; dźwięk jest w pliku " << plik.string()
              << "\n";
    return BLAD;
}

// Tekst z argumentów; pojedynczy "-" = czytaj ze standardowego wejścia (dla innych programów).
std::string tekst_z(const std::vector<std::string>& czesci) {
    std::string t;
    if (czesci.size() == 1 && czesci[0] == "-") {
        t = czytaj_wszystko(std::cin);
    } else {
        for (size_t i = 0; i < czesci.size(); ++i) t += (i ? " " : "") + czesci[i];
    }
    t = przytnij(t);
    if (t.empty()) throw ZleUzycie("brak tekstu do powiedzenia");
    return t;
}

// Wyciąga "-o PLIK" / "--do PLIK" z argumentów; reszta zostaje jako pozycyjne.
std::vector<std::string> wyjmij_wyjscie(const std::vector<std::string>& args, std::string& wyjscie) {
    std::vector<std::string> reszta;
    bool koniec_opcji = false;
    for (size_t i = 0; i < args.size(); ++i) {
        if (!koniec_opcji && args[i] == "--") { koniec_opcji = true; continue; }
        if (!koniec_opcji && (args[i] == "-o" || args[i] == "--do")) {
            if (i + 1 >= args.size()) throw ZleUzycie(args[i] + " wymaga nazwy pliku");
            wyjscie = args[++i];
            continue;
        }
        reszta.push_back(args[i]);
    }
    return reszta;
}

struct Urzadzenie {
    std::string nazwa;  // to, co dostaje arecord/aplay -D
    std::string opis;
};

// Rodzaj urządzenia dźwiękowego: mikrofon (nagrywanie) albo głośnik (odtwarzanie).
struct Rodzaj {
    const char* klucz;       // klucz w pliku ustawień
    const char* w_proc;      // słowo w /proc/asound/pcm
    const char* w_pipewire;  // media.class węzła PipeWire
    const char* lista;       // polecenie wypisujące listę
    const char* nazwa;       // dla ludzi
};
const Rodzaj MIKROFON{"mikrofon", "capture", "Audio/Source", "mikrofony", "mikrofon"};
const Rodzaj GLOSNIK{"glosnik", "playback", "Audio/Sink", "glosniki", "głośnik"};

const std::string PIPEWIRE = "pipewire:NODE=";  // urządzenie PipeWire dla aplay/arecord: pipewire:NODE=<node.name>


// Urządzenia z PipeWire - ta sama lista co w ustawieniach dźwięku pulpitu: gniazdo słuchawek, USB, Bluetooth.
// ok=false, gdy PipeWire nie działa (np. serwer bez pulpitu) - wtedy zostaje lista z jądra.
std::vector<Urzadzenie> urzadzenia_pipewire(const Rodzaj& r, bool& ok) {
    std::vector<Urzadzenie> w;
    std::string zrzut = wyjscie_programu({"pw-dump"}, ok);
    if (!ok) return w;
    json::Value wezly;
    try {
        wezly = json::parsuj(zrzut);
    } catch (const std::exception&) {
        ok = false;
        return w;
    }
    for (const auto& o : wezly.tab) {
        const json::Value& p = o["info"]["props"];
        if (p["media.class"].tekst() != r.w_pipewire) continue;
        std::string nazwa = p["node.name"].tekst();
        if (nazwa.empty()) continue;
        std::string opis = p["node.description"].tekst(p["node.nick"].tekst(nazwa));
        opis.erase(std::remove_if(opis.begin(), opis.end(), [](char c) { return c == '[' || c == ']'; }), opis.end());
        if (p["device.api"].tekst() == "bluez5") opis += " (Bluetooth)";
        w.push_back({PIPEWIRE + nazwa, opis});
    }
    return w;
}

std::string& pole(Ustawienia& u, const Rodzaj& r) { return &r == &MIKROFON ? u.mikrofon : u.glosnik; }
const std::string& pole(const Ustawienia& u, const Rodzaj& r) { return &r == &MIKROFON ? u.mikrofon : u.glosnik; }

// Lista urządzeń prosto z jądra (/proc/asound) - bez bibliotek. Pierwsza pozycja "default" to urządzenie
// ustawione w systemie (PipeWire / ustawienia dźwięku pulpitu) - do wyboru, ale nigdy wybierane samo.
std::vector<Urzadzenie> urzadzenia(const Rodzaj& r) {
    std::vector<Urzadzenie> w{{"default", std::string("ten, który jest ustawiony w systemie (ustawienia dźwięku)")}};
    // Na pulpicie urządzeniami zarządza PipeWire - tylko przez niego działa gniazdo słuchawek i Bluetooth,
    // a bezpośredni dostęp do karty (plughw) omija go i gra nie tam, gdzie trzeba.
    bool jest_pipewire = false;
    auto pw = urzadzenia_pipewire(r, jest_pipewire);
    if (jest_pipewire) {
        w.insert(w.end(), pw.begin(), pw.end());
        return w;
    }
    std::map<int, std::string> karty;  // numer karty -> pełna nazwa
    std::ifstream kf("/proc/asound/cards");
    for (std::string l; std::getline(kf, l);) {
        // " 0 [PCH            ]: HDA-Intel - HDA Intel PCH"
        std::istringstream in(l);
        int nr;
        if (!(in >> nr)) continue;
        auto myslnik = l.find(" - ");
        if (myslnik != std::string::npos) karty[nr] = przytnij(l.substr(myslnik + 3));
    }
    std::ifstream pf("/proc/asound/pcm");
    for (std::string l; std::getline(pf, l);) {
        // "00-02: ALC3234 Alt Analog : ALC3234 Alt Analog : capture 1"
        if (l.find(r.w_proc) == std::string::npos) continue;
        int karta = 0, urz = 0;
        if (std::sscanf(l.c_str(), "%d-%d:", &karta, &urz) != 2) continue;
        auto d = l.find(": ");
        auto k = l.find(" : ", d + 2);
        std::string nazwa = d == std::string::npos ? "?" : przytnij(l.substr(d + 2, k - d - 2));
        // plughw sam dopasowuje częstotliwość i liczbę kanałów - mniej "nieobsługiwany format"
        w.push_back({"plughw:" + std::to_string(karta) + "," + std::to_string(urz),
                     nazwa + " (" + (karty.count(karta) ? karty[karta] : "karta " + std::to_string(karta)) + ")"});
    }
    return w;
}

// Zapisuje jedno ustawienie w pliku, zostawiając resztę pliku (i komentarze) bez zmian.
void zapisz_ustawienie(const Ustawienia& u, const std::string& klucz, const std::string& wartosc) {
    std::string nowy;
    bool jest = false;
    std::ifstream f(u.plik);
    for (std::string l; std::getline(f, l);) {
        std::string t = przytnij(l);
        auto r = t.find('=');
        if (!t.empty() && t[0] != '#' && r != std::string::npos && przytnij(t.substr(0, r)) == klucz) {
            if (!jest) nowy += klucz + "=" + wartosc + "\n";
            jest = true;
            continue;
        }
        nowy += l + "\n";
    }
    if (!jest) nowy += klucz + "=" + wartosc + "\n";
    zapisz_plik(u.plik, nowy);
}

// PipeWire przy nieistniejącym urządzeniu NIE zgłasza błędu, tylko gra po cichu na domyślnym.
// Dlatego przed graniem i nagrywaniem sprawdzamy, czy wybrane urządzenie jest podłączone.
void sprawdz_podlaczenie(const std::string& urzadzenie, const Rodzaj& r) {
    if (urzadzenie.rfind(PIPEWIRE, 0) != 0) return;
    bool ok = false;
    auto lista = urzadzenia_pipewire(r, ok);
    if (!ok) throw std::runtime_error("PipeWire nie odpowiada - nie mogę użyć urządzenia: " + std::string(r.nazwa));
    for (const auto& x : lista)
        if (x.nazwa == urzadzenie) return;
    throw std::runtime_error(std::string("wybrany ") + r.nazwa + " jest niedostępny (odłączony?) - wybierz inny: komunikator " +
                             r.lista);
}

void wypisz_urzadzenia(const Ustawienia& u, const Rodzaj& r, const std::vector<Urzadzenie>& lista, std::ostream& out) {
    for (size_t i = 0; i < lista.size(); ++i)
        out << (lista[i].nazwa == pole(u, r) ? "* " : "  ") << i + 1 << "  " << lista[i].nazwa << "  "
            << lista[i].opis << "\n";
    const std::string& wybrany = pole(u, r);
    if (!wybrany.empty() &&
        std::none_of(lista.begin(), lista.end(), [&](const Urzadzenie& x) { return x.nazwa == wybrany; }))
        out << "* -  " << wybrany << "  (wybrany wcześniej, teraz go nie ma - odłączony?)\n";
}

// "2" -> nazwa z listy; nazwa urządzenia (np. plughw:1,0) przechodzi bez zmian. Pusty wynik = zły numer.
std::string wybierz_z_listy(const std::string& wybor, const std::vector<Urzadzenie>& lista) {
    size_t poz = 0;
    int nr = 0;
    try {
        nr = std::stoi(wybor, &poz);
    } catch (const std::exception&) {
        return wybor;
    }
    if (poz != wybor.size()) return wybor;
    if (nr < 1 || static_cast<size_t>(nr) > lista.size()) return "";
    return lista[static_cast<size_t>(nr) - 1].nazwa;
}

// Urządzenie musi być wybrane świadomie. W terminalu pytamy od razu; program wywołujący dostaje
// kod 2 i instrukcję - nic nie dzieje się "po cichu" na domyślnym urządzeniu.
void wymagaj(Ustawienia& u, const Rodzaj& r) {
    if (!pole(u, r).empty()) return;
    auto lista = urzadzenia(r);
    if (!isatty(STDIN_FILENO) || !isatty(STDERR_FILENO))
        throw ZleUzycie(std::string("nie wybrano urządzenia: ") + r.nazwa + ". Wybierz: komunikator " + r.lista +
                        ", potem komunikator " + r.klucz + " NUMER");
    std::cerr << "Nie wybrano urządzenia: " << r.nazwa << ". Dostępne:\n";
    wypisz_urzadzenia(u, r, lista, std::cerr);
    for (;;) {
        std::cerr << "Podaj numer: " << std::flush;
        std::string l;
        if (!std::getline(std::cin, l)) throw ZleUzycie(std::string("nie wybrano urządzenia: ") + r.nazwa);
        std::string w = wybierz_z_listy(przytnij(l), lista);
        if (w.empty() || przytnij(l).empty()) {
            std::cerr << "Nie ma takiego numeru.\n";
            continue;
        }
        pole(u, r) = w;
        zapisz_ustawienie(u, r.klucz, w);
        std::cerr << "Zapamiętane: " << r.nazwa << " = " << w << " (zmiana: komunikator " << r.klucz
                  << " NUMER)\n";
        return;
    }
}

// Nagrywa z wybranego mikrofonu przez arecord (część alsa-utils, jest na każdym Linuksie z dźwiękiem).
// 16 kHz mono 16 bit - dokładnie to, czego chce rozpoznawanie mowy. Dźwięk płynie potokiem, więc da się
// na bieżąco podawać poziom (poziomy=true: linie "poziom 0.42" co 0,1 s) i przerwać nagranie wcześniej.
Audio nagraj(const Ustawienia& u, int sekundy, const fs::path& plik, bool poziomy) {
    fs::create_directories(plik.parent_path());
    sprawdz_podlaczenie(u.mikrofon, MIKROFON);
    std::cerr << "nagrywam " << sekundy << " s z \"" << u.mikrofon << "\"... mów teraz\n";
    int rura[2];
    if (pipe2(rura, O_CLOEXEC) != 0) throw std::runtime_error("nie mogę utworzyć potoku");
    posix_spawn_file_actions_t akcje;
    posix_spawn_file_actions_init(&akcje);
    posix_spawn_file_actions_adddup2(&akcje, rura[1], STDOUT_FILENO);
    std::vector<std::string> args = {"arecord", "-q", "-D", u.mikrofon, "-f", "S16_LE", "-r", "16000",
                                     "-c", "1", "-d", std::to_string(sekundy), "-t", "raw"};
    std::vector<char*> argv;
    for (auto& x : args) argv.push_back(x.data());
    argv.push_back(nullptr);
    pid_t pid;
    int r = posix_spawnp(&pid, "arecord", &akcje, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&akcje);
    close(rura[1]);
    if (r != 0) {
        close(rura[0]);
        throw std::runtime_error("brak programu arecord (pakiet alsa-utils)");
    }
    dziecko = pid;

    Audio a;
    a.rate = 16000;
    a.width = 2;
    a.channels = 1;
    const size_t probek_w_oknie = 1600;  // 0,1 s
    size_t przetworzone = 0, w_oknie = 0;
    int szczyt_okna = 0;
    char bufor[4096];
    for (;;) {
        ssize_t n = read(rura[0], bufor, sizeof bufor);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        a.pcm.append(bufor, static_cast<size_t>(n));
        if (!poziomy) continue;
        for (; przetworzone + 1 < a.pcm.size(); przetworzone += 2) {
            int v = static_cast<int16_t>(static_cast<unsigned char>(a.pcm[przetworzone]) |
                                         (static_cast<unsigned char>(a.pcm[przetworzone + 1]) << 8));
            szczyt_okna = std::max(szczyt_okna, v < 0 ? -v : v);
            if (++w_oknie == probek_w_oknie) {
                // skala decybelowa -60..0 dB -> 0..1: pasek rusza się też przy cichym mówieniu
                double db = szczyt_okna > 0 ? 20.0 * std::log10(szczyt_okna / 32768.0) : -60.0;
                std::printf("poziom %.2f\n", std::clamp((db + 60.0) / 60.0, 0.0, 1.0));
                std::fflush(stdout);
                w_oknie = 0;
                szczyt_okna = 0;
            }
        }
    }
    close(rura[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    dziecko = 0;
    if (!przerwano && !(WIFEXITED(status) && WEXITSTATUS(status) == 0))
        throw std::runtime_error("nagrywanie z \"" + u.mikrofon +
                                 "\" nie powiodło się - sprawdź: komunikator mikrofony (zajęty lub odłączony?)");
    a.pcm.resize(a.pcm.size() - a.pcm.size() % 2);
    zapisz_plik(plik, wav_bajty(a));

    // Ostrzeżenie o ciszy: najczęstsza przyczyna "nic nie rozpoznał" to wyciszony albo zły mikrofon.
    int szczyt = 0;
    for (size_t i = 0; i + 1 < a.pcm.size(); i += 2) {
        int v = static_cast<int16_t>(static_cast<unsigned char>(a.pcm[i]) |
                                     (static_cast<unsigned char>(a.pcm[i + 1]) << 8));
        szczyt = std::max(szczyt, v < 0 ? -v : v);
    }
    if (szczyt < 200) {
        std::cerr << "uwaga: nagranie to prawie cisza - mikrofon wyciszony albo wybrany zły (komunikator mikrofony)\n";
        if (poziomy) std::cout << "cisza" << std::endl;
    }
    return a;
}

int sekundy_z(const std::vector<std::string>& args, int domyslne) {
    if (args.empty()) return domyslne;
    try {
        size_t poz = 0;
        int s = std::stoi(args[0], &poz);
        if (poz == args[0].size() && s >= 1 && s <= 600) return s;
    } catch (const std::exception&) {
    }
    throw ZleUzycie("czas nagrania to liczba sekund od 1 do 600, a nie \"" + args[0] + "\"");
}

void pomoc() {
    std::cout << R"(komunikator )" << WERSJA << R"( - mowa z linii poleceń

Mówienie:
  komunikator powiedz TEKST...          powiedz od razu (głośnik tego komputera)
  komunikator powiedz -o PLIK.wav TEKST zapisz mowę do pliku zamiast mówić
  echo "tekst" | komunikator powiedz -  tekst ze standardowego wejścia

Gotowe komunikaty (działają też bez usługi mowy - dźwięk jest zapisany):
  komunikator zapisz NAZWA TEKST...     nagraj komunikat pod nazwą
  komunikator odtworz NAZWA             odtwórz zapisany komunikat
  komunikator odtworz NAZWA -o PLIK.wav skopiuj jego dźwięk do pliku
  komunikator lista                     wszystkie komunikaty (NAZWA<tab>TEKST)
  komunikator usun NAZWA                usuń komunikat

Mikrofon i głośnik (przy pierwszym użyciu program sam poprosi o wybór):
  komunikator mikrofony                 lista mikrofonów (* = wybrany)
  komunikator mikrofon NUMER            wybierz mikrofon z listy (zapamiętuje)
  komunikator glosniki                  lista głośników (* = wybrany)
  komunikator glosnik NUMER             wybierz głośnik z listy (zapamiętuje)
  komunikator sluchaj [SEKUNDY]         nagraj z mikrofonu i wypisz rozpoznany tekst (domyślnie 5 s)
  komunikator nagraj [SEKUNDY] [-o PLIK.wav]  tylko nagraj (Ctrl+C kończy wcześniej i zapisuje)
             --poziomy                  wypisuj poziom dźwięku na bieżąco ("poziom 0.42" co 0,1 s)

Rozpoznawanie mowy:
  komunikator rozpoznaj PLIK.wav        wypisz, co powiedziano w nagraniu
  arecord -f S16_LE -r 16000 -c 1 -d 5 | komunikator rozpoznaj -

Diagnostyka:
  komunikator sprawdz                   czy usługi mowy odpowiadają
  komunikator glosy                     głosy dostępne w usłudze mowy
  komunikator ustawienia                bieżące ustawienia i skąd pochodzą
  komunikator ustaw KLUCZ WARTOŚĆ       zmień ustawienie, np. ustaw tts 192.168.1.10:10200
  komunikator ustaw rozpoznawanie gemini   rozpoznawanie przez Gemini (chmura), zapas: whisper
  komunikator klucz gemini              zapisz klucz Gemini (wklej, Enter; nie jest wyświetlany)
  komunikator wersja

Kody wyjścia: 0 = dobrze, 1 = błąd wykonania, 2 = złe polecenie, 130 = zatrzymane.
Szczegóły: INSTRUKCJA.md
)";
}

int cmd_powiedz(const Ustawienia& u, const std::vector<std::string>& args) {
    std::string wyjscie;
    std::string tekst = tekst_z(wyjmij_wyjscie(args, wyjscie));
    if (wyjscie.empty() && uzywa_aplay(u)) sprawdz_podlaczenie(u.glosnik, GLOSNIK);
    Audio a = wyoming::syntezuj(u.tts, tekst, u.glos);
    if (!wyjscie.empty()) {
        zapisz_plik(fs::absolute(wyjscie), wav_bajty(a));
        std::cerr << "zapisano " << wyjscie << " (" << sekundy(a) << ")\n";
        return OK;
    }
    fs::path ostatni = u.dane / "ostatni.wav";
    zapisz_plik(ostatni, wav_bajty(a));
    return odtworz_plik(u, ostatni);
}

int cmd_zapisz(const Ustawienia& u, const std::vector<std::string>& args) {
    if (args.size() < 2) throw ZleUzycie("użycie: komunikator zapisz NAZWA TEKST...");
    const std::string& nazwa = args[0];
    sprawdz_nazwe(nazwa);
    std::string tekst = tekst_z(std::vector<std::string>(args.begin() + 1, args.end()));
    Audio a = wyoming::syntezuj(u.tts, tekst, u.glos);
    fs::path kat = katalog_komunikatow(u);
    bool byl = fs::exists(kat / (nazwa + ".txt"));
    zapisz_plik(kat / (nazwa + ".wav"), wav_bajty(a));
    zapisz_plik(kat / (nazwa + ".txt"), tekst + "\n");
    std::cerr << (byl ? "nadpisano " : "zapisano ") << nazwa << " (" << sekundy(a) << ")\n";
    return OK;
}

int cmd_odtworz(const Ustawienia& u, const std::vector<std::string>& args) {
    std::string wyjscie;
    auto reszta = wyjmij_wyjscie(args, wyjscie);
    if (reszta.size() != 1) throw ZleUzycie("użycie: komunikator odtworz NAZWA [-o PLIK.wav]");
    sprawdz_nazwe(reszta[0]);
    fs::path kat = katalog_komunikatow(u);
    fs::path wav = kat / (reszta[0] + ".wav"), txt = kat / (reszta[0] + ".txt");
    if (!fs::exists(txt)) {
        std::cerr << "komunikator: nie ma komunikatu \"" << reszta[0] << "\" (zobacz: komunikator lista)\n";
        return BLAD;
    }
    if (wyjscie.empty() && uzywa_aplay(u)) sprawdz_podlaczenie(u.glosnik, GLOSNIK);
    if (!fs::exists(wav)) {
        // dźwięk zginął, a tekst jest - odtwarzamy dźwięk z usługi mowy
        zapisz_plik(wav, wav_bajty(wyoming::syntezuj(u.tts, przytnij(czytaj_plik(txt)), u.glos)));
    }
    if (!wyjscie.empty()) {
        zapisz_plik(fs::absolute(wyjscie), czytaj_plik(wav));
        return OK;
    }
    return odtworz_plik(u, wav);
}

int cmd_lista(const Ustawienia& u) {
    fs::path kat = katalog_komunikatow(u);
    std::vector<std::string> nazwy;
    if (fs::exists(kat))
        for (auto& e : fs::directory_iterator(kat))
            if (e.path().extension() == ".txt") nazwy.push_back(e.path().stem().string());
    std::sort(nazwy.begin(), nazwy.end());
    for (auto& n : nazwy) {
        std::string t = przytnij(czytaj_plik(kat / (n + ".txt")));
        std::replace(t.begin(), t.end(), '\n', ' ');
        std::cout << n << "\t" << t << "\n";
    }
    if (nazwy.empty()) std::cerr << "(brak zapisanych komunikatów)\n";
    return OK;
}

int cmd_usun(const Ustawienia& u, const std::vector<std::string>& args) {
    if (args.size() != 1) throw ZleUzycie("użycie: komunikator usun NAZWA");
    sprawdz_nazwe(args[0]);
    fs::path kat = katalog_komunikatow(u);
    bool byl = fs::remove(kat / (args[0] + ".txt"));
    byl = fs::remove(kat / (args[0] + ".wav")) || byl;
    if (!byl) {
        std::cerr << "komunikator: nie ma komunikatu \"" << args[0] << "\"\n";
        return BLAD;
    }
    std::cerr << "usunięto " << args[0] << "\n";
    return OK;
}

// Klucz Gemini: zmienna KOMUNIKATOR_GEMINI_KLUCZ albo plik obok ustawień (prawa 600). Nigdy nie wypisywany.
fs::path plik_klucza(const Ustawienia& u) { return u.plik.parent_path() / "gemini-klucz"; }

std::string klucz_gemini(const Ustawienia& u) {
    if (const char* v = std::getenv("KOMUNIKATOR_GEMINI_KLUCZ"); v && *v) return przytnij(v);
    std::ifstream f(plik_klucza(u));
    std::string k;
    std::getline(f, k);
    return przytnij(k);
}

// Rozpoznawanie: Gemini, gdy wybrane - a przy jakimkolwiek kłopocie lokalny whisper. Brak internetu
// nie może zostawić użytkownika bez rozpoznawania.
std::string rozpoznaj_mowe(const Ustawienia& u, const Audio& a) {
    if (u.rozpoznawanie == "gemini") {
        try {
            const char* run = std::getenv("XDG_RUNTIME_DIR");
            std::string tmp = run && *run ? run : u.dane.string();
            fs::create_directories(tmp);
            return gemini::rozpoznaj(a, klucz_gemini(u), u.gemini_model, u.jezyk, tmp);
        } catch (const Blad& e) {
            zapisz_w_dzienniku("Gemini: " + std::string(e.what()) + " | " + e.szczegoly());
            std::cerr << "uwaga: Gemini niedostępne - rozpoznaję lokalnie (whisper)\n";
        } catch (const std::exception& e) {
            zapisz_w_dzienniku(std::string("Gemini: ") + e.what());
            std::cerr << "uwaga: Gemini niedostępne - rozpoznaję lokalnie (whisper)\n";
        }
    }
    return wyoming::rozpoznaj(u.stt, a, u.jezyk);
}

int cmd_klucz(const Ustawienia& u, const std::vector<std::string>& args) {
    if (args.size() != 1 || (args[0] != "gemini" && args[0] != "--usun"))
        throw ZleUzycie("użycie: komunikator klucz gemini  (klucz podaj na wejściu, np. wklej i Enter)  |  klucz --usun");
    if (args[0] == "--usun") {
        fs::remove(plik_klucza(u));
        std::cerr << "klucz Gemini usunięty\n";
        return OK;
    }
    // Klucz ze standardowego wejścia, nie z argumentów: nie zostaje w historii poleceń ani na liście procesów.
    if (isatty(STDIN_FILENO)) std::cerr << "Wklej klucz Gemini i naciśnij Enter: ";
    std::string k;
    std::getline(std::cin, k);
    k = przytnij(k);
    if (k.size() < 20 || k.find_first_of(" \t\"'") != std::string::npos)
        throw ZleUzycie("to nie wygląda na klucz Gemini (aistudio.google.com -> Get API key)");
    fs::create_directories(plik_klucza(u).parent_path());
    mode_t stara = umask(077);
    try {
        zapisz_plik(plik_klucza(u), k + "\n");
    } catch (...) {
        umask(stara);
        throw;
    }
    umask(stara);
    fs::permissions(plik_klucza(u), fs::perms::owner_read | fs::perms::owner_write);
    std::cerr << "klucz Gemini zapisany (" << plik_klucza(u).string() << ", tylko dla Ciebie)\n";
    return OK;
}

int cmd_rozpoznaj(const Ustawienia& u, const std::vector<std::string>& args) {
    if (args.size() != 1) throw ZleUzycie("użycie: komunikator rozpoznaj PLIK.wav  (albo - dla wejścia)");
    std::string bajty = args[0] == "-" ? czytaj_wszystko(std::cin) : czytaj_plik(args[0]);
    Audio a = wav_z_bajtow(bajty);
    if (a.pcm.empty()) throw std::runtime_error("nagranie jest puste");
    std::cout << przytnij(rozpoznaj_mowe(u, a)) << "\n";
    return OK;
}

int cmd_sprawdz(const Ustawienia& u) {
    int wynik = OK;
    auto sprawdz = [&](const char* co, const std::string& adres, const char* klucz, const char* usluga) {
        try {
            json::Value info = wyoming::opisz(adres, usluga);
            std::string programy;
            for (auto& p : info[klucz].tab)
                programy += (programy.empty() ? "" : ", ") + p["name"].tekst("?") + " " + p["version"].tekst();
            std::cout << "OK    " << co << " " << adres << "  " << (programy.empty() ? "(bez opisu)" : programy)
                      << "\n";
        } catch (const Blad& e) {
            zapisz_w_dzienniku(e.szczegoly());
            std::cout << "BŁĄD  " << co << " " << adres << "  " << e.what() << "\n";
            wynik = BLAD;
        }
    };
    sprawdz("mowa (tts)         ", u.tts, "tts", "usługa mowy");
    sprawdz("rozpoznawanie (stt)", u.stt, "asr", "usługa rozpoznawania mowy");
    return wynik;
}

int cmd_glosy(const Ustawienia& u) {
    json::Value info = wyoming::opisz(u.tts, "usługa mowy");
    for (auto& program : info["tts"].tab)
        for (auto& g : program["voices"].tab) {
            bool zainstalowany = g["installed"].typ != json::Value::Typ::Bool || g["installed"].b;
            if (zainstalowany) std::cout << g["name"].tekst("?") << "\n";
        }
    return OK;
}

int cmd_urzadzenia(const Ustawienia& u, const Rodzaj& r) {
    wypisz_urzadzenia(u, r, urzadzenia(r), std::cout);
    if (pole(u, r).empty())
        std::cerr << "(nic nie wybrano - wybierz: komunikator " << r.klucz << " NUMER)\n";
    return OK;
}

int cmd_wybierz(const Ustawienia& u, const Rodzaj& r, const std::vector<std::string>& args) {
    if (args.empty()) {
        if (pole(u, r).empty()) {
            std::cerr << "nic nie wybrano - lista: komunikator " << r.lista << "\n";
            return BLAD;
        }
        std::cout << pole(u, r) << "\n";
        return OK;
    }
    if (args.size() != 1)
        throw ZleUzycie(std::string("użycie: komunikator ") + r.klucz + " NUMER  (numer z: komunikator " + r.lista + ")");
    std::string w = wybierz_z_listy(args[0], urzadzenia(r));
    if (w.empty()) throw ZleUzycie("nie ma numeru " + args[0] + " na liście (komunikator " + r.lista + ")");
    zapisz_ustawienie(u, r.klucz, w);
    std::cerr << "wybrany " << r.nazwa << ": " << w << " (zapisane w " << u.plik.string() << ")\n";
    return OK;
}

int cmd_nagraj(const Ustawienia& u, const std::vector<std::string>& args) {
    std::string wyjscie;
    auto reszta = wyjmij_wyjscie(args, wyjscie);
    bool poziomy = false;
    reszta.erase(std::remove_if(reszta.begin(), reszta.end(),
                                [&](const std::string& x) { return x == "--poziomy" && (poziomy = true); }),
                 reszta.end());
    if (reszta.size() > 1) throw ZleUzycie("użycie: komunikator nagraj [SEKUNDY] [--poziomy] [-o PLIK.wav]");
    fs::path plik = wyjscie.empty() ? u.dane / "nagranie.wav" : fs::absolute(wyjscie);
    nagraj(u, sekundy_z(reszta, 5), plik, poziomy);
    std::cout << plik.string() << "\n";
    return OK;
}

int cmd_sluchaj(const Ustawienia& u, const std::vector<std::string>& args) {
    if (args.size() > 1) throw ZleUzycie("użycie: komunikator sluchaj [SEKUNDY]");
    Audio a = nagraj(u, sekundy_z(args, 5), u.dane / "nagranie.wav", false);
    if (a.pcm.empty()) throw std::runtime_error("nic się nie nagrało");
    std::cout << przytnij(rozpoznaj_mowe(u, a)) << "\n";
    return OK;
}

int cmd_ustaw(const Ustawienia& u, const std::vector<std::string>& args) {
    static const std::set<std::string> klucze = {"tts", "stt", "glos", "jezyk", "odtwarzacz", "mikrofon", "glosnik", "dane",
                                                  "rozpoznawanie", "gemini_model"};
    if (args.size() != 2 || !klucze.count(args[0]))
        throw ZleUzycie("użycie: komunikator ustaw KLUCZ WARTOŚĆ  (klucze: tts stt glos jezyk odtwarzacz mikrofon glosnik dane)");
    const std::string& v = args[1];
    if (v.find('\n') != std::string::npos) throw ZleUzycie("wartość nie może mieć wielu linii");
    if (args[0] == "rozpoznawanie" && v != "whisper" && v != "gemini")
        throw ZleUzycie("rozpoznawanie: whisper (lokalnie) albo gemini (chmura)");
    if ((args[0] == "tts" || args[0] == "stt") && v.rfind(':') == std::string::npos)
        throw ZleUzycie("adres podaj jako host:port, np. 192.168.1.10:10200");
    zapisz_ustawienie(u, args[0], v);
    std::cerr << "zapisano " << args[0] << "=" << v << "\n";
    return OK;
}

int cmd_ustawienia(const Ustawienia& u) {
    std::cout << "plik ustawień: " << u.plik.string() << (fs::exists(u.plik) ? "" : "  (nie istnieje - domyślne)")
              << "\n"
              << "tts=" << u.tts << "\nstt=" << u.stt << "\nglos=" << u.glos << "\njezyk=" << u.jezyk << "\nrozpoznawanie=" << u.rozpoznawanie
              << "\ngemini_model=" << u.gemini_model << "\ngemini_klucz=" << (klucz_gemini(u).empty() ? "brak" : "zapisany")
              << "\nodtwarzacz=" << u.odtwarzacz << "\nmikrofon=" << u.mikrofon << "\nglosnik=" << u.glosnik << "\ndane=" << u.dane.string() << "\n";
    return OK;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty() || args[0] == "pomoc" || args[0] == "-h" || args[0] == "--help") {
        pomoc();
        return args.empty() ? ZLE_UZYCIE : OK;
    }
    std::string polecenie = args[0];
    args.erase(args.begin());
    struct sigaction sa {};
    sa.sa_handler = na_przerwanie;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    try {
        if (polecenie == "wersja" || polecenie == "--version") {
            std::cout << "komunikator " << WERSJA << "\n";
            return OK;
        }
        Ustawienia u = wczytaj_ustawienia();
        if (polecenie == "nagraj" || polecenie == "sluchaj") wymagaj(u, MIKROFON);
        // głośnik potrzebny tylko, gdy naprawdę gramy (bez -o) i przez aplay
        bool do_pliku = std::find(args.begin(), args.end(), "-o") != args.end() ||
                        std::find(args.begin(), args.end(), "--do") != args.end();
        if ((polecenie == "powiedz" || polecenie == "odtworz") && !do_pliku && uzywa_aplay(u)) wymagaj(u, GLOSNIK);
        if (polecenie == "powiedz") return cmd_powiedz(u, args);
        if (polecenie == "zapisz") return cmd_zapisz(u, args);
        if (polecenie == "odtworz") return cmd_odtworz(u, args);
        if (polecenie == "lista") return cmd_lista(u);
        if (polecenie == "usun") return cmd_usun(u, args);
        if (polecenie == "rozpoznaj") return cmd_rozpoznaj(u, args);
        if (polecenie == "sprawdz") return cmd_sprawdz(u);
        if (polecenie == "glosy") return cmd_glosy(u);
        if (polecenie == "ustawienia") return cmd_ustawienia(u);
        if (polecenie == "ustaw") return cmd_ustaw(u, args);
        if (polecenie == "klucz") return cmd_klucz(u, args);
        if (polecenie == "mikrofony") return cmd_urzadzenia(u, MIKROFON);
        if (polecenie == "mikrofon") return cmd_wybierz(u, MIKROFON, args);
        if (polecenie == "glosniki") return cmd_urzadzenia(u, GLOSNIK);
        if (polecenie == "glosnik") return cmd_wybierz(u, GLOSNIK, args);
        if (polecenie == "nagraj") return cmd_nagraj(u, args);
        if (polecenie == "sluchaj") return cmd_sluchaj(u, args);
        throw ZleUzycie("nieznane polecenie \"" + polecenie + "\" (komunikator pomoc)");
    } catch (const ZleUzycie& e) {
        std::cerr << "komunikator: " << e.what() << "\n";
        return ZLE_UZYCIE;
    } catch (const Blad& e) {
        // Człowiek dostaje zdanie z podpowiedzią; szczegóły techniczne idą do dziennika
        // (a na ekran tylko na życzenie: KOMUNIKATOR_SZCZEGOLY=1).
        zapisz_w_dzienniku(polecenie + ": " + e.what() + " | " + e.szczegoly());
        std::cerr << "komunikator: " << e.what() << "\n";
        if (const char* s = std::getenv("KOMUNIKATOR_SZCZEGOLY"); s && *s == '1' && !e.szczegoly().empty())
            std::cerr << "  szczegóły: " << e.szczegoly() << "\n";
        return BLAD;
    } catch (const std::exception& e) {
        // Nieprzewidziany błąd: komunikaty z wnętrza programu bywają techniczne, więc nie pokazujemy ich
        // wprost, tylko odsyłamy do dziennika. Nasze własne, już ludzkie komunikaty (runtime_error z main.cpp)
        // też tu trafiają - dlatego sprawdzamy, czy tekst wygląda na techniczny.
        std::string tekst = e.what();
        zapisz_w_dzienniku(polecenie + ": " + tekst);
        bool techniczny = tekst.find("JSON") != std::string::npos || tekst.find("std::") != std::string::npos ||
                          tekst.find("errno") != std::string::npos || tekst.find("basic_string") != std::string::npos ||
                          tekst.find("filesystem error") != std::string::npos;
        if (techniczny) {
            std::string dz = sciezka_dziennika();
            std::cerr << "komunikator: Coś poszło nie tak." << (dz.empty() ? "" : " Szczegóły zapisałem w dzienniku: " + dz)
                      << "\n";
        } else {
            std::cerr << "komunikator: " << tekst << "\n";
        }
        return BLAD;
    }
}
