#include "wav.hpp"

#include <cstdint>
#include <stdexcept>

#include "bledy.hpp"

namespace {

void put16(std::string& w, uint16_t v) {
    w += static_cast<char>(v & 0xFF);
    w += static_cast<char>(v >> 8);
}

void put32(std::string& w, uint32_t v) {
    for (int k = 0; k < 4; ++k) w += static_cast<char>((v >> (8 * k)) & 0xFF);
}

[[noreturn]] void zly(const std::string& szczegoly) {
    throw Blad(Rodzaj::ZlyDzwiek,
               "To nie jest nagranie, które umiem odczytać. Potrzebny jest plik WAV bez kompresji (PCM).",
               "WAV: " + szczegoly);
}

uint32_t get(const std::string& b, size_t poz, int ile) {
    uint32_t v = 0;
    for (int k = ile - 1; k >= 0; --k) v = (v << 8) | static_cast<unsigned char>(b[poz + k]);
    return v;
}

}  // namespace

std::string wav_bajty(const Audio& a) {
    uint32_t dane = static_cast<uint32_t>(a.pcm.size());
    std::string w;
    w.reserve(44 + a.pcm.size());
    w += "RIFF";
    put32(w, 36 + dane);
    w += "WAVEfmt ";
    put32(w, 16);
    put16(w, 1);  // PCM
    put16(w, static_cast<uint16_t>(a.channels));
    put32(w, static_cast<uint32_t>(a.rate));
    put32(w, static_cast<uint32_t>(a.rate * a.width * a.channels));
    put16(w, static_cast<uint16_t>(a.width * a.channels));
    put16(w, static_cast<uint16_t>(a.width * 8));
    w += "data";
    put32(w, dane);
    w += a.pcm;
    return w;
}

Audio wav_z_bajtow(const std::string& b) {
    if (b.size() < 12 || b.compare(0, 4, "RIFF") != 0 || b.compare(8, 4, "WAVE") != 0) zly("brak nagłówka RIFF/WAVE");
    Audio a;
    bool jest_fmt = false;
    size_t poz = 12;
    while (poz + 8 <= b.size()) {
        std::string id = b.substr(poz, 4);
        uint32_t dl = get(b, poz + 4, 4);
        size_t tresc = poz + 8;
        if (id == "fmt ") {
            if (tresc + 16 > b.size()) zly("ucięty fragment fmt");
            uint32_t format = get(b, tresc, 2);
            if (format != 1 && format != 0xFFFE) zly("format " + std::to_string(format) + " zamiast PCM (1)");
            // WAVE_FORMAT_EXTENSIBLE: prawdziwy format siedzi w podformacie (pierwsze 2 bajty GUID-u).
            if (format == 0xFFFE && (dl < 40 || tresc + 40 > b.size() || get(b, tresc + 24, 2) != 1))
                zly("format rozszerzony bez podformatu PCM");
            a.channels = static_cast<int>(get(b, tresc + 2, 2));
            a.rate = static_cast<int>(get(b, tresc + 4, 4));
            a.width = static_cast<int>(get(b, tresc + 14, 2)) / 8;
            if (a.channels < 1 || a.rate < 1 || a.width < 1) zly("niemożliwe parametry: kanały/częstotliwość/bity");
            jest_fmt = true;
        } else if (id == "data") {
            if (!jest_fmt) zly("fragment data przed fmt");
            // arecord piszący do potoku nie zna długości i wpisuje 0 albo maksimum - wtedy bierzemy resztę pliku
            size_t dostepne = b.size() - tresc;
            size_t ile = (dl == 0 || dl > dostepne) ? dostepne : dl;
            long long ramka = static_cast<long long>(a.width) * a.channels;
            a.pcm = b.substr(tresc, ile - ile % ramka);
            return a;
        }
        poz = tresc + dl + (dl & 1);  // fragmenty są wyrównane do 2 bajtów
        if (poz < tresc) break;      // przepełnienie przy uszkodzonym pliku
    }
    zly("brak fragmentu data");
}
