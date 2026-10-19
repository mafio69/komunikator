// Minimalny JSON: tyle, ile potrzeba do protokołu Wyoming. Bez zewnętrznych bibliotek.
#pragma once
#include <map>
#include <string>
#include <vector>

namespace json {

struct Value {
    enum class Typ { Null, Bool, Liczba, Tekst, Tablica, Obiekt };
    Typ typ = Typ::Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Value> tab;
    std::map<std::string, Value> obj;

    // Brakujący klucz zwraca pustą wartość (Null) zamiast wyjątku - wygodne przy opcjonalnych polach.
    const Value& operator[](const std::string& klucz) const;
    std::string tekst(const std::string& domyslny = "") const;
    long long calk(long long domyslna) const;
    bool jest_obiektem() const { return typ == Typ::Obiekt; }
    bool jest_tablica() const { return typ == Typ::Tablica; }
};

Value parsuj(const std::string& wejscie);       // rzuca std::runtime_error przy błędzie
std::string cytuj(const std::string& tekst);    // "tekst" z poprawnym escapowaniem

}  // namespace json
