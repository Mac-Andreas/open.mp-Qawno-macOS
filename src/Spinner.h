// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef SPINNER_H
#define SPINNER_H

#include <QWidget>

// Small indeterminate spinner — three orbiting dots — painted by hand so it
// stays crisp at any size and doesn't need a GIF/movie asset shipped with
// the bundle.
class Spinner : public QWidget {
  Q_OBJECT
 public:
  explicit Spinner(QWidget* parent = nullptr);
  void start();
  void stop();

  void setSpinSize(int px);
  void setColor(const QColor& c);

 protected:
  void paintEvent(QPaintEvent*) override;

 private:
  int angle_ = 0;
  int size_ = 18;
  QColor color_;
};

#endif // SPINNER_H
