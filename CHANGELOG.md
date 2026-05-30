# Changelog

## 2.1.0

### Native compiler — Wine/CrossOver removed

The macOS build now ships and runs a **native `pawncc`** — compiling no longer
needs Wine or CrossOver.

- **Bundled native compiler.** A native `pawncc` (+ `libpawnc.dylib`) is bundled
  into `Qawno.app/Contents/Resources/native/` and deployed into each project's
  `qawno/native/` folder on compile. It emits open.mp-compatible AMX
  (magic `0xF1E0`), so the output loads on the server.
- **No Wine, no CrossOver, no bottle.** Removed the bundled-Wine download, the
  CrossOver detection/bottle setup, and the `pawncc.exe`-under-Wine pipeline.
- **Compiler-path migration.** Existing settings that pointed at the old
  `pawn-cc.sh` / `run-pawncc-wine.sh` wrappers are migrated automatically to the
  native compiler (`%p/qawno/native/pawncc`).
- **Start page cleanup.** Removed the Wine, Pawn-compiler, and Qawno-files
  status cards (the app ships its own compiler, so there's nothing to detect).
- **Code removal.** Deleted `WineManager`, `CrossOver`, the Wine shell wrappers,
  and the Wine update-checker polling — net ~1,300 fewer lines.
- Updater no longer polls the Wine builds repo.

## 2.0.0

- Initial native macOS port of Qawno (Apple Silicon + Intel): native window
  chrome, dark/light/system themes, in-app compile, updater.
