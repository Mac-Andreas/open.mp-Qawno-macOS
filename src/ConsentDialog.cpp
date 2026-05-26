// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "ConsentDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ConsentDialog::ConsentDialog(QWidget* parent) : QDialog(parent) {
  setModal(true);
  setWindowTitle(tr("Help improve Qawno"));
  setMinimumWidth(440);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(24, 22, 24, 18);
  root->setSpacing(12);

  auto* heading = new QLabel(tr("Anonymous usage data"));
  QFont f = heading->font(); f.setPointSize(f.pointSize() + 3); f.setBold(true);
  heading->setFont(f);
  root->addWidget(heading);

  auto* body = new QLabel(tr(
      "Qawno can send anonymous usage metrics (feature counts, error rates, "
      "OS version) to help us prioritise fixes and improvements.\n\n"
      "Nothing identifying you, your projects, or your code is ever sent. "
      "A random per-install ID lets us count unique users without knowing "
      "who they are. You can change this any time in Settings → Privacy."));
  body->setWordWrap(true);
  root->addWidget(body);

  auto* link = new QLabel(tr("<a href=\"https://mac-andreas.github.io/dashboard\">"
                              "See what we collect →</a>"));
  link->setOpenExternalLinks(true);
  link->setTextInteractionFlags(Qt::TextBrowserInteraction);
  root->addWidget(link);

  auto* btnRow = new QHBoxLayout;
  btnRow->addStretch(1);
  auto* deny  = new QPushButton(tr("No thanks"));
  auto* grant = new QPushButton(tr("Share anonymous data"));
  grant->setDefault(true);
  grant->setStyleSheet(
      "QPushButton { background:#3D7CB8; color:white; border:none; "
      "border-radius:8px; padding:8px 18px; font-weight:600; }"
      "QPushButton:hover { background:#4A8AC8; }");
  deny->setStyleSheet(
      "QPushButton { background:transparent; color:palette(text); "
      "border:1px solid palette(mid); border-radius:8px; padding:8px 18px; }"
      "QPushButton:hover { background:rgba(127,140,160,0.15); }");
  btnRow->addWidget(deny);
  btnRow->addWidget(grant);
  root->addLayout(btnRow);

  connect(deny,  &QPushButton::clicked, this, [this] { outcome_ = Denied;  accept(); });
  connect(grant, &QPushButton::clicked, this, [this] { outcome_ = Granted; accept(); });
}
