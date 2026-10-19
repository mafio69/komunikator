// Dźwięk w pamięci (surowe PCM) i jego zapis/odczyt jako plik WAV.
#pragma once
#include <string>

struct Audio {
    int rate = 22050;   // próbek na sekundę
    int width = 2;      // bajtów na próbkę (2 = 16 bitów)
    int channels = 1;   // kanały
    std::string pcm;    // surowe próbki, little-endian

    double sekundy() const {
        long long ramka = static_cast<long long>(width) * channels;
        return ramka > 0 && rate > 0 ? static_cast<double>(pcm.size()) / ramka / rate : 0;
    }
};

std::string wav_bajty(const Audio& a);           // gotowy plik WAV w pamięci
Audio wav_z_bajtow(const std::string& bajty);    // rzuca std::runtime_error, gdy to nie jest WAV PCM
