// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "UpdatePromptDialog.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

UpdatePromptDialog::UpdatePromptDialog(const QStringList& unsavedTitles,
                                       const QString& releaseTag,
                                       QWidget* parent)
    : QDialog(parent), unsaved_(unsavedTitles) {
  setModal(true);
  setWindowTitle(tr("Install update"));
  setObjectName("updatePrompt");
  resize(460, 0);

  auto root = new QVBoxLayout(this);
  root->setContentsMargins(20, 20, 20, 16);
  root->setSpacing(12);

  auto heading = new QLabel(tr("Install Qawno %1?").arg(releaseTag.isEmpty() ? tr("update")
                                                                              : releaseTag));
  heading->setObjectName("updatePromptHeading");
  QFont f = heading->font(); f.setPointSize(f.pointSize() + 2); f.setBold(true);
  heading->setFont(f);
  root->addWidget(heading);

  auto body = new QLabel(tr("Installing closes Qawno and re-launches the new version."));
  body->setWordWrap(true);
  root->addWidget(body);

  if (!unsaved_.isEmpty()) {
    // Inline dotted-bordered notice for the unsaved-work decision — mirrors
    // the spec's "dotted rectangle rounded box" wording.
    auto notice = new QFrame;
    notice->setObjectName("updatePromptUnsaved");
    notice->setStyleSheet(
        "QFrame#updatePromptUnsaved { border: 1px dashed palette(mid); border-radius: 8px; padding: 10px; }");
    auto nv = new QVBoxLayout(notice);
    nv->setContentsMargins(10, 8, 10, 8);
    nv->setSpacing(6);
    auto titleRow = new QHBoxLayout;
    auto info = new QLabel(QStringLiteral("ⓘ"));
    info->setStyleSheet("font-size: 14px;");
    auto t = new QLabel(tr("You have unsaved changes in %n file(s).", "", int(unsaved_.size())));
    QFont tf = t->font(); tf.setBold(true); t->setFont(tf);
    titleRow->addWidget(info);
    titleRow->addWidget(t, 1);
    nv->addLayout(titleRow);
    auto names = new QLabel(unsaved_.join(", "));
    names->setWordWrap(true);
    nv->addWidget(names);
    auto group = new QButtonGroup(notice);
    saveOpt_    = new QRadioButton(tr("Save all and update"));
    discardOpt_ = new QRadioButton(tr("Discard changes and update"));
    group->addButton(saveOpt_);
    group->addButton(discardOpt_);
    nv->addWidget(saveOpt_);
    nv->addWidget(discardOpt_);
    root->addWidget(notice);
    connect(saveOpt_,    &QRadioButton::toggled, this, [this](bool){ rebuildState(); });
    connect(discardOpt_, &QRadioButton::toggled, this, [this](bool){ rebuildState(); });
  }

  auto btnRow = new QHBoxLayout;
  btnRow->addStretch(1);
  laterBtn_ = new QPushButton(tr("Update later"));
  proceedBtn_ = new QPushButton(unsaved_.isEmpty() ? tr("Close and continue")
                                                    : tr("Continue"));
  proceedBtn_->setDefault(true);
  btnRow->addWidget(laterBtn_);
  btnRow->addWidget(proceedBtn_);
  root->addLayout(btnRow);

  connect(laterBtn_, &QPushButton::clicked, this, [this] {
    outcome_ = Postpone;
    accept();
  });
  connect(proceedBtn_, &QPushButton::clicked, this, [this] {
    if (unsaved_.isEmpty()) outcome_ = ProceedNoUnsaved;
    else if (saveOpt_ && saveOpt_->isChecked()) outcome_ = ProceedSave;
    else if (discardOpt_ && discardOpt_->isChecked()) outcome_ = ProceedDiscard;
    else return;
    accept();
  });

  rebuildState();
}

void UpdatePromptDialog::rebuildState() {
  if (!proceedBtn_) return;
  if (unsaved_.isEmpty()) {
    proceedBtn_->setEnabled(true);
    return;
  }
  const bool decided = (saveOpt_ && saveOpt_->isChecked())
                    || (discardOpt_ && discardOpt_->isChecked());
  proceedBtn_->setEnabled(decided);
}
