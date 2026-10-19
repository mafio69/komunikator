// Zrzuty ekranów okna bez monitora (QT_QPA_PLATFORM=offscreen) - do porównania z projektem.
// Klika w okno tak jak człowiek (po napisach przycisków), niczego nie mówi na głos.
//   zrzuty KATALOG_WYJŚCIA SCENARIUSZ   (scenariusze: kreator, glowne, rozpoznawanie, brak-serwera, nagrywanie, niedostepny, zamkniecie)
#include <QAbstractButton>
#include <QApplication>
#include <cstdlib>
#include <QDir>
#include <QEventLoop>
#include <QFontDatabase>
#include <QTimer>
#include <QToolButton>

#include "../motyw.hpp"
#include "../okno.hpp"

static void czekaj(int ms) {
    QEventLoop p;
    QTimer::singleShot(ms, &p, &QEventLoop::quit);
    p.exec();
}

static void kliknij(QWidget* okno, const QString& napis) {
    czekaj(300);  // nowe widżety (np. karty) stają się widoczne dopiero w pętli zdarzeń
    for (auto* b : okno->findChildren<QAbstractButton*>())
        if (b->text() == napis && b->isVisible()) {
            b->click();
            return;
        }
    for (auto* b : okno->findChildren<QAbstractButton*>())
        if (b->accessibleName().startsWith(napis) && b->isVisible()) {
            b->click();
            return;
        }
    qWarning("nie znalazłem przycisku: %s", qPrintable(napis));
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc < 3) return 2;
    QDir wyj(argv[1]);
    QString scen = argv[2];
    QFontDatabase::addApplicationFont(":/fonty/AtkinsonHyperlegible-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonty/AtkinsonHyperlegible-Bold.ttf");
    QFont f("Atkinson Hyperlegible");
    f.setPixelSize(20);
    QApplication::setFont(f);

    for (bool ciemny : {true, false}) {
        motyw::ustaw(ciemny);
        QString m = ciemny ? "ciemny" : "jasny";
        auto zrzut = [&](QWidget* w, const QString& nazwa) {
            czekaj(150);
            w->grab().save(wyj.filePath(nazwa + "-" + m + ".png"));
        };
        Okno okno;
        okno.resize(480, 760);
        okno.show();
        czekaj(1500);  // sprawdzenie serwera mowy

        if (scen == "kreator") {
            zrzut(&okno, "kreator-1");
            kliknij(&okno, "Dalej");
            zrzut(&okno, "kreator-2");
        } else if (scen == "glowne") {
            zrzut(&okno, "powiedz");
            kliknij(&okno, "Słuchaj");
            zrzut(&okno, "sluchaj");
            kliknij(&okno, "Komunikaty");
            zrzut(&okno, "komunikaty");
            okno.resize(1280, 720);
            czekaj(300);
            zrzut(&okno, "komunikaty-szerokie");
            okno.resize(480, 760);
            kliknij(&okno, "Mikrofon i głośnik");
            kliknij(&okno, "Zaawansowane");
            zrzut(&okno, "ustawienia");
        } else if (scen == "rozpoznawanie") {
            okno.resize(480, 1400);  // cała strona ustawień, bez przewijania
            kliknij(&okno, "Mikrofon i głośnik");
            kliknij(&okno, "Lokalnie");
            zrzut(&okno, "ustawienia-lokalnie");
            kliknij(&okno, "Gemini");
            zrzut(&okno, "ustawienia-gemini");
        } else if (scen == "brak-serwera") {
            kliknij(&okno, "Komunikaty");
            zrzut(&okno, "brak-serwera");
        } else if (scen == "niedostepny") {
            kliknij(&okno, "Słuchaj");
            zrzut(&okno, "niedostepny");
        } else if (scen == "zamkniecie") {
            kliknij(&okno, "Komunikaty");
            kliknij(&okno, "Odtwórz");
            czekaj(3000);
            if (system("ps -eo args | grep -qx 'sleep 31'") != 0) qWarning("BŁĄD TESTU: odtwarzacz nie wystartował");
            zrzut(&okno, "odtwarzanie");
            if (!ciemny) return 0;  // wyjście z main niszczy okno w trakcie odtwarzania
        } else if (scen == "nagrywanie") {
            kliknij(&okno, "Słuchaj");
            kliknij(&okno, "Nagrywaj 5");
            okno.findChildren<PrzyciskMikrofonu*>().first()->click();
            czekaj(2000);
            zrzut(&okno, "nagrywanie");
            czekaj(4500);  // koniec nagrania (cisza na serwerze bez mikrofonu)
            zrzut(&okno, "po-nagraniu");
        }
    }
    return 0;
}
