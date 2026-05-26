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

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QThread>
#include <QTranslator>

#include <qawno.h>
#include "MainWindow.h"
#include "UpdateChecker.h"
#include "SettingsDialog.h"
#include "LocaleManager.h"
#include "TelemetryClient.h"
#include "ConsentDialog.h"
#include <QDateTime>
#include <QTimer>

#include <string.h>

class ColourTranslator : public QTranslator
{
public:
  QString translate(const char* context, const char* sourceText, const char* disambiguation, int n) const override {
    static QChar color[] = { 'c', 'o', 'l', 'o', 'r' };
    static QChar Color[] = { 'C', 'o', 'l', 'o', 'r' };
    static QChar COLOR[] = { 'C', 'O', 'L', 'O', 'R' };
    static QChar colour[] = { 'c', 'o', 'l', 'o', 'u', 'r' };
    static QChar Colour[] = { 'C', 'o', 'l', 'o', 'u', 'r' };
    static QChar COLOUR[] = { 'C', 'O', 'L', 'O', 'U', 'R' };
    return QString(sourceText).replace(color, 5, colour, 6).replace(Color, 5, Colour, 6).replace(COLOR, 5, COLOUR, 6);
  }
};

int main(int argc, char **argv) {
  // Show QAction icons in menus (Qt/macOS hides them by default).
  QApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
  QApplication app(argc, argv);

  QCoreApplication::setApplicationName("Qawno");
  QCoreApplication::setApplicationVersion(QAWNO_VERSION_STRING);
  QCoreApplication::setOrganizationName("Zeex");
  QCoreApplication::setOrganizationDomain("zeex.github.io");
  QCoreApplication::installTranslator(new ColourTranslator());
  LocaleManager::instance();  // loads the saved-language .qm at startup

  // App-wide UI font — prefer the user's saved choice, fall back to the
  // platform's modern system UI font. Editor + output keep their own
  // monospace fonts; this applies to chrome (menus, dialogs, sidebar, etc).
  {
    QSettings s;
    const QString appFamily = s.value("AppFont").toString();
    QFont uiFont;
    if (!appFamily.isEmpty()) {
      uiFont.setFamily(appFamily);
    } else {
#ifdef Q_OS_MACOS
      uiFont.setFamily("SF Pro Text");  // macOS Big Sur+ system UI font
#elif defined(Q_OS_WIN)
      uiFont.setFamily("Segoe UI");
#else
      uiFont.setFamily("Inter");
#endif
    }
    uiFont.setPointSize(s.value("AppFontSize", 13).toInt());
    app.setFont(uiFont);
  }

  // Window/dock/cmd-tab icon (the bundle's .icns covers Finder; this covers the
  // running app's title bar and task switcher).
  app.setWindowIcon(QIcon(":/assets/images/icon_128x128.png"));

#ifdef Q_OS_MACOS
  // macOS auto-injects "Start Dictation" and "Emoji & Symbols" into the Edit
  // menu of any text app. Disable them by writing to the app's preferences
  // domain (must match the bundle identifier so AppKit's standardUserDefaults
  // picks them up). Takes effect on the next launch.
  {
    QSettings macDefaults("io.openmultiplayer.qawno", QString());
    macDefaults.setValue("NSDisabledDictationMenuItem", true);
    macDefaults.setValue("NSDisabledCharacterPaletteMenuItem", true);
    macDefaults.sync();
  }
#endif

#ifdef Q_OS_MACOS
  // A .app launched from Finder/Dock starts with the working directory set to
  // "/". qawno loads its templates (gamemode.new, ...) and scans ./include for
  // the natives list relative to the working directory, so point it at the
  // folder that contains the .app bundle, where those resources live.
  {
    QDir dir(QCoreApplication::applicationDirPath()); // Qawno.app/Contents/MacOS
    dir.cdUp();  // Contents
    dir.cdUp();  // Qawno.app
    dir.cdUp();  // folder containing Qawno.app
    QDir::setCurrent(dir.absolutePath());
  }
#endif

  // Single-instance gate via QLocalServer. If another Qawno is already running,
  // ask the user whether to terminate it and take over, or just abort.
  static const QString kSocketName = "io.openmultiplayer.qawno.single";
  {
    QLocalSocket probe;
    probe.connectToServer(kSocketName);
    if (probe.waitForConnected(150)) {
      // Already running. Send a "raise" ping; offer to terminate it.
      probe.write("raise\n");
      probe.flush();
      probe.waitForBytesWritten(150);
      probe.disconnectFromServer();

      QMessageBox box;
      box.setWindowTitle("Qawno");
      box.setIcon(QMessageBox::Question);
      box.setText("Qawno is already running.");
      box.setInformativeText("Close the running instance and open a new one, or cancel?");
      QPushButton* closeOther = box.addButton("Close running & open new", QMessageBox::AcceptRole);
      QPushButton* cancelBtn  = box.addButton(QMessageBox::Cancel);
      box.exec();
      if (box.clickedButton() == closeOther) {
        // Tell the existing instance to quit, then re-acquire the socket.
        QLocalSocket kill;
        kill.connectToServer(kSocketName);
        if (kill.waitForConnected(150)) {
          kill.write("quit\n");
          kill.flush();
          kill.waitForBytesWritten(150);
          kill.disconnectFromServer();
        }
        // Give the previous server a beat to release the socket file.
        for (int i = 0; i < 20; ++i) {
          QLocalSocket check;
          check.connectToServer(kSocketName);
          if (!check.waitForConnected(50)) break;
          check.disconnectFromServer();
          QThread::msleep(50);
        }
        // Continue to start a fresh instance below.
      } else {
        return 0;
      }
    }
  }

  QLocalServer::removeServer(kSocketName);
  QLocalServer* server = new QLocalServer(&app);
  server->listen(kSocketName);

  MainWindow mainWindow;
  // Handle pings from a second-instance attempt.
  QObject::connect(server, &QLocalServer::newConnection, &mainWindow,
                   [server, &mainWindow] {
    while (QLocalSocket* c = server->nextPendingConnection()) {
      QObject::connect(c, &QLocalSocket::readyRead, c, [c, &mainWindow] {
        const QByteArray cmd = c->readAll().trimmed();
        if (cmd == "raise") {
          mainWindow.show();
          mainWindow.raise();
          mainWindow.activateWindow();
        } else if (cmd == "quit") {
          QCoreApplication::quit();
        }
        c->disconnectFromServer();
      });
    }
  });

  mainWindow.show();

  // First-run telemetry consent prompt. Shown once; the user's choice is
  // persisted so subsequent launches skip the dialog. Settings → Privacy
  // exposes the toggle so it can be flipped later.
  QTimer::singleShot(400, &mainWindow, [&mainWindow] {
    auto* tc = TelemetryClient::instance();
    if (tc->consent() == TelemetryClient::Consent::Granted) {
      tc->trackSessionStart();
      return;
    }
    if (tc->consent() == TelemetryClient::Consent::Denied) return;
    ConsentDialog dlg(&mainWindow);
    dlg.exec();
    tc->setConsent(dlg.outcome() == ConsentDialog::Granted
                    ? TelemetryClient::Consent::Granted
                    : TelemetryClient::Consent::Denied);
    if (dlg.outcome() == ConsentDialog::Granted) tc->trackSessionStart();
  });

  // Daily update probe: if the last successful check is older than ~24h
  // (or has never run), kick off a background check shortly after startup.
  // The 3s delay keeps the network call off the critical path of the first
  // paint, which matters on cold-launch.
  QTimer::singleShot(3000, &app, [] {
    QSettings s;
    const qint64 last = s.value("UpdatesLastCheckMs", 0).toLongLong();
    const qint64 now  = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kDayMs = 24LL * 60 * 60 * 1000;
    if (now - last < kDayMs) return;
    s.setValue("UpdatesLastCheckMs", now);
    UpdateChecker::instance()->checkAll();
  });

  // Auto-download newly-found releases when the setting is on. Hooked once,
  // process-wide; downloadAsset() is idempotent for already-fetched files.
  QObject::connect(UpdateChecker::instance(), &UpdateChecker::releaseFetched,
                   &app, [](UpdateChecker::Component which, UpdateChecker::Release) {
    if (!SettingsDialog::autoDownloadUpdates()) return;
    UpdateChecker::instance()->downloadAsset(which);
  });

  return app.exec();
}
