# Installing komunikator

*Polish version: [INSTALACJA.md](INSTALACJA.md)*

`komunikator` is a single program. Building it needs only a C++ compiler (g++ 9 or newer) and
`make`. It needs no libraries, no Home Assistant, no Python and no internet connection.

It records and plays sound with `arecord` and `aplay` (the `alsa-utils` package, present on every
Linux system with audio). To **work**, it needs two speech services that speak the Wyoming protocol:

| Service | Purpose | Default address |
|---|---|---|
| Piper | text → speech | `127.0.0.1:10200` |
| Whisper (faster-whisper) | speech → text | `127.0.0.1:10300` |

The quickest way to get both is Docker:

```bash
docker run -d --name wyoming-piper   -p 10200:10200 rhasspy/wyoming-piper   --voice pl_PL-gosia-medium
docker run -d --name wyoming-whisper -p 10300:10300 rhasspy/wyoming-whisper --model small --language pl
```

Saved messages play **without** these services, because their audio is stored on disk.

## The quick way: the installer

One command does everything: installs missing packages (it asks for your sudo password), builds
the engine and the window, installs both, asks for the address of the speech server
(Enter = `127.0.0.1`) and checks the connection.

```bash
bash ~/komunikator/instaluj.sh
```

Options: `--serwer ADDRESS` (no question asked), `--bez-okna` (engine only, without Qt),
`--odinstaluj` (uninstall). The same steps, one by one, follow below in case the installer stops.

## 1. Build

```bash
cd ~/komunikator && make
```

Result: the `komunikator` binary in the same directory.

## 2. Test

```bash
cd ~/komunikator && make test
```

Both suites should finish with zero failures:

```text
== testy jednostkowe: dobrze 23, źle 0 ==
...
== wynik: dobrze 35, źle 0 ==
```

Nothing is played aloud during tests. When the speech services are not running, the tests that
need them are skipped rather than counted as failures.

For the full quality check — warnings as errors and the whole suite under AddressSanitizer and
UndefinedBehaviorSanitizer:

```bash
cd ~/komunikator && make sprawdz
```

## 3. Install (so it works from anywhere)

```bash
cd ~/komunikator && make install
```

This copies the program to `~/.local/bin/komunikator`. On most distributions that directory is
already on your `PATH`, so plain `komunikator` works right away. No sudo needed.

Check it:

```bash
komunikator sprawdz
```

Both lines should start with `OK`.

## 4. Choose the microphone and the speaker

The program never picks them on its own. The first time you record or speak, it shows a list and
asks for a number. You can also choose right away:

```bash
komunikator mikrofony       # list microphones
komunikator mikrofon 2      # choose microphone number 2
komunikator glosniki        # list speakers
komunikator glosnik 2       # choose speaker number 2
```

The `default` entry means the device selected in your system's sound settings.
A USB microphone or speaker plugged in later appears on the list by itself.

## 5. Settings (optional)

The settings file is created automatically when you choose a microphone or a speaker. To change
anything else, edit `~/.config/komunikator/ustawienia`:

```ini
# addresses of the speech services (host:port)
tts=127.0.0.1:10200
stt=127.0.0.1:10300
# Piper voice; empty = the service's default voice
glos=
# recognition language
jezyk=pl
# recognition: whisper (local) or gemini (cloud; falls back to whisper on any failure)
rozpoznawanie=whisper
# Gemini model; the default alias always points to the newest Flash model
gemini_model=gemini-flash-latest
# how to play sound; empty = only print the path of the audio file
odtwarzacz=aplay -q
# chosen devices (set with: komunikator mikrofon NUMBER / komunikator glosnik NUMBER)
mikrofon=plughw:0,0
glosnik=plughw:0,0
# where saved messages live (default ~/.local/share/komunikator)
# dane=~/komunikaty
```

Every setting can also be given as an environment variable, which takes precedence over the file:
`KOMUNIKATOR_TTS`, `KOMUNIKATOR_STT`, `KOMUNIKATOR_GLOS`, `KOMUNIKATOR_JEZYK`,
`KOMUNIKATOR_ODTWARZACZ`, `KOMUNIKATOR_MIKROFON`, `KOMUNIKATOR_GLOSNIK`, `KOMUNIKATOR_DANE`,
`KOMUNIKATOR_ROZPOZNAWANIE`, `KOMUNIKATOR_GEMINI_KLUCZ`.

The Gemini API key does not go into this file. Store it with `komunikator klucz gemini`
(see [INSTRUCTIONS.md](INSTRUCTIONS.md)).

To see what is currently in effect and where each value comes from: `komunikator ustawienia`.

## 6. Another Linux computer

```bash
cd ~/komunikator && make statyczny
```

This produces a single file (about 3 MB) with no shared libraries. Copy it over and point the
speech services at your server, e.g. `tts=192.168.1.10:10200`. With the static build, use IP
addresses rather than host names — that is how glibc behaves when linked statically.

## 7. The window (desktop app)

The window is a separate program, `komunikator-okno`, that sits on top of the engine. The engine
must be installed first (step 3). The window needs Qt 5, which on Ubuntu is one command (needs sudo):

```bash
sudo apt install g++ make qtbase5-dev libqt5svg5
```

Build and install (no sudo):

```bash
cd ~/komunikator && make okno-install
```

The program appears in the application menu as **Komunikator**; you can also start it with
`komunikator-okno`. On first start, a short wizard asks you to choose a microphone and a speaker.

Closing the window hides it to the tray icon next to the clock. The program keeps running in the
background; **Quit** is in that icon's menu (right click).

Screenshots of every screen, for comparison with the design — no monitor needed and nothing
spoken aloud:

```bash
cd ~/komunikator/okno && make zrzuty
```

Output: `okno/build/zrzuty/*.png`.

## Uninstall

```bash
cd ~/komunikator && make uninstall
```

Saved messages stay in `~/.local/share/komunikator`. Delete that directory by hand if you no longer
need them. The diagnostic log is in `~/.local/state/komunikator/`.
