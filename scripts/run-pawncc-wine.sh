#!/usr/bin/env bash
#
# run-pawncc-wine.sh — compile Pawn via the Windows pawncc.exe under CrossOver.
#
# The native macOS pawncc emits AMX magic 0xF1E1, which the open.mp server
# rejects ("Invalid/unsupported P-code file format"). The official Windows
# pawncc.exe emits the expected 0xF1E0, so route compilation through it.
#
# qawno passes absolute macOS paths (-i/Users/..., -o/Users/..., /Users/.../x.pwn).
# Under Wine those resolve through the Z: drive, so we rewrite them to Z:\...
#
# Point qawno's Compiler path (Settings -> Compiler -> Compiler path) at this
# script, and keep pawncc.exe + its libraries (pawnc.dll) beside it.
#
# The Qawno bottle is created from qawno's welcome-screen CrossOver card, or:
#   export CX_ROOT="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver"
#   "$CX_ROOT/bin/cxbottle" --create --bottle Qawno --template win10
set -euo pipefail

CX_ROOT="${CX_ROOT:-/Applications/CrossOver.app/Contents/SharedSupport/CrossOver}"
export CX_ROOT
WINE="${WINE:-$CX_ROOT/bin/wine}"
BOTTLE="${QAWNO_WINE_BOTTLE:-Qawno}"
export WINEDEBUG="${WINEDEBUG:--all}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PAWNCC="${PAWNCC_EXE:-$SCRIPT_DIR/pawncc.exe}"

if [[ ! -x "$WINE" ]]; then
  echo "CrossOver Wine not found at: $WINE" >&2
  echo "Install CrossOver, or set WINE=/path/to/wine" >&2
  exit 1
fi
if [[ ! -f "$PAWNCC" ]]; then
  echo "pawncc.exe not found in: $SCRIPT_DIR" >&2
  echo "Copy the Windows pawncc.exe (and pawnc.dll) here, or set PAWNCC_EXE." >&2
  exit 1
fi

# Translate absolute Unix paths to Wine's Z: drive so pawncc.exe can resolve
# includes (-i), the output (-o) and the input file. Wine maps "/" to Z:\.
to_wine() {
  local p="$1"
  if [[ "$p" == /* ]]; then
    printf 'Z:%s' "${p//\//\\}"
  else
    printf '%s' "$p"
  fi
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

# Wine reads stdin; close it so a prompt can't hang the compile.
# ${args[@]+"${args[@]}"} expands safely under `set -u` when args is empty.
exec "$WINE" --bottle "$BOTTLE" "$PAWNCC" ${args[@]+"${args[@]}"} </dev/null
