#!/usr/bin/env bash
# Instalator komunikatora (silnik + okno) na Ubuntu / Debianie. Jedno polecenie, bez przepisywania komend:
#
#   bash instaluj.sh                      silnik i okno, zapyta o adres serwera mowy
#   bash instaluj.sh --serwer 1.2.3.4     bez pytania o adres
#   bash instaluj.sh --bez-okna           sam silnik (bez Qt)
#   bash instaluj.sh --odinstaluj         usuwa programy (zapisane komunikaty i ustawienia zostają)
#
# Co robi: doinstalowuje brakujące pakiety (jedyny krok z sudo, zapyta o hasło), buduje od zera,
# instaluje do ~/.local (bez sudo), ustawia adres serwera mowy i sprawdza połączenie.
set -uo pipefail
cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"

SERWER_DOMYSLNY="127.0.0.1"   # usługi mowy na tym samym komputerze; inny adres podasz przy instalacji
PREFIX="${PREFIX:-$HOME/.local}"
SERWER=""
OKNO=1
ODINSTALUJ=0
while [ $# -gt 0 ]; do
  case "$1" in
    --serwer) SERWER="${2:-}"; shift ;;
    --bez-okna) OKNO=0 ;;
    --odinstaluj) ODINSTALUJ=1 ;;
    -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
    *) echo "Nieznana opcja: $1 (pomoc: bash instaluj.sh --help)"; exit 2 ;;
  esac
  shift
done

krok() { printf '\n\033[1m== %s ==\033[0m\n' "$1"; }
ok()   { printf '  \033[32m✔\033[0m %s\n' "$1"; }
blad() { printf '  \033[31m✘\033[0m %s\n' "$1"; exit 1; }

if [ "$ODINSTALUJ" = 1 ]; then
  krok "Odinstalowanie"
  make -s uninstall PREFIX="$PREFIX" && ok "silnik usunięty"
  make -s -C okno uninstall PREFIX="$PREFIX" && ok "okno usunięte"
  echo "  Komunikaty zostały w ~/.local/share/komunikator, ustawienia w ~/.config/komunikator."
  exit 0
fi

krok "1/5 Pakiety"
POTRZEBNE=(g++ make alsa-utils curl)  # curl: rozpoznawanie przez Gemini
[ "$OKNO" = 1 ] && POTRZEBNE+=(qtbase5-dev libqt5svg5)
BRAKUJE=()
for p in "${POTRZEBNE[@]}"; do
  dpkg -s "$p" >/dev/null 2>&1 || BRAKUJE+=("$p")
done
if [ ${#BRAKUJE[@]} -eq 0 ]; then
  ok "wszystko jest: ${POTRZEBNE[*]}"
else
  echo "  Brakuje: ${BRAKUJE[*]} - instaluję (sudo zapyta o hasło)"
  command -v apt-get >/dev/null || blad "to nie jest Ubuntu/Debian - doinstaluj ręcznie: ${BRAKUJE[*]}"
  sudo apt-get install -y "${BRAKUJE[@]}" || blad "instalacja pakietów nie powiodła się"
  ok "doinstalowane: ${BRAKUJE[*]}"
fi

krok "2/5 Budowanie silnika"
make -s clean && make -s >/dev/null || blad "budowanie silnika nie powiodło się (szczegóły: make)"
make -s install PREFIX="$PREFIX" >/dev/null || blad "instalacja silnika nie powiodła się"
ok "silnik: $PREFIX/bin/komunikator ($("$PREFIX/bin/komunikator" wersja))"

if [ "$OKNO" = 1 ]; then
  krok "3/5 Budowanie okna"
  make -s -C okno clean && make -s -C okno -j"$(nproc)" >/dev/null 2>&1 || blad "budowanie okna nie powiodło się (szczegóły: make -C okno)"
  make -s -C okno install PREFIX="$PREFIX" >/dev/null || blad "instalacja okna nie powiodła się"
  command -v update-desktop-database >/dev/null && update-desktop-database "$PREFIX/share/applications" 2>/dev/null
  ok "okno: $PREFIX/bin/komunikator-okno, w menu aplikacji jako „Komunikator”"
else
  krok "3/5 Okno - pominięte (--bez-okna)"
fi

krok "4/5 Serwer mowy"
K="$PREFIX/bin/komunikator"
OBECNY="$("$K" ustawienia | sed -n 's/^tts=//p')"
if [ -z "$SERWER" ]; then
  if [ -t 0 ]; then
    read -r -p "  Adres serwera mowy [${SERWER_DOMYSLNY}]: " SERWER
  fi
  SERWER="${SERWER:-$SERWER_DOMYSLNY}"
fi
SERWER="${SERWER#*://}"; SERWER="${SERWER%%[:/]*}"   # wklejone http://...:port - zostaje sam adres
"$K" ustaw tts "$SERWER:10200" 2>/dev/null && "$K" ustaw stt "$SERWER:10300" 2>/dev/null \
  || blad "nie udało się zapisać adresu serwera"
ok "serwer mowy: $SERWER (było: ${OBECNY:-domyślne})"

krok "5/5 Sprawdzenie"
if "$K" sprawdz; then
  ok "usługi mowy odpowiadają"
else
  echo "  Usługi mowy nie odpowiadają. Program jest zainstalowany - zapisane komunikaty działają."
  echo "  Sprawdź, czy laptop jest w Tailscale / w domowej sieci, i uruchom: komunikator sprawdz"
fi
case ":$PATH:" in
  *":$PREFIX/bin:"*) ;;
  *) echo "  Uwaga: $PREFIX/bin nie jest w PATH - wyloguj się i zaloguj ponownie (Ubuntu dodaje go sam)." ;;
esac

echo
if [ "$OKNO" = 1 ]; then
  echo "Gotowe. Uruchom „Komunikator” z menu aplikacji albo poleceniem: komunikator-okno"
else
  echo "Gotowe. Pomoc: komunikator pomoc"
fi
