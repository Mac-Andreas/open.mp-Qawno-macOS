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

#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>

#include "Compiler.h"

Compiler::Compiler() {
  QSettings settings;
  // Default compiler path: the native pawncc deployed into the project's
  // qawno/native/ subfolder. The native compiler runs directly on macOS
  // (Apple Silicon / Intel) and emits open.mp-compatible AMX (magic 0xF1E0),
  // so no Wine/CrossOver is involved in compiling.
  const QString kDefaultPath = "%p/qawno/native/pawncc";
  // Legacy Wine wrappers we transparently migrate away from.
  const QStringList kLegacyWinePaths = {
      "./run-pawncc-wine.sh", "%p/pawn-cc.sh", "%p/qawno/pawn-cc.sh",
      "%p/qawno/run-pawncc-wine.sh"};
  path_ = settings.value("CompilerPath", kDefaultPath).toString();
  // Migrate older Wine-based defaults to the native compiler.
  if (kLegacyWinePaths.contains(path_)) {
    path_ = kDefaultPath;
    settings.setValue("CompilerPath", path_);
  }
  // Default options: no "-r" report flag, so the compiler emits only the .amx
  // (the "-r" flag produced an extra .xml report next to the script).
  // Includes resolve from the project's qawno/include (same self-contained
  // layout) rather than from beside the Qawno binary.
  // Default includes use %P (project root, parent of qawno/) so layouts with
  // gamemodes/ + qawno/ siblings resolve correctly without per-project
  // customisation. The output path stays beside the input .pwn.
  const QString kDefaultOptions =
      "-;+ -(+ -\\ -Z- \"-i%p\" \"-i%P/qawno/include\" -d3 -t4 \"-o%p/%o\" \"%p/%i\"";
  QString stored = settings.value("CompilerOptions", kDefaultOptions).toString();
  // Strip a previously-saved "-r..." report flag from existing settings.
  stored.remove(QRegularExpression("\\s*\"?-r[^\"\\s]*\"?"));
  // Migrate stored "-i%q/include" (app-dir include) to the project layout.
  stored.replace("-i%q/include", "-i%P/qawno/include");
  // Migrate the older project-relative include path that assumed qawno/ was
  // a sibling of the .pwn — real layouts have it at the project root.
  stored.replace("-i%p/qawno/include", "-i%P/qawno/include");
  // Older default also passed "-i%p/%o" pointing at a directory named after
  // the .pwn (e.g. .../zombie/), which rarely exists. Switch to "-i%p" so
  // sibling includes of the .pwn are picked up.
  stored.replace("-i%p/%o", "-i%p");
  options_ = stored.split("\\s*");
}

Compiler::~Compiler() {
  QSettings settings;
  settings.setValue("CompilerPath", path_);
  settings.setValue("CompilerOptions", options_.join(" "));
}

QString Compiler::path() const {
  return path_;
}

void Compiler::setPath(const QString &path) {
  path_ = path;
}

QStringList Compiler::options() const {
  return options_;
}

void Compiler::setOptions(const QString &options) {
  options_ = options.split("\\s*");
}

void Compiler::setOptions(const QStringList &options) {
  options_ = options;
}

QString Compiler::output() const {
  return output_;
}

QString Compiler::command() const {
  // %p/%q tokens in path_ are project-/app-relative; without a project context
  // (no input file) leave them unsubstituted — settings UIs use this purely for
  // display.
  QFileInfo cmp(path_);
  QString program = cmp.isAbsolute()
                        ? path_
                        : QDir(QCoreApplication::applicationDirPath())
                              .absoluteFilePath(path_);
  return QString("\"%1\" %2").arg(program).arg(options_.join(" "));
}

QString Compiler::commandFor(const QString &inputFile) const {
  QString i = QFileInfo(inputFile).fileName();
  QString p = QFileInfo(inputFile).absolutePath();
  QString o = QFileInfo(inputFile).baseName();

  QString q = QCoreApplication::applicationDirPath();
  // %P = project root (parent of the discovered qawno/ folder). Falls back
  // to the .pwn's own directory when no qawno/ is found, preserving the old
  // single-folder behaviour.
  QString projectRoot = p;
  {
    QDir d(p);
    for (int n = 0; n < 6; ++n) {
      if (QFileInfo(d.absoluteFilePath("qawno")).isDir()) {
        projectRoot = d.absolutePath();
        break;
      }
      if (!d.cdUp()) break;
    }
  }
  // The compiler path itself may use the %p/%q/%d/%i/%o tokens (e.g. the new
  // default "%p/pawn-cc.sh" deploys the wrapper into the project folder so
  // the same script is reusable from a terminal). Expand tokens first, then
  // resolve a still-relative path against the app dir.
  QString resolvedPath = path_;
  resolvedPath.replace("%p", p).replace("%q", q).replace("%i", i).replace("%o", o);
  QFileInfo cmp = QFileInfo(resolvedPath);
  QString program = cmp.isAbsolute()
                        ? resolvedPath
                        : QDir(q).absoluteFilePath(resolvedPath);
  QString c = cmp.isAbsolute() ? cmp.absolutePath() : q + "/" + cmp.path();
  QString d = QDir::currentPath();

  // Add the input and output files to the command line.
  // Then replace `-r` with `-rfilename`.
  // The compiler path is quoted so a path with spaces stays one argument.
  return QString("\"%1\" %2")
    // Invoke the compiler (absolute, so CWD doesn't matter).
    .arg(program).arg(options_.join(" "))
    // Custom arguments.
    .replace("%c", c)
    .replace("%q", q)
    .replace("%d", d)
    .replace("%i", i)
    .replace("%o", o)
    // %P is the project root (parent of qawno/), %p stays as the .pwn's
    // own folder for backwards compatibility with existing CompilerOptions.
    .replace("%P", projectRoot)
    .replace("%p", p);
}

QString Compiler::detectVersion() const {
  // Resolve the compiler binary the same way commandFor() would, but without
  // an input file: %p has no meaning here, so any path containing it is
  // unusable for a version probe.
  QString q = QCoreApplication::applicationDirPath();
  QString resolved = path_;
  if (resolved.contains("%p") || resolved.contains("%i") || resolved.contains("%o")) {
    return QString();
  }
  resolved.replace("%q", q);
  QFileInfo cmp(resolved);
  QString program = cmp.isAbsolute() ? resolved
                                     : QDir(q).absoluteFilePath(resolved);
  if (!QFileInfo(program).exists()) return QString();

  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(program, QStringList(), QProcess::ReadOnly);
  if (!process.waitForFinished(3000)) {
    process.kill();
    return QString();
  }
  const QString out = QString::fromLocal8Bit(process.readAll());
  // pawncc banner: "Pawn compiler 3.10.10\t Copyright ..." — match the first
  // dotted version on the line.
  QRegularExpression re(R"(\b(\d+\.\d+(?:\.\d+){0,2})\b)");
  auto m = re.match(out);
  return m.hasMatch() ? m.captured(1) : QString();
}

void Compiler::run(const QString &inputFile) {
  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.setWorkingDirectory(QDir::currentPath());

  QString command = commandFor(inputFile);
  // startCommand parses the command line honouring quotes, so the quoted
  // compiler path (which may contain spaces) is treated as a single program.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  process.startCommand(command, QProcess::ReadOnly);
#else
  process.start(command, QProcess::ReadOnly);
#endif

  if (process.waitForFinished()) {
    output_ = process.readAll();
  } else {
    output_ = process.errorString();
  }
}
