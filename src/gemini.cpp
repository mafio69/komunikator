#include "gemini.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <stdexcept>

#include "bledy.hpp"
#include "jezyk.hpp"
#include "json.hpp"
#include "proces.hpp"

namespace gemini {

namespace {

std::string base64(const std::string& dane) {
    static const char* znaki = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string w;
    w.reserve((dane.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < dane.size(); i += 3) {
        unsigned v = (unsigned char)dane[i] << 16 | (unsigned char)dane[i + 1] << 8 | (unsigned char)dane[i + 2];
        w += znaki[v >> 18];
        w += znaki[(v >> 12) & 63];
        w += znaki[(v >> 6) & 63];
        w += znaki[v & 63];
    }
    if (i < dane.size()) {
        unsigned v = (unsigned char)dane[i] << 16;
        if (i + 1 < dane.size()) v |= (unsigned char)dane[i + 1] << 8;
        w += znaki[v >> 18];
        w += znaki[(v >> 12) & 63];
        w += i + 1 < dane.size() ? znaki[(v >> 6) & 63] : '=';
        w += '=';
    }
    return w;
}

// Plik tylko dla właściciela, usuwany przy wyjściu z zakresu - klucz i nagranie nie zostają na dysku.
struct PlikTymczasowy {
    std::string sciezka;
    PlikTymczasowy(const std::string& katalog, const char* nazwa, const std::string& tresc) {
        sciezka = katalog + "/komunikator-" + nazwa + "-" + std::to_string(getpid());
        mode_t stara = umask(077);
        std::ofstream f(sciezka, std::ios::binary | std::ios::trunc);
        umask(stara);
        f << tresc;
        if (!f)
            throw Blad(Rodzaj::System, T("Nie mogę zapisać pliku tymczasowego.", "Cannot write a temporary file."),
                       "plik tymczasowy w " + katalog);
    }
    ~PlikTymczasowy() { std::remove(sciezka.c_str()); }
};

// Wszystkie kłopoty z Gemini wyglądają dla człowieka tak samo - i tak wołający przechodzi na whispera.
[[noreturn]] void niedostepne(const std::string& szczegoly) {
    throw Blad(Rodzaj::Chmura, T("Rozpoznawanie przez Gemini jest teraz niedostępne.", "Recognition through Gemini is unavailable right now."),
               "Gemini: " + szczegoly);
}

}  // namespace

std::string rozpoznaj(const Audio& audio, const std::string& klucz, const std::string& model,
                      const std::string& jezyk, const std::string& katalog_tymczasowy) {
    if (klucz.empty()) niedostepne("brak klucza (komunikator klucz gemini)");

    std::string polecenie =
        "Przepisz dokładnie słowa wypowiedziane w tym nagraniu" +
        std::string(jezyk == "pl" || jezyk.empty() ? " (język polski)" : " (język: " + jezyk + ")") +
        ". Zwróć wyłącznie przepisany tekst, z interpunkcją, bez komentarzy i bez cudzysłowów. "
        "Jeśli w nagraniu nie ma mowy, zwróć pusty tekst.";
    std::string zadanie = "{\"contents\":[{\"parts\":[{\"text\":" + json::cytuj(polecenie) +
                          "},{\"inline_data\":{\"mime_type\":\"audio/wav\",\"data\":\"" + base64(wav_bajty(audio)) +
                          "\"}}]}],\"generationConfig\":{\"temperature\":0}}";

    // Klucz w nagłówku z pliku (curl -H @plik): nie pojawia się w wierszu poleceń ani na liście procesów.
    PlikTymczasowy naglowek(katalog_tymczasowy, "gemini-naglowek", "x-goog-api-key: " + klucz + "\n");
    PlikTymczasowy tresc(katalog_tymczasowy, "gemini-zadanie", zadanie);
    std::string adres = "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent";

    bool ok = false;
    std::string odp = wyjscie_programu({"curl", "-sS", "--max-time", "60", "-H", "@" + naglowek.sciezka, "-H",
                                        "Content-Type: application/json", "--data-binary", "@" + tresc.sciezka,
                                        adres},
                                       ok);
    if (odp.empty()) niedostepne(ok ? "pusta odpowiedź" : "curl bez odpowiedzi (brak internetu albo programu curl)");

    json::Value v;
    try {
        v = json::parsuj(odp);
    } catch (const std::exception& e) {
        niedostepne(std::string("niezrozumiała odpowiedź: ") + e.what());
    }
    if (v["error"].jest_obiektem()) niedostepne("błąd usługi: " + v["error"]["message"].tekst("bez opisu"));
    const json::Value& czesci = v["candidates"].tab.empty() ? json::Value() : v["candidates"].tab[0]["content"]["parts"];
    std::string tekst;
    for (const auto& c : czesci.tab) tekst += c["text"].tekst();
    if (!czesci.jest_tablica()) niedostepne("odpowiedź bez tekstu");
    return tekst;
}

}  // namespace gemini
