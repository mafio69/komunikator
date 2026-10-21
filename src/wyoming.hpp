// Klient protokołu Wyoming (Rhasspy / Home Assistant): piper = tekst na mowę, whisper = mowa na tekst.
// Protokół: linia JSON z nagłówkiem zdarzenia + opcjonalne bajty "data" i "payload". Tylko gniazda POSIX.
// Wszystkie błędy rzucane są jako Blad (bledy.hpp): przyjazny komunikat + szczegóły do dziennika.
#pragma once
#include <string>

#include "bledy.hpp"
#include "json.hpp"
#include "wav.hpp"

namespace wyoming {

struct Zdarzenie {
    std::string typ;
    json::Value dane;     // zawsze obiekt (może być pusty)
    std::string payload;  // np. próbki dźwięku
};

class Polaczenie {
public:
    // adres "host:port"; usluga - nazwa dla człowieka w komunikatach błędów, np. "usługa mowy"
    Polaczenie(const std::string& adres, int limit_sekund, const std::string& usluga);
    ~Polaczenie();
    Polaczenie(const Polaczenie&) = delete;
    Polaczenie& operator=(const Polaczenie&) = delete;

    void wyslij(const std::string& typ, const std::string& dane_json = "{}", const std::string& payload = "");
    Zdarzenie odbierz();

private:
    void wyslij_bajty(const std::string& b);
    void dociagnij();  // jeden odczyt z gniazda do bufora
    std::string czytaj_linie();
    std::string czytaj_dokladnie(size_t ile);
    [[noreturn]] void blad(Rodzaj rodzaj, const std::string& szczegoly) const;

    int fd_ = -1;
    std::string adres_;
    std::string usluga_;
    std::string bufor_;
};

// glos "" = domyślny głos serwera
Audio syntezuj(const std::string& adres, const std::string& tekst, const std::string& glos);
// jezyk "" = niech serwer zgaduje
std::string rozpoznaj(const std::string& adres, const Audio& audio, const std::string& jezyk);
// "describe" -> dane zdarzenia "info" (co serwer potrafi); usluga jak w Polaczenie
json::Value opisz(const std::string& adres, const std::string& usluga);

}  // namespace wyoming
