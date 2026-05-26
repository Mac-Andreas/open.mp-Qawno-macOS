// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef ICONLOADER_H
#define ICONLOADER_H

#include <QColor>
#include <QIcon>
#include <QSize>
#include <QString>

// Loads an SVG icon and recolours every fill/stroke to the theme's foreground
// colour. Renders at 2× the requested size for crisp output on retina
// displays, then bakes both 1× and 2× pixmaps into the returned QIcon so Qt
// picks the right resolution automatically.
class IconLoader {
 public:
  // dark=true → white tint; dark=false → near-black tint. Pass requested size
  // (typically 14–18 for toolbar icons).
  static QIcon themed(const QString& resourcePath, bool dark, QSize size = QSize(16, 16));
};

#endif // ICONLOADER_H
