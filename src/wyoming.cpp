#include "wyoming.hpp"

#include "jezyk.hpp"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace wyoming {

namespace {
constexpr size_t MAKS_LINIA = 1 << 20;        // nagłówek zdarzenia
constexpr size_t MAKS_BLOK = 64u << 20;       // data / payload jednego zdarzenia
constexpr size_t MAKS_DZWIEK = 512u << 20;    // całe nagranie z syntezy
}  // namespace

Polaczenie::Polaczenie(const std::string& adres, int limit_sekund, const std::string& usluga)
    : adres_(adres), usluga_(usluga) {
    auto dw = adres.rfind(':');
    if (dw == std::string::npos || dw == 0 || dw + 1 >= adres.size())
        blad(Rodzaj::ZlyAdres, "adres bez postaci host:port: \"" + adres + "\"");
    std::string host = adres.substr(0, dw), port = adres.substr(dw + 1);
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']') host = host.substr(1, host.size() - 2);

    addrinfo wskazowki{};
    wskazowki.ai_family = AF_UNSPEC;
    wskazowki.ai_socktype = SOCK_STREAM;
    addrinfo* wyniki = nullptr;
    int r = getaddrinfo(host.c_str(), port.c_str(), &wskazowki, &wyniki);
    if (r != 0) blad(Rodzaj::UslugaNiedostepna, std::string("getaddrinfo: ") + gai_strerror(r));

    int ostatni_blad = 0;
    for (addrinfo* a = wyniki; a; a = a->ai_next) {
        int f = socket(a->ai_family, a->ai_socktype | SOCK_CLOEXEC, a->ai_protocol);
        if (f < 0) { ostatni_blad = errno; continue; }
        timeval tv{limit_sekund, 0};
        setsockopt(f, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(f, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);  // dotyczy też connect()
        if (connect(f, a->ai_addr, a->ai_addrlen) == 0) { fd_ = f; break; }
        ostatni_blad = errno;
        close(f);
    }
    freeaddrinfo(wyniki);
    if (fd_ < 0) blad(Rodzaj::UslugaNiedostepna, std::string("connect: ") + std::strerror(ostatni_blad));
}

void Polaczenie::blad(Rodzaj rodzaj, const std::string& szczegoly) const {
    throw Blad(rodzaj, komunikat_uslugi(rodzaj, usluga_, adres_), usluga_ + " " + adres_ + ": " + szczegoly);
}

Polaczenie::~Polaczenie() {
    if (fd_ >= 0) close(fd_);
}

void Polaczenie::wyslij_bajty(const std::string& b) {
    size_t wyslane = 0;
    while (wyslane < b.size()) {
        ssize_t n = send(fd_, b.data() + wyslane, b.size() - wyslane, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            blad(Rodzaj::UslugaPrzerwala, std::string("send: ") + std::strerror(errno));
        }
        wyslane += static_cast<size_t>(n);
    }
}

void Polaczenie::wyslij(const std::string& typ, const std::string& dane_json, const std::string& payload) {
    // "data" wpisane w nagłówek - starsza, wciąż obsługiwana przez serwery postać zdarzenia
    std::string naglowek = "{\"type\":" + json::cytuj(typ) + ",\"data\":" + dane_json;
    if (!payload.empty()) naglowek += ",\"payload_length\":" + std::to_string(payload.size());
    naglowek += "}\n";
    wyslij_bajty(naglowek + payload);
}

void Polaczenie::dociagnij() {
    for (;;) {
        char kawalek[65536];
        ssize_t n = recv(fd_, kawalek, sizeof kawalek, 0);
        if (n > 0) { bufor_.append(kawalek, static_cast<size_t>(n)); return; }
        if (n == 0) blad(Rodzaj::UslugaPrzerwala, "serwer zamknął połączenie");
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) blad(Rodzaj::UslugaNieOdpowiada, "recv: przekroczony czas");
        blad(Rodzaj::UslugaPrzerwala, std::string("recv: ") + std::strerror(errno));
    }
}

std::string Polaczenie::czytaj_dokladnie(size_t ile) {
    while (bufor_.size() < ile) dociagnij();
    std::string w = bufor_.substr(0, ile);
    bufor_.erase(0, ile);
    return w;
}

std::string Polaczenie::czytaj_linie() {
    for (;;) {
        auto nl = bufor_.find('\n');
        if (nl != std::string::npos) {
            std::string linia = bufor_.substr(0, nl);
            bufor_.erase(0, nl + 1);
            return linia;
        }
        if (bufor_.size() > MAKS_LINIA) blad(Rodzaj::UslugaPrzerwala, "nagłówek zdarzenia dłuższy niż limit");
        dociagnij();
    }
}

Zdarzenie Polaczenie::odbierz() {
    std::string linia = czytaj_linie();
    json::Value naglowek;
    try {
        naglowek = json::parsuj(linia);
    } catch (const std::runtime_error& e) {
        blad(Rodzaj::UslugaPrzerwala, std::string("nagłówek zdarzenia: ") + e.what());
    }
    Zdarzenie z;
    z.typ = naglowek["type"].tekst();
    if (z.typ.empty()) blad(Rodzaj::UslugaPrzerwala, "zdarzenie bez typu");
    z.dane = naglowek["data"];
    if (!z.dane.jest_obiektem()) z.dane = json::parsuj("{}");

    long long dl_danych = naglowek["data_length"].calk(0);
    long long dl_ladunku = naglowek["payload_length"].calk(0);
    if (dl_danych < 0 || dl_ladunku < 0 || static_cast<size_t>(dl_danych) > MAKS_BLOK ||
        static_cast<size_t>(dl_ladunku) > MAKS_BLOK)
        blad(Rodzaj::UslugaPrzerwala, "podejrzana długość danych zdarzenia");
    if (dl_danych > 0) {
        json::Value dodatkowe;
        try {
            dodatkowe = json::parsuj(czytaj_dokladnie(static_cast<size_t>(dl_danych)));
        } catch (const Blad&) {
            throw;
        } catch (const std::runtime_error& e) {
            blad(Rodzaj::UslugaPrzerwala, std::string("dane zdarzenia: ") + e.what());
        }
        for (auto& [k, v] : dodatkowe.obj) z.dane.obj[k] = v;
    }
    if (dl_ladunku > 0) z.payload = czytaj_dokladnie(static_cast<size_t>(dl_ladunku));
    return z;
}

std::string usluga_mowy() { return T("usługa mowy", "speech service"); }
std::string usluga_rozpoznawania() { return T("usługa rozpoznawania mowy", "speech recognition service"); }

namespace {

// Parametry dźwięku od serwera muszą mieć sens - inaczej dalsze liczenie (WAV, długość) daje bzdury.
bool rozsadny_format(const Audio& a) {
    return a.rate >= 1000 && a.rate <= 384000 && (a.width >= 1 && a.width <= 4) && a.channels >= 1 && a.channels <= 8;
}
}  // namespace

Audio syntezuj(const std::string& adres, const std::string& tekst, const std::string& glos) {
    Polaczenie p(adres, 60, usluga_mowy());
    std::string dane = "{\"text\":" + json::cytuj(tekst);
    if (!glos.empty()) dane += ",\"voice\":{\"name\":" + json::cytuj(glos) + "}";
    dane += "}";
    p.wyslij("synthesize", dane);

    Audio a;
    for (;;) {
        Zdarzenie z = p.odbierz();
        if (z.typ == "audio-start" || z.typ == "audio-chunk") {
            a.rate = static_cast<int>(z.dane["rate"].calk(a.rate));
            a.width = static_cast<int>(z.dane["width"].calk(a.width));
            a.channels = static_cast<int>(z.dane["channels"].calk(a.channels));
        }
        if (z.typ == "audio-chunk") {
            a.pcm += z.payload;
            if (a.pcm.size() > MAKS_DZWIEK)
                throw Blad(Rodzaj::UslugaPrzerwala, komunikat_uslugi(Rodzaj::UslugaPrzerwala, usluga_mowy(), adres),
                           "synteza: więcej dźwięku niż limit");
        } else if (z.typ == "audio-stop") {
            break;
        } else if (z.typ == "error") {
            throw Blad(Rodzaj::UslugaOdmowila, komunikat_uslugi(Rodzaj::UslugaOdmowila, usluga_mowy(), adres),
                       "synteza: serwer zgłosił błąd: " + z.dane["text"].tekst("(bez opisu)"));
        }
    }
    if (a.pcm.empty())
        throw Blad(Rodzaj::UslugaOdmowila, komunikat_uslugi(Rodzaj::UslugaOdmowila, usluga_mowy(), adres),
                   "synteza: brak dźwięku w odpowiedzi");
    if (!rozsadny_format(a))
        throw Blad(Rodzaj::UslugaPrzerwala, komunikat_uslugi(Rodzaj::UslugaPrzerwala, usluga_mowy(), adres),
                   "synteza: niemożliwy format dźwięku rate=" + std::to_string(a.rate) + " width=" +
                       std::to_string(a.width) + " channels=" + std::to_string(a.channels));
    return a;
}

std::string rozpoznaj(const std::string& adres, const Audio& audio, const std::string& jezyk) {
    Polaczenie p(adres, 300, usluga_rozpoznawania());  // rozpoznawanie długiego nagrania na CPU potrafi trwać
    std::string format = "\"rate\":" + std::to_string(audio.rate) + ",\"width\":" + std::to_string(audio.width) +
                         ",\"channels\":" + std::to_string(audio.channels);
    p.wyslij("transcribe", jezyk.empty() ? "{}" : "{\"language\":" + json::cytuj(jezyk) + "}");
    p.wyslij("audio-start", "{" + format + ",\"timestamp\":0}");
    size_t krok = static_cast<size_t>(audio.width * audio.channels) * 2048;
    for (size_t poz = 0; poz < audio.pcm.size(); poz += krok)
        p.wyslij("audio-chunk", "{" + format + "}", audio.pcm.substr(poz, krok));
    p.wyslij("audio-stop", "{\"timestamp\":0}");

    for (;;) {
        Zdarzenie z = p.odbierz();
        if (z.typ == "transcript") return z.dane["text"].tekst();
        if (z.typ == "error")
            throw Blad(Rodzaj::UslugaOdmowila, komunikat_uslugi(Rodzaj::UslugaOdmowila, usluga_rozpoznawania(), adres),
                       "rozpoznawanie: serwer zgłosił błąd: " + z.dane["text"].tekst("(bez opisu)"));
        // transcript-start / transcript-chunk itp. (tryb strumieniowy) - czekamy na pełny wynik
    }
}

json::Value opisz(const std::string& adres, const std::string& usluga) {
    Polaczenie p(adres, 10, usluga);
    p.wyslij("describe");
    for (;;) {
        Zdarzenie z = p.odbierz();
        if (z.typ == "info") return z.dane;
    }
}

}  // namespace wyoming
