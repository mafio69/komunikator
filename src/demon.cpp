#include "demon.hpp"

#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace demon {

namespace {

constexpr size_t MAKS_ZADANIE = 64 * 1024;
std::atomic<bool> koniec{false};

void na_sygnal(int) { koniec = true; }

sockaddr_un adres_gniazda(const std::string& sciezka) {
    sockaddr_un a{};
    a.sun_family = AF_UNIX;
    if (sciezka.size() >= sizeof a.sun_path) throw std::runtime_error("za długa ścieżka gniazda: " + sciezka);
    std::memcpy(a.sun_path, sciezka.c_str(), sciezka.size() + 1);
    return a;
}

// -1 = brak demona (nie ma pliku albo nikt nie słucha)
int polacz(const std::string& sciezka) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    sockaddr_un a = adres_gniazda(sciezka);
    if (connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool wyslij_wszystko(int fd, const std::string& b) {
    size_t poz = 0;
    while (poz < b.size()) {
        ssize_t n = send(fd, b.data() + poz, b.size() - poz, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        poz += static_cast<size_t>(n);
    }
    return true;
}

// Każdą linię tekstu poprzedza znacznikiem ("> " albo "! ").
std::string oznacz(const std::string& tekst, const char* znacznik) {
    std::string w;
    std::istringstream in(tekst);
    for (std::string l; std::getline(in, l);) w += znacznik + l + "\n";
    return w;
}

void obsluz(int fd, const Wykonawca& wykonaj) {
    timeval tv{10, 0};  // na samo żądanie; mówienie może trwać dłużej i nie jest tym ograniczone
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    std::string linia;
    char c;
    bool pelna = false;
    while (linia.size() < MAKS_ZADANIE) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        if (c == '\n') { pelna = true; break; }
        linia += c;
    }
    if (!linia.empty() && linia.back() == '\r') linia.pop_back();

    std::string odpowiedz;
    std::string polecenie;
    std::vector<std::string> args;
    if (!pelna && linia.size() >= MAKS_ZADANIE) {
        odpowiedz = "BLAD 2\n! za długie żądanie\n";
    } else if (!rozbierz(linia, polecenie, args)) {
        odpowiedz = "BLAD 2\n! puste żądanie (np. \"powiedz Dzień dobry\")\n";
    } else {
        std::ostringstream wyj, bl;
        int kod = wykonaj(polecenie, args, wyj, bl);
        odpowiedz = (kod == 0 ? std::string("OK") : "BLAD " + std::to_string(kod)) + "\n" + oznacz(wyj.str(), "> ") +
                    oznacz(bl.str(), "! ");
    }
    wyslij_wszystko(fd, odpowiedz);
    close(fd);
}

}  // namespace

std::string sciezka_gniazda() {
    if (const char* v = std::getenv("KOMUNIKATOR_GNIAZDO"); v && *v) return v;
    if (const char* v = std::getenv("XDG_RUNTIME_DIR"); v && *v) return std::string(v) + "/komunikator.sock";
    return "/tmp/komunikator-" + std::to_string(getuid()) + ".sock";
}

bool rozbierz(const std::string& linia, std::string& polecenie, std::vector<std::string>& args) {
    std::istringstream in(linia);
    if (!(in >> polecenie)) return false;
    std::string reszta;
    std::getline(in, reszta);
    auto a = reszta.find_first_not_of(" \t");
    reszta = a == std::string::npos ? "" : reszta.substr(a);

    // Polecenia, których ostatni argument to tekst ze spacjami; reszta argumentów to pojedyncze słowa.
    static const std::set<std::string> z_tekstem = {"powiedz", "zapisz"};
    args.clear();
    if (polecenie == "zapisz") {
        std::istringstream r(reszta);
        std::string nazwa;
        if (r >> nazwa) args.push_back(nazwa);
        std::string tekst;
        std::getline(r, tekst);
        auto b = tekst.find_first_not_of(" \t");
        if (b != std::string::npos) args.push_back(tekst.substr(b));
    } else if (z_tekstem.count(polecenie)) {
        if (!reszta.empty()) args.push_back(reszta);
    } else if (polecenie == "rozpoznaj") {
        if (!reszta.empty()) args.push_back(reszta);  // ścieżka może zawierać spacje
    } else {
        std::istringstream r(reszta);
        for (std::string s; r >> s;) args.push_back(s);
    }
    return true;
}

int uruchom(const Wykonawca& wykonaj) {
    std::string sciezka = sciezka_gniazda();

    int stary = polacz(sciezka);
    if (stary >= 0) {
        close(stary);
        std::cerr << "komunikator: demon już działa (" << sciezka << ")\n";
        return 1;
    }
    unlink(sciezka.c_str());  // pozostałość po demonie, który nie posprzątał

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) throw std::runtime_error(std::string("socket: ") + std::strerror(errno));
    sockaddr_un a = adres_gniazda(sciezka);
    mode_t poprzednia = umask(077);  // gniazdo tylko dla właściciela: inni użytkownicy nie każą mówić
    int r = bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a);
    umask(poprzednia);
    if (r != 0) throw std::runtime_error("nie mogę utworzyć gniazda " + sciezka + ": " + std::strerror(errno));
    if (listen(fd, 16) != 0) throw std::runtime_error(std::string("listen: ") + std::strerror(errno));

    struct sigaction sa{};
    sa.sa_handler = na_sygnal;  // bez SA_RESTART: accept() ma się przerwać, żeby demon mógł posprzątać
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    signal(SIGPIPE, SIG_IGN);

    std::cerr << "komunikator: demon słucha na " << sciezka << "\n";
    while (!koniec) {
        int klient = accept4(fd, nullptr, nullptr, SOCK_CLOEXEC);
        if (klient < 0) {
            if (errno == EINTR) continue;
            std::cerr << "komunikator: accept: " << std::strerror(errno) << "\n";
            continue;
        }
        std::thread(obsluz, klient, std::cref(wykonaj)).detach();
    }
    close(fd);
    unlink(sciezka.c_str());
    std::cerr << "komunikator: demon zatrzymany\n";
    return 0;
}

bool przez_demona(const std::string& linia, int& kod) {
    int fd = polacz(sciezka_gniazda());
    if (fd < 0) return false;
    if (!wyslij_wszystko(fd, linia + "\n")) {
        close(fd);
        return false;
    }
    std::string odp;
    char buf[4096];
    for (;;) {
        ssize_t n = recv(fd, buf, sizeof buf, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        odp.append(buf, static_cast<size_t>(n));
    }
    close(fd);
    if (odp.empty()) {
        std::cerr << "komunikator: demon nie odpowiedział\n";
        kod = 1;
        return true;
    }
    std::istringstream in(odp);
    std::string status;
    std::getline(in, status);
    kod = status == "OK" ? 0 : (status.rfind("BLAD ", 0) == 0 ? std::atoi(status.c_str() + 5) : 1);
    for (std::string l; std::getline(in, l);) {
        if (l.rfind("> ", 0) == 0) std::cout << l.substr(2) << "\n";
        else if (l.rfind("! ", 0) == 0) std::cerr << l.substr(2) << "\n";
    }
    return true;
}

}  // namespace demon
