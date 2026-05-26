#!/usr/bin/env bash
#
# Build qawno as a native macOS .app bundle.
#
# Requires Homebrew with Qt (qt6) and cmake:
#     brew install qt cmake
#
# Usage:
#     ./build-macos.sh             # configure + build + bundle Qt + ad-hoc sign
#     ./build-macos.sh --no-deploy # skip Qt bundling (faster dev builds)
#     ./build-macos.sh --clean     # wipe the build directory first
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/build"

# Default: full deploy so the .app in the project root is standalone-runnable.
DEPLOY=1
for arg in "$@"; do
  case "$arg" in
    --deploy)    DEPLOY=1 ;;
    --no-deploy) DEPLOY=0 ;;
    --clean)     rm -rf "$BUILD" ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

# Point CMake at the Homebrew Qt so find_package(Qt6 ...) succeeds.
QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
if [[ -z "$QT_PREFIX" ]]; then
  echo "Qt not found via Homebrew. Run: brew install qt cmake" >&2
  exit 1
fi

cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX"

cmake --build "$BUILD" --parallel "$(sysctl -n hw.ncpu)"

APP="$BUILD/Qawno.app"

if [[ "$DEPLOY" -eq 1 ]]; then
  MACDEPLOYQT="$QT_PREFIX/bin/macdeployqt"
  echo "Bundling Qt frameworks with $MACDEPLOYQT ..."
  "$MACDEPLOYQT" "$APP"
fi

# Ship the bundle into ./dist (kept out of git). Wipe any previous build there.
mkdir -p "$ROOT/dist"
OUT="$ROOT/dist/Qawno.app"
rm -rf "$OUT"
cp -R "$APP" "$OUT"

# Ad-hoc sign so Gatekeeper accepts the bundle without a Developer ID. Without
# this, macdeployqt's framework dylibs end up with stale signatures and the app
# refuses to launch ("invalid signature").
if [[ "$DEPLOY" -eq 1 ]]; then
  codesign --force --deep --sign - "$OUT" >/dev/null 2>&1 || true
  xattr -dr com.apple.quarantine "$OUT" >/dev/null 2>&1 || true
fi

# Register the shipped bundle with Launch Services so .pwn files opened from
# Finder route to this build (overrides any older copy).
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister \
    -f "$OUT" >/dev/null 2>&1 || true

echo
echo "Built: $OUT"
echo "Run it with:  open \"$OUT\""
