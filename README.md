Qawno · macOS
=============

A native macOS port of [Qawno](https://github.com/openmultiplayer/qawno), the
open.mp Pawn editor. Apple Silicon + Intel, dark/light themes, a bundled native
Pawn compiler (no Wine), and native macOS chrome.

[![Latest release](https://img.shields.io/github/v/release/Mac-Andreas/Qawno-macOS?label=download&style=flat-square)](https://github.com/Mac-Andreas/Qawno-macOS/releases/latest)

---

What's new in this fork
-----------------------

Same Qawno you know, rebuilt for macOS with everything the original couldn't ship:

* **Native macOS chrome** — real traffic lights, system window shadow, proper close button, no Win32 emulation.
* **Dark / Light / System themes** — every panel re-themed top to bottom.
* **Native Pawn compiler** — a native `pawncc` ships in the app and compiles directly. No Wine, no CrossOver.
* **Auto-update Pawn compiler** — Updates tab checks GitHub, downloads, installs.
* **Per-language syntax themes** — Pawn, C/C++, Python, JavaScript, Rust.
* **App-wide font picker** — sets menus, sidebar, tab strip. Editor / output keep their own monospace.
* **Settings panel** — proper macOS Preferences layout (sidebar + cards), Folder Access, Privacy, Updates tabs.
* **Anonymous telemetry (opt-in)** — see [Telemetry](#telemetry-opt-in).
* **Translation infrastructure** — 28 locales scaffolded, English shipped.
* **Open / Save / Color dialogs** — native macOS panels.

---

Download
--------

Grab `Qawno.app` (zipped) from
[**Releases**](https://github.com/Mac-Andreas/Qawno-macOS/releases/latest), then
unzip it.

1. Drag `Qawno.app` to `/Applications` (or anywhere).
2. Double-click. macOS asks once for Documents access — say yes if your projects live there.
3. Open a `.pwn` and compile — the native compiler is already bundled.

That's it. No Xcode, no CrossOver, no Wine.

### First launch — "unidentified developer"

The app is **self-signed (ad-hoc), not notarized** — Apple notarization isn't
available for this build, so Gatekeeper will warn on first launch. It's safe to
open; you just have to tell macOS so once:

- **Right-click** `Qawno.app` → **Open** → **Open** in the dialog, **or**
- After macOS blocks it, go to **System Settings → Privacy & Security** and click
  **Open Anyway**, **or**
- From a terminal, clear the quarantine flag:

  ```sh
  xattr -dr com.apple.quarantine /Applications/Qawno.app
  ```

After the first successful open, it launches normally.

---

Compiling a Pawn project
------------------------

open.mp / SA-MP projects expect this Windows-style layout:

```
your-project/
├── qawno/                  ← drop the Windows pawncc files here
│   ├── pawncc.exe
│   ├── pawnc.dll
│   ├── libpawnc.so         (optional)
│   └── include/            ← .inc files
├── gamemodes/
│   └── your.pwn
├── filterscripts/
└── plugins/
```

1. **Get pawncc** — Windows release from
   <https://github.com/openmultiplayer/compiler/releases/latest>. Unzip into the project's `qawno/` folder. You only need `pawncc.exe`, `pawnc.dll`, and the `include/` directory.
2. **Open the project in Qawno** — drag the folder onto `Qawno.app`, or `File → Open` a `.pwn` inside it.
3. **Compile** — click **Compile** (or `⌘B`). Qawno walks up from the `.pwn` to find the `qawno/` folder, deploys the native `pawncc` into `qawno/native/`, runs it, and writes the `.amx` beside your `.pwn`.

### Running the open.mp server with the compiled .amx

Qawno produces a `.amx` byte-identical to one built on Windows. Three ways to run it:

* **Paste onto your Windows server / VPS**
  Copy the project (or just the new `.amx`) onto the Windows host and start `omp-server.exe` as usual. No compatibility shim needed.
* **Linux server**
  Drop the project onto your Linux box and run the open.mp Linux `omp-server` binary that ships with the server release.
* **Locally on macOS via Wine**
  `wine omp-server.exe` from inside the project. To reuse Qawno's bundled Wine prefix instead of installing a separate one:
  ```sh
  export WINEPREFIX="$(pwd)/.Qawno/wine/prefix"
  open -a "Qawno.app/Contents/Resources/wine/wine64" omp-server.exe
  ```

---

Features
--------

(Inherited from upstream Qawno + macOS additions.)

* **Syntax highlighting** with per-language themes (auto-inverts light/dark)
* **Auto completion** — known natives and user symbols
* **Natives list** sidebar
* **Multiple tabs** — open as many files as you like
* **Pawn compiler** with inline error/warning gutter underline
* **Find & Replace** (`⌘F`) — match case, whole-word, regex, backwards
* **Colour picker** (`⌘M`) — native macOS picker
* **Editing helpers** — `⌘D` duplicate, `⌘⇧D` duplicate selection, `⌘L` delete line, `⌘/` toggle comment, `⌘↑`/`⌘↓` scroll without moving cursor
* **Compile progress popup** — stays on top, real progress, wrap toggle, recompile button
* **Welcome page** — recent files
* **Updater** — checks GitHub for new Qawno or pawncc releases (daily background, or on demand)

---

Settings
--------

`⌘,` opens Settings. Sidebar layout, every page is its own card. Settings persist via `QSettings` (the macOS preferences plist).

* **General** — toggles + language picker (28 locales scaffolded, English shipped today)
* **Appearance** — System / Light / Dark tiles + per-editor syntax scheme
* **Fonts** — editor font, output font, app-wide UI font, per-language syntax palette
* **Compiler** — pawncc path + flags (`%P` = project root, `%p` = .pwn dir, `%i` / `%o` / `%q` tokens)
* **Folder Access** — list granted folders + deep-link to System Settings → Privacy
* **Privacy** — telemetry toggle + reset anonymous ID
* **Updates** — current vs latest Pawn compiler + Qawno; auto-download + reopen-files toggles

---

Telemetry (opt-in)
------------------

On first launch Qawno asks whether to share anonymous usage data. **Nothing identifying you, your projects, or your code is ever sent** — only feature counts, OS version, and a random per-install UUID you can reset from **Settings → Privacy**. Your choice is remembered; flip it any time in Settings.

* Aggregate dashboard: <https://mac-andreas.github.io/#dashboard>
* Client implementation: [`src/TelemetryClient.cpp`](src/TelemetryClient.cpp)

**No database key ships in the app.** Events POST to a Supabase Edge Function that holds the credential server-side and inserts on the client's behalf. The events table is locked to anonymous access, so the only write path is that function — even a decompiled binary exposes nothing but the public function URL.

---

Build from source
-----------------

```sh
brew install qt cmake
./build-macos.sh                 # default: build + bundle Qt + ad-hoc sign
./build-macos.sh --no-deploy     # skip Qt framework bundling (faster dev)
./build-macos.sh --clean         # wipe build/ first
open Qawno.app
```

Deeper notes on the macOS port: [README-macOS.md](README-macOS.md).

---

Repository layout
-----------------

```
src/                  C++ sources (Qt6 + Qt5 compatible)
assets/images/        Icons, flags, app .icns
i18n/                 Qt translations (28 locales, .ts files)
scripts/              pawn-cc.sh wrapper deployed into project qawno/ folders
build-macos.sh        Build + macdeployqt + ad-hoc codesign in one shot
ToDo.md               Living list of follow-ups
```

Telemetry backend (Supabase schema + RLS) and the GitHub Pages dashboard
live in a separate repo: [Mac-Andreas/mac-andreas.github.io](https://github.com/Mac-Andreas/mac-andreas.github.io).

---

License
-------

GPLv3 — same as upstream Qawno.

---

Credits
-------

* Upstream Qawno: [openmultiplayer/qawno](https://github.com/openmultiplayer/qawno)
* Pawn compiler: [openmultiplayer/compiler](https://github.com/openmultiplayer/compiler)
* Wine builds (macOS): [Gcenx/macOS_Wine_builds](https://github.com/Gcenx/macOS_Wine_builds)
* macOS port + UI rebuild: [@Mac-Andreas](https://github.com/Mac-Andreas)
