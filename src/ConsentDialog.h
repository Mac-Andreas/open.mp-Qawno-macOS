// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef CONSENTDIALOG_H
#define CONSENTDIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;

// First-run / on-demand privacy prompt for anonymous telemetry. Returns
// Granted or Denied; the chosen value is persisted to QSettings by the
// caller via TelemetryClient::setConsent so re-launches don't re-ask.
class ConsentDialog : public QDialog {
  Q_OBJECT
 public:
  enum Outcome { Granted, Denied };
  explicit ConsentDialog(QWidget* parent = nullptr);
  Outcome outcome() const { return outcome_; }

 private:
  Outcome outcome_ = Denied;
};

#endif // CONSENTDIALOG_H
