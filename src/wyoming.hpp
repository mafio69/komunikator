// Klient protokołu Wyoming (Rhasspy / Home Assistant): piper = tekst na mowę, whisper = mowa na tekst.
// Protokół: linia JSON z nagłówkiem zdarzenia + opcjonalne bajty "data" i "payload". Tylko gniazda POSIX.
#pragma once
#include <string>

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
    Polaczenie(const std::string& adres, int limit_sekund);  // adres "host:port"
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

    int fd_ = -1;
    std::string adres_;
    std::string bufor_;
};

// glos "" = domyślny głos serwera
Audio syntezuj(const std::string& adres, const std::string& tekst, const std::string& glos);
// jezyk "" = niech serwer zgaduje
std::string rozpoznaj(const std::string& adres, const Audio& audio, const std::string& jezyk);
// "describe" -> dane zdarzenia "info" (co serwer potrafi)
json::Value opisz(const std::string& adres);

}  // namespace wyoming
