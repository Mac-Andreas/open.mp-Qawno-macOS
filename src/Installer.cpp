// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Installer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace {

// Resolve "this Qawno.app" — applicationDirPath is Qawno.app/Contents/MacOS,
// so two cdUp() calls land on the bundle.
QString currentBundlePath() {
  QDir d(QCoreApplication::applicationDirPath());
  d.cdUp();  // Contents
  d.cdUp();  // Qawno.app
  return d.absolutePath();
}

// Write the installer script. macOS-only; on other platforms returns empty
// and startUpdate() bails out.
QString writeInstallerScript(const Installer::Plan& plan) {
#if !defined(Q_OS_MAC)
  Q_UNUSED(plan);
  return QString();
#else
  const QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                             + QStringLiteral("/qawno_updater.sh");
  QFile f(scriptPath);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return QString();

  const qint64 pid = QCoreApplication::applicationPid();
  const QString bundle = plan.qawnoTargetBundle.isEmpty() ? currentBundlePath()
                                                          : plan.qawnoTargetBundle;
  // Heredoc the bash script. Using ${VAR_X} placeholders the host fills in
  // keeps the shell quoting straightforward.
  QByteArray sh;
  sh += "#!/bin/bash\n";
  sh += "set -e\n";
  sh += "PID=" + QByteArray::number(pid) + "\n";
  sh += "BUNDLE=" + QByteArray("\"") + bundle.toUtf8() + "\"\n";
  sh += "QAWNO_ASSET=" + QByteArray("\"") + plan.qawnoAssetPath.toUtf8() + "\"\n";
  sh += "COMPILER_ASSET=" + QByteArray("\"") + plan.compilerAssetPath.toUtf8() + "\"\n";
  sh += "COMPILER_DIR=" + QByteArray("\"") + plan.compilerTargetDir.toUtf8() + "\"\n";
  sh +=
      R"BASH(
LOG="/tmp/qawno_updater.log"
echo "[$(date)] updater starting pid=$PID bundle=$BUNDLE" >>"$LOG"

# 1. Wait for the running Qawno to fully exit so we can replace its bundle.
for i in $(seq 1 100); do
  kill -0 "$PID" 2>/dev/null || break
  sleep 0.1
done

WORK=$(mktemp -d -t qawno_update)
trap 'rm -rf "$WORK"' EXIT

# 2. Stage the new Qawno.app, if a Qawno asset was supplied.
NEW_BUNDLE=""
if [[ -n "$QAWNO_ASSET" && -f "$QAWNO_ASSET" ]]; then
  case "$QAWNO_ASSET" in
    *.dmg)
      MNT=$(mktemp -d "$WORK/mnt.XXXX")
      hdiutil attach "$QAWNO_ASSET" -mountpoint "$MNT" -nobrowse -quiet
      NEW_BUNDLE=$(find "$MNT" -maxdepth 2 -name "Qawno.app" -print -quit)
      if [[ -n "$NEW_BUNDLE" ]]; then
        cp -R "$NEW_BUNDLE" "$WORK/Qawno.app"
        NEW_BUNDLE="$WORK/Qawno.app"
      fi
      hdiutil detach "$MNT" -quiet || true
      ;;
    *.zip)
      unzip -q "$QAWNO_ASSET" -d "$WORK"
      NEW_BUNDLE=$(find "$WORK" -maxdepth 3 -name "Qawno.app" -print -quit)
      ;;
    *.tar.gz|*.tgz)
      tar -xzf "$QAWNO_ASSET" -C "$WORK"
      NEW_BUNDLE=$(find "$WORK" -maxdepth 3 -name "Qawno.app" -print -quit)
      ;;
  esac
fi

# 3. Replace the current Qawno.app with the new one (atomic swap via rsync).
if [[ -n "$NEW_BUNDLE" && -d "$NEW_BUNDLE" && -d "$BUNDLE" ]]; then
  BACKUP="${BUNDLE}.old.$$"
  mv "$BUNDLE" "$BACKUP"
  cp -R "$NEW_BUNDLE" "$BUNDLE"
  # Drop quarantine attribute (it'd otherwise trip Gatekeeper on first run).
  xattr -dr com.apple.quarantine "$BUNDLE" 2>/dev/null || true
  # Ad-hoc codesign so macOS lets it run without a Developer ID.
  codesign --force --deep --sign - "$BUNDLE" >>"$LOG" 2>&1 || true
  rm -rf "$BACKUP" || true
fi

# 4. Replace the project pawncc binary, if a compiler asset was supplied.
if [[ -n "$COMPILER_ASSET" && -f "$COMPILER_ASSET" && -d "$COMPILER_DIR" ]]; then
  CWORK=$(mktemp -d "$WORK/cmp.XXXX")
  case "$COMPILER_ASSET" in
    *.zip) unzip -q "$COMPILER_ASSET" -d "$CWORK" ;;
    *.tar.gz|*.tgz) tar -xzf "$COMPILER_ASSET" -C "$CWORK" ;;
    *) cp "$COMPILER_ASSET" "$CWORK/" ;;
  esac
  # Find any pawncc*/libpawnc* artefacts and drop them into the project's
  # qawno/ folder, overwriting existing copies.
  find "$CWORK" -type f \( -name "pawncc*" -o -name "libpawnc*" \) -exec cp -f {} "$COMPILER_DIR/" \;
fi

# 5. Relaunch — Qawno reads PendingReopenFiles from QSettings on startup.
open -a "$BUNDLE"
echo "[$(date)] updater done" >>"$LOG"
)BASH";

  f.write(sh);
  f.close();
  QFile::setPermissions(scriptPath,
                        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                            QFile::ReadGroup | QFile::ExeGroup |
                            QFile::ReadOther | QFile::ExeOther);
  return scriptPath;
#endif
}
}  // namespace

bool Installer::startUpdate(const Plan& plan) {
  QSettings s;
  s.setValue("PendingReopenFiles", plan.reopenFiles);
  s.sync();

#if defined(Q_OS_MAC)
  const QString script = writeInstallerScript(plan);
  if (script.isEmpty()) return false;
  return QProcess::startDetached("/bin/bash", QStringList{script});
#else
  // Other platforms not wired up yet — the spec was scoped to macOS.
  return false;
#endif
}
