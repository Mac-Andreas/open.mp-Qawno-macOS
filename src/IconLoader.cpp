// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "IconLoader.h"

#include <QByteArray>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSvgRenderer>

namespace {
QPixmap renderSvg(const QByteArray& svg, QSize size) {
  QSvgRenderer renderer(svg);
  if (!renderer.isValid()) return QPixmap();
  QPixmap pm(size);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  renderer.render(&p);
  return pm;
}
}  // namespace

QIcon IconLoader::themed(const QString& resourcePath, bool dark, QSize size) {
  QFile f(resourcePath);
  if (!f.open(QIODevice::ReadOnly)) return QIcon();
  QByteArray svg = f.readAll();
  f.close();

  const char* tint = dark ? "#E5E9EF" : "#1A1E25";
  QString s = QString::fromUtf8(svg);
  // Replace every fill="#..." / stroke="#..." that isn't transparent/none.
  static QRegularExpression fillRx(
      "(fill|stroke)\\s*=\\s*\"(#[0-9a-fA-F]+|currentColor)\"");
  s.replace(fillRx, QString("\\1=\"%1\"").arg(tint));
  // Some SVGs declare colour inside style="fill: #xxx" instead.
  static QRegularExpression styleRx(R"((fill|stroke)\s*:\s*#[0-9a-fA-F]+)");
  s.replace(styleRx, QString("\\1:%1").arg(tint));
  svg = s.toUtf8();

  QIcon icon;
  // Render at requested size and at 2× for retina; QIcon will pick whichever
  // matches the device pixel ratio of the receiving widget.
  icon.addPixmap(renderSvg(svg, size));
  icon.addPixmap(renderSvg(svg, QSize(size.width() * 2, size.height() * 2)));
  return icon;
}
