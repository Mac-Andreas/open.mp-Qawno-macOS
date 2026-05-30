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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStack>
#include <QListWidget>
#include <QFileSystemWatcher>
#include "Server.h"
#include "EditorWidget.h"

namespace Ui {
  class MainWindow;
}

class QToolButton;
class QVBoxLayout;
class QAction;
class QLineEdit;
class QLabel;
class QPushButton;
class FindDialog;
class SettingsDialog;

class MainWindow: public QMainWindow {
 Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow() override;

 protected:
  void closeEvent(QCloseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

 private slots:
  void on_actionNewGM_triggered();
  void on_actionNewFS_triggered();
  void on_actionNewInc_triggered();
  void on_actionNewBlank_triggered();
  void on_actionOpen_triggered();
  void on_actionClose_triggered();
  void on_actionQuit_triggered();
  void on_actionSave_triggered();
  void on_actionSaveAs_triggered();
  void on_actionSaveAll_triggered();

  void on_actionPaste_triggered();
  void on_actionCopy_triggered();
  void on_actionCut_triggered();
  void on_actionRedo_triggered();
  void on_actionUndo_triggered();

  void on_actionFind_triggered();
  void on_actionFindNext_triggered();
  void on_actionReplaceNext_triggered();
  void on_actionReplaceAll_triggered();
  void on_actionGoToLine_triggered();
  void on_actionSettings_triggered();

  void on_actionCompile_triggered();
  void on_actionCompileRun_triggered();
  void on_actionRun_triggered();
  void on_actionMark_triggered();
  void on_actionNextErr_triggered();
  void on_actionDelline_triggered();
  void on_actionDupline_triggered();
  void on_actionDupsel_triggered();
  void on_actionComment_triggered();
  void on_actionColours_triggered();

  void on_actionEditorFont_triggered();
  void on_actionOutputFont_triggered();

  void on_actionDarkMode_triggered();
  void on_actionMRU_triggered();

  void on_actionCompiler_triggered();
  void on_actionServer_triggered();

  void on_actionAbout_triggered();
  void on_actionAboutQt_triggered();

  void on_editor_textChanged();
  void on_editor_cursorPositionChanged();

  void currentChanged(int index);
  void tabCloseRequested(int index);
  void currentRowChanged(int index);
  void itemDoubleClicked(QListWidgetItem*);
  void itemClicked(QListWidgetItem*);

  void errorClicked();

  // Opens the "Continue?" install prompt and, when accepted, kicks off the
  // updater (#11 — for now it logs the chosen path).
  void promptForUpdate();
  // Triggered when QFileSystemWatcher reports an external change to an open
  // .pwn. Reloads silently if Qawno's copy is clean; prompts otherwise.
  void fileChangedExternally(const QString& path);
  // Mark lines flagged by pawncc with a wavy underline. Called from runCompile
  // when streaming chunks arrive. Cleared on each new compile.
  void applyDiagnostic(const QString& filePath, int line, bool isError);
  void clearDiagnostics();

 private:
  QString deprototype(QString func);
  void hidePopup();
  void startWord();
  void updateTitle();
  void replaceSuggestion();
  void loadNativeList();
  int tryLoadFile(const QString& fileName);
  void jumpToLine(const QString& fileName, int line);
  bool loadFile(const QString& fileName, const char* encoding = "Windows-1251");
  bool isNewFile() const;
  bool isFileModified() const;
  void setFileModified(bool isModified);
  bool isFileEmpty() const;
  int getCurrentIndex() const;
  const QString& getCurrentName() const;
  EditorWidget* getCurrentEditor() const;
  bool eventFilter(QObject* watched, QEvent* event) override;
  void finishSymbol(QString const& symbol, bool add);
  void parseFile(QString const text, bool add);
  void scrollByLines(int n);
  // Compile the active file with a progress dialog; optionally run after.
  void runCompile(bool alsoRun);
  void setCompileActionsEnabled(bool on);

  // Builds the editor-action buttons + view toggles that live in the tab bar's
  // top-right corner (VS Code style: fixed while the tabs scroll).
  void buildTopControls();
  // Builds the VS Code-style start page shown when no files are open.
  QWidget* buildWelcome();
  // macOS: builds the two status tiles on the welcome page — CrossOver (detect
  // + 32-bit Qawno bottle) and Qawno files (the compiler files that must sit in
  // the qawno folder). Returns a row widget holding both.
  QWidget* buildMacToolingCards();
  QWidget* buildCrossOverCard();
  QWidget* buildQawnoFilesCard();
  QWidget* buildWineCard();
  QWidget* buildCompilerCard();
  // Re-tints every QAction icon for the current theme. Called from applyTheme
  // so dark-on-dark icons swap to a light tint (and vice versa).
  void applyThemedActionIcons(bool dark);
  void refreshCrossOverCard();
  void refreshQawnoFilesCard();
  void setupCrossOverBottle();
  void reinstallCrossOverBottle();
  // Preflight before compiling on macOS: CrossOver installed, Qawno bottle
  // present, and the project's qawno/ folder. Shows an error and returns false
  // if it can't be located.
  bool macCompilePreflight(const QString& pwnFile);
  // Copy the bundled native pawncc (+ libpawnc.dylib) into the given project's
  // qawno/native/ folder if not already there, creating the folder if missing,
  // so each project carries its own compiler and stays terminal/CI-reusable.
  void deployNativeCompiler(const QString& qawnoDir);
  void refreshRecents();
  void updateWelcomeVisibility();
  void addRecentFile(const QString& path);
  void zoomEditor(int delta);
  bool zoomFocused(int delta);
  void updateFileInfo();

  // Light/Dark/System theme handling. themeMode_: 0 = system, 1 = light,
  // 2 = dark. effectiveDarkMode() resolves "system" against the OS scheme.
  void applyTheme();
  void setThemeMode(int mode);
  bool effectiveDarkMode() const;

 private:
  Ui::MainWindow *ui_;
  QVector<EditorWidget*> editors_;
  Server server_;

  void createTab(const QString& title, const QString& tooltip);

 private:
  struct suggestions_s {
    QString const* Name;
    int Rank;

    bool operator<(suggestions_s const& right) const {
      if (Rank == right.Rank) {
        // Sort alphabetically.
        return Name->compare(*right.Name) < 0;
      } else {
        // Sort by inverse rank (lowest, potentially negative, first).
        return Rank < right.Rank;
      }
    }
  };

  struct predictions_s {
    int Rank;
    int Count;
  };

  QPalette defaultPalette;
  QPalette darkModePalette;
  QStringList fileNames_;

  // This is shared between all open editors, the neat side-effect being that we can get a cheap and
  // easy way to auto-complete text from custom includes without actually having to parse the
  // transitive includes.  Obviously not all includes, but combined with natives it is a lot.
  QHash<QString, predictions_s> predictions_;
  QVector<suggestions_s> suggestions_;
  QListWidget* popup_ = nullptr;

  // Store the currently edited word for faster lookups.
  int wordStart_ = -1; // `-1` when the current text isn't a symbol or number.
  int wordEnd_ = -1; // `-1` when the current text isn't a symbol.
  int prevEnd_ = -1;
  QString initialWord_; // What the word was before we were editing it.
  QString prevWord_; // Because the cursor position updates before the text.

  QColor lastColour_ = QColor(0xFF, 0xFF, 0xFF, 0xAA);

  // Other data.
  // Top-right corner controls and the start page.
  QToolButton* themeButton_ = nullptr;
  QToolButton* newButton_ = nullptr;
  QAction* themeSystemAct_ = nullptr;
  QAction* themeLightAct_ = nullptr;
  QAction* themeDarkAct_ = nullptr;
  QWidget* sidebarContainer_ = nullptr;
  QWidget* sidebarPane_ = nullptr;
  bool sidebarAutoHidden_ = false;
  QWidget* welcomeWidget_ = nullptr;
  QVBoxLayout* recentsLayout_ = nullptr;
  // macOS CrossOver card widgets (null on non-macOS builds).
  QLabel* cxStatusPill_ = nullptr;
  QLabel* cxDetail_ = nullptr;
  QPushButton* cxSetupButton_ = nullptr;
  QPushButton* cxReinstallButton_ = nullptr;
  // macOS "Qawno files" card widgets.
  QLabel* filesStatusPill_ = nullptr;
  QLabel* filesDetail_ = nullptr;
  int themeMode_ = 0; // 0 = system, 1 = light, 2 = dark.
  // Tab bar hover-to-close: index of the currently hovered tab (-1 = none).
  int hoveredTabIndex_ = -1;
  // "+" new-tab button parented to the tabBar; floats just right of the
  // rightmost tab (repositioned on tab add/remove/resize).
  QToolButton* addTabBtn_ = nullptr;
  void repositionAddTabBtn();

  // Shorten paths for display: strip the user's home prefix
  // (/Users/<user>/foo → /foo) and the Wine-mapped equivalent (Z:\Users\<user>
  // → \). When SettingsDialog::fullPathInOutput() is false, additionally trim
  // log paths to just the filename.
  QString shortenPath(const QString& path) const;
  QString stripUserPrefix(const QString& path) const;

  // Watches every loaded .pwn for external writes (other editors, CI, AI
  // tooling). When a file changes on disk and the in-editor buffer is clean,
  // reload silently; otherwise prompt before discarding edits.
  QFileSystemWatcher fileWatcher_;
  // Suppresses the watcher's own "we just saved" trigger.
  qint64 lastSelfSaveMs_ = 0;
  QString lastSelfSavedPath_;

  // Persistent Find & Replace popup (lazy-created on first use). Lives across
  // open/close cycles so its state (text, options) survives between sessions.
  FindDialog* findDialog_ = nullptr;
  SettingsDialog* settingsDialog_ = nullptr;
  void ensureFindDialog();

  // VS Code-style inline find bar pinned above the active editor. Shown via
  // the toolbar Search button; Esc/X hides it. The bar reuses
  // FindDialog state for its options.
  QWidget* inlineFindBar_ = nullptr;
  QLineEdit* inlineFindEdit_ = nullptr;
  QLineEdit* inlineReplaceEdit_ = nullptr;
  QWidget* inlineReplaceRow_ = nullptr;
  QLabel* inlineFindStatus_ = nullptr;
  QToolButton* inlineMatchCase_ = nullptr;
  QToolButton* inlineWholeWord_ = nullptr;
  QToolButton* inlineRegex_ = nullptr;
  void ensureInlineFindBar();
  void toggleInlineFindBar();
  void runInlineFind(bool backward);
  void runInlineReplaceOne();
  void runInlineReplaceAll();

  // Update the theme toolbar button's label + icon to reflect the current mode.
  void updateThemeButton();

  // Toolbar buttons stored here so toggling the "show text" setting can flip
  // them all between text+icon and icon-only.
  QVector<QToolButton*> toolbarButtons_;
  QWidget* topToolbar_ = nullptr;
  void applyToolbarStyle();
  // Single replace at the current cursor when the find term matches, then
  // advance to the next match. Returns true if a replacement happened.
  bool performReplaceOnce();
  // Replace every match in the active document; returns the count.
  int performReplaceAll();

  QStack<int> mru_;
  int findStart_ = 0;
  int findRound_ = 0;
  int mruIndex_ = 0;
  int markedIndex_ = -1;
  int newCount_ = 0;
  QMap<QString, int> words_;
};

#endif // MAINWINDOW_H
