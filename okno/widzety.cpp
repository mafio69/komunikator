#include "widzety.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "motyw.hpp"

// ---------------------------------------------------------------- PoziomDzwieku

PoziomDzwieku::PoziomDzwieku(const QString& kluczKoloru, QWidget* rodzic) : QWidget(rodzic), kolor_(kluczKoloru) {
    setFixedHeight(22);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName("Poziom dźwięku");
}

void PoziomDzwieku::ustaw(double wartosc) {
    wartosc_ = std::clamp(wartosc, 0.0, 1.0);
    update();
}

void PoziomDzwieku::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int odstep = 4;
    const int ile = std::max(8, (width() + odstep) / (14 + odstep));  // segment ok. 14 px
    const double szer = (width() - odstep * (ile - 1)) / double(ile);
    const int zapalone = int(std::round(wartosc_ * ile));
    for (int i = 0; i < ile; ++i) {
        QRectF r(i * (szer + odstep), 0, szer, height());
        p.setPen(Qt::NoPen);
        p.setBrush(motyw::kolor(i < zapalone ? kolor_ : "meterOff"));
        p.drawRoundedRect(r, 3, 3);
    }
}

// ---------------------------------------------------------------- PasekPracy

PasekPracy::PasekPracy(QWidget* rodzic) : QWidget(rodzic) {
    setFixedHeight(10);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    timer_.setInterval(30);
    connect(&timer_, &QTimer::timeout, this, [this] {
        krok_ = (krok_ + 1) % 100;
        update();
    });
}

void PasekPracy::start() {
    krok_ = 0;
    timer_.start();
}

void PasekPracy::stop() { timer_.stop(); }

void PasekPracy::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(motyw::kolor("meterOff"));
    p.drawRoundedRect(rect(), 5, 5);
    // odcinek 30% szerokości jeździ od lewej do prawej
    double dl = width() * 0.3;
    double x = (width() + dl) * krok_ / 100.0 - dl;
    double lewo = std::max(0.0, x), prawo = std::min(double(width()), x + dl);
    QRectF r(lewo, 0, prawo - lewo, height());
    if (r.width() > 0) {
        p.setBrush(motyw::kolor("amber"));
        p.drawRoundedRect(r, 5, 5);
    }
}

// ---------------------------------------------------------------- Kropka

Kropka::Kropka(QWidget* rodzic) : QWidget(rodzic) { setFixedSize(12, 12); }

void Kropka::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(motyw::kolor(kolor_));
    p.drawEllipse(rect());
}

// ---------------------------------------------------------------- PrzyciskMikrofonu

PrzyciskMikrofonu::PrzyciskMikrofonu(QWidget* rodzic) : QAbstractButton(rodzic) {
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName("Słuchaj");
    ustawSrednice(132);
}

void PrzyciskMikrofonu::ustawSrednice(int px) {
    srednica_ = px;
    setFixedSize(px + 24, px + 24);  // miejsce na pierścień nagrywania i obwódkę fokusu
    updateGeometry();
    update();
}

void PrzyciskMikrofonu::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPointF srodek = QRectF(rect()).center();
    double rKola = srednica_ / 2.0 - (nagrywa_ ? 10 : 0);  // w nagrywaniu koło mniejsze, wokół pierścień

    QString tlo = !isEnabled() ? "disabledBg" : nagrywa_ ? "red" : "amber";
    QString znak = !isEnabled() ? "disabledText" : nagrywa_ ? "onRed" : "onAmber";
    if (isDown()) p.setOpacity(0.85);

    if (nagrywa_) {
        p.setPen(QPen(motyw::kolor("red"), 4));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(srodek, srednica_ / 2.0 - 2, srednica_ / 2.0 - 2);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(motyw::kolor(tlo));
    p.drawEllipse(srodek, rKola, rKola);
    p.setOpacity(1);

    if (hasFocus()) {
        p.setPen(QPen(motyw::kolor("focus"), 3));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(srodek, srednica_ / 2.0 + 8, srednica_ / 2.0 + 8);
    }
    int ikona = int(rKola * 0.9);
    QPixmap px = motyw::pixmapa("mic", znak, ikona);
    p.drawPixmap(QPointF(srodek.x() - ikona / 2.0, srodek.y() - ikona / 2.0), px);
}

// ---------------------------------------------------------------- Baner

Baner::Baner(QWidget* rodzic) : QFrame(rodzic) {
    setObjectName("baner");
    auto* uklad = new QHBoxLayout(this);
    uklad->setContentsMargins(16, 14, 16, 14);
    uklad->setSpacing(12);
    ikona_ = new QLabel;
    ikona_->setAlignment(Qt::AlignTop);
    tytul_ = new QLabel;
    tytul_->setObjectName("banerTytul");
    tytul_->setWordWrap(true);
    opis_ = new QLabel;
    opis_->setObjectName("banerOpis");
    opis_->setWordWrap(true);
    auto* tekst = new QVBoxLayout;
    tekst->setSpacing(4);
    tekst->addWidget(tytul_);
    tekst->addWidget(opis_);
    uklad->addWidget(ikona_, 0, Qt::AlignTop);
    uklad->addLayout(tekst, 1);
    hide();
}

void Baner::pokaz(Rodzaj r, const QString& tytul, const QString& opis) {
    rodzaj_ = r;
    setProperty("rodzaj", r == Ostrzezenie ? "ostrzezenie" : r == Blad ? "blad" : "info");
    tytul_->setText(tytul);
    opis_->setText(opis);
    opis_->setVisible(!opis.isEmpty());
    odswiezIkone();
    motyw::odswiez(this);
    show();
}

void Baner::odswiezIkone() {
    const char* kolor = rodzaj_ == Ostrzezenie ? "warnLine" : rodzaj_ == Blad ? "errLine" : "infoLine";
    ikona_->setPixmap(motyw::pixmapa(rodzaj_ == Informacja ? "ok" : "error", kolor, 26));
}

// ---------------------------------------------------------------- PoleTekstu

void PoleTekstu::keyPressEvent(QKeyEvent* e) {
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !(e->modifiers() & Qt::ShiftModifier)) {
        emit enter();
        return;
    }
    QPlainTextEdit::keyPressEvent(e);
}

// ---------------------------------------------------------------- pomocnicze

QLabel* klawisz(const QString& tekst, QWidget* rodzic) {
    auto* l = new QLabel(tekst, rodzic);
    l->setObjectName("klawisz");
    l->setAlignment(Qt::AlignCenter);
    l->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return l;
}

QLabel* etykieta(const QString& tekst, const char* rola, QWidget* rodzic) {
    auto* l = new QLabel(tekst, rodzic);
    l->setProperty("rola", rola);
    l->setWordWrap(true);
    return l;
}
