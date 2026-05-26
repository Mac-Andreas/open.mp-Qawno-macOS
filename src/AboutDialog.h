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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

// A Qt-style "About" dialog: app icon + name + version, a short description of
// what qawno does, the macOS-port note, and an "About Qt" button.
class AboutDialog: public QDialog {
 Q_OBJECT

 public:
  explicit AboutDialog(QWidget *parent = nullptr);
  ~AboutDialog() override;
};

#endif // ABOUTDIALOG_H
