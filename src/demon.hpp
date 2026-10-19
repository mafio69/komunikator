// Tryb w tle: API przez gniazdo UNIX dla innych programów, kolejka mówienia (nikt nikogo nie przegada).
//
// Protokół (jedno żądanie na połączenie, tekst UTF-8):
//   żądanie:   jedna linia, np. "powiedz Kawa gotowa\n", "odtworz pies\n", "lista\n"
//   odpowiedź: "OK" albo "BLAD <kod>", potem linie wyniku:
//              "> ..." = standardowe wyjście polecenia, "! ..." = komunikaty i błędy
//   koniec odpowiedzi = zamknięcie połączenia.
#pragma once
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace demon {

// Wykonuje polecenie tak jak z linii poleceń; zwraca kod wyjścia (0/1/2).
using Wykonawca = std::function<int(const std::string& polecenie, const std::vector<std::string>& args,
                                    std::ostream& wyjscie, std::ostream& bledy)>;

std::string sciezka_gniazda();  // KOMUNIKATOR_GNIAZDO, albo $XDG_RUNTIME_DIR/komunikator.sock

// Linia żądania -> polecenie i argumenty (np. "zapisz pies Wyprowadź Dextera" -> zapisz, {pies, "Wyprowadź Dextera"})
bool rozbierz(const std::string& linia, std::string& polecenie, std::vector<std::string>& args);

int uruchom(const Wykonawca& wykonaj);  // działa do SIGTERM / SIGINT

// Klient: wysyła żądanie do działającego demona i przepisuje odpowiedź na stdout/stderr.
// Zwraca false, gdy demon nie działa (wtedy wołający robi wszystko sam).
bool przez_demona(const std::string& linia, int& kod);

}  // namespace demon
