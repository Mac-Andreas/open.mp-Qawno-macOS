// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef INSTALLER_H
#define INSTALLER_H

#include <QString>
#include <QStringList>

// Launches a detached shell script that:
//  - Waits for the current Qawno PID to exit.
//  - Replaces /Applications-style Qawno.app from the downloaded asset.
//  - Drops the macOS quarantine xattr and runs ad-hoc codesign so Gatekeeper
//    accepts the new bundle without a developer-signed pkg.
//  - Optionally overwrites the project's pawncc binary with the new asset.
//  - Re-launches Qawno (which restores tabs from QSettings).
class Installer {
 public:
  struct Plan {
    QString qawnoAssetPath;     // optional: zip/dmg of new Qawno.app
    QString qawnoTargetBundle;  // current Qawno.app path; resolved if empty
    QString compilerAssetPath;  // optional: zip/exe of new pawncc
    QString compilerTargetDir;  // current pawncc dir; resolved if empty
    QStringList reopenFiles;    // file paths to reopen after relaunch
  };

  // Persists reopen files in QSettings, writes the installer script to a
  // tmp path, and launches it detached. Returns true if the script was
  // started successfully — caller should then quit the app so the script
  // can take over.
  static bool startUpdate(const Plan& plan);
};

#endif // INSTALLER_H
