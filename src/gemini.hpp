// Rozpoznawanie mowy przez Google Gemini (chmura). Rozszerzenie, nie podstawa:
// gdy nie ma internetu, klucza albo Google zwróci błąd, wołający przechodzi na lokalnego whispera.
// Połączenie przez program curl (jest w każdym Ubuntu) - komunikator nadal nie ma bibliotek do budowy.
#pragma once
#include <string>

#include "wav.hpp"

namespace gemini {

// Tekst z nagrania. Rzuca std::runtime_error z opisem po polsku, gdy się nie da.
std::string rozpoznaj(const Audio& audio, const std::string& klucz, const std::string& model,
                      const std::string& jezyk, const std::string& katalog_tymczasowy);

}  // namespace gemini
