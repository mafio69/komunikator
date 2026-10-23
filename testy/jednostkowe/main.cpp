// Punkt wejścia testów jednostkowych: testy rejestrują się same w plikach test_*.cpp.
#include "jezyk.hpp"
#include "sprawdzian.hpp"

int main() {
    // Testy sprawdzają treść komunikatów - język ustalony, niezależny od maszyny, na której biegną.
    ustaw_jezyk_interfejsu(Jezyk::Polski);
    return sprawdzian::uruchom_wszystkie();
}
