# komunikator

Program w C++, który mówi tekst na głos, zapisuje gotowe komunikaty i rozpoznaje mowę z nagrań.
Działa z linii poleceń, więc mogą go używać ludzie, skrypty i inne programy.
Nie ma żadnych zależności poza usługami mowy (piper i whisper w protokole Wyoming).
Rozszerzenie: rozpoznawanie przez Gemini (Google, przez internet), z whisperem jako zapasem.

```bash
komunikator powiedz Dzień dobry
komunikator zapisz pies Wyprowadź Dextera na spacer
komunikator odtworz pies
komunikator rozpoznaj nagranie.wav
```

- [INSTALACJA.md](INSTALACJA.md): zbudowanie, testy, instalacja, ustawienia
- [INSTRUKCJA.md](INSTRUKCJA.md): wszystkie polecenia, przykłady dla innych programów, jak to działa
