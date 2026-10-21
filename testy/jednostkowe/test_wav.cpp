// Zapis i odczyt WAV (src/wav.cpp).
#include <string>

#include "bledy.hpp"
#include "sprawdzian.hpp"
#include "wav.hpp"

namespace {
Audio przyklad() {
    Audio a;
    a.rate = 16000;
    a.width = 2;
    a.channels = 1;
    a.pcm = std::string("\x01\x00\x02\x00\x03\x00\x04\x00", 8);
    return a;
}

bool zly_dzwiek(const std::string& bajty) {
    try {
        wav_z_bajtow(bajty);
    } catch (const Blad& b) {
        return b.rodzaj() == Rodzaj::ZlyDzwiek && std::string(b.what()).find("WAV bez kompresji") != std::string::npos;
    }
    return false;
}
}  // namespace

TEST(wav_zapis_i_odczyt_daja_to_samo) {
    Audio a = przyklad();
    std::string plik = wav_bajty(a);
    SPRAWDZ_ROWNE(size_t{44} + a.pcm.size(), plik.size());
    Audio b = wav_z_bajtow(plik);
    SPRAWDZ_ROWNE(a.rate, b.rate);
    SPRAWDZ_ROWNE(a.width, b.width);
    SPRAWDZ_ROWNE(a.channels, b.channels);
    SPRAWDZ_ROWNE(a.pcm, b.pcm);
}

TEST(wav_dlugosc_w_sekundach) {
    Audio a = przyklad();
    a.pcm.assign(32000, '\0');  // 16000 próbek po 2 bajty = 1 s
    SPRAWDZ_ROWNE(1.0, a.sekundy());
    a.rate = 0;
    SPRAWDZ_ROWNE(0.0, a.sekundy());
}

TEST(wav_dlugosc_zero_bierze_reszte_pliku) {
    // arecord piszący do potoku wpisuje długość 0 - liczy się to, co faktycznie przyszło
    std::string plik = wav_bajty(przyklad());
    plik.replace(40, 4, std::string(4, '\0'));
    SPRAWDZ_ROWNE(size_t{8}, wav_z_bajtow(plik).pcm.size());
}

TEST(wav_odrzuca_zle_pliki_przyjaznym_komunikatem) {
    SPRAWDZ(zly_dzwiek("to nie dźwięk"));
    SPRAWDZ(zly_dzwiek(""));
    std::string plik = wav_bajty(przyklad());
    std::string bez_danych = plik.substr(0, 36);  // RIFF + fmt, bez "data"
    SPRAWDZ(zly_dzwiek(bez_danych));
    std::string skompresowany = plik;
    skompresowany[20] = 2;  // format 2 = ADPCM
    SPRAWDZ(zly_dzwiek(skompresowany));
    std::string rozszerzony_bez_pcm = plik;
    rozszerzony_bez_pcm[20] = '\xFE';
    rozszerzony_bez_pcm[21] = '\xFF';  // 0xFFFE, ale fmt ma tylko 16 bajtów - brak podformatu
    SPRAWDZ(zly_dzwiek(rozszerzony_bez_pcm));
}
