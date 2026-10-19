# Prompt dla programisty / agenta — okno „Komunikator” (Qt5 Widgets + QSS)

Masz gotowy projekt wyglądu w tym folderze. Zadanie: **zbuduj okno aplikacji zgodne z projektem i podłącz je do istniejącego silnika** (program tekstowy: mówienie, nagrywanie + rozpoznawanie mowy, gotowe komunikaty). Nie przepisuj silnika — dodaj tylko warstwę okna.

## Materiały
- `design/Komunikator.dc.html` (+ `design/support.js`) — podgląd wszystkich ekranów i stanów, otwórz w przeglądarce. Sekcje: 1a okno główne, 1b ustawienia i kreator, 1c stany, 1d ikony/ikona aplikacji/zasobnik, 1e paleta i wymiary.
- `SPEC.md` — rozmiary, paleta, kontrast, wzorzec QSS. `palette.json` — kolory obu motywów.
- `icons/*.svg`, `app/*`, `tray/*` — gotowe zasoby.

## Zasady (obowiązują bezwzględnie)
1. **Czytelność przede wszystkim**: najmniejszy tekst 16 px, zwykle 20–30. Nie zmniejszaj niczego „żeby się zmieściło” — zawartość ma się przewijać.
2. **Tylko to, co umie QSS**: kolor, ramka, promień, proste tło. Zero cieni, rozmycia, półprzezroczystości, animacji nakładek. Wskaźnik „Pracuję…” i poziom dźwięku rysuj jako zwykłe widgety (paski/segmenty), odświeżane timerem.
3. **Wszystkie kolory z `palette.json`**, w dwóch motywach. Motyw domyślnie jak w systemie (`QPalette`/`colorScheme`), przełączalny w kodzie jednym wywołaniem (wczytanie innego arkusza QSS). Nie wpisuj kolorów na sztywno w widgetach.
4. **Font**: Atkinson Hyperlegible 400 i 700 (SIL OFL) — dołącz pliki .ttf z licencją, załaduj `QFontDatabase::addApplicationFont`. Pliki fontu nie są w paczce; pobierz je z Google Fonts.
5. **Nie wybieraj mikrofonu ani głośnika sam.** Pierwsze uruchomienie = kreator; zapisany wybór trzymaj w ustawieniach.
6. Komunikaty błędów po polsku, jedno zdanie + podpowiedź co zrobić (teksty są w projekcie, użyj ich).
7. Nie zmieniaj logiki silnika. Okno komunikuje się z nim przez cienką warstwę (sygnały/sloty lub wywołania), którą opiszesz w jednym miejscu — żeby dało się ją podmienić.

## Ekrany
- **Okno główne** (`QMainWindow`): trzy duże zakładki u góry (Powiedz · Słuchaj · Komunikaty; własny pasek przycisków + `QStackedWidget`, nie standardowy `QTabBar`), pasek stanu na dole zawsze widoczny.
  - **Powiedz**: pole wieloliniowe (fokus: ramka 3 px), przycisk Powiedz 84 px, „Zapisz jako komunikat” (pyta o krótką nazwę).
  - **Słuchaj**: wybór 5/10/30 s, duży mikrofon, pole rozpoznanego tekstu, Kopiuj.
  - **Komunikaty**: szukajka + karty (nazwa, treść do 2 linii, Odtwórz, Usuń). Do ~900 px szerokości 1 kolumna, szerzej 3 kolumny; zawartość max 1160 px, wyśrodkowana. Usuń pyta o potwierdzenie.
  - **Pasek stanu**: kropka + opis (zielona „Mowa działa” / czerwona „Brak serwera mowy”), po prawej wybrany mikrofon i głośnik — kliknięcie otwiera Ustawienia.
- **Ustawienia**: mikrofon (lista + Test + wskaźnik poziomu), głośnik (lista + Test odgrywa krótkie zdanie), „Zaawansowane” zwijane z adresem serwera mowy.
- **Kreator pierwszego uruchomienia**: krok 1 mikrofon (z testem i wskaźnikiem), krok 2 głośnik (z testem); „Dalej” aktywne dopiero po wyborze urządzenia.
- **Ikona w zasobniku** (`QSystemTrayIcon`): 4 stany — spoczynek, mówi, nagrywa, błąd. Użyj plików z `tray/` odpowiednio do jasności panelu (`for-dark-panel` = jasny znak na ciemnym panelu); jeśli nie umiesz wykryć panelu, użyj wariantu ciemnego.

## Stany (wszystkie obowiązkowe, patrz sekcja 1c)
nagrywanie (czerwony mikrofon z pierścieniem, odliczanie, poziom) · mówienie (Powiedz → Zatrzymaj) · rozpoznawanie („Pracuję…”, przyciski chwilowo wyłączone) · cisza w nagraniu (ostrzeżenie + „Mikrofon” + „Spróbuj jeszcze raz”) · brak serwera mowy (czerwony pasek, kropki przy Powiedz i Słuchaj, **Komunikaty nadal w pełnych kolorach i działają**) · brak mikrofonu/głośnika (prośba o wybór → Ustawienia) · pusta lista (zachęta + „Dodaj pierwszy komunikat”).

## Klawiatura i dostępność
- Enter w polu Powiedz = Powiedz; Shift+Enter = nowa linia; podczas mówienia ten przycisk to Zatrzymaj.
- Spacja na przycisku mikrofonu = Słuchaj / Stop. Esc zatrzymuje mówienie/nagrywanie.
- Kolejność Tab: zakładki → zawartość → pasek stanu. Każdy element ma widoczny fokus 3 px (`:focus`, ramka 2→3 px: zmniejsz `padding` o 1 px, żeby nic nie skakało).
- Ustaw `accessibleName` na przyciskach z samą ikoną (Usuń, Kopiuj, Test).

## Rozmiary okna
Minimum 480×600, brak maksimum (do 1920×1080 i pełnego ekranu). Układ ma się dopasować: karty komunikatów zmieniają liczbę kolumn, mikrofon 132 px przy małym oknie, 150 px przy dużym.

## Ikony
SVG są czarne (#000). Koloruj w kodzie kolorem tekstu przycisku (`QPainter` + `CompositionMode_SourceIn` na `QPixmap`, ładowane przez `QSvgRenderer`), w rozmiarze przycisku × 1,3. Ikony ładuj z zasobów Qt (`.qrc`).

## Testy i gotowość
1. Ręcznie w obu motywach, przy 480×600, 1024×768 i 1920×1080: każdy ekran i każdy stan z listy powyżej.
2. Nawigacja samą klawiaturą: da się wejść wszędzie, fokus zawsze widoczny.
3. Z wyłączonym serwerem mowy: Komunikaty działają, Powiedz/Słuchaj pokazują czerwony stan, program nie zawiesza się.
4. Zrzuty ekranu wszystkich ekranów obok projektu — różnice tylko w granicach renderowania Qt.
5. Brak ostrzeżeń Qt w konsoli, brak twardo wpisanych kolorów poza arkuszami QSS.

Jeśli czegoś w projekcie brakuje albo jest sprzeczne z możliwościami QSS — nie wymyślaj, wypisz pytanie na końcu pracy.
