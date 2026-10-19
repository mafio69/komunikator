// Motyw: paleta z palette.json, arkusz QSS z szablonu i ikony barwione na kolor z palety.
// Kolory w kodzie są tylko kluczami palety ("blue", "onAmber"...) - wartości są wyłącznie w palette.json.
#pragma once
#include <QColor>
#include <QIcon>
#include <QMap>
#include <QString>

namespace motyw {

void ustaw(bool ciemny);   // wczytuje paletę i arkusz dla całej aplikacji (przełączenie = jedno wywołanie)
bool ciemny();
bool systemowyCiemny();    // KOMUNIKATOR_MOTYW=jasny|ciemny, ustawienia GNOME albo paleta systemu
QColor kolor(const QString& klucz);
QPixmap pixmapa(const QString& ikona, const QString& kluczKoloru, int px);
QIcon ikona(const QString& ikona, const QString& kluczKoloru, int px);
QIcon ikonaZasobnika(const QString& stan);  // idle, speaking, recording, error

// Ponowne zastosowanie stylu po zmianie właściwości (np. mowi=true) - Qt sam tego nie robi.
void odswiez(class QWidget* w);

}  // namespace motyw
