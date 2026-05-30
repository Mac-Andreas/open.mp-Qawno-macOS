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

#include "CrossOver.h"

#include <QDir>
#include <QFileInfo>

QString CrossOver::projectQawnoDir(const QString& pwnFile) {
  if (pwnFile.isEmpty()) {
    return QString();
  }
  // Walk upward from the .pwn file looking for a qawno/ folder. Typical
  // open.mp project layouts put it at the project root next to gamemodes/,
  // filterscripts/, includes/ — not directly beside every .pwn. Fall back
  // to <pwnDir>/qawno as the search target if none found, so the existing
  // error message still points at a sensible path.
  QDir d(QFileInfo(pwnFile).absolutePath());
  for (int i = 0; i < 6; ++i) {
    const QString candidate = d.absoluteFilePath(QStringLiteral("qawno"));
    if (QFileInfo(candidate).isDir()) {
      return candidate;
    }
    if (!d.cdUp()) break;
  }
  return QFileInfo(pwnFile).absolutePath() + QStringLiteral("/qawno");
}
