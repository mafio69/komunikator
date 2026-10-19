// komunikator-okno: okno dla programu "komunikator". Wygląd: design/komunikator-design/komunikator/SPEC.md.
#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QImageReader>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <unistd.h>

#include "motyw.hpp"
#include "okno.hpp"

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    QApplication::setApplicationName("komunikator-okno");
    QApplication::setApplicationDisplayName("Komunikator");
    QApplication::setDesktopFileName("komunikator-okno");
    QApplication::setQuitOnLastWindowClosed(false);  // praca w tle z ikoną w zasobniku

    // Font dołączony do programu (SIL OFL) - nie zależy od tego, co jest zainstalowane w systemie
    QFontDatabase::addApplicationFont(":/fonty/AtkinsonHyperlegible-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonty/AtkinsonHyperlegible-Bold.ttf");
    QFont font("Atkinson Hyperlegible");
    font.setPixelSize(20);
    QApplication::setFont(font);

    motyw::ustaw(motyw::systemowyCiemny());

    // Jedna kopia: ponowne uruchomienie (np. z menu, gdy okno schowane w zasobniku) pokazuje istniejące okno.
    const QString gniazdo = QString("komunikator-okno-%1").arg(getuid());
    {
        QLocalSocket inna;
        inna.connectToServer(gniazdo);
        if (inna.waitForConnected(300)) {
            inna.write("pokaz\n");
            inna.waitForBytesWritten(300);
            return 0;
        }
    }
    QLocalServer::removeServer(gniazdo);  // pozostałość po kopii, która nie posprzątała
    QLocalServer serwer;
    serwer.setSocketOptions(QLocalServer::UserAccessOption);
    serwer.listen(gniazdo);

    Okno okno;
    okno.show();
    QObject::connect(&serwer, &QLocalServer::newConnection, &okno, [&] {
        while (QLocalSocket* s = serwer.nextPendingConnection()) s->deleteLater();
        okno.showNormal();
        okno.raise();
        okno.activateWindow();
    });

    // Ikony są w SVG - bez wtyczki SVG Qt (pakiet libqt5svg5) przyciski byłyby puste
    if (!QImageReader::supportedImageFormats().contains("svg"))
        QMessageBox::warning(&okno, "Komunikator",
                             "Brak obsługi ikon SVG. Zainstaluj pakiet libqt5svg5 i uruchom program ponownie.");
    return app.exec();
}
