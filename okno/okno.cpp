#include "okno.hpp"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QShortcut>
#include <QVBoxLayout>

#include "motyw.hpp"

namespace {

const char* const IKONY_ZAKLADEK[3] = {"say", "mic", "list"};
const char* const KOLORY_ZAKLADEK[3] = {"blue", "amber", "violet"};
const char* const NAPISY_NA_KOLORZE[3] = {"onBlue", "onAmber", "onViolet"};
const int DLUGOSCI[3] = {5, 10, 30};

// Nazwa urządzenia do pokazania ludziom: "ALC3234 Analog (HDA Intel PCH)" -> "ALC3234 Analog".
QString krotkaNazwa(const QString& nazwa, const QString& opis) {
    if (nazwa.isEmpty()) return "Nie wybrano";
    if (nazwa == "default") return "Systemowy";
    int nawias = opis.indexOf(" (");
    return nawias > 0 ? opis.left(nawias) : (opis.isEmpty() ? nazwa : opis);
}

QString nazwaNaLiscie(const Urzadzenie& u) {
    return u.nazwa == "default" ? QString("Systemowy (z ustawień dźwięku)") : u.opis;
}

QLabel* obrazek(QWidget* rodzic = nullptr) {
    auto* l = new QLabel(rodzic);
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    return l;
}

}  // namespace

// ============================================================================ budowa

Okno::Okno() {
    setWindowTitle("Komunikator");
    setWindowIcon(QIcon(":/app/komunikator.svg"));
    setMinimumSize(480, 600);
    resize(560, 760);

    ekrany_ = new QStackedWidget;
    ekrany_->addWidget(zbudujKreator());
    ekrany_->addWidget(zbudujGlowny());
    ekrany_->addWidget(zbudujUstawienia());
    setCentralWidget(ekrany_);

    // --- silnik -> okno
    connect(&silnik_, &Silnik::stanSerwera, this, &Okno::ustawStanSerwera);
    connect(&silnik_, &Silnik::poziom, this, [this](double v) {
        if (aktywnyPoziom_) aktywnyPoziom_->ustaw(v);
    });
    connect(&silnik_, &Silnik::mowaStart, this, [this] {
        ustawZasobnik("speaking");
        if (zrodloMowy_ == ZrodloMowy::Powiedz) {
            przyciskPowiedz_->setProperty("mowi", true);
            tekstPowiedz_->setText("Zatrzymaj");
            przyciskPowiedz_->setAccessibleName("Zatrzymaj");
            motyw::odswiez(przyciskPowiedz_);
            odswiezIkony();
        }
    });
    connect(&silnik_, &Silnik::mowaKoniec, this, [this](bool ok, bool zatrzymana, const QString& blad) {
        ZrodloMowy zrodlo = zrodloMowy_;
        zrodloMowy_ = ZrodloMowy::Brak;
        odtwarzany_.clear();
        ustawZasobnik(serwerDziala_ ? "idle" : "error");
        przyciskPowiedz_->setProperty("mowi", false);
        tekstPowiedz_->setText("Powiedz");
        przyciskPowiedz_->setAccessibleName("Powiedz");
        motyw::odswiez(przyciskPowiedz_);
        odswiezIkony();
        odswiezPrzyciskiOdtwarzania();
        if (ok || zatrzymana) return;
        if (zrodlo == ZrodloMowy::Powiedz)
            banerPowiedz_->pokaz(Baner::Blad, "Nie udało się powiedzieć", blad);
        else if (zrodlo == ZrodloMowy::Komunikat)
            banerKomunikaty_->pokaz(Baner::Blad, "Nie udało się odtworzyć", blad);
        else
            QMessageBox::warning(this, "Test głośnika", blad);
    });
    connect(&silnik_, &Silnik::nagranieKoniec, this, [this](bool ok, bool cisza, const QString& blad) {
        ustawZasobnik(serwerDziala_ ? "idle" : "error");
        if (testowanyPrzycisk_) {  // test mikrofonu w ustawieniach / kreatorze
            testowanyPrzycisk_->setText("Test");
            testowanyPrzycisk_ = nullptr;
            if (aktywnyPoziom_) aktywnyPoziom_->ustaw(0);
            aktywnyPoziom_ = nullptr;
            QFile::remove(QDir::temp().filePath(QString("komunikator-test-%1.wav").arg(QCoreApplication::applicationPid())));
            if (!ok) QMessageBox::warning(this, "Test mikrofonu", blad);
            return;
        }
        odliczanie_.stop();
        aktywnyPoziom_ = nullptr;
        if (!ok) {
            ustawSluchanie(Sluchanie::Gotowe);
            banerSluchaj_->pokaz(Baner::Blad, "Nie udało się nagrać", blad);
            banerUrzadzenia_ = false;
            przyciskiCiszy_->show();  // "Mikrofon" (ustawienia) i "Spróbuj jeszcze raz"
            QFile::remove(plikNagrania_);
            return;
        }
        if (cisza) {
            ustawSluchanie(Sluchanie::Gotowe);
            banerSluchaj_->pokaz(Baner::Ostrzezenie, "Nic nie słychać — sprawdź mikrofon",
                                 "Mów bliżej mikrofonu albo wybierz inny w ustawieniach.");
            banerUrzadzenia_ = false;
            przyciskiCiszy_->show();
            QFile::remove(plikNagrania_);
            return;
        }
        ustawSluchanie(Sluchanie::Pracuje);
        silnik_.rozpoznaj(plikNagrania_);
    });
    connect(&silnik_, &Silnik::rozpoznano, this, [this](bool ok, const QString& wynik, const QString& uwaga) {
        QFile::remove(plikNagrania_);  // nagranie było potrzebne tylko do rozpoznania
        ustawSluchanie(Sluchanie::Gotowe);
        if (ok && wynik.isEmpty()) {
            banerSluchaj_->pokaz(Baner::Ostrzezenie, "Nie rozpoznałem żadnych słów",
                                 "Mów wyraźnie, bliżej mikrofonu, i spróbuj jeszcze raz.");
            banerUrzadzenia_ = false;
        } else if (ok) {
            rozpoznany_->setPlainText(wynik);
            kopiuj_->setEnabled(true);
            if (!uwaga.isEmpty()) {
                banerSluchaj_->pokaz(Baner::Informacja, "Rozpoznane lokalnie — Gemini nie zadziałało", uwaga);
                banerUrzadzenia_ = false;
            }
        } else {
            banerSluchaj_->pokaz(Baner::Blad, "Nie udało się rozpoznać mowy", wynik);
        }
    });
    connect(&silnik_, &Silnik::zapisano, this, [this](bool ok, const QString& blad) {
        przyciskZapisz_->setEnabled(true);
        if (ok) {
            banerPowiedz_->pokaz(Baner::Informacja, "Zapisano komunikat", "Znajdziesz go w zakładce Komunikaty.");
            odswiezKomunikaty();
        } else {
            banerPowiedz_->pokaz(Baner::Blad, "Nie udało się zapisać komunikatu", blad);
        }
    });

    // --- odliczanie nagrania
    odliczanie_.setInterval(1000);
    connect(&odliczanie_, &QTimer::timeout, this, [this] {
        if (pozostalo_ > 0) --pozostalo_;
        licznik_->setText(QString("%1 s").arg(pozostalo_));
    });

    // --- klawiatura: Esc zatrzymuje mówienie i nagrywanie
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, &Okno::zatrzymajWszystko);

    // --- zasobnik
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        zasobnik_ = new QSystemTrayIcon(this);
        auto* menu = new QMenu(this);
        menu->addAction("Pokaż okno", this, [this] {
            showNormal();
            raise();
            activateWindow();
        });
        menu->addAction("Zakończ", qApp, &QApplication::quit);
        zasobnik_->setContextMenu(menu);
        connect(zasobnik_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
            if (r != QSystemTrayIcon::Trigger) return;
            if (isVisible()) hide();
            else {
                showNormal();
                raise();
                activateWindow();
            }
        });
        ustawZasobnik("idle");
        zasobnik_->show();
    }

    // --- serwer mowy: od razu i co 30 s
    sprawdzanie_.setInterval(30000);
    connect(&sprawdzanie_, &QTimer::timeout, &silnik_, &Silnik::sprawdz);
    connect(&sprawdzanie_, &QTimer::timeout, this, [this] {
        if (ekrany_->currentIndex() == GLOWNY) odswiezUrzadzenia(false);  // słuchawki odłączone / podłączone
    });
    sprawdzanie_.start();
    silnik_.sprawdz();

    odswiezKomunikaty();
    odswiezUrzadzenia();
    odswiezIkony();
    pokazStrone(POWIEDZ);

    // Pierwsze uruchomienie: bez wybranego mikrofonu albo głośnika nic nie dzieje się "samo" - kreator.
    auto ust = silnik_.ustawienia();
    pokazEkran(ust.value("mikrofon").isEmpty() || ust.value("glosnik").isEmpty() ? KREATOR : GLOWNY);
}

Okno::~Okno() { zatrzymajWszystko(); }

QWidget* Okno::kolumna(QWidget* tresc, int margines) {
    // Zawartość w kolumnie najwyżej 1160 px, wyśrodkowana - na szerokim ekranie nie rozjeżdża się
    auto* w = new QWidget;
    w->setObjectName("przezroczysty");
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(margines, margines, margines, margines);
    tresc->setMaximumWidth(1160);
    h->addStretch(0);
    h->addWidget(tresc, 1);
    h->addStretch(0);
    return w;
}

QPushButton* Okno::przycisk(const QString& tekst, const QString& ikona, const char* kolor) {
    auto* p = new QPushButton(tekst);
    p->setProperty("ikona", ikona);
    if (kolor) p->setProperty("kolor", kolor);
    p->setIconSize(QSize(24, 24));
    p->setCursor(Qt::PointingHandCursor);
    return p;
}

QWidget* Okno::zbudujGlowny() {
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // zakładki
    auto* rzad = new QWidget;
    rzad->setObjectName("tresc");
    auto* h = new QHBoxLayout(rzad);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    const char* napisy[3] = {"Powiedz", "Słuchaj", "Komunikaty"};
    for (int i = 0; i < 3; ++i) {
        auto* z = new QToolButton;
        z->setObjectName("zakladka");
        z->setProperty("kolor", KOLORY_ZAKLADEK[i]);
        z->setText(napisy[i]);
        z->setCheckable(true);
        z->setAutoExclusive(true);
        z->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        z->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        z->setIconSize(QSize(26, 26));
        z->setCursor(Qt::PointingHandCursor);
        connect(z, &QToolButton::clicked, this, [this, i] { pokazStrone(Strona(i)); });
        if (i < 2) {
            znaczki_[i] = obrazek(z);
            znaczki_[i]->setAccessibleName("Brak serwera mowy");
            znaczki_[i]->hide();
            z->installEventFilter(this);  // znaczek trzyma się prawego górnego rogu
        }
        zakladki_[i] = z;
        h->addWidget(z);
    }
    v->addWidget(kolumna(rzad, 16));

    strony_ = new QStackedWidget;
    strony_->addWidget(zbudujPowiedz());
    strony_->addWidget(zbudujSluchaj());
    strony_->addWidget(zbudujKomunikaty());
    v->addWidget(strony_, 1);
    v->addWidget(zbudujPasekStanu());
    return w;
}

QWidget* Okno::zbudujPowiedz() {
    auto* tresc = new QWidget;
    tresc->setObjectName("tresc");
    auto* v = new QVBoxLayout(tresc);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(16);

    banerPowiedz_ = new Baner;
    v->addWidget(banerPowiedz_);
    v->addWidget(etykieta("Co powiedzieć?", "naglowek"));

    poleMowy_ = new PoleTekstu;
    poleMowy_->setObjectName("poleMowy");
    poleMowy_->setPlaceholderText("Wpisz, co mam powiedzieć…");
    poleMowy_->setMinimumHeight(130);
    poleMowy_->setAccessibleName("Co powiedzieć");
    connect(poleMowy_, &PoleTekstu::enter, this, &Okno::mowLubZatrzymaj);
    v->addWidget(poleMowy_, 1);

    // Powiedz: ikona + napis + podpowiedź "Enter"; podczas mówienia ten sam przycisk to Zatrzymaj
    przyciskPowiedz_ = new QPushButton;
    przyciskPowiedz_->setObjectName("powiedz");
    przyciskPowiedz_->setAccessibleName("Powiedz");
    przyciskPowiedz_->setMinimumHeight(84);
    przyciskPowiedz_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    przyciskPowiedz_->setCursor(Qt::PointingHandCursor);
    auto* h = new QHBoxLayout(przyciskPowiedz_);
    h->setContentsMargins(20, 0, 20, 0);
    h->setSpacing(14);
    ikonaPowiedz_ = obrazek();
    tekstPowiedz_ = new QLabel("Powiedz");
    tekstPowiedz_->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* enter = klawisz("Enter");
    enter->setAttribute(Qt::WA_TransparentForMouseEvents);
    h->addWidget(ikonaPowiedz_);
    h->addWidget(tekstPowiedz_);
    h->addStretch(1);
    h->addWidget(enter);
    connect(przyciskPowiedz_, &QPushButton::clicked, this, &Okno::mowLubZatrzymaj);
    v->addWidget(przyciskPowiedz_);

    przyciskZapisz_ = przycisk("Zapisz jako komunikat", "save");
    przyciskZapisz_->setProperty("duzy", true);
    connect(przyciskZapisz_, &QPushButton::clicked, this, &Okno::zapiszJakoKomunikat);
    v->addWidget(przyciskZapisz_);
    v->addStretch(0);
    return kolumna(tresc);
}

QWidget* Okno::zbudujSluchaj() {
    auto* tresc = new QWidget;
    tresc->setObjectName("tresc");
    auto* v = new QVBoxLayout(tresc);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(14);

    // długość nagrania
    auto* rzad = new QHBoxLayout;
    rzad->setSpacing(10);
    rzad->addWidget(etykieta("Nagrywaj:", "etykieta"));
    for (int i = 0; i < 3; ++i) {
        segmenty_[i] = new QPushButton(QString("%1 s").arg(DLUGOSCI[i]));
        segmenty_[i]->setObjectName("segment");
        segmenty_[i]->setCheckable(true);
        segmenty_[i]->setAutoExclusive(true);
        segmenty_[i]->setAccessibleName(QString("Nagrywaj %1 sekund").arg(DLUGOSCI[i]));
        rzad->addWidget(segmenty_[i], 1);
    }
    segmenty_[1]->setChecked(true);
    v->addLayout(rzad);

    banerSluchaj_ = new Baner;
    v->addWidget(banerSluchaj_);

    // gdy nie wybrano mikrofonu - prosto do ustawień
    wybierzMikrofon_ = przycisk("Wybierz mikrofon", "settings", "amber");
    wybierzMikrofon_->setProperty("duzy", true);
    connect(wybierzMikrofon_, &QPushButton::clicked, this, [this] { pokazEkran(USTAWIENIA); });
    wybierzMikrofon_->hide();
    v->addWidget(wybierzMikrofon_);

    // mikrofon
    mikrofon_ = new PrzyciskMikrofonu;
    connect(mikrofon_, &PrzyciskMikrofonu::clicked, this, &Okno::sluchajLubZatrzymaj);
    v->addWidget(mikrofon_, 0, Qt::AlignHCenter);

    // pod mikrofonem: "Słuchaj [spacja]" albo licznik nagrywania
    podMikrofonem_ = new QStackedWidget;
    podMikrofonem_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* spoczynek = new QWidget;
    auto* hs = new QHBoxLayout(spoczynek);
    hs->setContentsMargins(0, 0, 0, 0);
    hs->addStretch(1);
    podpisSluchaj_ = etykieta("Słuchaj", "podpisMikrofonu");
    podpisSluchaj_->setWordWrap(false);
    klawiszSluchaj_ = klawisz("spacja");
    hs->addWidget(podpisSluchaj_);
    hs->addSpacing(10);
    hs->addWidget(klawiszSluchaj_, 0, Qt::AlignVCenter);
    hs->addStretch(1);
    auto* nagrywanie = new QWidget;
    auto* vn = new QVBoxLayout(nagrywanie);
    vn->setContentsMargins(0, 0, 0, 0);
    vn->setSpacing(10);
    auto* hn = new QHBoxLayout;
    hn->addStretch(1);
    licznik_ = etykieta("10 s", "licznik");
    licznik_->setWordWrap(false);
    hn->addWidget(licznik_);
    hn->addSpacing(14);
    auto* opisNagr = new QVBoxLayout;
    opisNagr->setSpacing(4);
    auto* nagrywam = etykieta("Nagrywam…", "naglowek");
    nagrywam->setWordWrap(false);
    opisNagr->addWidget(nagrywam);
    opisNagr->addWidget(klawisz("spacja = stop"), 0, Qt::AlignLeft);
    hn->addLayout(opisNagr);
    hn->addStretch(1);
    vn->addLayout(hn);
    poziomNagrania_ = new PoziomDzwieku("red");
    vn->addWidget(poziomNagrania_);
    podMikrofonem_->addWidget(spoczynek);
    podMikrofonem_->addWidget(nagrywanie);
    v->addWidget(podMikrofonem_);

    // cisza w nagraniu: dwie drogi dalej
    przyciskiCiszy_ = new QWidget;
    auto* hc = new QHBoxLayout(przyciskiCiszy_);
    hc->setContentsMargins(0, 0, 0, 0);
    hc->setSpacing(12);
    auto* doUstawien = przycisk("Mikrofon", "settings");
    connect(doUstawien, &QPushButton::clicked, this, [this] { pokazEkran(USTAWIENIA); });
    auto* jeszczeRaz = przycisk("Spróbuj jeszcze raz", "mic", "amber");
    connect(jeszczeRaz, &QPushButton::clicked, this, &Okno::sluchajLubZatrzymaj);
    hc->addWidget(doUstawien, 1);
    hc->addWidget(jeszczeRaz, 2);
    przyciskiCiszy_->hide();
    v->addWidget(przyciskiCiszy_);

    // "Pracuję…"
    panelPracy_ = new QFrame;
    panelPracy_->setObjectName("panelPracy");
    auto* vp = new QVBoxLayout(panelPracy_);
    vp->setContentsMargins(16, 14, 16, 14);
    vp->setSpacing(10);
    vp->addWidget(etykieta("Pracuję…", "naglowek"));
    pasekPracy_ = new PasekPracy;
    vp->addWidget(pasekPracy_);
    vp->addWidget(etykieta("Zamieniam nagranie na tekst. To potrwa kilka sekund.", "tresc"));
    panelPracy_->hide();
    v->addWidget(panelPracy_);

    // wynik
    wynik_ = new QWidget;
    auto* vw = new QVBoxLayout(wynik_);
    vw->setContentsMargins(0, 0, 0, 0);
    vw->setSpacing(12);
    vw->addWidget(etykieta("Rozpoznany tekst", "naglowek"));
    rozpoznany_ = new QPlainTextEdit;
    rozpoznany_->setReadOnly(true);
    rozpoznany_->setPlaceholderText("Tu pojawi się tekst");
    rozpoznany_->setMinimumHeight(64);
    rozpoznany_->setMaximumHeight(160);
    rozpoznany_->setAccessibleName("Rozpoznany tekst");
    vw->addWidget(rozpoznany_);
    kopiuj_ = przycisk("Kopiuj", "copy");
    kopiuj_->setAccessibleName("Kopiuj rozpoznany tekst");
    kopiuj_->setEnabled(false);
    connect(kopiuj_, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(rozpoznany_->toPlainText()); });
    vw->addWidget(kopiuj_);
    v->addWidget(wynik_);
    v->addStretch(1);

    auto* przewijanie = new QScrollArea;
    przewijanie->setWidgetResizable(true);
    przewijanie->setFrameShape(QFrame::NoFrame);
    przewijanie->setWidget(kolumna(tresc));
    return przewijanie;
}

QWidget* Okno::zbudujKomunikaty() {
    auto* tresc = new QWidget;
    tresc->setObjectName("tresc");
    auto* v = new QVBoxLayout(tresc);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(12);

    szukaj_ = new QLineEdit;
    szukaj_->setPlaceholderText("Szukaj komunikatu");
    szukaj_->setAccessibleName("Szukaj komunikatu");
    szukaj_->setClearButtonEnabled(true);
    szukaj_->setMaximumWidth(560);
    szukaj_->setObjectName("szukaj");
    auto* lupa = obrazek(szukaj_);
    lupa->setObjectName("lupa");
    lupa->setGeometry(16, 0, 24, 56);
    lupa->setAlignment(Qt::AlignCenter);
    connect(szukaj_, &QLineEdit::textChanged, this, &Okno::ulozKarty);
    v->addWidget(szukaj_);

    banerKomunikaty_ = new Baner;
    v->addWidget(banerKomunikaty_);

    listaLubPusto_ = new QStackedWidget;
    // karty
    auto* przewijanie = new QScrollArea;
    przewijanie->setWidgetResizable(true);
    przewijanie->setFrameShape(QFrame::NoFrame);
    karty_ = new QWidget;
    karty_->setObjectName("tresc");
    auto* vk = new QVBoxLayout(karty_);
    vk->setContentsMargins(0, 0, 4, 0);
    siatka_ = new QGridLayout;
    siatka_->setSpacing(12);
    vk->addLayout(siatka_);
    brakWynikow_ = etykieta("", "tresc");
    brakWynikow_->hide();
    vk->addWidget(brakWynikow_);
    vk->addStretch(1);
    przewijanie->setWidget(karty_);
    listaLubPusto_->addWidget(przewijanie);

    // pusta lista
    auto* pusto = new QWidget;
    auto* vp = new QVBoxLayout(pusto);
    vp->addStretch(1);
    auto* ikona = obrazek();
    ikona->setProperty("ikona", "list");
    ikona->setObjectName("ikonaPusto");
    ikona->setAlignment(Qt::AlignCenter);
    vp->addWidget(ikona);
    auto* t1 = etykieta("Nie masz jeszcze komunikatów", "naglowek");
    t1->setAlignment(Qt::AlignCenter);
    auto* t2 = etykieta("Zapisz zdanie z zakładki Powiedz — pojawi się tutaj.", "tresc");
    t2->setAlignment(Qt::AlignCenter);
    vp->addWidget(t1);
    vp->addWidget(t2);
    vp->addSpacing(12);
    auto* dodaj = przycisk("Dodaj pierwszy komunikat", "plus", "violet");
    connect(dodaj, &QPushButton::clicked, this, [this] {
        pokazStrone(POWIEDZ);
        poleMowy_->setFocus();
    });
    vp->addWidget(dodaj, 0, Qt::AlignHCenter);
    vp->addStretch(2);
    listaLubPusto_->addWidget(pusto);

    v->addWidget(listaLubPusto_, 1);
    return kolumna(tresc);
}

QWidget* Okno::zbudujKarte(const Komunikat& k) {
    auto* karta = new QFrame;
    karta->setObjectName("karta");
    auto* v = new QVBoxLayout(karta);
    v->setContentsMargins(14, 14, 14, 14);
    v->setSpacing(6);
    v->addWidget(etykieta(k.nazwa, "naglowek"));
    auto* tekst = etykieta(k.tekst, "tresc");
    QFont f = tekst->font();
    f.setPixelSize(18);
    tekst->setMaximumHeight(QFontMetrics(f).lineSpacing() * 2 + 2);  // najwyżej 2 linie
    tekst->setToolTip(k.tekst);
    v->addWidget(tekst);
    v->addStretch(1);
    v->addSpacing(6);
    auto* h = new QHBoxLayout;
    h->setSpacing(10);
    bool gra = odtwarzany_ == k.nazwa;
    auto* odtworz = przycisk(gra ? "Zatrzymaj" : "Odtwórz", gra ? "stop" : "play", gra ? "red" : "violet");
    odtworz->setAccessibleName((gra ? "Zatrzymaj " : "Odtwórz ") + k.nazwa);
    odtworz->setProperty("komunikat", k.nazwa);
    auto* usun = przycisk("Usuń", "delete", "danger");
    usun->setAccessibleName("Usuń " + k.nazwa);
    QString nazwa = k.nazwa;
    connect(odtworz, &QPushButton::clicked, this, [this, nazwa] { odtworzKomunikat(nazwa); });
    connect(usun, &QPushButton::clicked, this, [this, nazwa] { usunKomunikat(nazwa); });
    h->addWidget(odtworz, 3);
    h->addWidget(usun, 2);
    v->addLayout(h);
    return karta;
}

QWidget* Okno::zbudujUstawienia() {
    auto* tresc = new QWidget;
    tresc->setObjectName("tresc");
    auto* v = new QVBoxLayout(tresc);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(16);

    auto* naglowek = new QHBoxLayout;
    auto* wroc = przycisk("Wróć", "back");
    connect(wroc, &QPushButton::clicked, this, [this] {
        zatrzymajWszystko();
        pokazEkran(powrot_);
    });
    naglowek->addWidget(wroc);
    naglowek->addSpacing(8);
    naglowek->addWidget(etykieta("Ustawienia", "tytul"), 1);
    v->addLayout(naglowek);

    auto panel = [](const QString& ikona, const QString& tytul, QVBoxLayout*& uklad) {
        auto* p = new QFrame;
        p->setObjectName("panel");
        uklad = new QVBoxLayout(p);
        uklad->setContentsMargins(14, 14, 14, 14);
        uklad->setSpacing(12);
        if (!tytul.isEmpty()) {
            auto* h = new QHBoxLayout;
            h->setSpacing(10);
            auto* ik = obrazek();
            ik->setProperty("ikona", ikona);
            ik->setObjectName("ikonaPanelu");
            h->addWidget(ik);
            h->addWidget(etykieta(tytul, "naglowek"), 1);
            uklad->addLayout(h);
        }
        return p;
    };

    // Mikrofon
    QVBoxLayout* um;
    v->addWidget(panel("mic", "Mikrofon", um));
    ustMikrofon_ = new QComboBox;
    ustMikrofon_->setAccessibleName("Mikrofon");
    um->addWidget(ustMikrofon_);
    auto* hm = new QHBoxLayout;
    hm->setSpacing(12);
    ustTestMikrofonu_ = przycisk("Test", "mic");
    ustTestMikrofonu_->setAccessibleName("Test mikrofonu");
    hm->addWidget(ustTestMikrofonu_);
    auto* vm = new QVBoxLayout;
    vm->setSpacing(4);
    vm->addWidget(etykieta("Poziom dźwięku", "maly"));
    ustPoziom_ = new PoziomDzwieku("blue");
    vm->addWidget(ustPoziom_);
    hm->addLayout(vm, 1);
    um->addLayout(hm);
    connect(ustTestMikrofonu_, &QPushButton::clicked, this,
            [this] { testMikrofonu(ustMikrofon_, ustTestMikrofonu_, ustPoziom_); });
    connect(ustMikrofon_, qOverload<int>(&QComboBox::activated), this, [this](int i) {
        if (i < 0) return;
        silnik_.zatrzymajNagranie();
        QString blad;
        if (!silnik_.wybierz(true, ustMikrofon_->itemData(i).toString(), &blad))
            QMessageBox::warning(this, "Mikrofon", blad);
        odswiezUrzadzenia();
    });

    // Głośnik
    QVBoxLayout* ug;
    v->addWidget(panel("speaker", "Głośnik", ug));
    ustGlosnik_ = new QComboBox;
    ustGlosnik_->setAccessibleName("Głośnik");
    ug->addWidget(ustGlosnik_);
    ustTestGlosnika_ = przycisk("Test — odegraj zdanie", "speaker");
    ug->addWidget(ustTestGlosnika_);
    connect(ustTestGlosnika_, &QPushButton::clicked, this, [this] { testGlosnika(ustGlosnik_); });
    connect(ustGlosnik_, qOverload<int>(&QComboBox::activated), this, [this](int i) {
        if (i < 0) return;
        QString blad;
        if (!silnik_.wybierz(false, ustGlosnik_->itemData(i).toString(), &blad))
            QMessageBox::warning(this, "Głośnik", blad);
        odswiezUrzadzenia();
    });

    // Rozpoznawanie mowy: Gemini (chmura, dokładniejsze) albo whisper na serwerze
    QVBoxLayout* ur;
    v->addWidget(panel("mic", "Rozpoznawanie mowy", ur));
    auto* hr = new QHBoxLayout;
    hr->setSpacing(8);
    rozGemini_ = new QPushButton("Gemini");
    rozLokalnie_ = new QPushButton("Lokalnie");
    rozGemini_->setAccessibleName("Rozpoznawanie przez Gemini, przez internet");
    rozLokalnie_->setAccessibleName("Rozpoznawanie lokalne, whisper na serwerze");
    for (QPushButton* b : {rozGemini_, rozLokalnie_}) {
        b->setObjectName("segment");
        b->setCheckable(true);
        b->setAutoExclusive(true);
        hr->addWidget(b, 1);
    }
    ur->addLayout(hr);
    ur->addWidget(etykieta("Gemini rozumie lepiej, ale potrzebuje internetu i klucza. "
                           "Gdy Gemini zawiedzie, komunikator sam rozpozna lokalnie.",
                           "maly"));
    connect(rozGemini_, &QPushButton::clicked, this, [this] { wybierzRozpoznawanie(true); });
    connect(rozLokalnie_, &QPushButton::clicked, this, [this] { wybierzRozpoznawanie(false); });

    kluczTresc_ = new QWidget;
    kluczTresc_->setObjectName("przezroczysty");
    auto* vk = new QVBoxLayout(kluczTresc_);
    vk->setContentsMargins(0, 0, 0, 0);
    vk->setSpacing(8);
    stanKlucza_ = etykieta("", "maly");
    stanKlucza_->setWordWrap(true);
    stanKlucza_->setTextFormat(Qt::RichText);
    stanKlucza_->setOpenExternalLinks(true);
    stanKlucza_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    vk->addWidget(stanKlucza_);
    klucz_ = new QLineEdit;
    klucz_->setAccessibleName("Klucz Gemini");
    klucz_->setEchoMode(QLineEdit::Password);
    klucz_->setPlaceholderText("Wklej klucz Gemini");
    vk->addWidget(klucz_);
    auto* hk = new QHBoxLayout;
    hk->setSpacing(12);
    zapiszKlucz_ = przycisk("Zapisz klucz", "save");
    usunKlucz_ = przycisk("Usuń klucz", "delete", "danger");
    hk->addWidget(zapiszKlucz_, 1);
    hk->addWidget(usunKlucz_, 1);
    vk->addLayout(hk);
    ur->addWidget(kluczTresc_);
    auto zapisz = [this] {
        if (klucz_->text().trimmed().isEmpty()) return;
        QString blad;
        bool ok = silnik_.zapiszKlucz(klucz_->text(), &blad);
        klucz_->clear();  // klucz nie zostaje w polu ani w pamięci okna
        if (!ok) QMessageBox::warning(this, "Klucz Gemini", blad);
        odswiezRozpoznawanie();
    };
    connect(zapiszKlucz_, &QPushButton::clicked, this, zapisz);
    connect(klucz_, &QLineEdit::returnPressed, this, zapisz);
    connect(usunKlucz_, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "Klucz Gemini", "Usunąć zapisany klucz Gemini?") != QMessageBox::Yes) return;
        QString blad;
        if (!silnik_.usunKlucz(&blad)) QMessageBox::warning(this, "Klucz Gemini", blad);
        odswiezRozpoznawanie();
    });

    // Zaawansowane (zwijane)
    QVBoxLayout* uz;
    v->addWidget(panel("", "", uz));
    zaawansowane_ = new QToolButton;
    zaawansowane_->setObjectName("naglowekPanelu");
    zaawansowane_->setText("Zaawansowane");
    zaawansowane_->setCheckable(true);
    zaawansowane_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    zaawansowane_->setLayoutDirection(Qt::RightToLeft);  // strzałka po prawej
    zaawansowane_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    zaawansowane_->setIconSize(QSize(22, 22));
    uz->addWidget(zaawansowane_);
    zaawansowaneTresc_ = new QWidget;
    zaawansowaneTresc_->setObjectName("przezroczysty");
    auto* vz = new QVBoxLayout(zaawansowaneTresc_);
    vz->setContentsMargins(0, 0, 0, 0);
    vz->setSpacing(8);
    vz->addWidget(etykieta("Adres serwera mowy", "maly"));
    adres_ = new QLineEdit;
    adres_->setAccessibleName("Adres serwera mowy");
    adres_->setPlaceholderText("np. 192.168.1.10");
    connect(adres_, &QLineEdit::editingFinished, this, &Okno::zmienAdresSerwera);
    vz->addWidget(adres_);
    zaawansowaneTresc_->hide();
    uz->addWidget(zaawansowaneTresc_);
    connect(zaawansowane_, &QToolButton::toggled, this, [this](bool tak) {
        zaawansowaneTresc_->setVisible(tak);
        odswiezIkony();
    });

    v->addStretch(1);
    auto* przewijanie = new QScrollArea;
    przewijanie->setWidgetResizable(true);
    przewijanie->setFrameShape(QFrame::NoFrame);
    przewijanie->setWidget(kolumna(tresc));
    return przewijanie;
}

QWidget* Okno::zbudujKreator() {
    krokiKreatora_ = new QStackedWidget;

    auto krok = [this](int nr, const QString& tytul, const QString& opis, QVBoxLayout*& tresc,
                       QHBoxLayout*& przyciski) {
        auto* w = new QWidget;
        w->setObjectName("tresc");
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 8, 0, 0);
        v->setSpacing(16);
        v->addWidget(etykieta(QString("Krok %1 z 2").arg(nr), "krok"));
        auto* pasek = new QHBoxLayout;
        pasek->setSpacing(8);
        for (int i = 1; i <= 2; ++i) {
            auto* s = new QFrame;
            s->setObjectName("krokPasek");
            s->setProperty("aktywny", i <= nr);
            pasek->addWidget(s);
        }
        v->addLayout(pasek);
        v->addSpacing(4);
        v->addWidget(etykieta(tytul, "tytulKreatora"));
        v->addWidget(etykieta(opis, "tresc"));
        tresc = new QVBoxLayout;
        tresc->setSpacing(14);
        v->addLayout(tresc);
        v->addStretch(1);
        przyciski = new QHBoxLayout;
        przyciski->setSpacing(12);
        v->addLayout(przyciski);
        return kolumna(w, 20);
    };

    // krok 1: mikrofon
    QVBoxLayout* t1;
    QHBoxLayout* p1;
    krokiKreatora_->addWidget(krok(1, "Wybierz mikrofon", "Powiedz coś. Pasek poniżej powinien się ruszać.", t1, p1));
    kreMikrofon_ = new QComboBox;
    kreMikrofon_->setAccessibleName("Mikrofon");
    t1->addWidget(kreMikrofon_);
    auto* h1 = new QHBoxLayout;
    h1->setSpacing(12);
    kreTestMikrofonu_ = przycisk("Test", "mic");
    kreTestMikrofonu_->setAccessibleName("Test mikrofonu");
    krePoziom_ = new PoziomDzwieku("blue");
    h1->addWidget(kreTestMikrofonu_);
    h1->addWidget(krePoziom_, 1);
    t1->addLayout(h1);
    kreDalej_ = przycisk("Dalej", "next", "blue");
    kreDalej_->setProperty("duzy", true);
    kreDalej_->setMinimumHeight(64);
    p1->addWidget(kreDalej_);
    connect(kreTestMikrofonu_, &QPushButton::clicked, this,
            [this] { testMikrofonu(kreMikrofon_, kreTestMikrofonu_, krePoziom_); });
    connect(kreMikrofon_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
        kreDalej_->setEnabled(i >= 0);
        kreTestMikrofonu_->setEnabled(i >= 0);
        silnik_.zatrzymajNagranie();
    });
    connect(kreDalej_, &QPushButton::clicked, this, [this] {
        silnik_.zatrzymajNagranie();
        QString blad;
        if (!silnik_.wybierz(true, kreMikrofon_->currentData().toString(), &blad)) {
            QMessageBox::warning(this, "Mikrofon", blad);
            return;
        }
        krokiKreatora_->setCurrentIndex(1);
        kreGlosnik_->setFocus();
    });

    // krok 2: głośnik
    QVBoxLayout* t2;
    QHBoxLayout* p2;
    krokiKreatora_->addWidget(
        krok(2, "Wybierz głośnik", "Naciśnij Test. Powinieneś usłyszeć krótkie zdanie.", t2, p2));
    kreGlosnik_ = new QComboBox;
    kreGlosnik_->setAccessibleName("Głośnik");
    t2->addWidget(kreGlosnik_);
    auto* testG = przycisk("Test — odegraj zdanie", "speaker");
    t2->addWidget(testG);
    auto* wstecz = przycisk("Wstecz", "back");
    wstecz->setMinimumHeight(64);
    kreGotowe_ = przycisk("Gotowe", "ok", "blue");
    kreGotowe_->setMinimumHeight(64);
    p2->addWidget(wstecz, 1);
    p2->addWidget(kreGotowe_, 2);
    connect(testG, &QPushButton::clicked, this, [this] { testGlosnika(kreGlosnik_); });
    connect(kreGlosnik_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, testG](int i) {
        kreGotowe_->setEnabled(i >= 0);
        testG->setEnabled(i >= 0);
    });
    connect(wstecz, &QPushButton::clicked, this, [this] {
        silnik_.zatrzymajMowe();
        krokiKreatora_->setCurrentIndex(0);
    });
    connect(kreGotowe_, &QPushButton::clicked, this, [this] {
        silnik_.zatrzymajMowe();
        QString blad;
        if (!silnik_.wybierz(false, kreGlosnik_->currentData().toString(), &blad)) {
            QMessageBox::warning(this, "Głośnik", blad);
            return;
        }
        odswiezUrzadzenia();
        pokazEkran(GLOWNY);
    });
    return krokiKreatora_;
}

QWidget* Okno::zbudujPasekStanu() {
    pasekStanu_ = new QFrame;
    pasekStanu_->setObjectName("pasekStanu");
    auto* h = new QHBoxLayout(pasekStanu_);
    h->setContentsMargins(16, 0, 12, 0);
    h->setSpacing(10);
    kropka_ = new Kropka;
    stanOpis_ = new QLabel("Sprawdzam serwer mowy…");
    stanOpis_->setObjectName("stanOpis");
    h->addWidget(kropka_);
    h->addWidget(stanOpis_);
    h->addStretch(1);

    auto* urz = new QPushButton;
    przyciskUrzadzen_ = urz;
    urz->setObjectName("urzadzenia");
    urz->setAccessibleName("Mikrofon i głośnik — otwórz ustawienia");
    urz->setCursor(Qt::PointingHandCursor);
    auto* hu = new QHBoxLayout(urz);
    hu->setContentsMargins(10, 0, 10, 0);
    hu->setSpacing(6);
    ikonaMikrofonu_ = obrazek();
    nazwaMikrofonu_ = new QLabel;
    nazwaMikrofonu_->setAttribute(Qt::WA_TransparentForMouseEvents);
    ikonaGlosnika_ = obrazek();
    nazwaGlosnika_ = new QLabel;
    nazwaGlosnika_->setAttribute(Qt::WA_TransparentForMouseEvents);
    ikonaUstawien_ = obrazek();
    hu->addWidget(ikonaMikrofonu_);
    hu->addWidget(nazwaMikrofonu_);
    hu->addSpacing(8);
    hu->addWidget(ikonaGlosnika_);
    hu->addWidget(nazwaGlosnika_);
    hu->addSpacing(8);
    hu->addWidget(ikonaUstawien_);
    connect(urz, &QPushButton::clicked, this, [this] { pokazEkran(USTAWIENIA); });
    h->addWidget(urz);
    return pasekStanu_;
}

// ============================================================================ nawigacja i odświeżanie

void Okno::pokazStrone(Strona s) {
    strony_->setCurrentIndex(s);
    zakladki_[s]->setChecked(true);
    if (s == POWIEDZ) poleMowy_->setFocus();
    if (s == SLUCHAJ && mikrofon_->isEnabled()) mikrofon_->setFocus();
    if (s == KOMUNIKATY) ulozKarty();
}

void Okno::pokazEkran(Ekran e) {
    if (e == USTAWIENIA) {
        powrot_ = ekrany_->currentIndex() == KREATOR ? GLOWNY : Ekran(ekrany_->currentIndex());
        if (powrot_ == USTAWIENIA) powrot_ = GLOWNY;
        auto ust = silnik_.ustawienia();
        QString tts = ust.value("tts");
        adres_->setText(tts.left(tts.lastIndexOf(':')));
        odswiezRozpoznawanie();
    }
    if (e == KREATOR) {
        wypelnij(kreMikrofon_, silnik_.urzadzenia(true), true, true);
        wypelnij(kreGlosnik_, silnik_.urzadzenia(false), false, true);
        krokiKreatora_->setCurrentIndex(0);
    }
    odswiezUrzadzenia();
    ekrany_->setCurrentIndex(e);
    if (e == KREATOR) kreMikrofon_->setFocus();
    if (e == GLOWNY) pokazStrone(Strona(strony_->currentIndex()));
}

void Okno::odswiezIkony() {
    // Ikony są barwione na kolor tekstu przycisku - po zmianie motywu lub stanu trzeba je przemalować.
    for (int i = 0; i < 3; ++i) {
        QIcon ik;
        ik.addPixmap(motyw::pixmapa(IKONY_ZAKLADEK[i], "text", 26), QIcon::Normal, QIcon::Off);
        ik.addPixmap(motyw::pixmapa(IKONY_ZAKLADEK[i], NAPISY_NA_KOLORZE[i], 26), QIcon::Normal, QIcon::On);
        zakladki_[i]->setIcon(ik);
    }
    for (QLabel* z : znaczki_) z->setPixmap(motyw::pixmapa("error", "danger", 20));

    bool mowi = przyciskPowiedz_->property("mowi").toBool();
    ikonaPowiedz_->setPixmap(motyw::pixmapa(mowi ? "stop" : "say", mowi ? "onRed" : "onBlue", 32));

    // zwykłe przyciski: ikona w kolorze tekstu przycisku
    for (QPushButton* p : findChildren<QPushButton*>()) {
        QString ikona = p->property("ikona").toString();
        if (ikona.isEmpty()) continue;
        QString kolor = p->property("kolor").toString();
        QString napis = kolor == "blue" ? "onBlue" : kolor == "amber" ? "onAmber" : kolor == "violet" ? "onViolet"
                      : kolor == "red" ? "onRed" : kolor == "danger" ? "danger" : "text";
        QIcon ik;
        ik.addPixmap(motyw::pixmapa(ikona, napis, 24), QIcon::Normal);
        ik.addPixmap(motyw::pixmapa(ikona, "disabledText", 24), QIcon::Disabled);
        p->setIcon(ik);
    }
    for (QLabel* l : findChildren<QLabel*>("ikonaPanelu"))
        l->setPixmap(motyw::pixmapa(l->property("ikona").toString(), "text", 24));
    for (QLabel* l : findChildren<QLabel*>("ikonaPusto")) l->setPixmap(motyw::pixmapa("list", "violet", 64));
    for (Baner* b : findChildren<Baner*>()) b->odswiezIkone();

    zaawansowane_->setIcon(motyw::ikona(zaawansowane_->isChecked() ? "back" : "chevron", "text", 22));
    if (zaawansowane_->isChecked()) {
        // strzałka w górę = "chevron" obrócony; "back" byłby w lewo - rysujemy obrót sami
        QTransform t;
        t.rotate(180);
        zaawansowane_->setIcon(QIcon(motyw::pixmapa("chevron", "text", 22).transformed(t)));
    }
    if (auto* lupa = szukaj_->findChild<QLabel*>("lupa")) {
        lupa->setPixmap(motyw::pixmapa("search", "faint", 22));
        lupa->setFixedHeight(szukaj_->height());
    }

    QString kluczMik = nazwaMikrofonu_->property("brak").toBool() ? "danger" : "text";
    ikonaMikrofonu_->setPixmap(motyw::pixmapa("mic", kluczMik, 18));
    ikonaGlosnika_->setPixmap(motyw::pixmapa("speaker", nazwaGlosnika_->property("brak").toBool() ? "danger" : "text", 18));
    ikonaUstawien_->setPixmap(motyw::pixmapa("settings", "text", 18));
    mikrofon_->update();
}

void Okno::odswiezKomunikaty() {
    komunikaty_ = silnik_.lista();
    ulozKarty();
}

void Okno::ulozKarty() {
    // czyścimy siatkę
    while (QLayoutItem* it = siatka_->takeAt(0)) {
        if (QWidget* w = it->widget()) {
            w->hide();  // deleteLater działa dopiero w pętli zdarzeń - do tego czasu karta nie może prześwitywać
            w->deleteLater();
        }
        delete it;
    }
    for (int c = 0; c < 3; ++c) siatka_->setColumnStretch(c, 0);
    listaLubPusto_->setCurrentIndex(komunikaty_.isEmpty() ? 1 : 0);
    szukaj_->setVisible(!komunikaty_.isEmpty());

    QString filtr = szukaj_->text().trimmed();
    int n = 0;
    for (const Komunikat& k : komunikaty_) {
        if (!filtr.isEmpty() && !k.nazwa.contains(filtr, Qt::CaseInsensitive) &&
            !k.tekst.contains(filtr, Qt::CaseInsensitive))
            continue;
        siatka_->addWidget(zbudujKarte(k), n / kolumny_, n % kolumny_);
        ++n;
    }
    for (int c = 0; c < kolumny_; ++c) siatka_->setColumnStretch(c, 1);
    brakWynikow_->setVisible(n == 0 && !komunikaty_.isEmpty());
    brakWynikow_->setText(QString("Nie ma komunikatu pasującego do „%1”.").arg(filtr));
    odswiezIkony();
}

void Okno::odswiezPrzyciskiOdtwarzania() {
    for (QPushButton* p : karty_->findChildren<QPushButton*>()) {
        QString nazwa = p->property("komunikat").toString();
        if (nazwa.isEmpty()) continue;
        bool gra = !odtwarzany_.isEmpty() && odtwarzany_ == nazwa;
        p->setText(gra ? "Zatrzymaj" : "Odtwórz");
        p->setProperty("ikona", gra ? "stop" : "play");
        p->setProperty("kolor", gra ? "red" : "violet");
        p->setAccessibleName((gra ? "Zatrzymaj " : "Odtwórz ") + nazwa);
        motyw::odswiez(p);
    }
    odswiezIkony();
}

void Okno::wypelnij(QComboBox* lista, const QVector<Urzadzenie>& urzadzenia, bool mikrofony, bool zPustym) {
    lista->blockSignals(true);
    lista->clear();
    int wybrany = -1;
    for (const Urzadzenie& u : urzadzenia) {
        lista->addItem(nazwaNaLiscie(u), u.nazwa);
        if (u.wybrany) wybrany = lista->count() - 1;
    }
    lista->setPlaceholderText(mikrofony ? "Wybierz mikrofon…" : "Wybierz głośnik…");
    lista->setCurrentIndex(zPustym && wybrany < 0 ? -1 : wybrany);
    lista->blockSignals(false);
    emit lista->currentIndexChanged(lista->currentIndex());
}

void Okno::odswiezUrzadzenia(bool zListami) {
    // Jedno pobranie list na odświeżenie - każde to zapytanie do PipeWire.
    auto ust = silnik_.ustawienia();
    auto mikrofony = silnik_.urzadzenia(true);
    auto glosniki = silnik_.urzadzenia(false);
    // 0 = wybrany i podłączony, 1 = nie wybrano, 2 = wybrany, ale niedostępny (odłączony)
    auto stan = [](const QVector<Urzadzenie>& lista, const QString& nazwa, QString& tekst) {
        if (nazwa.isEmpty()) {
            tekst = "Nie wybrano";
            return 1;
        }
        for (const Urzadzenie& u : lista)
            if (u.nazwa == nazwa) {
                tekst = krotkaNazwa(nazwa, u.opis);
                return 0;
            }
        tekst = "Niedostępny";
        return 2;
    };
    QString tMik, tGl;
    int sMik = stan(mikrofony, ust.value("mikrofon"), tMik);
    int sGl = stan(glosniki, ust.value("glosnik"), tGl);
    // długie nazwy (np. "G533 Wireless Headset Dongle Analog Stereo") nie mogą rozpychać paska stanu
    auto ustawNazwe = [](QLabel* l, const QString& t) {
        QFont f = l->font();
        f.setPixelSize(16);
        f.setBold(true);
        l->setText(QFontMetrics(f).elidedText(t, Qt::ElideRight, 115));
        l->setToolTip(t);
    };
    ustawNazwe(nazwaMikrofonu_, tMik);
    nazwaMikrofonu_->setProperty("brak", sMik != 0);
    ustawNazwe(nazwaGlosnika_, tGl);
    nazwaGlosnika_->setProperty("brak", sGl != 0);
    motyw::odswiez(pasekStanu_);
    przyciskUrzadzen_->setMinimumSize(przyciskUrzadzen_->layout()->sizeHint().expandedTo(QSize(0, 40)));

    if (zListami) {
        wypelnij(ustMikrofon_, mikrofony, true, false);
        wypelnij(ustGlosnik_, glosniki, false, false);
    }

    // Słuchaj bez działającego mikrofonu: jasny komunikat i prosta droga do ustawień
    bool brakMik = sMik != 0;
    if (sluchanie_ != Sluchanie::Nagrywa) mikrofon_->setEnabled(!brakMik && sluchanie_ != Sluchanie::Pracuje);
    podpisSluchaj_->setProperty("rola", brakMik ? "wylaczony" : "podpisMikrofonu");
    motyw::odswiez(podpisSluchaj_);
    wybierzMikrofon_->setVisible(brakMik);
    if (brakMik) {
        if (sMik == 1) banerSluchaj_->pokaz(Baner::Blad, "Nie wybrano mikrofonu", "Wybierz mikrofon w ustawieniach.");
        else banerSluchaj_->pokaz(Baner::Blad, "Wybrany mikrofon jest niedostępny",
                                  "Podłącz go albo wybierz inny w ustawieniach.");
        banerUrzadzenia_ = true;
    } else if (banerUrzadzenia_) {
        banerSluchaj_->hide();
        banerUrzadzenia_ = false;
    }
    odswiezIkony();
}

void Okno::ustawStanSerwera(bool dziala) {
    serwerDziala_ = dziala;
    kropka_->ustawKolor(dziala ? "ok" : "danger");
    stanOpis_->setText(dziala ? "Mowa działa" : "Brak serwera mowy");
    pasekStanu_->setProperty("blad", !dziala);
    motyw::odswiez(pasekStanu_);
    for (QLabel* z : znaczki_) z->setVisible(!dziala);
    if (dziala) banerKomunikaty_->hide();
    else banerKomunikaty_->pokaz(Baner::Informacja, "Zapisane komunikaty nadal działają");
    if (!silnik_.mowi() && !silnik_.nagrywa()) ustawZasobnik(dziala ? "idle" : "error");
    odswiezIkony();
}

void Okno::ustawZasobnik(const QString& stan) {
    if (!zasobnik_) return;
    zasobnik_->setIcon(motyw::ikonaZasobnika(stan));
    zasobnik_->setToolTip(stan == "speaking" ? "Komunikator — mówię" : stan == "recording" ? "Komunikator — nagrywam"
                          : stan == "error"  ? "Komunikator — brak serwera mowy" : "Komunikator");
}

// ============================================================================ Powiedz

void Okno::mowLubZatrzymaj() {
    if (silnik_.mowi()) {
        silnik_.zatrzymajMowe();
        return;
    }
    QString tekst = poleMowy_->toPlainText().trimmed();
    if (tekst.isEmpty()) {
        banerPowiedz_->pokaz(Baner::Ostrzezenie, "Najpierw wpisz, co mam powiedzieć");
        poleMowy_->setFocus();
        return;
    }
    banerPowiedz_->hide();
    zrodloMowy_ = ZrodloMowy::Powiedz;
    silnik_.powiedz(tekst);
}

void Okno::zapiszJakoKomunikat() {
    QString tekst = poleMowy_->toPlainText().trimmed();
    if (tekst.isEmpty()) {
        banerPowiedz_->pokaz(Baner::Ostrzezenie, "Najpierw wpisz zdanie", "Potem zapiszesz je jako komunikat.");
        poleMowy_->setFocus();
        return;
    }
    bool ok = false;
    QString nazwa = QInputDialog::getText(this, "Zapisz jako komunikat", "Krótka nazwa, np. Wyprowadź psa:",
                                          QLineEdit::Normal, QString(), &ok)
                        .simplified();
    if (!ok || nazwa.isEmpty()) return;
    for (const Komunikat& k : komunikaty_) {
        if (k.nazwa == nazwa &&
            QMessageBox::question(this, "Zapisz jako komunikat",
                                  QString("Komunikat „%1” już jest. Zastąpić go?").arg(nazwa)) != QMessageBox::Yes)
            return;
    }
    przyciskZapisz_->setEnabled(false);
    banerPowiedz_->pokaz(Baner::Informacja, "Zapisuję…", "Nagrywam komunikat, to chwila.");
    silnik_.zapisz(nazwa, tekst.simplified());
}

// ============================================================================ Komunikaty

void Okno::odtworzKomunikat(const QString& nazwa) {
    if (silnik_.mowi() && odtwarzany_ == nazwa) {
        silnik_.zatrzymajMowe();
        return;
    }
    banerKomunikaty_->setVisible(!serwerDziala_);
    zrodloMowy_ = ZrodloMowy::Komunikat;
    silnik_.odtworz(nazwa);
    odtwarzany_ = nazwa;
    odswiezPrzyciskiOdtwarzania();
}

void Okno::usunKomunikat(const QString& nazwa) {
    QMessageBox pytanie(QMessageBox::Question, "Usuń komunikat", QString("Usunąć komunikat „%1”?").arg(nazwa),
                        QMessageBox::NoButton, this);
    auto* tak = pytanie.addButton("Usuń", QMessageBox::DestructiveRole);
    pytanie.addButton("Anuluj", QMessageBox::RejectRole);
    pytanie.exec();
    if (pytanie.clickedButton() != tak) return;
    QString blad;
    if (!silnik_.usun(nazwa, &blad)) QMessageBox::warning(this, "Usuń komunikat", blad);
    odswiezKomunikaty();
}

// ============================================================================ Słuchaj

void Okno::sluchajLubZatrzymaj() {
    if (sluchanie_ == Sluchanie::Nagrywa) {
        silnik_.zatrzymajNagranie();
        return;
    }
    if (sluchanie_ == Sluchanie::Pracuje || !mikrofon_->isEnabled()) return;
    silnik_.zatrzymajNagranie();  // ewentualny test mikrofonu
    int sekundy = 10;
    for (int i = 0; i < 3; ++i)
        if (segmenty_[i]->isChecked()) sekundy = DLUGOSCI[i];
    plikNagrania_ = QDir::temp().filePath(QString("komunikator-okno-%1.wav").arg(QCoreApplication::applicationPid()));
    banerSluchaj_->hide();
    przyciskiCiszy_->hide();
    pozostalo_ = sekundy;
    licznik_->setText(QString("%1 s").arg(sekundy));
    poziomNagrania_->ustaw(0);
    aktywnyPoziom_ = poziomNagrania_;
    ustawSluchanie(Sluchanie::Nagrywa);
    ustawZasobnik("recording");
    odliczanie_.start();
    silnik_.nagraj(sekundy, plikNagrania_);
}

void Okno::ustawSluchanie(Sluchanie s) {
    sluchanie_ = s;
    mikrofon_->ustawNagrywanie(s == Sluchanie::Nagrywa);
    mikrofon_->setAccessibleName(s == Sluchanie::Nagrywa ? "Zatrzymaj nagrywanie" : "Słuchaj");
    mikrofon_->setEnabled(s != Sluchanie::Pracuje && !nazwaMikrofonu_->property("brak").toBool());
    for (QPushButton* seg : segmenty_) seg->setEnabled(s == Sluchanie::Gotowe);
    podMikrofonem_->setCurrentIndex(s == Sluchanie::Nagrywa ? 1 : 0);
    podpisSluchaj_->setProperty("rola", s == Sluchanie::Pracuje ? "wylaczony" : "podpisMikrofonu");
    klawiszSluchaj_->setVisible(s != Sluchanie::Pracuje);
    motyw::odswiez(podpisSluchaj_);
    panelPracy_->setVisible(s == Sluchanie::Pracuje);
    if (s == Sluchanie::Pracuje) pasekPracy_->start();
    else pasekPracy_->stop();
    kopiuj_->setEnabled(s == Sluchanie::Gotowe && !rozpoznany_->toPlainText().isEmpty());
    if (s == Sluchanie::Nagrywa) rozpoznany_->clear();
    if (s == Sluchanie::Gotowe && strony_->currentIndex() == SLUCHAJ) mikrofon_->setFocus();
}

// ============================================================================ testy urządzeń

void Okno::testMikrofonu(QComboBox* lista, QPushButton* przycisk, PoziomDzwieku* poziom) {
    if (testowanyPrzycisk_ == przycisk) {  // drugi klik = stop
        silnik_.zatrzymajNagranie();
        return;
    }
    if (lista->currentIndex() < 0 || sluchanie_ != Sluchanie::Gotowe) return;
    testowanyPrzycisk_ = przycisk;
    aktywnyPoziom_ = poziom;
    przycisk->setText("Stop");
    ustawZasobnik("recording");
    // 30 s nasłuchu albo do drugiego kliknięcia; nagranie testowe nie jest nigdzie używane
    silnik_.nagraj(30, QDir::temp().filePath(QString("komunikator-test-%1.wav").arg(QCoreApplication::applicationPid())),
                   lista->currentData().toString());
}

void Okno::testGlosnika(QComboBox* lista) {
    if (lista->currentIndex() < 0) return;
    zrodloMowy_ = ZrodloMowy::Test;
    silnik_.powiedz("To jest test głośnika. Słychać mnie dobrze?", lista->currentData().toString());
}

void Okno::zatrzymajWszystko() {
    silnik_.zatrzymajMowe();
    silnik_.zatrzymajNagranie();
}

void Okno::zmienAdresSerwera() {
    QString host = adres_->text().trimmed();
    host.remove(QRegularExpression("^[a-z]+://"));  // ktoś wkleił http://...
    host.remove(QRegularExpression("[:/].*$"));
    if (host.isEmpty()) return;
    auto ust = silnik_.ustawienia();
    auto port = [](const QString& adres, const QString& domyslny) {
        int d = adres.lastIndexOf(':');
        return d > 0 ? adres.mid(d + 1) : domyslny;
    };
    QString tts = host + ":" + port(ust.value("tts"), "10200");
    QString stt = host + ":" + port(ust.value("stt"), "10300");
    if (tts == ust.value("tts") && stt == ust.value("stt")) return;
    QString blad;
    if (!silnik_.ustaw("tts", tts, &blad) || !silnik_.ustaw("stt", stt, &blad)) {
        QMessageBox::warning(this, "Adres serwera mowy", blad);
        return;
    }
    adres_->setText(host);
    stanOpis_->setText("Sprawdzam serwer mowy…");
    silnik_.sprawdz();
}

void Okno::wybierzRozpoznawanie(bool gemini) {
    QString blad;
    if (!silnik_.ustaw("rozpoznawanie", gemini ? "gemini" : "whisper", &blad))
        QMessageBox::warning(this, "Rozpoznawanie mowy", blad);
    odswiezRozpoznawanie();
    if (gemini && kluczTresc_->isVisible() && usunKlucz_->isHidden()) klucz_->setFocus();
}

void Okno::odswiezRozpoznawanie() {
    auto ust = silnik_.ustawienia();
    bool gemini = ust.value("rozpoznawanie") == "gemini";
    bool jestKlucz = ust.value("gemini_klucz") == "zapisany";
    (gemini ? rozGemini_ : rozLokalnie_)->setChecked(true);
    kluczTresc_->setVisible(gemini || jestKlucz);
    if (jestKlucz)
        stanKlucza_->setText("Klucz Gemini jest zapisany.");
    else
        stanKlucza_->setText(QString("Brak klucza — do czasu jego zapisania komunikator rozpoznaje lokalnie. "
                                     "Klucz za darmo:<br><a style=\"color:%1\" href=\"https://aistudio.google.com/apikey\">"
                                     "<span style=\"white-space:nowrap\">aistudio.google.com/apikey</span></a>")
                                 .arg(motyw::kolor("blue").name()));
    klucz_->setPlaceholderText(jestKlucz ? "Wklej nowy klucz, żeby zmienić" : "Wklej klucz Gemini");
    usunKlucz_->setVisible(jestKlucz);
}

// ============================================================================ zdarzenia okna

void Okno::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    int kolumny = width() >= 900 ? 3 : 1;
    if (kolumny != kolumny_) {
        kolumny_ = kolumny;
        ulozKarty();
    }
    mikrofon_->ustawSrednice(width() >= 900 && height() >= 800 ? 150 : 132);
}

void Okno::closeEvent(QCloseEvent* e) {
    // Z zasobnikiem zamknięcie okna = praca w tle; "Zakończ" jest w menu ikony.
    if (zasobnik_ && zasobnik_->isVisible()) {
        hide();
        if (!podpowiedzZasobnika_) {
            zasobnik_->showMessage("Komunikator działa w tle", "Kliknij ikonę, żeby wrócić. Zakończ: prawy przycisk.",
                                   QSystemTrayIcon::Information, 5000);
            podpowiedzZasobnika_ = true;
        }
        e->ignore();
        return;
    }
    zatrzymajWszystko();
    e->accept();
}

bool Okno::eventFilter(QObject* o, QEvent* e) {
    if (e->type() == QEvent::Resize) {
        for (int i = 0; i < 2; ++i)
            if (o == zakladki_[i]) znaczki_[i]->setGeometry(zakladki_[i]->width() - 30, 8, 22, 22);
    }
    return QMainWindow::eventFilter(o, e);
}
