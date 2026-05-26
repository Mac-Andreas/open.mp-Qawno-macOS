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

#ifndef SERVER_H
#define SERVER_H

#include <QString>
#include <QStringList>

#if defined(Q_OS_WIN)
// On Windows the server is spawned with the Win32 API so it gets its own
// console window, exactly as it always has.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
// Every other platform (macOS, Linux) drives the server through QProcess.
QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE
#endif

class Server {
 public:
  Server();
  ~Server();

  QString path() const;
  void setPath(const QString &path);

  QStringList options() const;
  void setOptions(const QString &options);
  void setOptions(const QStringList &options);

  QStringList extras() const;
  void setExtras(const QString &extras);
  void setExtras(const QStringList &extras);

  QString output() const;

  QString command() const;
  QString commandFor(const QString &inputFile) const;

  void run(const QString &inputFile);

 private:
  QString path_;
  QStringList options_;
  QStringList extras_;
  QString output_;

#if defined(Q_OS_WIN)
  HANDLE thread_;
  PROCESS_INFORMATION pi_;

  static DWORD WINAPI threaded(LPVOID);
#else
  QProcess *process_ = nullptr;
#endif
};

#endif // SERVER_H
