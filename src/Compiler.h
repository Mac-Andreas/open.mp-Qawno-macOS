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

#ifndef COMPILER_H
#define COMPILER_H

#include <QString>
#include <QStringList>

class Compiler {
 public:
  Compiler();
  ~Compiler();

  QString path() const;
  void setPath(const QString &path);

  QStringList options() const;
  void setOptions(const QString &options);
  void setOptions(const QStringList &options);

  QString output() const;

  QString command() const;
  QString commandFor(const QString &inputFile) const;

  void run(const QString &inputFile);

  // Runs the compiler with no input and parses the banner for a "x.y.z" or
  // "vX.Y.Z" version string. Returns empty if the compiler can't be invoked or
  // the banner doesn't include a recognisable version number.
  QString detectVersion() const;

 private:
  QString path_;
  QStringList options_;
  QString output_;
};

#endif // COMPILER_H
