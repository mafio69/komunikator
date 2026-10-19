// Uruchamianie programów pomocniczych (pw-dump, curl) i odczyt ich wyjścia - bez powłoki, bez bibliotek.
#pragma once
#include <string>
#include <vector>

// Zwraca standardowe wyjście programu; ok=false, gdy programu nie ma albo zakończył się błędem
// (wyjście i tak jest zwracane - np. curl oddaje treść błędu HTTP).
std::string wyjscie_programu(std::vector<std::string> args, bool& ok);
