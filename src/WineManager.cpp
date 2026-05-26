// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "WineManager.h"
#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace {
// Wine + caches live in a hidden ".Qawno" folder next to the Qawno.app bundle
// (not in ~/Library/Application Support). Keeps every dependency colocated
// with the app the user actually launched — uninstall = trash one folder.
QString appData() {
  QDir d(QCoreApplication::applicationDirPath());  // Qawno.app/Contents/MacOS
  d.cdUp();  // Contents
  d.cdUp();  // Qawno.app
  d.cdUp();  // folder containing Qawno.app
  return d.absoluteFilePath(".Qawno");
}
}  // namespace

WineManager* WineManager::instance() {
  // Heap-leaked — see LocaleManager for rationale.
  static WineManager* s_instance = new WineManager(nullptr);
  return s_instance;
}

WineManager::WineManager(QObject* parent) : QObject(parent) {
  QSettings s;
  installedVersion_ = s.value("WineInstalledVersion").toString();
  // Wire the download finish from UpdateChecker to our install pipeline so
  // the welcome card only needs to call download() and watch the state.
  connect(UpdateChecker::instance(), &UpdateChecker::downloadProgress, this,
          [this](UpdateChecker::Component c, qint64 bytes, qint64 total) {
            if (c != UpdateChecker::Component::Wine) return;
            if (total > 0) setProgress(int((bytes * 100) / total));
          });
  connect(UpdateChecker::instance(), &UpdateChecker::downloadFinished, this,
          [this](UpdateChecker::Component c, QString path) {
            if (c != UpdateChecker::Component::Wine) return;
            onDownloadDone(path);
          });
  connect(UpdateChecker::instance(), &UpdateChecker::downloadFailed, this,
          [this](UpdateChecker::Component c, QString err) {
            if (c != UpdateChecker::Component::Wine) return;
            emit logLine(tr("Download failed: %1").arg(err));
            setState(State::Error);
          });

  if (QFileInfo::exists(binPath())) {
    setState(State::Ready);
  }
}

QString WineManager::appQawnoDir() const {
  return appData();
}

QString WineManager::rootDir() const {
  return appData() + "/wine";
}

QString WineManager::binPath() const {
  // Gcenx archives extract to a Wine Staging.app bundle. The wine binary
  // lives at Contents/Resources/wine/bin/wine (no wine64; the new builds
  // are universal/x86_64-only depending on flavour).
  return rootDir() + "/Wine Staging.app/Contents/Resources/wine/bin/wine";
}

QString WineManager::prefixDir() const {
  return rootDir() + "/prefix";
}

bool WineManager::isReady() const {
  return state_ == State::Ready;
}

QString WineManager::installedVersion() const {
  return installedVersion_;
}

void WineManager::setState(State s) {
  if (state_ == s) return;
  state_ = s;
  emit stateChanged(s);
}

void WineManager::setProgress(int p) {
  if (progressPercent_ == p) return;
  progressPercent_ = p;
  emit progressChanged(p);
}

void WineManager::download() {
  if (state_ == State::Downloading || state_ == State::Installing) return;
  setProgress(0);
  setState(State::Downloading);
  emit logLine(tr("Fetching Wine release info…"));
  // UpdateChecker fetches metadata then we call downloadAsset once we know
  // there's a release. If we already have cached release data, skip straight
  // to download.
  auto* uc = UpdateChecker::instance();
  const auto rel = uc->latest(UpdateChecker::Component::Wine);
  if (rel.assetUrl.isEmpty()) {
    connect(uc, &UpdateChecker::releaseFetched, this,
            [uc](UpdateChecker::Component c, UpdateChecker::Release) {
              if (c == UpdateChecker::Component::Wine) {
                uc->downloadAsset(UpdateChecker::Component::Wine);
              }
            },
            Qt::SingleShotConnection);
    uc->check(UpdateChecker::Component::Wine);
  } else {
    uc->downloadAsset(UpdateChecker::Component::Wine);
  }
}

void WineManager::onDownloadDone(const QString& path) {
  emit logLine(tr("Download complete: %1").arg(path));
  // Auto-chain into install — user explicitly asked for wine, no reason to
  // make them click again.
  install();
}

void WineManager::install() {
  auto* uc = UpdateChecker::instance();
  const QString archive = uc->downloadedPath(UpdateChecker::Component::Wine);
  if (archive.isEmpty() || !QFileInfo::exists(archive)) {
    emit logLine(tr("No Wine archive to install"));
    setState(State::Error);
    return;
  }
  runInstaller(archive);
}

void WineManager::uninstall() {
  if (state_ == State::Downloading || state_ == State::Installing) {
    emit logLine(tr("Cannot uninstall while a download/install is in progress"));
    return;
  }
  QDir d(rootDir());
  if (d.exists()) {
    if (!d.removeRecursively()) {
      emit logLine(tr("Failed to remove %1").arg(rootDir()));
      setState(State::Error);
      return;
    }
  }
  installedVersion_.clear();
  QSettings().remove("WineInstalledVersion");
  setProgress(0);
  setState(State::NotInstalled);
  emit logLine(tr("Wine uninstalled"));
}

void WineManager::deleteCache() {
  const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QDir cache(base + "/updates/wine");
  if (cache.exists()) {
    cache.removeRecursively();
  }
  emit logLine(tr("Wine download cache cleared"));
}

void WineManager::runInstaller(const QString& archivePath) {
  setProgress(0);
  setState(State::Installing);
  emit logLine(tr("Extracting Wine into %1…").arg(rootDir()));
  QDir().mkpath(rootDir());

  // tar -xJf handles .tar.xz natively on macOS. Run detached-ish (synchronous
  // with progress tick) so the user sees a Installing… state while it runs.
  auto* proc = new QProcess(this);
  proc->setProgram("/usr/bin/tar");
  proc->setArguments({"-xJf", archivePath, "-C", rootDir()});
  connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this, proc](int code, QProcess::ExitStatus) {
            proc->deleteLater();
            if (code != 0) {
              emit logLine(tr("tar exited with code %1").arg(code));
              setState(State::Error);
              return;
            }
            // Strip macOS quarantine on every binary so Gatekeeper doesn't
            // block wine64.
            QProcess::execute("/usr/bin/xattr",
                              {"-dr", "com.apple.quarantine", rootDir()});
            // Record the installed version from the release tag.
            const auto rel = UpdateChecker::instance()->latest(
                UpdateChecker::Component::Wine);
            installedVersion_ = rel.tag.isEmpty() ? rel.version : rel.tag;
            QSettings().setValue("WineInstalledVersion", installedVersion_);
            if (QFileInfo::exists(binPath())) {
              emit logLine(tr("Wine ready at %1").arg(binPath()));
              setProgress(100);
              setState(State::Ready);
            } else {
              emit logLine(tr("Extraction finished but wine binary not found at %1")
                               .arg(binPath()));
              setState(State::Error);
            }
          });
  proc->start();
}
