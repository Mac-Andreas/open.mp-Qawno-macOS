// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef UPDATEPROMPTDIALOG_H
#define UPDATEPROMPTDIALOG_H

#include <QDialog>
#include <QStringList>

class QLabel;
class QPushButton;
class QRadioButton;

// "Continue?" modal shown when the user clicks the Update button. Greys out
// the background by virtue of being application-modal. Lets the user defer
// or proceed; if unsaved tabs exist, asks Save vs Discard before enabling the
// proceed button.
class UpdatePromptDialog : public QDialog {
  Q_OBJECT
 public:
  enum Outcome {
    Postpone,        // user chose "Update later"
    ProceedSave,     // proceed and save all dirty buffers first
    ProceedDiscard,  // proceed and discard dirty buffers
    ProceedNoUnsaved // no unsaved work — just proceed
  };

  UpdatePromptDialog(const QStringList& unsavedTitles,
                     const QString& releaseTag,
                     QWidget* parent = nullptr);

  Outcome outcome() const { return outcome_; }

 private:
  void rebuildState();

  Outcome outcome_ = Postpone;
  QStringList unsaved_;
  QRadioButton* saveOpt_ = nullptr;
  QRadioButton* discardOpt_ = nullptr;
  QPushButton* proceedBtn_ = nullptr;
  QPushButton* laterBtn_ = nullptr;
};

#endif // UPDATEPROMPTDIALOG_H
