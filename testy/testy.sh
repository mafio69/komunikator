#!/usr/bin/env bash
# Testy komunikatora. Część A działa zawsze (bez usług mowy), część B tylko gdy piper i whisper odpowiadają.
# Nic nie gra na głos: odtwarzacz jest wyłączony, wszystko trafia do katalogu tymczasowego.
#   bash testy/testy.sh      (albo: make test)
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
# KOMUNIKATOR_BIN pozwala sprawdzić inną kompilację tym samym zestawem (np. z sanitizerami: make sprawdz).
K="${KOMUNIKATOR_BIN:-$PWD/komunikator}"
[ -x "$K" ] || { echo "najpierw: make"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
export KOMUNIKATOR_DANE="$TMP/dane" KOMUNIKATOR_ODTWARZACZ="" XDG_CONFIG_HOME="$TMP/konfig"
DOBRZE=0; ZLE=0
ok()   { echo "  ✔ $1"; DOBRZE=$((DOBRZE+1)); }
zle()  { echo "  ✘ $1"; ZLE=$((ZLE+1)); }
kod()  { "$K" "${@:2}" >/dev/null 2>&1; [ $? -eq "$1" ]; }   # kod OCZEKIWANY polecenie...

echo "== A. bez usług mowy =="
kod 0 wersja                          && ok "wersja"                       || zle "wersja"
kod 2                                 && ok "bez polecenia -> kod 2"        || zle "bez polecenia -> kod 2"
kod 2 cośtam                          && ok "nieznane polecenie -> kod 2"   || zle "nieznane polecenie"
kod 2 zapisz "../ucieczka" tekst      && ok "nazwa z / odrzucona"           || zle "nazwa z / przepuszczona"
kod 2 zapisz ".ukryty" tekst          && ok "nazwa z kropką odrzucona"      || zle "nazwa z kropką przepuszczona"
kod 2 usun " ab"                      && ok "nazwa ze spacją na początku odrzucona" || zle "spacja na początku przepuszczona"
kod 2 ustaw tts bez-portu             && ok "ustaw: adres bez portu odrzucony" || zle "ustaw bez portu"
kod 2 ustaw nieznany 1                && ok "ustaw: nieznany klucz odrzucony" || zle "ustaw nieznany klucz"
kod 1 odtworz nieistniejacy           && ok "brak komunikatu -> kod 1"      || zle "brak komunikatu"
kod 1 rozpoznaj "$TMP/brak.wav"       && ok "brak pliku -> kod 1"           || zle "brak pliku"
echo "to nie dźwięk" > "$TMP/zly.wav"
kod 1 rozpoznaj "$TMP/zly.wav"        && ok "zły WAV -> kod 1"              || zle "zły WAV"
KOMUNIKATOR_TTS=127.0.0.1:1 "$K" powiedz test >/dev/null 2>&1; [ $? -eq 1 ] \
                                      && ok "usługa nieosiągalna -> kod 1"  || zle "usługa nieosiągalna"
# Zasada projektu: człowiek widzi zdanie z podpowiedzią, nigdy surowy błąd systemu.
KOMUNIKATOR_TTS=127.0.0.1:1 "$K" powiedz test 2>"$TMP/komunikat" >/dev/null
grep -q "Nie mogę połączyć się z: usługa mowy" "$TMP/komunikat" && ! grep -qiE "refused|connect:|errno" "$TMP/komunikat" \
                                      && ok "brak usługi -> przyjazny komunikat" || zle "brak usługi -> techniczny komunikat: $(cat "$TMP/komunikat")"
[ "$("$K" lista 2>/dev/null)" = "" ]  && ok "pusta lista"                   || zle "pusta lista"
kod 2 sluchaj 1 </dev/null            && ok "bez wybranego mikrofonu -> kod 2" || zle "mikrofon wybrany sam z siebie"
KOMUNIKATOR_ODTWARZACZ="aplay -q" "$K" powiedz test </dev/null >/dev/null 2>&1; [ $? -eq 2 ] \
                                      && ok "bez wybranego głośnika -> kod 2" || zle "głośnik wybrany sam z siebie"
"$K" mikrofony 2>/dev/null | grep -q "^  1  default" && ok "lista mikrofonów, nic nie zaznaczone" || zle "lista mikrofonów"
"$K" mikrofon 1 2>/dev/null && [ "$("$K" mikrofon)" = "default" ] && ok "wybór mikrofonu zapamiętany" || zle "wybór mikrofonu"
kod 2 glosnik 99                      && ok "zły numer głośnika -> kod 2"   || zle "zły numer głośnika"
kod 2 ustaw rozpoznawanie cośtam      && ok "ustaw: złe rozpoznawanie odrzucone" || zle "złe rozpoznawanie przepuszczone"
echo abc | "$K" klucz gemini >/dev/null 2>&1; [ $? -eq 2 ] \
                                      && ok "klucz Gemini: śmieci odrzucone" || zle "klucz Gemini: śmieci przyjęte"
# zmyślony klucz o poprawnym wyglądzie - zostaje w katalogu testu, nigdzie nie jest wysyłany.
# Składany w trakcie testu, żeby skanery sekretów nie brały go za prawdziwy klucz.
KLUCZ_TEST="AIzaTEST$(printf '0%.0s' {1..31})"
echo "$KLUCZ_TEST" | "$K" klucz gemini >/dev/null 2>&1 \
  && [ "$(stat -c %a "$XDG_CONFIG_HOME/komunikator/gemini-klucz")" = 600 ] \
  && "$K" ustawienia 2>/dev/null | grep -q "^gemini_klucz=zapisany$" \
  && ! "$K" ustawienia 2>/dev/null | grep -q "AIzaTEST" \
                                      && ok "klucz Gemini: tylko dla właściciela, niewyświetlany" || zle "zapis klucza Gemini"
"$K" klucz --usun >/dev/null 2>&1 && "$K" ustawienia 2>/dev/null | grep -q "^gemini_klucz=brak$" \
                                      && ok "klucz Gemini usunięty" || zle "usuwanie klucza Gemini"

echo "== B. z usługami mowy =="
if ! "$K" sprawdz >/dev/null 2>&1; then
  echo "  • usługi mowy nie odpowiadają - pomijam (komunikator sprawdz)"
else
  "$K" powiedz -o "$TMP/a.wav" "Kot siedzi na oknie." 2>/dev/null && [ -s "$TMP/a.wav" ] \
    && ok "powiedz -o tworzy WAV" || zle "powiedz -o"
  echo "Pies śpi." | "$K" powiedz -o "$TMP/b.wav" - 2>/dev/null && [ -s "$TMP/b.wav" ] \
    && ok "tekst ze standardowego wejścia" || zle "tekst ze standardowego wejścia"
  T="$("$K" rozpoznaj "$TMP/a.wav" 2>/dev/null)"
  # sprawdzamy długie, wyraźne słowo - krótkie ("kot") whisper czasem myli ("pod"), a to nie błąd programu
  echo "$T" | grep -qi "oknie" && ok "rozpoznaj: \"$T\"" || zle "rozpoznaj: \"$T\""
  "$K" rozpoznaj - < "$TMP/a.wav" 2>/dev/null | grep -qi "oknie" \
    && ok "rozpoznaj ze standardowego wejścia" || zle "rozpoznaj -"
  # Gemini bez klucza: rozpoznaje lokalnie i mówi o tym, zamiast zawieść
  T="$(KOMUNIKATOR_ROZPOZNAWANIE=gemini "$K" rozpoznaj "$TMP/a.wav" 2>"$TMP/uwaga")"
  echo "$T" | grep -qi "oknie" && grep -q "^uwaga: Gemini niedostępne" "$TMP/uwaga" \
    && ok "Gemini bez klucza -> zapas: whisper" || zle "Gemini bez klucza: \"$T\" $(cat "$TMP/uwaga")"
  "$K" zapisz próba "Komunikat próbny." 2>/dev/null && ok "zapisz (polska nazwa)" || zle "zapisz"
  "$K" lista 2>/dev/null | grep -q "^próba	Komunikat próbny.$" && ok "lista pokazuje komunikat" || zle "lista"
  "$K" zapisz "Wyprowadź psa" "Proszę, wyprowadź psa." 2>/dev/null && "$K" lista 2>/dev/null | grep -q "^Wyprowadź psa	" \
    && ok "nazwa ze spacją (jak w oknie)" || zle "nazwa ze spacją"
  "$K" odtworz próba -o "$TMP/c.wav" 2>/dev/null && cmp -s "$TMP/c.wav" "$KOMUNIKATOR_DANE/komunikaty/próba.wav" \
    && ok "odtworz -o kopiuje dźwięk" || zle "odtworz -o"
  KOMUNIKATOR_TTS=127.0.0.1:1 "$K" odtworz próba -o "$TMP/d.wav" 2>/dev/null \
    && ok "zapisany komunikat działa bez usługi mowy" || zle "komunikat bez usługi mowy"
  if command -v pw-dump >/dev/null && pw-dump >/dev/null 2>&1; then
    KOMUNIKATOR_ODTWARZACZ="aplay -q" KOMUNIKATOR_GLOSNIK="pipewire:NODE=odlaczone_sluchawki" \
      "$K" powiedz test >/dev/null 2>&1; [ $? -eq 1 ] \
      && ok "odłączony głośnik PipeWire -> błąd, nie granie na domyślnym" || zle "odłączony głośnik przepuszczony"
  fi
  "$K" usun próba 2>/dev/null && [ ! -e "$KOMUNIKATOR_DANE/komunikaty/próba.txt" ] \
    && ok "usun" || zle "usun"
fi

echo "== wynik: dobrze $DOBRZE, źle $ZLE =="
[ "$ZLE" -eq 0 ]
