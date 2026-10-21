# Using komunikator

*Polish version: [INSTRUKCJA.md](INSTRUKCJA.md)*

`komunikator` speaks text aloud, stores ready-made spoken messages and turns recorded speech into
text. People, shell scripts, cron jobs and other programs all use it the same way.

The command names are Polish. The full list is always one command away: `komunikator pomoc`.

| Command | What it does |
|---|---|
| `powiedz` | say text now |
| `zapisz` / `odtworz` / `lista` / `usun` | save / play / list / delete a message |
| `sluchaj` / `nagraj` | record and transcribe / only record |
| `rozpoznaj` | transcribe a WAV file |
| `mikrofony` / `mikrofon` / `glosniki` / `glosnik` | list / choose microphones and speakers |
| `sprawdz` / `glosy` / `ustawienia` / `ustaw` / `klucz` | diagnostics, voices, settings, Gemini key |

## Say something right now

```bash
komunikator powiedz Good morning
```

It speaks through the chosen speaker of this computer (see *Microphone and speaker*).

Instead of speaking, save the speech to a file:

```bash
komunikator powiedz -o ~/greeting.wav Good morning
```

Text can also come from standard input — write `-` instead of the text:

```bash
echo "The washing machine is done" | komunikator powiedz -
```

## Saved messages

A message is a piece of text together with its recorded audio, stored under a name.

```bash
komunikator zapisz dog Time to walk the dog
komunikator zapisz door The front door is open
komunikator lista
komunikator odtworz dog
komunikator usun door
```

- The audio is synthesized once, by `zapisz`. After that, `odtworz` plays it **without the speech
  service** — it works even when the Piper container is down.
- Saving again under the same name replaces the message.
- Names may contain letters (including Polish ones), digits, spaces inside the name and `- _ .`.
  A name cannot start with a dot or a dash, start or end with a space, or contain a slash.
- The files live in `~/.local/share/komunikator/komunikaty/`:
  `NAME.txt` (the text — you may edit it) and `NAME.wav` (the audio).
  After editing the text by hand, delete `NAME.wav`; the next `odtworz` will record it again.

## Microphone and speaker

The program never chooses on its own. When no microphone or speaker has been chosen:

- in a terminal, it shows the list, asks for a number and remembers the choice;
- when called by another program, it exits with code `2` and explains what to do.

```bash
komunikator mikrofony        # the list; an asterisk marks the chosen one
komunikator mikrofon 2       # choose number 2
komunikator glosniki
komunikator glosnik 2
```

On a desktop computer the list comes from PipeWire, so it matches the sound settings of your
desktop — including headphones, USB and Bluetooth devices — and the program plays and records
through PipeWire like any other application. Without PipeWire (for example on a headless server)
the list comes straight from the sound cards.

Headphones in the 3.5 mm jack are not a separate device, only an output of the same sound card:
choose the built-in card (or `default`) and select *Headphones* in the system sound settings.

If the chosen device is unplugged, the program reports it instead of quietly playing somewhere else.

## Listen (microphone → text)

```bash
komunikator sluchaj                      # record 5 s and print what you said
komunikator sluchaj 10                   # 10 s
komunikator nagraj 10 -o ~/note.wav      # only record, no transcription
```

If the recording is almost silent, the program warns that the microphone is muted or the wrong one
is chosen. `Ctrl+C` ends a recording early and keeps what was captured.

## Transcribe a file

```bash
komunikator rozpoznaj ~/recording.wav
```

Prints the recognized text. It accepts uncompressed WAV (PCM), e.g. from a voice recorder or from
`arecord`. A live stream works too:

```bash
arecord -f S16_LE -r 16000 -c 1 -d 5 | komunikator rozpoznaj -
```

Recognizing a longer recording takes a few seconds, because Whisper runs on the CPU.

## Recognition through Gemini (more accurate, needs internet)

Local Whisper works offline but makes more mistakes; Google Gemini understands speech better.
It needs a free API key: <https://aistudio.google.com/apikey> → *Create API key*.

```bash
komunikator klucz gemini                  # paste the key and press Enter (it is not shown)
komunikator ustaw rozpoznawanie gemini
```

Back to local recognition: `komunikator ustaw rozpoznawanie whisper`.
Remove the key: `komunikator klucz --usun`.

The same is available in the window: *Settings → Speech recognition*.

When Gemini fails (no internet, a wrong key, an error at Google), komunikator transcribes the
recording locally instead and prints a line starting with `uwaga: Gemini` on `stderr`; the window
shows *Recognized locally*. The details of the failure go to the diagnostic log.

The key is stored in `~/.config/komunikator/gemini-klucz`, readable only by you (mode `0600`).
It never appears in commands, in the process list or in `komunikator ustawienia` — there you only
see `gemini_klucz=zapisany` (saved) or `brak` (none). It is sent to Google in a request header,
through a temporary file that is deleted right after sending. The connection is made with `curl`.

## When something does not work

```bash
komunikator sprawdz
```

Every error is a plain sentence that says what happened and what to do. The most common ones:

- *Nie mogę połączyć się z: usługa mowy …* (“I can't reach the speech service”) — the speech
  container is not running or the address is wrong. Check `docker ps`, restart the container
  (`docker restart wyoming-piper` or `docker restart wyoming-whisper`) and the address in
  `komunikator ustawienia`.
- *nagrywanie … nie powiodło się* (“recording failed”) — the microphone is busy in another program
  or unplugged. Choose another one (`komunikator mikrofony`); `default` almost always works.
- *nie znalazłem odtwarzacza* (“player not found”) — `aplay` is missing. The audio is still in the
  file named in the message.

The technical details of every error are written to `~/.local/state/komunikator/dziennik.log`.
To also print them on screen, run the command with `KOMUNIKATOR_SZCZEGOLY=1`.

## The window

`komunikator-okno` (application menu → *Komunikator*) does the same things as the terminal commands:

- **Powiedz (Say)** — type text; Enter speaks, Shift+Enter adds a new line. While speaking, the
  same button becomes **Stop**. *Save as message* stores the sentence under a name.
- **Słuchaj (Listen)** — choose 5, 10 or 30 s and press the microphone (or Space). A second Space
  ends the recording early. The recognized text can be copied.
- **Komunikaty (Messages)** — search, **Play** and **Delete**. They work without the speech server too.
- **Status bar** — whether the speech server is up, plus the chosen microphone and speaker.
  Clicking it opens the settings (device choice and test, recognition: Gemini or local, server address).
- **Esc** stops speaking and recording.
- Unplugged headphones: the status bar shows *Unavailable* in red, and *Listen* leads to the settings.
- The program runs as a single instance: starting it again from the menu brings back the window
  hidden in the tray.
- Quitting while speaking or recording stops it — nothing keeps playing in the background.

The look comes from the design in `design/` (specification `SPEC.md`). The light or dark theme
follows the system and can be forced: `KOMUNIKATOR_MOTYW=jasny komunikator-okno`
(`jasny` = light, `ciemny` = dark).

## For other programs

komunikator is built to be called from code.

**Exit codes**

| Code | Meaning |
|:---:|---|
| `0` | success |
| `1` | runtime error (service, file, player) |
| `2` | wrong command, invalid name, or no microphone/speaker chosen |
| `130` | stopped (`Ctrl+C` or `SIGTERM`); speaking also stops the player, recording keeps what was captured |

**Input and output**

- The result (recognized text, the list) goes to standard output; messages and errors go to
  standard error, so results never mix with diagnostics.
- `komunikator lista` prints `NAME<TAB>TEXT`, one line per message.
- Settings can be overridden per call with environment variables, e.g.
  `KOMUNIKATOR_ODTWARZACZ= komunikator powiedz …` (empty = do not play, only print the file path).

Examples:

```bash
# bash: an alarm that degrades gracefully — if the speech service is down, play the saved message
komunikator powiedz "The washing machine is done" || komunikator odtworz washing
```

```python
# Python
import subprocess

text = subprocess.run(["komunikator", "rozpoznaj", "recording.wav"],
                      capture_output=True, text=True, check=True).stdout.strip()
```

## How it works inside

komunikator talks to the speech services over the **Wyoming protocol**: a plain TCP connection
where each event is one line of JSON, optionally followed by raw audio bytes. Home Assistant uses
the same protocol to talk to Piper and Whisper. The code is in `src/`:

| File | Responsibility |
|---|---|
| `main.cpp` | commands, settings, saved messages, devices |
| `wyoming.cpp` | the Wyoming connection and events: `synthesize`, `transcribe`, `describe` |
| `json.cpp` | a small, strict JSON reader and writer (no libraries) |
| `wav.cpp` | reading and writing WAV files |
| `bledy.cpp` | human-readable errors and the diagnostic log |
| `gemini.cpp` | optional cloud recognition |

## What comes next (extensions, separate from the core)

The core stays self-contained. Extensions will be optional add-ons:

- a voice-satellite speaker (e.g. Home Assistant Voice) reached through Home Assistant;
- receiving spoken messages addressed to the computer;
- a resident mode with a UNIX-socket API and a speaking queue (draft in `src/demon.*`, not wired in yet);
- possibly network access for programs on other computers.
