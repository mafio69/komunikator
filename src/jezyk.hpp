// Język komunikatów dla człowieka: polski albo angielski.
//
// Tłumaczone są tylko zdania czytane przez ludzi. Wszystko, co czytają programy, jest stałe
// w obu językach: nazwy poleceń (są też angielskie odpowiedniki), kody wyjścia, wynik na stdout
// oraz sygnały dla okna ("poziom 0.42", "cisza", linie klucz=wartość z `ustawienia`).
#pragma once

#include <initializer_list>
#include <string>

enum class Jezyk { Polski, Angielski };

Jezyk jezyk_interfejsu();
void ustaw_jezyk_interfejsu(Jezyk jezyk);

// "pl", "en", także pełne nazwy locale ("pl_PL.UTF-8", "en_GB"). false, gdy nie rozpoznano.
bool jezyk_z_nazwy(const std::string& nazwa, Jezyk& wynik);

// KOMUNIKATOR_INTERFEJS, a potem locale: LC_ALL, LC_MESSAGES, LANG. Neutralne "C" i "POSIX"
// są pomijane (np. LC_ALL=C.UTF-8 obok LANG=pl_PL ustawia tylko sortowanie, nie język).
// Bez żadnej wskazówki: angielski.
Jezyk jezyk_z_srodowiska();

// Zdanie w bieżącym języku. Kolejne "{}" w tekście zastępowane są kolejnymi argumentami:
// zdanie tłumaczy się w całości, bo szyk różni się między językami.
//   T("nie ma komunikatu \"{}\"", "there is no message \"{}\"", {nazwa})
std::string T(const char* pl, const char* en, std::initializer_list<std::string> wstawki = {});
