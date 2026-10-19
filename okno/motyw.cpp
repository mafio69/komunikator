#include "motyw.hpp"

#include <QApplication>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPalette>
#include <QProcess>
#include <QStyle>
#include <QWidget>
#include <algorithm>

namespace motyw {

namespace {
bool g_ciemny = true;
QMap<QString, QString> g_paleta;
}  // namespace

bool ciemny() { return g_ciemny; }

bool systemowyCiemny() {
    QString wymuszony = qEnvironmentVariable("KOMUNIKATOR_MOTYW");
    if (wymuszony == "jasny") return false;
    if (wymuszony == "ciemny") return true;
    QProcess p;
    p.start("gsettings", {"get", "org.gnome.desktop.interface", "color-scheme"});
    if (p.waitForFinished(1500) && p.exitCode() == 0) {
        QString w = QString::fromUtf8(p.readAllStandardOutput());
        if (w.contains("dark")) return true;
        if (w.contains("light")) return false;
    }
    // Brak ustawienia GNOME: zgadujemy po jasności tła okien systemu
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

void ustaw(bool ciemnyMotyw) {
    g_ciemny = ciemnyMotyw;
    QFile pf(":/palette.json");
    pf.open(QIODevice::ReadOnly);
    QJsonObject wszystko = QJsonDocument::fromJson(pf.readAll()).object();
    QJsonObject p = wszystko[ciemnyMotyw ? "dark" : "light"].toObject();
    g_paleta.clear();
    for (auto it = p.begin(); it != p.end(); ++it) g_paleta[it.key()] = it.value().toString();

    QFile sf(":/styl.qss");
    sf.open(QIODevice::ReadOnly);
    QString qss = QString::fromUtf8(sf.readAll());
    // Najpierw dłuższe klucze, żeby @title nie zjadło początku @titleText
    QStringList klucze = g_paleta.keys();
    std::sort(klucze.begin(), klucze.end(), [](const QString& a, const QString& b) { return a.size() > b.size(); });
    for (const QString& k : klucze) qss.replace("@" + k, g_paleta[k]);
    qApp->setStyleSheet(qss);
    for (QWidget* w : QApplication::allWidgets()) w->update();  // widgety rysowane ręcznie biorą nowe kolory
}

QColor kolor(const QString& klucz) {
    return QColor(g_paleta.value(klucz, "#FF00FF"));  // jaskrawy róż = brakujący klucz, od razu widać
}

QPixmap pixmapa(const QString& nazwa, const QString& kluczKoloru, int px) {
    qreal dpr = qApp->devicePixelRatio();
    QImageReader r(":/icons/" + nazwa + ".svg");
    r.setScaledSize(QSize(qRound(px * dpr), qRound(px * dpr)));
    QImage obraz = r.read();
    if (obraz.isNull()) return QPixmap();
    obraz = obraz.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    {
        // SVG są czarne - zostawiamy kształt, podmieniamy kolor
        QPainter p(&obraz);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(obraz.rect(), kolor(kluczKoloru));
    }
    QPixmap wynik = QPixmap::fromImage(obraz);
    wynik.setDevicePixelRatio(dpr);
    return wynik;
}

QIcon ikona(const QString& nazwa, const QString& kluczKoloru, int px) {
    return QIcon(pixmapa(nazwa, kluczKoloru, px));
}

QIcon ikonaZasobnika(const QString& stan) {
    // Wariant dla ciemnego panelu, gdy nie da się wykryć jasności panelu - zgodnie z projektem
    return QIcon(":/tray/tray-" + stan + "-for-dark-panel.svg");
}

void odswiez(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
    for (QWidget* d : w->findChildren<QWidget*>()) {
        d->style()->unpolish(d);
        d->style()->polish(d);
    }
    w->update();
}

}  // namespace motyw
