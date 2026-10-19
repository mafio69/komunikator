# Komunikator — specyfikacja wyglądu (Qt5 Widgets + QSS)

Podgląd wszystkich ekranów: `Komunikator.dc.html` (sekcje 1a–1e). Wszystko da się zapisać w QSS: kolory, ramki, promienie, proste wypełnienia. Brak cieni, rozmycia, półprzezroczystości.

## Font
Atkinson Hyperlegible, wagi 400 i 700, licencja SIL OFL 1.1 (wolno dołączać do programu). Pliki .ttf należy pobrać z Google Fonts / Braille Institute i dołączyć razem z plikiem licencji. Załadować przez `QFontDatabase::addApplicationFont`.

## Rozmiary tekstu
| Rola | Rozmiar | Waga | Uwagi |
|---|---|---|---|
| Tytuł kreatora | 36 px | Bold | „Wybierz mikrofon” |
| Tytuł ekranu | 30 px | Bold | „Ustawienia” |
| Przycisk główny (Powiedz, Zatrzymaj) | 30 px | Bold | wysokość 84 |
| Podpis mikrofonu, licznik | 28 px / 52 px | Bold | „Słuchaj”, „7 s” |
| Tekst w polu Powiedz | 24 px | Regular | interlinia 1,4 |
| Nagłówek sekcji, nazwa komunikatu, tekst baneru, etykieta pola | 22 px | Bold |  |
| Przycisk standardowy, lista rozwijana, zakładka | 20 px (zakładka 19) | Bold |  |
| Treść komunikatu na karcie, opis w banerze | 18 px | Regular |  |
| Pasek stanu, klawisz-podpowiedź, opis pomocniczy | 16 px | Regular / Bold | najmniejszy rozmiar w programie |

## Wymiary
| Element | Wartość | Uwagi |
|---|---|---|
| Przycisk główny (Powiedz) | 84 px | promień 16 |
| Przycisk standardowy | 60 px | promień 12, ramka 2 |
| Segment, lista rozwijana, wyszukiwarka, Odtwórz, Usuń, Test | 56 px | promień 12 |
| Zakładka (ikona nad podpisem) | 76 px | promień 14 |
| Pasek stanu | 56 px | góra: linia 2 px |
| Przycisk mikrofonu | 132 px (150 przy dużym oknie) | koło, w nagrywaniu pierścień 4 px |
| Karta komunikatu | auto, padding 14 | promień 16, ramka 2 |
| Wskaźnik poziomu | 22 px | segmenty po 4 px odstępu, promień 3 |
| Obwódka fokusu | 3 px | kolor „Fokus” |
| Odstępy | 4 · 8 · 12 · 16 · 24 | margines okna 16, między kartami 12 |
| Okno | 480×600 … 1920×1080 | od 900 px siatka kart 2–3 kolumn |

## Paleta (hex)
Pełne wartości także w `palette.json`.

| Rola | Ciemny | Jasny | Gdzie |
|---|---|---|---|
| Tło okna | `#111418` | `#F3F5F7` | Tło całego okna, za zakładkami |
| Panel / karta | `#1A1F25` | `#FFFFFF` | Karty komunikatów, pasek stanu, nieaktywne zakładki i przyciski dodatkowe |
| Pole | `#0C0F12` | `#FFFFFF` | Wnętrze pól tekstowych i list rozwijanych |
| Ramka | `#5B6773` | `#6A7683` | Ramki pól, przycisków, nieaktywnych zakładek (≥ 3:1) |
| Ramka lekka | `#2E3640` | `#C9D0D7` | Linie podziału, ramki kart |
| Tekst | `#F4F6F8` | `#0D1217` | Tekst główny |
| Tekst pomocniczy | `#C4CCD5` | `#38434E` | Treść komunikatów na kartach, opisy pod nagłówkami |
| Tekst słaby | `#A0ABB7` | `#55616D` | Podpowiedzi w polach, ikona szukania |
| Powiedz — niebieski | `#6CB2FF` / `#06182B` | `#0B57D0` / `#FFFFFF` | Zakładka i przycisk Powiedz, pole w fokusie, kreator (tło / tekst na nim) |
| Słuchaj — bursztyn | `#FFB84D` / `#2A1700` | `#A35700` / `#FFFFFF` | Zakładka Słuchaj, przycisk mikrofonu, wybrana długość nagrania |
| Komunikaty — fiolet | `#B9A2FF` / `#1B0B4A` | `#5B34C4` / `#FFFFFF` | Zakładka Komunikaty, przycisk Odtwórz, pusty ekran |
| Nagrywanie / Zatrzymaj | `#FF5A4D` / `#220300` | `#C62828` / `#FFFFFF` | Mikrofon podczas nagrywania, przycisk Zatrzymaj |
| Usuń / błąd | `#FF8076` | `#B3261E` | Przycisk Usuń, brak serwera, czerwona kropka, wybrany zły mikrofon |
| Działa | `#4FD98A` | `#16703A` | Zielona kropka paska stanu i ikona „OK” |
| Fokus | `#FFD84D` | `#0A2A8A` | Obwódka 3 px elementu wybranego klawiaturą |
| Wyłączone | `#2A3037` / `#8E99A5` | `#DDE2E7` / `#5D6975` | Nieaktywne przyciski (tło / tekst) |
| Wskaźnik poziomu — wyłączone | `#2E3640` | `#D3D9DF` | Niezapalone segmenty wskaźnika i paska postępu |
| Baner ostrzeżenia | `#3A2A0A` / `#FFB84D` / `#FFE6BC` | `#FFF1D6` / `#A35700` / `#4A2A00` | Cisza w nagraniu (tło / ramka+ikona / tekst) |
| Baner błędu | `#3B1512` / `#FF8076` / `#FFDAD5` | `#FDE3E0` / `#B3261E` / `#5C0F0A` | Brak mikrofonu |
| Baner informacji | `#14283D` / `#6CB2FF` / `#D6E9FF` | `#E3EEFC` / `#0B57D0` / `#0A2A5C` | „Zapisane komunikaty nadal działają” |

## Kontrast (WCAG)
| Para | Ciemny | Jasny | Wymóg |
|---|---|---|---|
| Tekst na tle | 17:1 | 17.2:1 | ≥ 4.5:1 |
| Tekst pomocniczy na tle | 11.4:1 | 9.2:1 | ≥ 4.5:1 |
| Tekst słaby na tle | 7.9:1 | 5.8:1 | ≥ 4.5:1 |
| Ramka na tle (element interfejsu) | 3.2:1 | 4.2:1 | ≥ 3:1 |
| Tekst na niebieskim | 8.1:1 | 6.4:1 | ≥ 4.5:1 |
| Tekst na bursztynie | 10:1 | 5.4:1 | ≥ 4.5:1 |
| Tekst na fiolecie | 8.1:1 | 7.7:1 | ≥ 4.5:1 |
| Tekst na czerwieni | 6.3:1 | 5.6:1 | ≥ 4.5:1 |
| Usuń / błąd na karcie | 6.8:1 | 6.5:1 | ≥ 4.5:1 |
| Fokus na tle | 13.4:1 | 11.2:1 | ≥ 3:1 |

## Zasady układu
- Trzy zakładki u góry (Powiedz · Słuchaj · Komunikaty), pasek stanu na dole zawsze widoczny.
- Najczęstsze czynności największe: Powiedz 84 px, mikrofon 132–150 px, Odtwórz 56 px.
- Karty komunikatów: 1 kolumna do ~900 px, 3 kolumny szerzej; zawartość w kolumnie max 1160 px.
- Brak serwera mowy: czerwony pasek stanu + kropka przy zakładkach Powiedz i Słuchaj. Komunikaty zostają w pełnych kolorach.
- Kolor = orientacja (niebieski mówię, bursztyn słucham, fiolet komunikaty), ale zawsze towarzyszy mu ikona i podpis.
- Motyw domyślnie jak system (jasny/ciemny).

## Klawiatura
- Enter w polu Powiedz = Powiedz (Shift+Enter = nowa linia). Podczas mówienia ten sam przycisk to Zatrzymaj.
- Spacja na przycisku mikrofonu = Słuchaj / Stop.
- Fokus: obwódka 3 px w kolorze „Fokus”. W QSS zmiana ramki 2→3 px przesuwa układ — zmniejsz `padding` o 1 px w `:focus`.

## Wzorzec QSS (ciemny)
```
QWidget { font-family: "Atkinson Hyperlegible"; font-size: 20px; color: #F4F6F8; background: #111418; }
QPushButton { background: #1A1F25; color: #F4F6F8; border: 2px solid #5B6773; border-radius: 12px; min-height: 56px; padding: 0 18px; font-weight: 700; }
QPushButton:focus { border: 3px solid #FFD84D; padding: 0 17px; }
QPushButton:pressed { background: #252C34; }
QPushButton:disabled { background: #2A3037; color: #8E99A5; border-color: #2A3037; }
QPushButton#say { background: #6CB2FF; color: #06182B; border-color: #6CB2FF; border-radius: 16px; min-height: 80px; font-size: 30px; }
QPushButton#say[speaking="true"] { background: #FF5A4D; color: #220300; border-color: #FF5A4D; }
QPushButton#play { background: #B9A2FF; color: #1B0B4A; border-color: #B9A2FF; }
QPushButton#delete { color: #FF8076; border-color: #FF8076; }
QTextEdit, QLineEdit, QComboBox { background: #0C0F12; border: 2px solid #5B6773; border-radius: 12px; padding: 12px 16px; }
QTextEdit:focus, QLineEdit:focus { border: 3px solid #6CB2FF; padding: 11px 15px; }
```

## Ikony
`icons/*.svg` — 24×24, obrys 2,25 px, kolor #000000. Koloruj w kodzie (QPainter + `CompositionMode_SourceIn`) kolorem tekstu przycisku. Wymagane: say, mic, speaker, play, stop, save, delete, copy, settings, search, ok, error. Pomocnicze: back, next, chevron, plus, list.

## Ikona aplikacji
`app/komunikator.svg` oraz `app/komunikator-{256,128,64,48,32,16}.png`.

## Zasobnik
`tray/tray-{idle,speaking,recording,error}-{22,16}-for-{dark,light}-panel.png` i `.svg`. Rozróżnienie stanów kształtem (dymek / dymek z falą / mikrofon z kropką / wykrzyknik), nie samym kolorem. „for-dark-panel” = jasny znak na ciemnym panelu.
