# Instalacja komunikatora

Komunikator to jeden program. Do zbudowania potrzebny jest tylko kompilator C++ (g++ 9 lub nowszy)
i `make`. Nie potrzebuje żadnych bibliotek, Home Assistanta, Pythona ani internetu.

Do nagrywania i grania używa `arecord` i `aplay` (pakiet alsa-utils, jest w każdym Linuksie
z dźwiękiem). Do **działania** potrzebuje dwóch usług mowy w protokole Wyoming:

| usługa | do czego | domyślny adres |
|---|---|---|
| piper | tekst → mowa | `127.0.0.1:10200` |
| whisper (faster-whisper) | mowa → tekst | `127.0.0.1:10300` |

Na tym serwerze obie już działają jako kontenery `wyoming-piper` i `wyoming-whisper`.
Zapisane komunikaty grają **bez** tych usług, bo dźwięk jest zapisany na dysku.

## Szybko: instalator

Jedno polecenie robi wszystko: doinstaluje brakujące pakiety (zapyta o hasło do sudo), zbuduje
silnik i okno, zainstaluje je, zapyta o adres serwera mowy (Enter = `127.0.0.1`) i sprawdzi
połączenie.

```bash
bash ~/komunikator/instaluj.sh
```

Opcje: `--serwer ADRES` (bez pytania), `--bez-okna` (sam silnik, bez Qt), `--odinstaluj`.
Poniżej to samo krok po kroku, gdyby instalator się zatrzymał.

## 1. Zbudowanie

Wykonaj na serwerze, w terminalu:

```bash
cd ~/komunikator && make
```

Wynik: plik `komunikator` w tym samym katalogu.

## 2. Sprawdzenie

```bash
cd ~/komunikator && make test
```

Ma się skończyć napisem `wynik: dobrze 29, źle 0`. Testy nic nie mówią na głos.
Gdy usługi mowy nie działają, część B jest pomijana, a nie liczona jako błąd.

## 3. Instalacja (żeby działało z każdego miejsca)

```bash
cd ~/komunikator && make install
```

Kopiuje program do `~/.local/bin/komunikator`. Ten katalog jest już w ścieżce,
więc od razu działa samo `komunikator`. Bez sudo.

Sprawdź:

```bash
komunikator sprawdz
```

Obie linie mają zaczynać się od `OK`.

## 4. Wybór mikrofonu i głośnika

Program nie wybiera ich sam. Przy pierwszym nagraniu albo mówieniu pokaże listę i zapyta o numer.
Możesz też wybrać od razu:

```bash
komunikator mikrofony
komunikator mikrofon 2
komunikator glosniki
komunikator glosnik 2
```

Pozycja `default` oznacza urządzenie ustawione w systemie (ustawienia dźwięku pulpitu).
Podłączony później mikrofon albo głośnik USB pojawi się na liście sam.

## 5. Ustawienia (nieobowiązkowe)

Plik powstaje sam przy wyborze mikrofonu lub głośnika. Żeby coś zmienić, utwórz plik
`~/.config/komunikator/ustawienia`:

```ini
# adresy usług mowy (host:port)
tts=127.0.0.1:10200
stt=127.0.0.1:10300
# głos pipera; puste = domyślny głos usługi (tu: pl_PL-gosia-medium)
glos=
# język rozpoznawania
jezyk=pl
# rozpoznawanie: whisper (lokalnie) albo gemini (chmura; przy awarii i tak whisper)
rozpoznawanie=whisper
# model Gemini; domyślny alias wskazuje zawsze najnowszy model Flash
gemini_model=gemini-flash-latest
# czym grać dźwięk; puste = tylko wypisz ścieżkę do pliku
odtwarzacz=aplay -q
# wybrane urządzenia (ustawia je: komunikator mikrofon NUMER / komunikator glosnik NUMER)
mikrofon=plughw:0,0
glosnik=plughw:0,0
# gdzie trzymać komunikaty (domyślnie ~/.local/share/komunikator)
# dane=~/komunikaty
```

Każde ustawienie można też podać zmienną środowiska, która ma pierwszeństwo przed plikiem:
`KOMUNIKATOR_TTS`, `KOMUNIKATOR_STT`, `KOMUNIKATOR_GLOS`, `KOMUNIKATOR_JEZYK`,
`KOMUNIKATOR_ODTWARZACZ`, `KOMUNIKATOR_MIKROFON`, `KOMUNIKATOR_GLOSNIK`, `KOMUNIKATOR_DANE`,
`KOMUNIKATOR_ROZPOZNAWANIE`, `KOMUNIKATOR_GEMINI_KLUCZ`.

Klucza Gemini nie wpisuje się do tego pliku. Zapisuje go `komunikator klucz gemini`
(szczegóły w INSTRUKCJA.md).

Co jest aktualnie ustawione: `komunikator ustawienia`.

## 6. Na inny komputer z Linuksem

```bash
cd ~/komunikator && make statyczny
```

Powstaje jeden plik (ok. 3 MB) bez bibliotek dzielonych. Wystarczy go skopiować i ustawić adresy
usług mowy na IP serwera, np. `tts=192.168.1.10:10200`. Przy wersji statycznej podawaj adresy IP,
a nie nazwy hostów (tak działa glibc przy łączeniu statycznym).

## 7. Okno (aplikacja na pulpit)

Okno to osobny program `komunikator-okno`, nakładka na silnik. Silnik musi być zainstalowany
(punkt 3). Okno potrzebuje Qt5, które na Ubuntu instaluje się jednym poleceniem (wymaga sudo):

```bash
sudo apt install g++ make qtbase5-dev libqt5svg5
```

Zbudowanie i instalacja (bez sudo):

```bash
cd ~/komunikator && make okno-install
```

Program pojawi się w menu aplikacji jako „Komunikator”. Można go też uruchomić poleceniem
`komunikator-okno`. Przy pierwszym uruchomieniu kreator poprosi o wybór mikrofonu i głośnika.

Zamknięcie okna chowa je do ikony obok zegara. Program działa dalej w tle, a „Zakończ” jest
w menu tej ikony (prawy przycisk).

Zrzuty wszystkich ekranów do porównania z projektem, bez monitora i bez mówienia na głos:

```bash
cd ~/komunikator/okno && make zrzuty
```

Wynik: `okno/build/zrzuty/*.png`.

## Odinstalowanie

```bash
cd ~/komunikator && make uninstall
```

Zapisane komunikaty zostają w `~/.local/share/komunikator`. Usuń ten katalog ręcznie,
jeśli nie są już potrzebne.
