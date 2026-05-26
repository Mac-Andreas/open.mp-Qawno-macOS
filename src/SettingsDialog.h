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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QFont>
#include <QList>
#include <QPoint>
#include <QString>

class QFrame;

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QToolButton;
class IosToggle;

// Frameless, in-window Settings popup. Modelled after FindDialog:
//  - No system chrome (no traffic-light min/max).
//  - Single X close button on a custom drag-able title bar.
//  - Auto-applies every change (no Apply / OK / Cancel buttons).
//  - Settings persisted in QSettings; theme/fonts pushed into open editors
//    via the applied() signal MainWindow subscribes to.
class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(QWidget* parent = nullptr);
  ~SettingsDialog() override;

  // Initial values from caller — editor + output fonts live on the widgets
  // themselves, not in QSettings; the dialog round-trips them through these.
  void setEditorFont(const QFont& f);
  void setOutputFont(const QFont& f);
  QFont editorFont() const;
  QFont outputFont() const;

  void applyTheme(bool dark);

  static bool fullPathInOutput();
  static QString syntaxTheme();
  static int    uiThemeMode();
  static bool showToolbarText();
  static bool autoDownloadUpdates();
  static bool reopenFilesAfterUpdate();

 signals:
  void applied();
  // Emitted when the user clicks the Update button in the Updates pane.
  // MainWindow handles the prompt + relaunch dance.
  void updateRequested();

 protected:
  bool eventFilter(QObject* obj, QEvent* ev) override;
  void showEvent(QShowEvent*) override;

 private:
  void buildUi();
  void loadFromSettings();
  void save();  // pushes every value into QSettings + emits applied()
  void resetDefaults();
  QStringList monospaceFontsForPicker() const;

  // ---- title bar ----
  QWidget* titleBar_ = nullptr;
  QLabel* titleLabel_ = nullptr;
  QPushButton* closeBtn_ = nullptr;
  // ---- general tab ----
  IosToggle* fullPath_ = nullptr;
  IosToggle* showToolbarText_ = nullptr;
  IosToggle* hoverTooltips_ = nullptr;
  IosToggle* lintSquiggles_ = nullptr;
  QPushButton* resetBtn_ = nullptr;
  // ---- appearance tab ----
  QToolButton* themeLight_ = nullptr;
  QToolButton* themeDark_ = nullptr;
  QToolButton* themeSystem_ = nullptr;
  QComboBox* syntaxTheme_ = nullptr;
  // ---- fonts tab ----
  QComboBox* editorFontCombo_ = nullptr;
  QComboBox* outputFontCombo_ = nullptr;
  QFont editorFont_;
  QFont outputFont_;
  // ---- compiler tab ----
  QLineEdit* compilerPath_ = nullptr;
  QLineEdit* compilerOptions_ = nullptr;
  // ---- updates tab ----
  QLabel* compilerCurrentVer_ = nullptr;
  QPushButton* compilerUpdateBtn_ = nullptr;
  QLabel* qawnoCurrentVer_ = nullptr;
  QPushButton* qawnoUpdateBtn_ = nullptr;
  IosToggle* autoDownload_ = nullptr;
  IosToggle* reopenAfterUpdate_ = nullptr;
  QTabWidget* tabs_ = nullptr;
  QListWidget* nav_ = nullptr;
  QStackedWidget* pages_ = nullptr;
  QList<QFrame*> cards_;
  struct HeaderIcon { QLabel* label; QString path; };
  QList<HeaderIcon> pageHeaderIcons_;

  // dragging
  bool dragging_ = false;
  QPoint dragOffset_;
  bool blockSave_ = false;
};

#endif // SETTINGSDIALOG_H
