# Instrukcja komunikatora

Komunikator mówi tekst na głos, nagrywa gotowe komunikaty i zamienia nagraną mowę na tekst.
Mogą go używać ludzie, skrypty, cron i inne programy, tak samo jak dziś `powiedz` i `sluchaj`.

Pełna lista poleceń: `komunikator pomoc`.

## Powiedz coś od razu

```bash
komunikator powiedz Dzień dobry
```

Mówi przez wybrany głośnik tego komputera (zobacz „Mikrofon i głośnik”). Do głośnika Voice PE w salonie posłuży
rozszerzenie, które powstanie w kolejnym etapie.

Zamiast mówić, zapisz mowę do pliku:

```bash
komunikator powiedz -o ~/powitanie.wav Dzień dobry
```

## Gotowe komunikaty

Komunikat to tekst plus nagrany dźwięk, zapisany pod nazwą.

```bash
komunikator zapisz pies Wyprowadź Dextera na spacer
komunikator zapisz drzwi Drzwi wejściowe są otwarte
komunikator lista
komunikator odtworz pies
komunikator usun drzwi
```

- Dźwięk powstaje raz, przy `zapisz`. Potem `odtworz` gra go **bez usługi mowy**,
  czyli działa nawet wtedy, gdy kontener z piperem leży.
- Ponowne `zapisz` z tą samą nazwą nadpisuje komunikat.
- Nazwa: litery (także polskie), cyfry i znaki `- _ .`. Bez spacji i ukośników.
- Pliki leżą w `~/.local/share/komunikator/komunikaty/`:
  `NAZWA.txt` (tekst, możesz go edytować) i `NAZWA.wav` (dźwięk).
  Po ręcznej zmianie tekstu w `.txt` usuń `NAZWA.wav`: następne `odtworz` nagra go od nowa.

## Mikrofon i głośnik

Program niczego nie wybiera sam. Gdy mikrofon albo głośnik nie jest wybrany:
- w terminalu pokaże listę i zapyta o numer, a wybór zapamięta,
- wywołany przez inny program zakończy się kodem `2` i podpowie, co zrobić.

```bash
komunikator mikrofony        # lista, gwiazdka = wybrany
komunikator mikrofon 2       # wybierz numer 2
komunikator glosniki
komunikator glosnik 2
```

Na komputerze z pulpitem lista pochodzi z PipeWire: jest taka sama jak w ustawieniach dźwięku
Ubuntu, razem ze słuchawkami w gnieździe, USB i Bluetooth. Program gra i nagrywa przez PipeWire,
tak jak inne aplikacje. Bez PipeWire (np. serwer bez pulpitu) lista pochodzi wprost z kart dźwiękowych.

Słuchawki w gnieździe mini-jack to nie osobne urządzenie, tylko gniazdo tej samej karty co
głośnik. Wybierz kartę wbudowaną (albo `default`), a w ustawieniach dźwięku Ubuntu wyjście
„Słuchawki”.

Gdy wybrane urządzenie jest odłączone, program zgłasza błąd zamiast grać gdzie indziej.

`default` na liście to urządzenie ustawione w systemie. Mikrofon i głośnik USB pojawiają się
na liście same po podłączeniu. Gdy wybrane urządzenie zostanie odłączone, lista to pokaże.

## Posłuchaj (mikrofon → tekst)

```bash
komunikator sluchaj          # nagrywa 5 s i wypisuje, co powiedziałeś
komunikator sluchaj 10       # 10 s
komunikator nagraj 10 -o ~/notatka.wav   # tylko nagranie, bez rozpoznawania
```

Gdy nagranie jest prawie ciszą, program ostrzeże, że mikrofon jest wyciszony albo wybrany zły.

## Rozpoznaj mowę z pliku

```bash
komunikator rozpoznaj ~/nagranie.wav
```

Wypisuje rozpoznany tekst. Przyjmuje nieskompresowany WAV (PCM), np. z dyktafonu albo z `arecord`.

Rozpoznawanie dłuższego nagrania trwa kilka sekund, bo whisper liczy na procesorze.

## Rozpoznawanie przez Gemini (dokładniejsze, przez internet)

Whisper na serwerze działa bez internetu, ale myli się częściej. Gemini (Google) rozumie lepiej.
Potrzebny jest darmowy klucz: https://aistudio.google.com/apikey → „Create API key”.

```bash
komunikator klucz gemini              # wklej klucz i naciśnij Enter (nie jest wyświetlany)
komunikator ustaw rozpoznawanie gemini
```

Powrót do rozpoznawania lokalnego: `komunikator ustaw rozpoznawanie whisper`.
Usunięcie klucza: `komunikator klucz --usun`.

To samo da się zrobić w oknie: Ustawienia → Rozpoznawanie mowy.

Gdy Gemini zawiedzie (brak internetu, zły klucz, błąd Google), komunikator sam rozpozna
nagranie lokalnie i wypisze na stderr wiersz zaczynający się od `uwaga: Gemini`. Okno pokazuje
wtedy informację „Rozpoznane lokalnie”.

Klucz leży w `~/.config/komunikator/gemini-klucz` z prawami tylko dla Ciebie (600).
Nie pojawia się w poleceniach, na liście procesów ani w `komunikator ustawienia`: tam widać tylko
`gemini_klucz=zapisany` albo `brak`. Do Google idzie w nagłówku, przez plik tymczasowy usuwany
zaraz po wysłaniu. Do połączenia służy program `curl`.

## Gdy coś nie działa

```bash
komunikator sprawdz
```

- `BŁĄD ... brak połączenia` znaczy, że kontener nie działa. Sprawdź `docker ps` i uruchom go
  ponownie: `docker restart wyoming-piper` albo `docker restart wyoming-whisper`.
- `nagrywanie ... nie powiodło się` znaczy, że mikrofon jest zajęty przez inny program albo odłączony.
  Wybierz inny (`komunikator mikrofony`), najpewniej działa `default`.
- `nie znalazłem odtwarzacza` znaczy, że nie ma `aplay`. Dźwięk i tak jest w pliku podanym w komunikacie.
- `komunikator ustawienia` pokazuje, jakich adresów program używa.

## Okno

`komunikator-okno` (menu aplikacji → Komunikator) robi to samo co polecenia w terminalu:
- **Powiedz**: wpisz tekst, Enter mówi, Shift+Enter robi nową linię. Podczas mówienia ten sam
  przycisk to **Zatrzymaj**. „Zapisz jako komunikat” zapisuje zdanie pod nazwą.
- **Słuchaj**: wybierz 5, 10 albo 30 s i naciśnij mikrofon (albo spację). Druga spacja kończy
  nagrywanie wcześniej. Rozpoznany tekst można skopiować.
- **Komunikaty**: wyszukiwanie, **Odtwórz** i **Usuń**. Działają też bez serwera mowy.
- **Pasek na dole**: czy serwer mowy działa oraz wybrany mikrofon i głośnik. Kliknięcie
  otwiera ustawienia (wybór, test, rozpoznawanie: Gemini albo lokalnie, adres serwera).
- **Esc** zatrzymuje mówienie i nagrywanie.
- Odłączone słuchawki: pasek stanu pokazuje na czerwono „Niedostępny”, a Słuchaj prowadzi do ustawień.
- Program działa w jednej kopii: ponowne uruchomienie z menu pokazuje okno schowane w zasobniku.
- Zamknięcie programu w trakcie mówienia albo nagrywania zatrzymuje je, nic nie gra dalej w tle.

Wygląd pochodzi z projektu w `design/` (specyfikacja `SPEC.md`). Motyw jasny lub ciemny jest
taki jak w systemie. Można go wymusić: `KOMUNIKATOR_MOTYW=jasny komunikator-okno`.

## Dla innych programów

Komunikator jest zrobiony tak, żeby dało się go łatwo wywoływać z kodu.

**Kody wyjścia**
- `0`: dobrze,
- `1`: błąd wykonania (usługa, plik, odtwarzacz),
- `2`: złe polecenie, zła nazwa albo niewybrany mikrofon lub głośnik,
- `130`: zatrzymane (Ctrl+C albo sygnał SIGTERM). Mówienie wyłącza też odtwarzacz,
  a nagrywanie zapisuje to, co zdążyło się nagrać.

**Wejście i wyjście**
- Wynik (rozpoznany tekst, lista) idzie na standardowe wyjście, a komunikaty i błędy na
  standardowe wyjście błędów. Wynik nie miesza się więc z diagnostyką.
- Tekst można podać przez standardowe wejście, jeśli zamiast tekstu wpiszesz `-`:
  `echo "Uwaga" | komunikator powiedz -`.
- `komunikator lista` wypisuje `NAZWA<tabulator>TEKST`, jedną linię na komunikat.
- Ustawienia można nadpisać zmienną środowiska, np. `KOMUNIKATOR_ODTWARZACZ= komunikator powiedz ...`
  (puste = nie graj, tylko wypisz ścieżkę do pliku).

Przykłady:

```bash
# bash: alarm, a gdy usługa mowy leży - gotowy komunikat z dysku
komunikator powiedz "Pralka skończyła" || komunikator odtworz pralka

# Python
import subprocess
tekst = subprocess.run(["komunikator", "rozpoznaj", "nagranie.wav"],
                       capture_output=True, text=True, check=True).stdout.strip()
```

## Jak to działa w środku

Komunikator łączy się z usługami mowy przez **protokół Wyoming**. To zwykłe połączenie TCP:
najpierw linia JSON z opisem zdarzenia, a po niej surowe bajty dźwięku. Ten sam protokół
wykorzystuje Home Assistant do łączenia się z piperem i whisperem. Kod jest w katalogu `src/`:

| plik | co robi |
|---|---|
| `main.cpp` | polecenia, ustawienia, zapisane komunikaty |
| `wyoming.cpp` | połączenie i zdarzenia Wyoming: `synthesize`, `transcribe`, `describe` |
| `json.cpp` | mały czytnik i zapis JSON (bez bibliotek) |
| `wav.cpp` | zapis i odczyt plików WAV |

## Co dalej (rozszerzenia, osobno od podstawy)

Podstawa ma zostać samodzielna. Rozszerzenia będą dodatkiem, którego można nie używać:

- głośnik Voice PE w salonie, przez Home Assistanta (tak jak dziś `powiedz`),
- odbieranie wiadomości „Okay Nabu, wiadomość…” (tak jak dziś skrypt `sluchaj.sh`),
- praca w tle z API przez gniazdo UNIX i kolejką mówienia (szkic w `src/demon.*`, jeszcze niepodłączony),
- ewentualnie dostęp przez sieć dla programów z innych komputerów.
