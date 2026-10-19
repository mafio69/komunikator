// Okno komunikatora: zakładki Powiedz / Słuchaj / Komunikaty, ustawienia, kreator pierwszego uruchomienia,
// pasek stanu i ikona w zasobniku. Całą pracę wykonuje silnik (silnik.hpp) - okno tylko pokazuje i zleca.
#pragma once
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolButton>

#include "silnik.hpp"
#include "widzety.hpp"

class Okno : public QMainWindow {
    Q_OBJECT
public:
    Okno();
    ~Okno() override;

protected:
    void resizeEvent(QResizeEvent* e) override;
    void closeEvent(QCloseEvent* e) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    enum Ekran { KREATOR, GLOWNY, USTAWIENIA };
    enum Strona { POWIEDZ, SLUCHAJ, KOMUNIKATY };
    enum class Sluchanie { Gotowe, Nagrywa, Pracuje };
    enum class ZrodloMowy { Brak, Powiedz, Komunikat, Test };

    // budowa
    QWidget* zbudujGlowny();
    QWidget* zbudujPowiedz();
    QWidget* zbudujSluchaj();
    QWidget* zbudujKomunikaty();
    QWidget* zbudujUstawienia();
    QWidget* zbudujKreator();
    QWidget* zbudujPasekStanu();
    QWidget* zbudujKarte(const Komunikat& k);
    static QWidget* kolumna(QWidget* tresc, int margines = 16);
    QPushButton* przycisk(const QString& tekst, const QString& ikona, const char* kolor = nullptr);

    // działanie
    void pokazStrone(Strona s);
    void pokazEkran(Ekran e);
    void odswiezIkony();
    void odswiezKomunikaty();
    void ulozKarty();
    void odswiezUrzadzenia(bool zListami = true);
    void wypelnij(QComboBox* lista, const QVector<Urzadzenie>& urzadzenia, bool mikrofony, bool zPustym);
    void odswiezPrzyciskiOdtwarzania();
    void ustawStanSerwera(bool dziala);
    void mowLubZatrzymaj();
    void zapiszJakoKomunikat();
    void odtworzKomunikat(const QString& nazwa);
    void usunKomunikat(const QString& nazwa);
    void sluchajLubZatrzymaj();
    void ustawSluchanie(Sluchanie s);
    void testMikrofonu(QComboBox* lista, QPushButton* przycisk, PoziomDzwieku* poziom);
    void testGlosnika(QComboBox* lista);
    void zatrzymajWszystko();
    void zmienAdresSerwera();
    void wybierzRozpoznawanie(bool gemini);
    void odswiezRozpoznawanie();
    void ustawZasobnik(const QString& stan);

    Silnik silnik_;
    QStackedWidget* ekrany_;
    QStackedWidget* strony_;
    QToolButton* zakladki_[3];
    QLabel* znaczki_[2];  // czerwone "!" przy Powiedz i Słuchaj, gdy nie ma serwera

    // Powiedz
    PoleTekstu* poleMowy_;
    QPushButton* przyciskPowiedz_;
    QLabel* ikonaPowiedz_;
    QLabel* tekstPowiedz_;
    QPushButton* przyciskZapisz_;
    Baner* banerPowiedz_;

    // Słuchaj
    QPushButton* segmenty_[3];
    PrzyciskMikrofonu* mikrofon_;
    QStackedWidget* podMikrofonem_;
    QLabel* podpisSluchaj_;
    QLabel* klawiszSluchaj_;
    QLabel* licznik_;
    PoziomDzwieku* poziomNagrania_;
    QFrame* panelPracy_;
    PasekPracy* pasekPracy_;
    Baner* banerSluchaj_;
    QWidget* przyciskiCiszy_;
    QPushButton* wybierzMikrofon_;
    QPlainTextEdit* rozpoznany_;
    QPushButton* kopiuj_;
    QWidget* wynik_;
    Sluchanie sluchanie_ = Sluchanie::Gotowe;
    int pozostalo_ = 0;
    QTimer odliczanie_;
    QString plikNagrania_;

    // Komunikaty
    QLineEdit* szukaj_;
    Baner* banerKomunikaty_;
    QStackedWidget* listaLubPusto_;
    QWidget* karty_;
    QGridLayout* siatka_;
    QLabel* brakWynikow_;
    QVector<Komunikat> komunikaty_;
    int kolumny_ = 1;
    QString odtwarzany_;

    // Ustawienia
    QComboBox* ustMikrofon_;
    QComboBox* ustGlosnik_;
    QPushButton* ustTestMikrofonu_;
    PoziomDzwieku* ustPoziom_;
    QPushButton* ustTestGlosnika_;
    QToolButton* zaawansowane_;
    QWidget* zaawansowaneTresc_;
    QLineEdit* adres_;
    QPushButton* rozGemini_;
    QPushButton* rozLokalnie_;
    QWidget* kluczTresc_;
    QLabel* stanKlucza_;
    QLineEdit* klucz_;
    QPushButton* zapiszKlucz_;
    QPushButton* usunKlucz_;
    Ekran powrot_ = GLOWNY;

    // Kreator
    QStackedWidget* krokiKreatora_;
    QComboBox* kreMikrofon_;
    QComboBox* kreGlosnik_;
    QPushButton* kreTestMikrofonu_;
    PoziomDzwieku* krePoziom_;
    QPushButton* kreDalej_;
    QPushButton* kreGotowe_;

    // Pasek stanu
    QFrame* pasekStanu_;
    QPushButton* przyciskUrzadzen_;
    Kropka* kropka_;
    QLabel* stanOpis_;
    QLabel* nazwaMikrofonu_;
    QLabel* nazwaGlosnika_;
    QLabel* ikonaMikrofonu_;
    QLabel* ikonaGlosnika_;
    QLabel* ikonaUstawien_;
    bool serwerDziala_ = true;
    bool banerUrzadzenia_ = false;  // baner na Słuchaj pochodzi od braku mikrofonu (znika sam po wyborze)

    // Nagrywanie testowe (ustawienia / kreator)
    QPushButton* testowanyPrzycisk_ = nullptr;
    PoziomDzwieku* aktywnyPoziom_ = nullptr;
    ZrodloMowy zrodloMowy_ = ZrodloMowy::Brak;

    QSystemTrayIcon* zasobnik_ = nullptr;
    bool podpowiedzZasobnika_ = false;
    QTimer sprawdzanie_;
};
