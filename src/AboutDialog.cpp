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

#include "AboutDialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <qawno.h>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("About Qawno"));

  QHBoxLayout *top = new QHBoxLayout;
  top->setSpacing(18);

  // App icon.
  QLabel *icon = new QLabel;
  QPixmap pm(":/assets/images/icon_128x128.png");
  if (!pm.isNull()) {
    icon->setPixmap(pm.scaled(96, 96, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation));
  }
  icon->setAlignment(Qt::AlignTop);
  top->addWidget(icon, 0, Qt::AlignTop);

  // Name, version, description, credits.
  QLabel *text = new QLabel;
  text->setTextFormat(Qt::RichText);
  text->setOpenExternalLinks(true);
  text->setWordWrap(true);
  text->setText(tr(
      "<h2 style='margin:0'>Qawno</h2>"
      "<p style='margin:2px 0 10px 0; color:#888'>Version %1 · macOS</p>"
      "<p>The open.mp Pawn editor — a code editor for SA-MP / open.mp "
      "gamemodes, filterscripts and includes, with syntax highlighting, "
      "auto-completion from the native list, and one-click compiling.</p>"
      "<p>This macOS build is fully native (Apple Silicon and Intel) and ships "
      "a native Pawn compiler — no Wine or CrossOver required.</p>"
      "<p style='color:#888; margin-top:10px'>"
      "Copyright © 2011–2015 Zeex<br>"
      "Updated for open.mp, 2022 Y_Less<br>"
      "macOS port by xyranaut</p>")
      .arg(QAWNO_VERSION_STRING));
  text->setMinimumWidth(420);
  top->addWidget(text, 1);

  QVBoxLayout *root = new QVBoxLayout(this);
  root->setContentsMargins(20, 20, 20, 16);
  root->setSpacing(16);
  root->addLayout(top);

  // OK + About Qt (mirrors Qt's own about dialogs).
  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
  QPushButton *aboutQt = buttons->addButton(tr("About Qt"),
                                            QDialogButtonBox::HelpRole);
  connect(aboutQt, &QPushButton::clicked, qApp, &QApplication::aboutQt);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  root->addWidget(buttons);
}

AboutDialog::~AboutDialog() = default;
