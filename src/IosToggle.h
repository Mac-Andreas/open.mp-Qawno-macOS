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

#ifndef IOSTOGGLE_H
#define IOSTOGGLE_H

#include <QAbstractButton>

// iOS-style pill toggle: a rounded track with a sliding knob. Toggle on click,
// emits toggled(bool) like a QCheckBox. Sized to its sizeHint by default.
class IosToggle : public QAbstractButton {
  Q_OBJECT
 public:
  explicit IosToggle(QWidget* parent = nullptr);
  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent*) override;
};

#endif // IOSTOGGLE_H
