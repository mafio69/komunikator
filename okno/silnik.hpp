// Cienka warstwa między oknem a silnikiem (program "komunikator").
// Wszystko, co okno wie o silniku, jest w tej jednej klasie: polecenia, ich wyjście i kody wyjścia.
// Żeby podmienić silnik (np. na bibliotekę albo demona), wystarczy przepisać silnik.cpp - okno zostaje.
#pragma once
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>

struct Komunikat {
    QString nazwa;
    QString tekst;
};

struct Urzadzenie {
    QString nazwa;  // dla silnika, np. plughw:0,0 albo default
    QString opis;   // dla ludzi
    bool wybrany = false;
};

class Silnik : public QObject {
    Q_OBJECT
public:
    explicit Silnik(QObject* rodzic = nullptr);
    ~Silnik() override;

    // Szybkie odczyty i zapisy ustawień (czekają na wynik, trwają ułamek sekundy).
    QVector<Komunikat> lista();
    QVector<Urzadzenie> urzadzenia(bool mikrofony);
    QMap<QString, QString> ustawienia();
    bool wybierz(bool mikrofon, const QString& nazwa, QString* blad = nullptr);
    bool ustaw(const QString& klucz, const QString& wartosc, QString* blad = nullptr);
    bool usun(const QString& nazwa, QString* blad = nullptr);
    bool zapiszKlucz(const QString& klucz, QString* blad = nullptr);  // klucz Gemini idzie wejściem, nie w argumentach
    bool usunKlucz(QString* blad = nullptr);

    // Długie czynności w tle - wynik przychodzi sygnałem.
    void sprawdz();                                                   // -> stanSerwera
    void zapisz(const QString& nazwa, const QString& tekst);          // -> zapisano
    void powiedz(const QString& tekst, const QString& glosnik = {});  // -> mowaKoniec (glosnik: tylko do testu)
    void odtworz(const QString& nazwa);                               // -> mowaKoniec
    void zatrzymajMowe();
    bool mowi() const { return mowa_ != nullptr; }
    void nagraj(int sekundy, const QString& plik, const QString& mikrofon = {});  // -> poziom, nagranieKoniec
    void zatrzymajNagranie();                                         // zapisuje to, co już nagrane
    bool nagrywa() const { return nagranie_ != nullptr; }
    void rozpoznaj(const QString& plik);                              // -> rozpoznano

signals:
    void stanSerwera(bool dziala);
    void zapisano(bool ok, const QString& blad);
    void mowaStart();
    void mowaKoniec(bool ok, bool zatrzymana, const QString& blad);
    void poziom(double wartosc);  // 0..1, co 0,1 s
    void nagranieKoniec(bool ok, bool cisza, const QString& blad);
    void rozpoznano(bool ok, const QString& tekstAlboBlad, const QString& uwaga);  // uwaga: np. zapas na whisper

private:
    QProcess* uruchom(const QStringList& args, const QMap<QString, QString>& srodowisko = {},
                     const QByteArray& wejscie = {});
    bool wykonaj(const QStringList& args, QString* wyjscie, QString* blad, const QByteArray& wejscie = {});
    void mow(const QStringList& args, const QString& glosnik);
    static QString bladZ(QProcess* p);

    QString program_;
    QProcess* mowa_ = nullptr;
    QProcess* nagranie_ = nullptr;
    QProcess* sprawdzanie_ = nullptr;
    bool mowaZatrzymana_ = false;
    bool cisza_ = false;
    QByteArray bufor_;
};
