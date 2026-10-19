# Brief dla grafika: wygląd aplikacji „komunikator”

## Co to jest

Mała aplikacja na laptopa z Linuksem (Ubuntu, Dell Latitude 5420). Służy do:
- **mówienia**: wpisany tekst program czyta na głos,
- **słuchania**: nagrywa z mikrofonu i zamienia mowę na tekst,
- **gotowych komunikatów**: zapisane zdania do odtworzenia jednym kliknięciem (np. „Wyprowadź psa”).

Silnik już działa jako program tekstowy. Potrzebny jest wygląd okna, które go obsłuży.

## Najważniejsza zasada: czytelność

- Duże litery i mało tekstu. Wszędzie, gdzie się da, ikona z krótkim podpisem zamiast zdania.
- Wysoki kontrast. Tryb jasny i ciemny.
- Najczęstsze czynności („Powiedz”, „Słuchaj”, „Odtwórz”) mają być największe i od razu widoczne.
- Komunikaty o błędach zwykłym językiem, jednym zdaniem, z podpowiedzią, co zrobić.
- Font o otwartej licencji, bardzo czytelny, z polskimi znakami, np. Atkinson Hyperlegible albo
  Lexend. Do Twojej propozycji.

## Ekrany

### 1. Okno główne

Trzy części. Mogą być zakładkami albo jednym widokiem, do Twojej decyzji:

| część | elementy |
|---|---|
| **Powiedz** | pole tekstowe (kilka linii), przycisk **Powiedz**, przycisk **Zapisz jako komunikat** (pyta o krótką nazwę) |
| **Słuchaj** | duży przycisk mikrofonu, wybór długości nagrania (5 / 10 / 30 s), pole z rozpoznanym tekstem, przycisk **Kopiuj** |
| **Komunikaty** | lista lub kafelki: nazwa i treść; przy każdym **Odtwórz** i **Usuń**; na górze wyszukiwanie |

Pasek stanu na dole:
- czy usługi mowy działają (zielona albo czerwona kropka i krótki opis),
- jaki mikrofon i głośnik są wybrane (kliknięcie otwiera ustawienia).

### 2. Ustawienia

- **Mikrofon**: lista rozwijana z dostępnych urządzenia, obok przycisk **Test** i wskaźnik poziomu
  dźwięku (pasek, który rusza się, gdy mówisz).
- **Głośnik**: lista rozwijana i przycisk **Test** (odgrywa krótkie zdanie).
- **Serwer mowy**: adres (pole tekstowe, rzadko zmieniane, może być w sekcji „zaawansowane”).

### 3. Pierwsze uruchomienie

Program **nie wybiera mikrofonu ani głośnika sam**. Przy pierwszym uruchomieniu prosi o wybór.
Najlepiej jako prosty kreator z dwoma krokami („Wybierz mikrofon” → „Wybierz głośnik”), każdy z testem.

### 4. Ikona w zasobniku (obok zegara)

Program może działać w tle. Ikona w zasobniku w stanach: spoczynek, mówi, nagrywa, błąd.

## Stany do zaprojektowania

| stan | co widać |
|---|---|
| nagrywanie | wyraźnie (np. czerwony mikrofon), odliczanie sekund, poziom dźwięku |
| mówienie | przycisk **Powiedz** zmienia się w **Zatrzymaj** |
| rozpoznawanie | krótkie „pracuję…” (trwa kilka sekund) |
| cisza w nagraniu | ostrzeżenie: „Nic nie słychać — sprawdź mikrofon” |
| brak serwera mowy | czerwony pasek stanu; **zapisane komunikaty dalej działają**, to ma być widać (nie wyszarzać ich) |
| brak mikrofonu/głośnika | prośba o wybór, prowadzi do ustawień |
| pusta lista komunikatów | zachęta i przycisk „Dodaj pierwszy komunikat” |

## Ograniczenia techniczne

Okno będzie zbudowane w **Qt5 (Widgets)** i stylowane arkuszem stylów Qt (QSS, podobny do CSS).
Proszę projektować w tych granicach:
- kolory, czcionki, odstępy, zaokrąglenia, obramowania, proste gradienty: tak,
- rozmycie tła, złożone animacje, nakładające się półprzezroczyste warstwy: lepiej nie,
- okno o zmiennej wielkości, od około 480×600 do pełnego ekranu 1920×1080, układ ma się dopasować,
- obsługa z klawiatury: Enter = Powiedz, spacja na przycisku mikrofonu = Słuchaj.

## Co potrzebuję od Ciebie

1. Projekty ekranów z punktów 1–3 w trybie jasnym i ciemnym. Figma albo PNG.
2. Paletę kolorów (kody hex) z opisem, gdzie który kolor jest używany.
3. Font lub fonty z licencją pozwalającą dołączyć je do programu (np. SIL OFL).
4. Ikony w SVG, jednokolorowe: powiedz, słuchaj/mikrofon, głośnik, odtwórz, zatrzymaj, zapisz,
   usuń, kopiuj, ustawienia, szukaj, stan OK, błąd.
5. Ikonę aplikacji: SVG oraz PNG 256, 128, 64, 48, 32 i 16 px.
6. Ikonę zasobnika w czterech stanach (SVG, czytelna w 16 i 22 px na jasnym i ciemnym panelu).
7. Krótką specyfikację: rozmiary fontów, odstępy, promienie zaokrągleń, wysokość przycisków.

## Poza zakresem

Wersja na telefon i strona internetowa. Nazwa „komunikator” zostaje. Logo może być proste.
