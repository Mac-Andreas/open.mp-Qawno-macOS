// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "LocaleManager.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTranslator>

LocaleManager* LocaleManager::instance() {
  // Heap-allocated + intentionally leaked. QObject-derived singletons can't
  // be `static` safely: parenting to qApp causes a double-free (Qt deletes
  // via parent chain, then static dtor runs and frees again), while leaving
  // the static dtor with no parent crashes the network stack because the
  // event loop is already gone. Letting the OS reclaim the heap at process
  // exit is the simplest correct option.
  static LocaleManager* s_instance = new LocaleManager(nullptr);
  return s_instance;
}

LocaleManager::LocaleManager(QObject* parent) : QObject(parent) {
  // Order matches the open.mp launcher's macOS locale picker as of 2026-05.
  // Add a new entry here and drop the matching <code>.qm into the resource
  // bundle to ship a new translation.
  locales_ = {
    {"en",    "English",                 ":/assets/images/flags/en.svg"},
    {"ru",    "Русский",                 ":/assets/images/flags/ru.svg"},
    {"es",    "Español",                 ":/assets/images/flags/es.svg"},
    {"pt_BR", "Português (Brasil)",      ":/assets/images/flags/pt_BR.svg"},
    {"pt",    "Português",               ":/assets/images/flags/pt.svg"},
    {"de",    "Deutsch",                 ":/assets/images/flags/de.svg"},
    {"fr",    "Français",                ":/assets/images/flags/fr.svg"},
    {"pl",    "Polski",                  ":/assets/images/flags/pl.svg"},
    {"it",    "Italiano",                ":/assets/images/flags/it.svg"},
    {"tr",    "Türkçe",                  ":/assets/images/flags/tr.svg"},
    {"nl",    "Nederlands",              ":/assets/images/flags/nl.svg"},
    {"zh_CN", "简体中文",                  ":/assets/images/flags/zh_CN.svg"},
    {"zh_TW", "繁體中文",                  ":/assets/images/flags/zh_TW.svg"},
    {"ko",    "한국어",                    ":/assets/images/flags/ko.svg"},
    {"ja",    "日本語",                    ":/assets/images/flags/ja.svg"},
    {"ar",    "العربية",                 ":/assets/images/flags/ar.svg"},
    {"hi",    "हिन्दी",                    ":/assets/images/flags/hi.svg"},
    {"id",    "Bahasa Indonesia",        ":/assets/images/flags/id.svg"},
    {"th",    "ไทย",                      ":/assets/images/flags/th.svg"},
    {"vi",    "Tiếng Việt",              ":/assets/images/flags/vi.svg"},
    {"uk",    "Українська",              ":/assets/images/flags/uk.svg"},
    {"ro",    "Română",                  ":/assets/images/flags/ro.svg"},
    {"cs",    "Čeština",                 ":/assets/images/flags/cs.svg"},
    {"hu",    "Magyar",                  ":/assets/images/flags/hu.svg"},
    {"sv",    "Svenska",                 ":/assets/images/flags/sv.svg"},
    {"fi",    "Suomi",                   ":/assets/images/flags/fi.svg"},
    {"da",    "Dansk",                   ":/assets/images/flags/da.svg"},
    {"nb",    "Norsk Bokmål",            ":/assets/images/flags/nb.svg"},
    {"el",    "Ελληνικά",                ":/assets/images/flags/el.svg"},
  };

  install(currentCode());
}

const QVector<LocaleManager::Locale>& LocaleManager::locales() const {
  return locales_;
}

QString LocaleManager::currentCode() const {
  return QSettings().value("Language", "en").toString();
}

void LocaleManager::setCurrent(const QString& code) {
  QSettings().setValue("Language", code);
  install(code);
  emit localeChanged(code);
}

void LocaleManager::install(const QString& code) {
  auto* app = QCoreApplication::instance();
  if (current_) {
    app->removeTranslator(current_);
    delete current_;
    current_ = nullptr;
  }
  if (code == "en") return;  // English is the source language — no .qm needed
  current_ = new QTranslator(app);
  if (current_->load(QString(":/i18n/qawno_%1.qm").arg(code))) {
    app->installTranslator(current_);
  } else {
    delete current_;
    current_ = nullptr;
  }
}
