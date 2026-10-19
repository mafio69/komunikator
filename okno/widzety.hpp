// Własne kontrolki: to, czego nie da się zrobić samym QSS (koło mikrofonu, segmenty poziomu, pasek "Pracuję…").
// Kolory biorą z palety motywu, nigdy wpisane na sztywno.
#pragma once
#include <QAbstractButton>
#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTimer>
#include <QWidget>

// Wskaźnik poziomu dźwięku: rząd segmentów, zapala się tyle, ile głośno.
class PoziomDzwieku : public QWidget {
    Q_OBJECT
public:
    explicit PoziomDzwieku(const QString& kluczKoloru, QWidget* rodzic = nullptr);
    void ustaw(double wartosc);  // 0..1
    void ustawKolor(const QString& klucz) { kolor_ = klucz; update(); }
    QSize sizeHint() const override { return {320, 22}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString kolor_;
    double wartosc_ = 0;
};

// Pasek "Pracuję…": przesuwający się odcinek, bez znanego końca.
class PasekPracy : public QWidget {
    Q_OBJECT
public:
    explicit PasekPracy(QWidget* rodzic = nullptr);
    void start();
    void stop();
    QSize sizeHint() const override { return {320, 10}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QTimer timer_;
    int krok_ = 0;
};

// Kropka stanu (zielona / czerwona).
class Kropka : public QWidget {
    Q_OBJECT
public:
    explicit Kropka(QWidget* rodzic = nullptr);
    void ustawKolor(const QString& klucz) { kolor_ = klucz; update(); }
    QSize sizeHint() const override { return {12, 12}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString kolor_ = "ok";
};

// Duży okrągły przycisk mikrofonu: spoczynek (bursztyn), nagrywanie (czerwony z pierścieniem), wyłączony.
class PrzyciskMikrofonu : public QAbstractButton {
    Q_OBJECT
public:
    explicit PrzyciskMikrofonu(QWidget* rodzic = nullptr);
    void ustawNagrywanie(bool tak) { nagrywa_ = tak; update(); }
    void ustawSrednice(int px);
    QSize sizeHint() const override { return {srednica_ + 24, srednica_ + 24}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    bool nagrywa_ = false;
    int srednica_ = 132;
};

// Baner: ostrzeżenie / błąd / informacja, z ikoną, tytułem i opisem.
class Baner : public QFrame {
    Q_OBJECT
public:
    enum Rodzaj { Ostrzezenie, Blad, Informacja };
    explicit Baner(QWidget* rodzic = nullptr);
    void pokaz(Rodzaj r, const QString& tytul, const QString& opis = {});
    void odswiezIkone();

private:
    Rodzaj rodzaj_ = Informacja;
    QLabel* ikona_;
    QLabel* tytul_;
    QLabel* opis_;
};

// Pole tekstowe: Enter = wyślij, Shift+Enter = nowa linia.
class PoleTekstu : public QPlainTextEdit {
    Q_OBJECT
public:
    using QPlainTextEdit::QPlainTextEdit;
signals:
    void enter();

protected:
    void keyPressEvent(QKeyEvent* e) override;
};

// Podpowiedź klawisza w ramce, np. "Enter", "spacja".
QLabel* klawisz(const QString& tekst, QWidget* rodzic = nullptr);
// Etykieta z rolą typograficzną z arkusza QSS (tytul, naglowek, tresc, maly...).
QLabel* etykieta(const QString& tekst, const char* rola, QWidget* rodzic = nullptr);
