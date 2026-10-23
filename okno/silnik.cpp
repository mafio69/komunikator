#include "silnik.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

namespace {
constexpr int ZATRZYMANO = 130;  // kod wyjścia silnika po przerwaniu
}

Silnik::Silnik(QObject* rodzic) : QObject(rodzic) {
    // Kolejność: KOMUNIKATOR_SILNIK, program obok okna, program z PATH.
    QString zSrodowiska = qEnvironmentVariable("KOMUNIKATOR_SILNIK");
    QString obok = QCoreApplication::applicationDirPath() + "/komunikator";
    if (!zSrodowiska.isEmpty()) program_ = zSrodowiska;
    else if (QFileInfo(obok).isExecutable()) program_ = obok;
    else program_ = "komunikator";
}

Silnik::~Silnik() {
    // SIGTERM pozwala silnikowi wyłączyć aplay/arecord; samo zniszczenie QProcess to SIGKILL i osierocony odtwarzacz
    for (QProcess* p : findChildren<QProcess*>()) {
        if (p->state() == QProcess::NotRunning) continue;
        p->disconnect(this);
        p->terminate();
        if (!p->waitForFinished(2000)) p->kill();
    }
}

QProcess* Silnik::uruchom(const QStringList& args, const QMap<QString, QString>& srodowisko,
                          const QByteArray& wejscie) {
    auto* p = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto it = srodowisko.begin(); it != srodowisko.end(); ++it) env.insert(it.key(), it.value());
    p->setProcessEnvironment(env);
    p->start(program_, args);
    if (!wejscie.isEmpty()) p->write(wejscie);
    p->closeWriteChannel();  // silnik nigdy nie czeka na klawiaturę - okno nie jest terminalem
    return p;
}

// Ostatnia sensowna linia z błędów silnika, bez przedrostka "komunikator:", z wielkiej litery.
QString Silnik::bladZ(QProcess* p) {
    if (p->error() == QProcess::FailedToStart)
        return "Nie znalazłem programu komunikator. Zainstaluj go: make install.";
    QStringList linie = QString::fromUtf8(p->readAllStandardError()).split('\n', Qt::SkipEmptyParts);
    // Bez numerów kodów i tekstów technicznych - silnik zapisuje szczegóły w swoim dzienniku.
    QString l = linie.isEmpty() ? QString("Coś poszło nie tak. Spróbuj jeszcze raz.") : linie.last().trimmed();
    l.remove(QRegularExpression("^komunikator:\\s*"));
    if (!l.isEmpty()) l[0] = l[0].toUpper();
    return l;
}

bool Silnik::wykonaj(const QStringList& args, QString* wyjscie, QString* blad, const QByteArray& wejscie) {
    QProcess* p = uruchom(args, {}, wejscie);
    bool ok = p->waitForFinished(10000) && p->exitStatus() == QProcess::NormalExit && p->exitCode() == 0;
    if (wyjscie) *wyjscie = QString::fromUtf8(p->readAllStandardOutput());
    if (!ok && blad) *blad = bladZ(p);
    p->deleteLater();
    return ok;
}

QVector<Komunikat> Silnik::lista() {
    QString w;
    QVector<Komunikat> wynik;
    if (!wykonaj({"lista"}, &w, nullptr)) return wynik;
    for (const QString& l : w.split('\n', Qt::SkipEmptyParts)) {
        int tab = l.indexOf('\t');
        if (tab > 0) wynik.push_back({l.left(tab), l.mid(tab + 1)});
    }
    return wynik;
}

QVector<Urzadzenie> Silnik::urzadzenia(bool mikrofony) {
    QString w;
    QVector<Urzadzenie> wynik;
    wykonaj({mikrofony ? "mikrofony" : "glosniki"}, &w, nullptr);
    // "* 2  plughw:0,0  ALC3234 Analog (HDA Intel PCH)"
    // "* -  ..." to dopisek o wybranym, ale odłączonym urządzeniu - nie jest pozycją listy
    static const QRegularExpression wzor("^([* ]) (\\d+)  (\\S+)  (.*)$");
    for (const QString& l : w.split('\n', Qt::SkipEmptyParts)) {
        auto m = wzor.match(l);
        if (m.hasMatch()) wynik.push_back({m.captured(3), m.captured(4), m.captured(1) == "*"});
    }
    return wynik;
}

QMap<QString, QString> Silnik::ustawienia() {
    QString w;
    QMap<QString, QString> wynik;
    wykonaj({"ustawienia"}, &w, nullptr);
    for (const QString& l : w.split('\n', Qt::SkipEmptyParts)) {
        int r = l.indexOf('=');
        if (r > 0) wynik[l.left(r)] = l.mid(r + 1);
    }
    return wynik;
}

bool Silnik::wybierz(bool mikrofon, const QString& nazwa, QString* blad) {
    return wykonaj({mikrofon ? "mikrofon" : "glosnik", nazwa}, nullptr, blad);
}

bool Silnik::ustaw(const QString& klucz, const QString& wartosc, QString* blad) {
    return wykonaj({"ustaw", klucz, wartosc}, nullptr, blad);
}

bool Silnik::usun(const QString& nazwa, QString* blad) { return wykonaj({"usun", nazwa}, nullptr, blad); }

bool Silnik::zapiszKlucz(const QString& klucz, QString* blad) {
    return wykonaj({"klucz", "gemini"}, nullptr, blad, klucz.trimmed().toUtf8() + "\n");
}

bool Silnik::usunKlucz(QString* blad) { return wykonaj({"klucz", "--usun"}, nullptr, blad); }

void Silnik::sprawdz() {
    if (sprawdzanie_) return;  // poprzednie sprawdzanie jeszcze trwa
    sprawdzanie_ = uruchom({"sprawdz"});
    connect(sprawdzanie_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int kod, auto) {
        sprawdzanie_->deleteLater();
        sprawdzanie_ = nullptr;
        emit stanSerwera(kod == 0);
    });
    connect(sprawdzanie_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || !sprawdzanie_) return;
        sprawdzanie_->deleteLater();
        sprawdzanie_ = nullptr;
        emit stanSerwera(false);
    });
}

void Silnik::zapisz(const QString& nazwa, const QString& tekst) {
    QProcess* p = uruchom({"zapisz", nazwa, tekst});
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, p](int kod, auto) {
        emit zapisano(kod == 0, kod == 0 ? QString() : bladZ(p));
        p->deleteLater();
    });
}

void Silnik::mow(const QStringList& args, const QString& glosnik) {
    zatrzymajMowe();
    QMap<QString, QString> env;
    if (!glosnik.isEmpty()) env["KOMUNIKATOR_GLOSNIK"] = glosnik;
    mowaZatrzymana_ = false;
    mowa_ = uruchom(args, env);
    QProcess* p = mowa_;
    emit mowaStart();
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, p](int kod, auto) {
        bool zatrzymana = mowaZatrzymana_ || kod == ZATRZYMANO;
        QString blad = (kod == 0 || zatrzymana) ? QString() : bladZ(p);
        if (mowa_ == p) mowa_ = nullptr;
        p->deleteLater();
        emit mowaKoniec(kod == 0, zatrzymana, blad);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        if (mowa_ == p) mowa_ = nullptr;
        p->deleteLater();
        emit mowaKoniec(false, false, bladZ(p));
    });
}

void Silnik::powiedz(const QString& tekst, const QString& glosnik) {
    // spacje i nowe linie zamieniamy w jeden ciąg - to i tak jedno zdanie do powiedzenia
    mow({"powiedz", "--", tekst.simplified()}, glosnik);
}

void Silnik::odtworz(const QString& nazwa) { mow({"odtworz", nazwa}, {}); }

void Silnik::zatrzymajMowe() {
    if (!mowa_) return;
    mowaZatrzymana_ = true;
    mowa_->terminate();  // SIGTERM: silnik zatrzymuje też odtwarzacz
}

void Silnik::nagraj(int sekundy, const QString& plik, const QString& mikrofon) {
    zatrzymajNagranie();
    QMap<QString, QString> env;
    if (!mikrofon.isEmpty()) env["KOMUNIKATOR_MIKROFON"] = mikrofon;
    cisza_ = false;
    bufor_.clear();
    nagranie_ = uruchom({"nagraj", QString::number(sekundy), "--poziomy", "-o", plik}, env);
    QProcess* p = nagranie_;
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p] {
        bufor_ += p->readAllStandardOutput();
        int nl;
        while ((nl = bufor_.indexOf('\n')) >= 0) {
            QString l = QString::fromUtf8(bufor_.left(nl)).trimmed();
            bufor_.remove(0, nl + 1);
            if (l.startsWith("poziom ")) emit poziom(l.mid(7).toDouble());
            else if (l == "cisza") cisza_ = true;
        }
    });
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, p](int kod, auto) {
        QString blad = kod == 0 ? QString() : bladZ(p);
        if (nagranie_ == p) nagranie_ = nullptr;
        p->deleteLater();
        emit nagranieKoniec(kod == 0, cisza_, blad);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        if (nagranie_ == p) nagranie_ = nullptr;
        p->deleteLater();
        emit nagranieKoniec(false, false, bladZ(p));
    });
}

void Silnik::zatrzymajNagranie() {
    if (nagranie_) nagranie_->terminate();  // silnik kończy arecord i zapisuje nagranie
}

void Silnik::rozpoznaj(const QString& plik) {
    QProcess* p = uruchom({"rozpoznaj", plik});
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, p](int kod, auto) {
        if (kod != 0) {
            emit rozpoznano(false, bladZ(p), {});
        } else {
            // silnik pisze "uwaga: ..." na stderr, gdy Gemini zawiodło i rozpoznał lokalnie
            QString uwaga;
            for (const QString& l : QString::fromUtf8(p->readAllStandardError()).split('\n'))
                // sygnał od silnika w obu językach: "uwaga: Gemini…" / "warning: Gemini…"
                if (l.startsWith("uwaga: Gemini") || l.startsWith("warning: Gemini")) uwaga = l.section(':', 1).trimmed();
            emit rozpoznano(true, QString::fromUtf8(p->readAllStandardOutput()).trimmed(), uwaga);
        }
        p->deleteLater();
    });
}
