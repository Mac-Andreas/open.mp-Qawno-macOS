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

#include "IosToggle.h"

#include <QPainter>

IosToggle::IosToggle(QWidget* parent) : QAbstractButton(parent) {
  setCheckable(true);
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::TabFocus);
}

QSize IosToggle::sizeHint() const {
  return QSize(44, 24);
}

void IosToggle::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const bool on = isChecked();
  const QRectF r = rect().adjusted(1, 1, -1, -1);
  const qreal radius = r.height() / 2.0;
  const QColor onColor(0x34, 0xC7, 0x59);
  const QColor offColor(0x6E, 0x73, 0x7B);
  QColor track = on ? onColor : offColor;
  if (!isEnabled()) track.setAlphaF(0.4);
  p.setPen(Qt::NoPen);
  p.setBrush(track);
  p.drawRoundedRect(r, radius, radius);
  const qreal knobDiameter = r.height() - 4;
  const qreal x = on ? r.right() - knobDiameter - 2 : r.left() + 2;
  const qreal y = r.top() + 2;
  p.setBrush(Qt::white);
  p.drawEllipse(QRectF(x, y, knobDiameter, knobDiameter));
}
