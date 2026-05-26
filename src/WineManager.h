// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef WINEMANAGER_H
#define WINEMANAGER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

// Owns Qawno's self-managed Wine install. Wine is downloaded once from
// Gcenx/macOS_Wine_builds, extracted under <AppData>/Qawno/wine/, and reused
// for every pawncc compile via the pawn-cc.sh wrapper. The class exposes the
// install state + progress signals so the welcome card can drive its UI.
class WineManager : public QObject {
  Q_OBJECT
 public:
  enum class State { NotInstalled, Downloading, Installing, Ready, Error };

  static WineManager* instance();

  // Filesystem layout — under <appdir>/.Qawno/wine.
  QString appQawnoDir() const;   // <folder containing Qawno.app>/.Qawno
  QString rootDir() const;       // <appQawnoDir>/wine
  QString binPath() const;       // <rootDir>/Wine Staging.app/Contents/Resources/wine/bin/wine64
  QString prefixDir() const;     // <appQawnoDir>/wine/prefix

  // Quick truthy check the welcome card uses to decide which button to show.
  bool isReady() const;
  // Currently-installed wine version, derived from the downloaded archive's
  // tag at install time. Empty if no wine present yet.
  QString installedVersion() const;

  State state() const { return state_; }
  int progressPercent() const { return progressPercent_; }

  // Trigger a download of the asset already discovered by UpdateChecker for
  // the Wine component. Safe to call when in any state; a no-op while busy.
  void download();
  // Extract the previously-downloaded archive and bootstrap the prefix. Runs
  // after the download finishes or when the user explicitly clicks Install.
  void install();
  // Remove the installed Wine bundle + prefix. Leaves the downloaded archive
  // alone so a reinstall doesn't require a second 180 MB fetch.
  void uninstall();
  // Wipe the downloaded archive(s) from the cache directory. Cheap reclaim
  // of disk space; doesn't touch the installed Wine.
  void deleteCache();

 signals:
  void stateChanged(WineManager::State s);
  void progressChanged(int percent);
  void logLine(QString line);

 private:
  explicit WineManager(QObject* parent = nullptr);
  void setState(State s);
  void setProgress(int p);
  void onDownloadDone(const QString& path);
  void runInstaller(const QString& archivePath);

  State state_ = State::NotInstalled;
  int progressPercent_ = 0;
  QString installedVersion_;
};

#endif // WINEMANAGER_H
