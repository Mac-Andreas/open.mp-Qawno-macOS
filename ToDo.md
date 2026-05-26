# Qawno macOS — Remaining Work

Tracks every loose end across the app + dashboard + Supabase pipeline. Group by
area; sub-items in dependency order. Strike out as you finish.

---

## 1. Telemetry rollout (live before public release)

- [ ] Create Supabase project for Mac-Andreas
- [ ] Apply [sql/qawno_events.sql](sql/qawno_events.sql) via SQL editor
- [ ] Copy project URL + anon key into `cmake/Telemetry.env` (gitignored)
- [ ] Rebuild — verify `track()` actually hits the network (curl the events table after one launch)
- [ ] Verify RLS: confirm `select * from public.qawno_events` from anon key returns 0 rows (expected)
- [ ] Verify aggregate views readable from anon (curl `/rest/v1/qawno_live_now`)
- [ ] Add `track()` calls at instrumentation points (currently only `session_start` fires):
  - [ ] `compile_run` — fire on each compile; props: `{success: bool, warnings: int, errors: int, duration_ms: int}`
  - [ ] `file_open` — props: `{ext: "pwn"|"inc"|"new"}` (NO path/name)
  - [ ] `update_check_run` — props: `{component: "wine"|"qawno"|"compiler", available: bool}`
  - [ ] `update_installed` — props: `{component, from_version, to_version}`
  - [ ] `wine_install_succeeded` / `wine_install_failed` — props: `{error_class?: string}`
  - [ ] `settings_changed` — props: `{key}` (NOT value, only which knob)

## 2. Public dashboard (mac-andreas.github.io)

- [ ] Push [dashboard-template/](dashboard-template/) to `Mac-Andreas/mac-andreas.github.io` root
- [ ] Fill `dashboard/config.js` with real Supabase URL + anon key
- [ ] Enable GitHub Pages on the repo (Settings → Pages → main branch)
- [ ] Verify `https://mac-andreas.github.io/dashboard/` renders charts
- [ ] Wire custom domain if planned
- [ ] Add Plausible/umami-style page-view counter for the dashboard itself (optional)
- [ ] Refactor `dashboard/app.js` view names to be slug-driven once second app lands

## 3. Translations (29 locales)

- [ ] Fill `.ts` files in [i18n/](i18n/) — currently `<translation type="unfinished">` everywhere
- [ ] Option A: machine-translate via DeepL/LLM batch, then human review
- [ ] Option B: open a `crowdin.com` project; sync TS files
- [ ] Replace placeholder 3-stripe flags in [assets/images/flags/](assets/images/flags/) with real flag-icons (MIT) SVGs
- [ ] Add `lupdate` step to `build-macos.sh` so the .ts files refresh on every build
- [ ] RTL audit for `ar` (Arabic) — Qt layout direction should auto-flip; sanity-check Find dialog + Settings nav
- [ ] Localise dynamic strings that use QString concatenation (search for `+ tr(`)

## 4. Wine bundling — polish

- [ ] Auto-prompt to install wine on first compile attempt if missing (currently silent fail at execve)
- [ ] Cache cleanup: button "Clear download cache" on Updates pane (Wine card has it, Compiler doesn't)
- [ ] Detect wine corruption (binary present but `wine --version` fails) — show Reinstall prompt
- [ ] On Apple Silicon, verify Rosetta 2 prompt fires for the x86_64 wine binary; bake a one-liner check at first launch

## 5. Compiler card / pipeline

- [ ] Compiler card "DETECTED" should also surface the detected version (currently text-only)
- [ ] When `pawncc.exe` missing AND no project open, show "Download pawncc" button that auto-installs latest release into `<app dir>/`
- [ ] Add a "Verify pawncc" button that runs `wine pawncc.exe` once and prints the banner — confirms wine + pawncc compatibility end-to-end
- [ ] `%P` token (project root) — document in Compiler tab's "Options" hint; current hint only mentions `%p/%i/%o/%q`
- [ ] Open compiled `.amx` location in Finder via a "Show output" button after success

## 6. Settings — outstanding cosmetics

- [ ] Reset button: lighter background in dark mode + visible text (currently low contrast)
- [ ] Theme tile selected state: live-preview the theme on hover before commit
- [ ] App Font picker: live preview the chosen font in a sample paragraph
- [ ] Folder Access: per-folder revoke button (deep-link Privacy & Security → specific row)
- [ ] Add a search box at the top of the nav (filter pages by name)
- [ ] Native macOS unified-toolbar look: `setUnifiedTitleAndToolBarOnMac(true)` once we have a QMainWindow-backed Settings

## 7. Compile popup — outstanding

- [ ] Progress estimator: hook into per-file include resolution count (read compiler verbose output) for accurate %
- [ ] "Open log file" button to save the compile log to disk
- [ ] Cancel button text → "Stop"
- [ ] Persist popup geometry (size + position) so the second compile reuses it

## 8. ConsentDialog + Privacy

- [ ] Add "Show me what gets sent" button — pops a sample JSON payload so users see exactly what we send
- [ ] Re-prompt after a major version bump (e.g. `1.x → 2.x`) using `TelemetryConsentVersion` setting
- [ ] Privacy tab: show the device_id (with copy button) so users can hand it to support if they want to nuke their own data
- [ ] Per-event toggles? (advanced — defer until we have event types nailed down)

## 9. Find dialog (Cmd+F)

- [ ] Add count badge ("3 of 17") when match exists
- [ ] Highlight all matches in the editor, not just the current one
- [ ] Keyboard: Cmd+G / Cmd+Shift+G for find next / previous (currently F3-only)
- [ ] Replace All confirmation when >100 replacements
- [ ] Multi-file find/replace across open tabs

## 10. Updater pipeline

- [ ] Self-update for Qawno.app: download + verify signature + atomic swap (currently launches downloaded asset manually)
- [ ] Update channel toggle: stable vs prerelease releases
- [ ] Update history pane (which version installed when)
- [ ] Background download progress in status bar (currently only on welcome card)

## 11. Build / release

- [ ] Notarise + staple the .app bundle (Apple Developer ID required)
- [ ] Codesign properly (currently ad-hoc); script the entitlements
- [ ] Build universal binary (currently arm64-only)
- [ ] Auto-generate a .dmg with backdrop image
- [ ] GitHub Actions: build on push to main, attach .dmg to releases
- [ ] Add CHANGELOG.md generated from commit messages

## 12. Welcome page / onboarding

- [ ] First-launch tutorial overlay (one-time, dismissable)
- [ ] Drag-drop a folder onto welcome to set as default project
- [ ] Recent files: pin/unpin entries
- [ ] Welcome cards: clickable "Status" pill opens the relevant Settings tab

## 13. Editor improvements

- [ ] Multi-cursor support
- [ ] Bracket matching highlight
- [ ] Auto-indent for blocks
- [ ] Code folding for functions
- [ ] Minimap
- [ ] Search results panel in sidebar
- [ ] Git gutter (added/modified/deleted line markers)

## 14. Docs

- [ ] README.md: update screenshots to match new Settings design
- [ ] CONTRIBUTING.md: how to add a translation, how to add a new tracked event
- [ ] Wire CHANGELOG.md
- [ ] Privacy policy markdown source — currently hard-coded in [dashboard-template/dashboard/privacy.html](dashboard-template/dashboard/privacy.html); single-source it

## 15. Known bugs / regressions

- [ ] Tab icons sometimes don't theme-swap until window resize (forced repaint missing)
- [ ] Settings dialog initial size on first-ever launch occasionally too narrow on small displays
- [ ] Compile output regex strips legitimate `[mvk-` strings in user code (rare but possible)
- [ ] `lrelease` CMake step runs every build even when .ts files unchanged — cache the .qm outputs

---

_Last updated: 2026-05-25_
