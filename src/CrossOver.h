// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// qawno is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with qawno. If not, see <http://www.gnu.org/licenses/>.

#ifndef CROSSOVER_H
#define CROSSOVER_H

#include <QObject>
#include <QString>
#include <QStringList>

// macOS-only helper around CrossOver's bundled Wine. The Pawn compiler
// (pawncc.exe) is run under CrossOver because the native macOS pawncc emits an
// AMX P-code format the open.mp server rejects (magic 0xF1E1 vs the expected
// 0xF1E0). This wraps detection and the one-time creation of a 32-bit bottle.
//
// Everything is static/stateless: CrossOver state lives on disk, so each call
// re-reads it rather than caching.
class CrossOver {
 public:
  // The 32-bit bottle this port compiles in. open.mp's toolchain is 32-bit, so
  // a 64-bit bottle fails with "cannot execute".
  static const QString kBottle;
  static const QString kBottleTemplate; // win10

  // CrossOver.app root and the SharedSupport CrossOver dir inside it.
  static QString appPath();          // "" if not installed
  static QString cxRoot();           // .../SharedSupport/CrossOver
  static QString winePath();         // cxRoot/bin/wine ("" if missing)
  static QString cxBottlePath();     // cxRoot/bin/cxbottle ("" if missing)

  static bool isInstalled();         // CrossOver.app present
  static QString version();          // CFBundleShortVersionString, e.g. "26.1"

  // Per-user bottles live under ~/Library/Application Support/CrossOver/Bottles.
  static QString bottlesDir();
  static QString bottlePath();       // bottlesDir/kBottle
  static bool bottleExists();        // our Qawno bottle is present

  // Create the 32-bit Qawno bottle (blocking; takes seconds). Returns true on
  // success; on failure fills *error with cxbottle's output. No-op (true) if
  // the bottle already exists.
  static bool createBottle(QString* error = nullptr);

  // Delete the Qawno bottle (for a clean reinstall). Returns true on success
  // (or if it was already gone); on failure fills *error.
  static bool deleteBottle(QString* error = nullptr);

  // The compiler files needed at compile time: pawncc.exe + pawnc.dll. These
  // live in the project's qawno/ subfolder (beside the .pwn file), not beside
  // Qawno.app — so the app is independent of any one pawncc install.
  static QStringList requiredFiles();         // file names, in display order

  // The qawno/ subfolder beside the input .pwn. Empty if pwnFile is empty.
  // Doesn't check existence — pair with missingFiles(dir).
  static QString projectQawnoDir(const QString& pwnFile);

  // requiredFiles() that are absent from the given qawno/ directory.
  static QStringList missingFiles(const QString& qawnoDir);
};

#endif // CROSSOVER_H
