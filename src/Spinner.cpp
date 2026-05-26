// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Spinner.h"

#include <QPainter>
#include <QTimer>

Spinner::Spinner(QWidget* parent) : QWidget(parent), color_(Qt::white) {
  setSpinSize(18);
  auto* t = new QTimer(this);
  connect(t, &QTimer::timeout, this, [this] {
    angle_ = (angle_ + 12) % 360;
    update();
  });
  t->start(33);  // ~30 fps
}

void Spinner::start() { show(); }
void Spinner::stop()  { hide(); }

void Spinner::setSpinSize(int px) {
  size_ = px;
  setFixedSize(px, px);
}

void Spinner::setColor(const QColor& c) {
  color_ = c;
  update();
}

void Spinner::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.translate(width() / 2.0, height() / 2.0);
  p.rotate(angle_);
  // 8-dot orbit with fading opacity — the classic "tail" spinner.
  const int dots = 8;
  const qreal r = size_ * 0.32;
  const qreal d = qMax<qreal>(2.0, size_ * 0.16);
  for (int i = 0; i < dots; ++i) {
    QColor c = color_;
    c.setAlphaF(0.15 + 0.85 * (i / qreal(dots - 1)));
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.save();
    p.rotate(i * (360.0 / dots));
    p.drawEllipse(QPointF(r, 0), d / 2.0, d / 2.0);
    p.restore();
  }
}
