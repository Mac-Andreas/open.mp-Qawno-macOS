#!/usr/bin/env bash
#
# pawn-cc.sh — Pawn compiler wrapper. Deployed by Qawno into your project folder
# so the same compile pipeline (Qawno or terminal/CI) works without coupling to
# the Qawno.app bundle.
#
# macOS: routes pawncc.exe through CrossOver Wine (the native pawncc emits an
# AMX magic the open.mp server rejects).
# Linux: invokes ./pawncc directly.
#
# Discovery order for pawncc.exe (macOS):
#   1) $PAWNCC_EXE if set and exists
#   2) <this-script-dir>/pawncc.exe
#   3) <this-script-dir>/qawno/pawncc.exe
#   4) /Applications/Qawno.app/Contents/MacOS/pawncc.exe
#   5) $QAWNO_APP/Contents/MacOS/pawncc.exe
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

find_pawncc() {
  if [[ -n "${PAWNCC_EXE:-}" && -f "$PAWNCC_EXE" ]]; then printf '%s' "$PAWNCC_EXE"; return; fi
  local candidates=(
    "$SCRIPT_DIR/pawncc.exe"
    "$SCRIPT_DIR/qawno/pawncc.exe"
    "/Applications/Qawno.app/Contents/MacOS/pawncc.exe"
    "${QAWNO_APP:-}/Contents/MacOS/pawncc.exe"
  )
  for c in "${candidates[@]}"; do
    if [[ -n "$c" && -f "$c" ]]; then printf '%s' "$c"; return; fi
  done
  printf ''
}

if [[ "$(uname -s)" == "Darwin" ]]; then
  # Prefer Qawno's self-managed Wine install (downloaded into the hidden
  # .Qawno folder next to Qawno.app on first launch). Fall back to CrossOver
  # for users who pre-configured it.
  # The .Qawno folder location is passed in via QAWNO_APPDATA — the host sets
  # it when invoking the compiler. When unset (terminal use), we walk up from
  # the script's own location looking for a sibling .Qawno folder.
  if [[ -z "${QAWNO_APPDATA:-}" ]]; then
    walk="$SCRIPT_DIR"
    for _ in 1 2 3 4 5 6; do
      if [[ -d "$walk/.Qawno" ]]; then QAWNO_APPDATA="$walk/.Qawno"; break; fi
      walk="$(dirname "$walk")"
      [[ "$walk" == "/" ]] && break
    done
  fi
  QAWNO_WINE_BIN="$QAWNO_APPDATA/wine/Wine Staging.app/Contents/Resources/wine/bin/wine"
  QAWNO_WINEPREFIX="$QAWNO_APPDATA/wine/prefix"
  if [[ -x "$QAWNO_WINE_BIN" ]]; then
    WINE="${WINE:-$QAWNO_WINE_BIN}"
    export WINEPREFIX="${WINEPREFIX:-$QAWNO_WINEPREFIX}"
    USE_BOTTLE=0
  else
    CX_ROOT="${CX_ROOT:-/Applications/CrossOver.app/Contents/SharedSupport/CrossOver}"
    export CX_ROOT
    WINE="${WINE:-$CX_ROOT/bin/wine}"
    USE_BOTTLE=1
  fi
  BOTTLE="${QAWNO_WINE_BOTTLE:-Qawno}"
  export WINEDEBUG="${WINEDEBUG:--all}"

  PAWNCC="$(find_pawncc)"
  if [[ ! -x "$WINE" ]]; then
    echo "CrossOver wine not found at: $WINE" >&2
    echo "Install CrossOver, or set WINE=/path/to/wine" >&2
    exit 1
  fi
  if [[ -z "$PAWNCC" ]]; then
    echo "pawncc.exe not found. Tried: \$PAWNCC_EXE, $SCRIPT_DIR, $SCRIPT_DIR/qawno, /Applications/Qawno.app/Contents/MacOS" >&2
    exit 1
  fi

  # Translate absolute Unix paths to Wine's Z: drive.
  to_wine() {
    local p="$1"
    if [[ "$p" == /* ]]; then printf 'Z:%s' "${p//\//\\}"; else printf '%s' "$p"; fi
  }

  args=()
  for a in "$@"; do
    case "$a" in
      -i/*)  args+=("-i$(to_wine "${a:2}")") ;;
      -o/*)  args+=("-o$(to_wine "${a:2}")") ;;
      /*)    args+=("$(to_wine "$a")") ;;
      *)     args+=("$a") ;;
    esac
  done

  if [[ "$USE_BOTTLE" == "1" ]]; then
    exec "$WINE" --bottle "$BOTTLE" "$PAWNCC" ${args[@]+"${args[@]}"} </dev/null
  else
    # Self-managed wine: WINEPREFIX is exported above, no --bottle flag needed.
    mkdir -p "$WINEPREFIX"
    exec "$WINE" "$PAWNCC" ${args[@]+"${args[@]}"} </dev/null
  fi
else
  PAWNCC_BIN="${PAWNCC_BIN:-$SCRIPT_DIR/pawncc}"
  if [[ ! -x "$PAWNCC_BIN" ]]; then
    echo "native pawncc not found at: $PAWNCC_BIN" >&2
    exit 1
  fi
  exec "$PAWNCC_BIN" "$@" </dev/null
fi
