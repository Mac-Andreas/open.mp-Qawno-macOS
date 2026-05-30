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

#include <QString>

// Project path helper for the macOS build.
//
// Historically this routed compilation through CrossOver's Wine; the macOS
// build now ships a native pawncc, so only this small utility remains. The
// name is kept to avoid churning call sites.
class CrossOver {
 public:
  // Walk up from a .pwn to the project's qawno/ folder (parent of gamemodes/,
  // includes/, …). Falls back to <pwnDir>/qawno when none is found.
  static QString projectQawnoDir(const QString& pwnFile);
};

#endif // CROSSOVER_H
