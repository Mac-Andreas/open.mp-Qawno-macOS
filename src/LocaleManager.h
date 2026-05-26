// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef LOCALEMANAGER_H
#define LOCALEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QTranslator;

// Locale registry + .qm loader. The list of locales mirrors the open.mp
// launcher's language picker so users see the same options Qawno-side.
class LocaleManager : public QObject {
  Q_OBJECT
 public:
  struct Locale {
    QString code;       // BCP47-ish, e.g. "en", "pt_BR", "zh_CN"
    QString nativeName; // shown in the dropdown, e.g. "Português (Brasil)"
    QString flagAsset;  // ":/assets/images/flags/<code>.svg" (may not exist yet)
  };

  static LocaleManager* instance();

  const QVector<Locale>& locales() const;

  // Currently active locale code (defaults to QSettings("Language") or "en").
  QString currentCode() const;

  // Switch locale immediately + persist. Re-installs the matching QTranslator
  // and emits localeChanged so widgets can refresh visible strings.
  void setCurrent(const QString& code);

 signals:
  void localeChanged(QString code);

 private:
  explicit LocaleManager(QObject* parent = nullptr);
  void install(const QString& code);

  QVector<Locale> locales_;
  QTranslator* current_ = nullptr;
};

#endif // LOCALEMANAGER_H
