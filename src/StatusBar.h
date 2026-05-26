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

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <QStatusBar>

class QLabel;
class QPushButton;

class StatusBar: public QStatusBar {
 Q_OBJECT

 public:
  StatusBar(QWidget *parent = 0);

  void setCursorPosition(int line, int column, int selected);
  // allSelected => whole document selected: show "Select All".
  void setCursorPosition(int line, int column, int selected, bool allSelected);
  // Right-aligned file info; hasFile=false clears it (home screen).
  void setFileInfo(int totalLines, qint64 bytes, bool hasFile);

  // Shows / hides the "Update available [Update]" chip sitting just left of
  // the line/byte counters. Empty label hides the chip entirely.
  void setUpdateAvailable(const QString& label);

 signals:
  void updateClicked();

 private:
  QLabel* info_ = nullptr;
  QPushButton* updateBtn_ = nullptr;
};

#endif // STATUSBAR_H
