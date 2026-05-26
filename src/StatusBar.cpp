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

#include <QLabel>
#include <QLocale>
#include <QPushButton>

#include "StatusBar.h"

StatusBar::StatusBar(QWidget *parent)
  : QStatusBar(parent)
{
  updateBtn_ = new QPushButton(this);
  updateBtn_->setObjectName("statusUpdateBtn");
  updateBtn_->setFlat(true);
  updateBtn_->setCursor(Qt::PointingHandCursor);
  updateBtn_->hide();
  connect(updateBtn_, &QPushButton::clicked, this, &StatusBar::updateClicked);
  addPermanentWidget(updateBtn_);

  info_ = new QLabel(this);
  info_->setObjectName("statusInfo");
  addPermanentWidget(info_);
}

void StatusBar::setUpdateAvailable(const QString& label) {
  if (!updateBtn_) return;
  if (label.isEmpty()) {
    updateBtn_->hide();
    updateBtn_->setText(QString());
  } else {
    updateBtn_->setText(label);
    updateBtn_->show();
  }
}

void StatusBar::setCursorPosition(int line, int column, int selected) {
  setCursorPosition(line, column, selected, false);
}

void StatusBar::setCursorPosition(int line, int column, int selected, bool allSelected) {
  if (allSelected) {
    showMessage(tr("Line %1, Column %2, Select All").arg(line).arg(column));
  } else if (selected) {
    showMessage(tr("Line %1, Column %2, Selected %3").arg(line).arg(column).arg(selected));
  } else {
    showMessage(tr("Line %1, Column %2").arg(line).arg(column));
  }
}

void StatusBar::setFileInfo(int totalLines, qint64 bytes, bool hasFile) {
  if (!info_) {
    return;
  }
  if (!hasFile) {
    info_->clear();
    return;
  }
  // Human-readable size with raw bytes, e.g. "15 KB (15360 bytes)".
  QString human;
  if (bytes < 1024) {
    human = tr("%1 bytes").arg(bytes);
  } else {
    double kb = bytes / 1024.0;
    human = tr("%1 KB (%2 bytes)")
                .arg(QLocale().toString(kb, 'f', kb < 10 ? 1 : 0),
                     QLocale().toString(bytes));
  }
  info_->setText(tr("☰ %1 lines    ▤ %2").arg(totalLines).arg(human));
}
