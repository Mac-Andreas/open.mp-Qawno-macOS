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
//
// You should have received a copy of the GNU General Public License
// along with qawno. If not, see <http://www.gnu.org/licenses/>.

#include "CrossOver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

const QString CrossOver::kBottle = QStringLiteral("Qawno");
const QString CrossOver::kBottleTemplate = QStringLiteral("win10");

QString CrossOver::appPath() {
  // The fixed install location; CrossOver always lives in /Applications.
  static const QString fixed = QStringLiteral("/Applications/CrossOver.app");
  if (QFileInfo::exists(fixed)) {
    return fixed;
  }
  return QString();
}

QString CrossOver::cxRoot() {
  const QString app = appPath();
  if (app.isEmpty()) {
    return QString();
  }
  return app + QStringLiteral("/Contents/SharedSupport/CrossOver");
}

QString CrossOver::winePath() {
  const QString root = cxRoot();
  if (root.isEmpty()) {
    return QString();
  }
  const QString wine = root + QStringLiteral("/bin/wine");
  return QFileInfo(wine).isExecutable() ? wine : QString();
}

QString CrossOver::cxBottlePath() {
  const QString root = cxRoot();
  if (root.isEmpty()) {
    return QString();
  }
  const QString cxbottle = root + QStringLiteral("/bin/cxbottle");
  return QFileInfo(cxbottle).isExecutable() ? cxbottle : QString();
}

bool CrossOver::isInstalled() {
  return !appPath().isEmpty();
}

QString CrossOver::version() {
  const QString app = appPath();
  if (app.isEmpty()) {
    return QString();
  }
  QSettings plist(app + QStringLiteral("/Contents/Info.plist"),
                  QSettings::NativeFormat);
  return plist.value(QStringLiteral("CFBundleShortVersionString")).toString();
}

QString CrossOver::bottlesDir() {
  // CrossOver keeps per-user bottles under Application Support, not the app.
  const QString home =
      QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  return home + QStringLiteral("/Library/Application Support/CrossOver/Bottles");
}

QString CrossOver::bottlePath() {
  return bottlesDir() + QStringLiteral("/") + kBottle;
}

bool CrossOver::bottleExists() {
  // A created bottle is a directory containing a Wine prefix (system.reg).
  const QString reg = bottlePath() + QStringLiteral("/system.reg");
  return QFileInfo::exists(reg);
}

bool CrossOver::createBottle(QString* error) {
  if (bottleExists()) {
    return true;
  }
  const QString cxbottle = cxBottlePath();
  if (cxbottle.isEmpty()) {
    if (error) {
      *error = QObject::tr("cxbottle not found; is CrossOver installed?");
    }
    return false;
  }

  QProcess proc;
  proc.setProcessChannelMode(QProcess::MergedChannels);
  // cxbottle resolves the engine relative to CX_ROOT.
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("CX_ROOT"), cxRoot());
  proc.setProcessEnvironment(env);

  // 32-bit Win10 bottle. cxbottle picks a 32-bit prefix from the template;
  // open.mp's toolchain requires it.
  proc.start(cxbottle, {QStringLiteral("--create"),
                        QStringLiteral("--bottle"), kBottle,
                        QStringLiteral("--template"), kBottleTemplate});
  if (!proc.waitForStarted(10000)) {
    if (error) {
      *error = QObject::tr("Could not start cxbottle.");
    }
    return false;
  }
  // Bottle creation runs the Wine first-boot; allow a few minutes.
  proc.waitForFinished(300000);

  if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0 ||
      !bottleExists()) {
    if (error) {
      *error = QString::fromLocal8Bit(proc.readAll());
      if (error->isEmpty()) {
        *error = QObject::tr("cxbottle exited with code %1.")
                     .arg(proc.exitCode());
      }
    }
    return false;
  }
  return true;
}

bool CrossOver::deleteBottle(QString* error) {
  if (!bottleExists()) {
    return true;
  }
  // cxbottle --delete removes it cleanly (kills the prefix's wineserver too).
  const QString cxbottle = cxBottlePath();
  if (!cxbottle.isEmpty()) {
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("CX_ROOT"), cxRoot());
    proc.setProcessEnvironment(env);
    proc.start(cxbottle,
               {QStringLiteral("--delete"), QStringLiteral("--bottle"), kBottle});
    if (proc.waitForStarted(10000)) {
      proc.waitForFinished(120000);
    }
  }
  // Fall back to removing the directory if cxbottle didn't (or isn't present).
  if (bottleExists()) {
    QDir(bottlePath()).removeRecursively();
  }
  if (bottleExists()) {
    if (error) {
      *error = QObject::tr("Could not remove the bottle directory.");
    }
    return false;
  }
  return true;
}

QStringList CrossOver::requiredFiles() {
  // Windows compiler binaries that must live in the project's qawno/ folder:
  // pawncc.exe needs pawnc.dll next to it. The pawn-cc.sh wrapper is deployed
  // by Qawno at compile time, so it isn't listed.
  return {QStringLiteral("pawncc.exe"),
          QStringLiteral("pawnc.dll")};
}

QString CrossOver::projectQawnoDir(const QString& pwnFile) {
  if (pwnFile.isEmpty()) {
    return QString();
  }
  // Walk upward from the .pwn file looking for a qawno/ folder. Typical
  // open.mp project layouts put it at the project root next to gamemodes/,
  // filterscripts/, includes/ — not directly beside every .pwn. Fall back
  // to <pwnDir>/qawno as the search target if none found, so the existing
  // error message still points at a sensible path.
  QDir d(QFileInfo(pwnFile).absolutePath());
  for (int i = 0; i < 6; ++i) {
    const QString candidate = d.absoluteFilePath(QStringLiteral("qawno"));
    if (QFileInfo(candidate).isDir()) {
      return candidate;
    }
    if (!d.cdUp()) break;
  }
  return QFileInfo(pwnFile).absolutePath() + QStringLiteral("/qawno");
}

QStringList CrossOver::missingFiles(const QString& qawnoDir) {
  QStringList missing;
  if (qawnoDir.isEmpty() || !QFileInfo(qawnoDir).isDir()) {
    // Whole folder absent — every required file is missing.
    return requiredFiles();
  }
  for (const QString& f : requiredFiles()) {
    if (!QFileInfo::exists(qawnoDir + QStringLiteral("/") + f)) {
      missing << f;
    }
  }
  return missing;
}
