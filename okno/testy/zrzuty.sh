#!/usr/bin/env bash
# Zrzuty wszystkich ekranów w obu motywach, bez monitora i bez dotykania Twoich ustawień.
# Nic nie mówi na głos. Wynik: build/zrzuty/*.png
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
WYJ="$PWD/build/zrzuty"; rm -rf "$WYJ"; mkdir -p "$WYJ"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
export QT_QPA_PLATFORM=offscreen KOMUNIKATOR_SILNIK="$PWD/../komunikator" KOMUNIKATOR_DANE="$TMP/dane"
K="$KOMUNIKATOR_SILNIK"

# 1. pierwsze uruchomienie: nic nie wybrane
XDG_CONFIG_HOME="$TMP/pusty" ./build/robzrzuty "$WYJ" kreator

# 2. urządzenia wybrane, kilka komunikatów
export XDG_CONFIG_HOME="$TMP/konfig"
"$K" mikrofon 1 2>/dev/null; "$K" glosnik 2 2>/dev/null
"$K" zapisz "Wyprowadź psa" "Proszę, wyprowadź psa na spacer." 2>/dev/null
"$K" zapisz "Woda" "Poproszę szklankę wody." 2>/dev/null
"$K" zapisz "Boli mnie głowa" "Boli mnie głowa. Poproszę tabletkę." 2>/dev/null
"$K" zapisz "Dziękuję" "Dziękuję, to bardzo miłe." 2>/dev/null
"$K" zapisz "Zadzwoń do Ani" "Zadzwoń, proszę, do Ani i powiedz, że jestem w domu." 2>/dev/null
"$K" zapisz "Chcę odpocząć" "Jestem zmęczony. Chcę chwilę odpocząć." 2>/dev/null
./build/robzrzuty "$WYJ" glowne
./build/robzrzuty "$WYJ" nagrywanie
./build/robzrzuty "$WYJ" rozpoznawanie
KOMUNIKATOR_TTS=127.0.0.1:1 KOMUNIKATOR_STT=127.0.0.1:1 ./build/robzrzuty "$WYJ" brak-serwera
# 3. wybrany mikrofon odłączony (nie ma takiego urządzenia)
KOMUNIKATOR_MIKROFON="pipewire:NODE=odlaczone_sluchawki" ./build/robzrzuty "$WYJ" niedostepny

# 4. zamknięcie programu w trakcie odtwarzania: odtwarzacz nie może grać dalej sam
printf '#!/bin/sh\nexec sleep 31\n' > "$TMP/cichy-odtwarzacz"; chmod +x "$TMP/cichy-odtwarzacz"
KOMUNIKATOR_ODTWARZACZ="$TMP/cichy-odtwarzacz" ./build/robzrzuty "$WYJ" zamkniecie
sleep 0.5
if ps -eo args | grep -qx 'sleep 31'; then echo "BŁĄD: odtwarzacz gra dalej po zamknięciu okna"; pkill -xf 'sleep 31'; else echo "OK: zamknięcie okna zatrzymało odtwarzacz"; fi
ls "$WYJ"
