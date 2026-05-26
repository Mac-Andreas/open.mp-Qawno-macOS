# Qawno on macOS

A native macOS port of [qawno](https://github.com/openmultiplayer/qawno), the
open.mp Pawn editor. This document covers building the editor and wiring up the
Pawn compiler on macOS through CrossOver.

> Upstream qawno is labelled "Windows only" because the editor was built with
> the Win32 API and the toolchain ships only for Windows/Linux. This port makes
> the editor build and run natively on macOS (Apple Silicon and Intel) and
> routes the Pawn compiler through CrossOver.

## What changed for macOS

- **Qt6** (`src/MainWindow.cpp`): `QRegExp` -> `QRegularExpression` so it
  compiles against Qt6 (still works on Qt5).
- **Build system** (`CMakeLists.txt`): version-agnostic Qt (Qt6 preferred, Qt5
  still works), C++17, the Windows-only `qawno.rc` is excluded off-Windows, and
  the macOS `.icns` / app-bundle wiring is fixed.
- **Editor font** (`src/EditorWidget.cpp`): uses *Menlo* on macOS.
- **CrossOver card** (`src/CrossOver.{h,cpp}`, welcome screen): detects
  CrossOver, shows its version, and creates the 32-bit `Qawno` bottle that the
  compiler runs in (see below).

---

## 1. Prerequisites

```sh
brew install qt cmake
```

CrossOver is also required to compile (it provides the Wine engine the Pawn
compiler runs under).

## 2. Build the editor

```sh
./build-macos.sh            # -> build/Qawno.app
./build-macos.sh --deploy   # bundle Qt frameworks into the .app (distributable)
open build/Qawno.app
```

## 3. Why the compiler runs under CrossOver

A native macOS `pawncc` builds and runs, but its output is **not usable**: it
emits AMX P-code with magic `0xF1E1`, and the open.mp server rejects that with
"Invalid/unsupported P-code file format". The official **Windows** `pawncc.exe`
emits the expected `0xF1E0`, so this port compiles through it under CrossOver's
Wine. open.mp's toolchain is 32-bit, so it needs a **32-bit** bottle (a 64-bit
bottle fails with "cannot execute").

## 4. Set up CrossOver (welcome screen card)

Open Qawno with no file open. The welcome screen shows a **CrossOver** card:

- A **Detected / Not detected** pill plus the CrossOver version.
- If detected and the bottle is missing, a **Set up Qawno bottle** button. It
  runs `cxbottle --create --bottle Qawno --template win10` (32-bit), which takes
  up to a minute. Once the bottle exists, the card confirms it's ready.

To create the bottle by hand instead:

```sh
export CX_ROOT="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver"
"$CX_ROOT/bin/cxbottle" --create --bottle Qawno --template win10
```

## 5. Stage the Windows compiler and point qawno at it

1. Put the **Windows** `pawncc.exe` (and `pawnc.dll`) next to
   [`scripts/run-pawncc-wine.sh`](scripts/run-pawncc-wine.sh).
2. In qawno: **Settings -> Compiler Settings -> Compiler path** -> the absolute
   path to `run-pawncc-wine.sh`. The wrapper rewrites the absolute macOS paths
   qawno passes (`-i`, `-o`, input file) onto Wine's `Z:` drive and runs
   `pawncc.exe` in the `Qawno` bottle.

Or seed the path directly (qawno's prefs domain is `io.github.zeex.Qawno`):

```sh
defaults write io.github.zeex.Qawno CompilerPath \
  -string "/abs/path/to/scripts/run-pawncc-wine.sh"
```

The default compiler options use `%p` (input dir), `%o` (output base name) and
`%i` (input file). Make sure one `-i` points at your open.mp includes.

Overrides for the wrapper: `WINE=...`, `QAWNO_WINE_BOTTLE=...` (default `Qawno`),
`PAWNCC_EXE=...`.

---

## Limitations / notes

- The native macOS `pawncc` is intentionally not used: its AMX output (`0xF1E1`)
  is rejected by the server. Compile through the Windows `pawncc.exe` under
  CrossOver as above.
- CrossOver is required to compile on macOS. Editing the source is fully native.
