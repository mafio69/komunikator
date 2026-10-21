// Błędy widoczne dla człowieka. Zasada projektu: użytkownik nigdy nie dostaje surowego błędu
// technicznego, tylko zdanie po ludzku z podpowiedzią, co zrobić. Szczegóły techniczne
// (errno, odpowiedź serwera, pozycja w JSON) trafiają do dziennika, żeby dało się zdiagnozować problem.
#pragma once

#include <stdexcept>
#include <string>

enum class Rodzaj {
    UslugaNiedostepna,   // nie da się połączyć (serwer wyłączony, zły adres, brak sieci)
    UslugaNieOdpowiada,  // połączenie jest, ale odpowiedź nie przychodzi w czasie
    UslugaPrzerwala,     // zerwane połączenie albo niezrozumiała odpowiedź
    UslugaOdmowila,      // usługa sama zgłosiła, że nie da rady
    ZlyAdres,            // adres usługi w złej postaci
    ZlyDzwiek,           // plik/strumień nie jest obsługiwanym nagraniem
    Chmura,              // usługa w internecie (Gemini) niedostępna albo odmówiła
    System,              // brak zasobu systemu (potok, plik tymczasowy, katalog domowy)
};

// what() zwraca komunikat dla człowieka; szczegoly() - opis techniczny do dziennika.
class Blad : public std::runtime_error {
public:
    Blad(Rodzaj rodzaj, const std::string& komunikat, std::string szczegoly = {});

    Rodzaj rodzaj() const noexcept { return rodzaj_; }
    const std::string& szczegoly() const noexcept { return szczegoly_; }

private:
    Rodzaj rodzaj_;
    std::string szczegoly_;
};

// Gotowe zdania o usługach mowy - jedno miejsce, żeby te same sytuacje brzmiały wszędzie tak samo.
// usluga: np. "usługa mowy" albo "usługa rozpoznawania mowy"; adres: "host:port".
std::string komunikat_uslugi(Rodzaj rodzaj, const std::string& usluga, const std::string& adres);

// Dopisuje wpis do dziennika (~/.local/state/komunikator/dziennik.log albo $XDG_STATE_HOME).
// Nigdy nie rzuca: brak dziennika nie może popsuć programu. Plik większy niż 1 MB
// przechodzi w dziennik.log.1 (jedna poprzednia wersja).
void zapisz_w_dzienniku(const std::string& wpis);

// Ścieżka dziennika do pokazania użytkownikowi ("" gdy nie da się jej ustalić).
std::string sciezka_dziennika();
