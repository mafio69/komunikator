// Klient Wyoming (src/wyoming.cpp) z udawanym serwerem w osobnym wątku: protokół i przyjazne błędy.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <thread>

#include "bledy.hpp"
#include "sprawdzian.hpp"
#include "wyoming.hpp"

namespace {

// Słowa, które nie mogą trafić do komunikatu dla człowieka.
bool techniczny(const std::string& tekst) {
    for (const char* slowo : {"Connection refused", "connect:", "recv", "errno", "JSON", "getaddrinfo", "pozycja"})
        if (tekst.find(slowo) != std::string::npos) return true;
    return false;
}

// Serwer na losowym porcie 127.0.0.1: odbiera jedną linię nagłówka i odsyła przygotowane bajty.
class UdawanySerwer {
public:
    explicit UdawanySerwer(std::string odpowiedz, bool milcz = false) {
        gniazdo_ = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(gniazdo_, reinterpret_cast<sockaddr*>(&a), sizeof a);
        listen(gniazdo_, 1);
        socklen_t dl = sizeof a;
        getsockname(gniazdo_, reinterpret_cast<sockaddr*>(&a), &dl);
        port_ = ntohs(a.sin_port);
        watek_ = std::thread([this, odpowiedz = std::move(odpowiedz), milcz] {
            int k = accept(gniazdo_, nullptr, nullptr);
            if (k < 0) return;
            char c;
            while (recv(k, &c, 1, 0) == 1 && c != '\n') {}
            if (milcz) {
                char reszta[256];
                while (recv(k, reszta, sizeof reszta, 0) > 0) {}  // czeka, aż klient się rozłączy
            } else {
                send(k, odpowiedz.data(), odpowiedz.size(), MSG_NOSIGNAL);
            }
            close(k);
        });
    }
    ~UdawanySerwer() {
        shutdown(gniazdo_, SHUT_RDWR);
        if (watek_.joinable()) watek_.join();
        close(gniazdo_);
    }
    std::string adres() const { return "127.0.0.1:" + std::to_string(port_); }

private:
    int gniazdo_ = -1;
    int port_ = 0;
    std::thread watek_;
};

int wolny_port() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(s, reinterpret_cast<sockaddr*>(&a), sizeof a);
    socklen_t dl = sizeof a;
    getsockname(s, reinterpret_cast<sockaddr*>(&a), &dl);
    close(s);  // port był wolny, a teraz nikt na nim nie słucha
    return ntohs(a.sin_port);
}

template <typename Dzialanie>
Blad zlap(Dzialanie dzialanie) {
    try {
        dzialanie();
    } catch (const Blad& b) {
        return b;
    }
    throw sprawdzian::Porazka{"oczekiwano wyjątku Blad"};
}

}  // namespace

TEST(wyoming_synteza_sklada_dzwiek_z_kawalkow) {
    std::string pcm1(400, '\x01'), pcm2(200, '\x02');
    UdawanySerwer s(
        "{\"type\":\"audio-start\",\"data\":{\"rate\":16000,\"width\":2,\"channels\":1}}\n"
        "{\"type\":\"audio-chunk\",\"data\":{\"rate\":16000,\"width\":2,\"channels\":1},\"payload_length\":400}\n" +
        pcm1 + "{\"type\":\"audio-chunk\",\"payload_length\":200}\n" + pcm2 + "{\"type\":\"audio-stop\"}\n");
    Audio a = wyoming::syntezuj(s.adres(), "dzień dobry", "");
    SPRAWDZ_ROWNE(16000, a.rate);
    SPRAWDZ_ROWNE(pcm1 + pcm2, a.pcm);
}

TEST(wyoming_zly_adres_to_przyjazny_komunikat) {
    for (const char* zly : {"bez-portu", ":10200", "host:"}) {
        Blad b = zlap([&] { wyoming::Polaczenie p(zly, 1, "usługa mowy"); });
        SPRAWDZ(b.rodzaj() == Rodzaj::ZlyAdres);
        SPRAWDZ(std::string(b.what()).find("host:port") != std::string::npos);
    }
}

TEST(wyoming_wylaczony_serwer_bez_technicznych_slow) {
    std::string adres = "127.0.0.1:" + std::to_string(wolny_port());
    Blad b = zlap([&] { wyoming::syntezuj(adres, "test", ""); });
    SPRAWDZ(b.rodzaj() == Rodzaj::UslugaNiedostepna);
    SPRAWDZ(!techniczny(b.what()));
    SPRAWDZ(std::string(b.what()).find("usługą mowy") == std::string::npos);  // forma "z: usługa mowy"
    SPRAWDZ(std::string(b.what()).find(adres) != std::string::npos);          // adres pomaga znaleźć kłopot
    SPRAWDZ(b.szczegoly().find("connect") != std::string::npos);              // szczegóły są dla dziennika
}

TEST(wyoming_smieci_od_serwera_bez_technicznych_slow) {
    UdawanySerwer s("to nie jest JSON\n");
    Blad b = zlap([&] { wyoming::syntezuj(s.adres(), "test", ""); });
    SPRAWDZ(b.rodzaj() == Rodzaj::UslugaPrzerwala);
    SPRAWDZ(!techniczny(b.what()));
    SPRAWDZ(b.szczegoly().find("JSON") != std::string::npos);
}

TEST(wyoming_blad_zgloszony_przez_usluge) {
    UdawanySerwer s("{\"type\":\"error\",\"data\":{\"text\":\"Voice not found\"}}\n");
    Blad b = zlap([&] { wyoming::syntezuj(s.adres(), "test", "nieznany-glos"); });
    SPRAWDZ(b.rodzaj() == Rodzaj::UslugaOdmowila);
    SPRAWDZ(std::string(b.what()).find("Voice not found") == std::string::npos);  // angielski tekst serwera tylko w dzienniku
    SPRAWDZ(b.szczegoly().find("Voice not found") != std::string::npos);
}

TEST(wyoming_niemozliwy_format_dzwieku_odrzucony) {
    UdawanySerwer s("{\"type\":\"audio-chunk\",\"data\":{\"rate\":0,\"width\":2,\"channels\":1},\"payload_length\":4}\nabcd"
                    "{\"type\":\"audio-stop\"}\n");
    Blad b = zlap([&] { wyoming::syntezuj(s.adres(), "test", ""); });
    SPRAWDZ(b.rodzaj() == Rodzaj::UslugaPrzerwala);
}

TEST(wyoming_milczacy_serwer_to_brak_odpowiedzi) {
    UdawanySerwer s("", /*milcz=*/true);
    Blad b = zlap([&] {
        wyoming::Polaczenie p(s.adres(), 1, "usługa mowy");
        p.wyslij("describe");
        p.odbierz();
    });
    SPRAWDZ(b.rodzaj() == Rodzaj::UslugaNieOdpowiada);
    SPRAWDZ(!techniczny(b.what()));
}
