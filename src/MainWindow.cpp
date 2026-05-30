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

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
// QTextCodec moved to the Core5Compat module in Qt6; it is still the only way
// to read/write the Windows-1251 encoding that SA-MP/open.mp scripts use.
#include <QtCore5Compat/QTextCodec>
#else
#include <QTextCodec>
#endif
#include <QTextStream>
#include <QFont>
#include <QFontDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QRegularExpression>
#include <QSettings>
#include <QUndoStack>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QColorDialog>
#include <QToolBar>
#include <QToolButton>
#include <QTabBar>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMenu>
#include <QActionGroup>
#include <QFrame>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>
#include <QGuiApplication>
#include <QScreen>
#include <QStyleHints>
#include <QDateTime>
#include <QProgressDialog>
#include <QProgressBar>
#include <QSplitter>
#include <QDialog>
#include <QPlainTextEdit>
#include <QProcess>
#include <QTimer>
#include <QStyle>
#include <QToolTip>
#include <QHelpEvent>
#include <QFileOpenEvent>

#include <memory>
#include <functional>

#include "AboutDialog.h"
#include "Compiler.h"
#include "CompilerSettingsDialog.h"
#include "Installer.h"
#include "UpdateChecker.h"
#include "UpdatePromptDialog.h"
#include "Spinner.h"
#include "IosToggle.h"
#include "IconLoader.h"
#include "qawno.h"
#include <QStandardPaths>
#include "ServerSettingsDialog.h"
#include "SettingsDialog.h"
#include "EditorWidget.h"
#include "FindDialog.h"
#include "GoToDialog.h"
#include "MainWindow.h"
#include "OutputWidget.h"
#include "StatusBar.h"

#include "ui_MainWindow.h"

// macOS defaults to a dark system appearance, so the editor's base background
// is dark out of the box; default qawno's dark theme on so the gutter, current
// line and syntax colours match. Other platforms keep the light default.
#ifdef Q_OS_MACOS
static constexpr bool kDefaultDarkMode = true;
#else
static constexpr bool kDefaultDarkMode = false;
#endif

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent),
    ui_(new Ui::MainWindow),
    editors_(),
    server_(),
    fileNames_(),
    mru_()
{
  ui_->setupUi(this);
  ui_->tabWidget->setTabsClosable(false);
  ui_->tabWidget->setMovable(true);
  ui_->tabWidget->setDocumentMode(true);
  ui_->tabWidget->setUsesScrollButtons(true);
  ui_->tabWidget->tabBar()->setExpanding(false);
  // Long file names get elided with "..." instead of stretching the tab.
  ui_->tabWidget->tabBar()->setElideMode(Qt::ElideRight);
  ui_->tabWidget->tabBar()->setUsesScrollButtons(true);
  ui_->tabWidget->setStyleSheet("QTabBar::tab { max-width: 140px; }");
  // Explicit iconSize otherwise the QSS `padding:` rule for QTabBar::tab can
  // squash icons down to 0×0 on macOS, hiding them entirely.
  ui_->tabWidget->setIconSize(QSize(14, 14));
  // Hover-to-close: file icon morphs into X on hover; clicking the icon closes
  // the tab. Handled in eventFilter() — mouse moves/clicks on tabBar().
  ui_->tabWidget->tabBar()->setMouseTracking(true);
  ui_->tabWidget->tabBar()->setAttribute(Qt::WA_Hover, true);
  ui_->tabWidget->tabBar()->installEventFilter(this);

  // Set a sane minimum window size so the layout never collapses badly.
  setMinimumSize(720, 480);

  // Modern line icons for the editor actions.
  ui_->actionNewGM->setIcon(QIcon(":/assets/images/icons/new.svg"));
  ui_->actionOpen->setIcon(QIcon(":/assets/images/icons/open.svg"));
  ui_->actionSave->setIcon(QIcon(":/assets/images/icons/save.svg"));
  ui_->actionCompileRun->setIcon(QIcon(":/assets/images/icons/run.svg"));
  ui_->actionCompileRun->setText(tr("Compile && Run"));

  // Menu-item icons. Use our custom SVGs where we have them; fall back to the
  // platform's standard icons for the common edit/file actions.
  ui_->actionNewFS->setIcon(QIcon(":/assets/images/icons/new.svg"));
  ui_->actionNewInc->setIcon(QIcon(":/assets/images/icons/new.svg"));
  ui_->actionNewBlank->setIcon(QIcon(":/assets/images/icons/new.svg"));
  ui_->actionSaveAs->setIcon(QIcon(":/assets/images/icons/save.svg"));
  ui_->actionSaveAll->setIcon(QIcon(":/assets/images/icons/save.svg"));
  ui_->actionCompile->setIcon(QIcon(":/assets/images/icons/run.svg"));
  ui_->actionRun->setIcon(QIcon(":/assets/images/icons/run.svg"));
  auto std = [this](QStyle::StandardPixmap sp) { return style()->standardIcon(sp); };
  ui_->actionClose->setIcon(QIcon(":/assets/images/icons/close.svg"));
  ui_->actionQuit->setIcon(QIcon(":/assets/images/icons/quit.svg"));
#ifdef Q_OS_MACOS
  // macOS auto-binds Cmd+Q to the application's Quit menu item via the
  // standard QuitRole; the explicit Ctrl+Q from the .ui file would conflict.
  ui_->actionQuit->setShortcut(QKeySequence());
#endif
  ui_->actionUndo->setIcon(QIcon(":/assets/images/icons/undo.svg"));
  ui_->actionRedo->setIcon(QIcon(":/assets/images/icons/redo.svg"));
  ui_->actionCut->setIcon(QIcon(":/assets/images/icons/cut.svg"));
  ui_->actionCopy->setIcon(QIcon(":/assets/images/icons/copy.svg"));
  ui_->actionPaste->setIcon(QIcon(":/assets/images/icons/paste.svg"));
  ui_->actionFind->setIcon(QIcon(":/assets/images/icons/find.svg"));
  ui_->actionFindNext->setIcon(QIcon(":/assets/images/icons/find-next.svg"));
  ui_->actionGoToLine->setIcon(QIcon(":/assets/images/icons/goto-line.svg"));
  ui_->actionDupsel->setIcon(QIcon(":/assets/images/icons/duplicate-selection.svg"));
  ui_->actionDupline->setIcon(QIcon(":/assets/images/icons/duplicate-line.svg"));
  ui_->actionDelline->setIcon(QIcon(":/assets/images/icons/delete-line.svg"));
  ui_->actionComment->setIcon(QIcon(":/assets/images/icons/comment.svg"));
  ui_->actionColours->setIcon(QIcon(":/assets/images/icons/color-picker.svg"));
  ui_->actionColours->setText(tr("Colour &Picker"));
  ui_->actionMark->setIcon(std(QStyle::SP_DialogApplyButton));
  ui_->actionNextErr->setIcon(std(QStyle::SP_MessageBoxWarning));
  ui_->actionCompiler->setIcon(std(QStyle::SP_FileDialogDetailedView));
  ui_->actionServer->setIcon(std(QStyle::SP_ComputerIcon));
  ui_->actionAbout->setIcon(std(QStyle::SP_MessageBoxInformation));
  ui_->actionAboutQt->setIcon(std(QStyle::SP_MessageBoxInformation));

  // The INCLUDES list lives in its own bordered box, with a header that looks
  // like part of it. Wrap the existing `functions` list (and its header) in
  // that container, then drop it back into the right-hand splitter pane.
  static const int kSidebarWidth = 250;

  // Right-hand sidebar: the INCLUDES box. The action toolbar now lives in a
  // full-width strip above the splitter, so the sidebar pane no longer needs
  // a top spacer — INCLUDES header sits at the top of the pane, on the same
  // row as the tab strip in the editor pane.
  sidebarContainer_ = new QWidget;
  sidebarContainer_->setObjectName("sidebarPaneOuter");
  sidebarPane_ = sidebarContainer_;
  QVBoxLayout* outerSidebarLayout = new QVBoxLayout(sidebarContainer_);
  outerSidebarLayout->setContentsMargins(0, 0, 0, 0);
  outerSidebarLayout->setSpacing(0);

  QWidget* sidebarBox = new QWidget(sidebarContainer_);
  sidebarBox->setObjectName("sidebarBox");
  QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarBox);
  sidebarLayout->setContentsMargins(0, 0, 0, 0);
  sidebarLayout->setSpacing(0);
  QLabel* includesHeader = new QLabel(tr("INCLUDES LIBRARY"), sidebarBox);
  includesHeader->setObjectName("sidebarHeader");
  includesHeader->setAlignment(Qt::AlignCenter);
  includesHeader->setFixedHeight(28);
  includesHeader->setToolTip(tr("Natives and functions, grouped by include file"));
  sidebarLayout->addWidget(includesHeader, 0);
  sidebarLayout->addWidget(ui_->functions, 1); // reparents it out of the splitter
  outerSidebarLayout->addWidget(sidebarBox, 1);

  ui_->divider->insertWidget(1, sidebarContainer_);
  ui_->divider->setSizes({ 900, kSidebarWidth });

  // Make the horizontal splitter (editor | sidebar) user-draggable.
  // The sidebar pane has a hard minimum width — dragging the splitter past
  // that minimum is blocked, so the INCLUDES panel can't be accidentally
  // shrunk to a sliver or collapsed by drag.
  static constexpr int kSidebarMinWidth = 180;
  sidebarContainer_->setMinimumWidth(kSidebarMinWidth);
  ui_->divider->setHandleWidth(5);
  ui_->divider->setChildrenCollapsible(false);

  // Thin handle between editor and output panel (kills the big dark gap).
  ui_->splitter->setHandleWidth(3);
  ui_->splitter->setChildrenCollapsible(false);

  // Compact VS Code-style action buttons + view toggles in the tab bar corner.
  buildTopControls();

  // Note: the window-level stylesheet (and its theme colors) is applied in
  // applyTheme(); the radius/structure for sidebar box + list lives there too.

  setStatusBar(new StatusBar(this));

  QSettings settings;

  // Explicit light palette: macOS defaults the system appearance to dark, so a
  // bare default palette leaves a dark Base/Text under the light syntax scheme.
  // Force light surfaces and dark text so light mode is readable.
  QPalette lightPalette;
  lightPalette.setColor(QPalette::Window, QColor(0xF4F6F8));
  lightPalette.setColor(QPalette::WindowText, QColor(0x1A1E25));
  lightPalette.setColor(QPalette::Base, QColor(0xFFFFFF));
  lightPalette.setColor(QPalette::AlternateBase, QColor(0xEEF1F4));
  lightPalette.setColor(QPalette::ToolTipBase, QColor(0xFFFFFF));
  lightPalette.setColor(QPalette::ToolTipText, QColor(0x1A1E25));
  lightPalette.setColor(QPalette::Text, QColor(0x1A1E25));
  lightPalette.setColor(QPalette::Button, QColor(0xE8ECF0));
  lightPalette.setColor(QPalette::ButtonText, QColor(0x1A1E25));
  lightPalette.setColor(QPalette::BrightText, Qt::red);
  lightPalette.setColor(QPalette::Link, QColor(0x1E6BB8));
  lightPalette.setColor(QPalette::Highlight, QColor(0xCCE0F5));
  lightPalette.setColor(QPalette::HighlightedText, QColor(0x1A1E25));
  defaultPalette = lightPalette;

  QPalette darkPalette;
  darkPalette.setColor(QPalette::Window, QColor(0x282C34));
  darkPalette.setColor(QPalette::WindowText, Qt::white);
  darkPalette.setColor(QPalette::Base, QColor(0x282C34));
  darkPalette.setColor(QPalette::AlternateBase, QColor(0x282C34));
  darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
  darkPalette.setColor(QPalette::ToolTipText, Qt::white);
  darkPalette.setColor(QPalette::Text, Qt::white);
  darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
  darkPalette.setColor(QPalette::ButtonText, Qt::white);
  darkPalette.setColor(QPalette::BrightText, Qt::red);
  darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
  darkPalette.setColor(QPalette::Highlight, QColor(0x324054));
  darkPalette.setBrush(QPalette::HighlightedText, QBrush(Qt::NoBrush));

  darkModePalette = darkPalette;

  // Restore the theme. Migrate the old boolean "DarkMode" setting if present.
  if (settings.contains("ThemeMode")) {
    themeMode_ = settings.value("ThemeMode", 0).toInt();
  } else {
    themeMode_ = settings.value("DarkMode", kDefaultDarkMode).toBool() ? 2 : 1;
  }

  bool useMRU = settings.value("MRU", false).toBool();
  ui_->actionMRU->setChecked(useMRU);

  {
    QSize want = settings.value("WindowSize", QSize(1000, 700)).toSize();
    // Never restore larger than the available screen (avoids opening offscreen
    // / oversized when toggling between the welcome page and the editor).
    if (QScreen* scr = QGuiApplication::primaryScreen()) {
      QSize avail = scr->availableSize();
      want.setWidth(qMin(want.width(), avail.width()));
      want.setHeight(qMin(want.height(), avail.height()));
    }
    resize(want);
  }
  if (settings.value("Maximized", false).toBool()) {
    setWindowState(Qt::WindowMaximized);
  }

  int loaded = 0;
  if (QApplication::instance()->arguments().size() > 1) {
    if (loadFile(QApplication::instance()->arguments()[1])) {
      ++loaded;
    }
  } else if (settings.contains("LastFiles")) {
    QStringList lastOpenedFileNames = settings.value("LastFiles").toStringList();
    int idx = settings.contains("LastStarts") && settings.contains("LastEnds") ? 0 : -1000000;
    QVariantList lastStarts = settings.value("LastStarts", QVariantList()).toList();
    QVariantList lastEnds = settings.value("LastEnds", QVariantList()).toList();
    for (auto const & i : lastOpenedFileNames) {
      if (!i.isEmpty() && loadFile(i)) {
        ++loaded;
        if (idx >= 0) {
          EditorWidget* editor = editors_.last();
          QTextCursor cursor = editor->textCursor();
          int p0 = lastStarts[idx].toInt();
          int p1 = lastStarts[idx].toInt();
          cursor.setPosition(p0, QTextCursor::MoveAnchor);
          cursor.setPosition(p1, QTextCursor::KeepAnchor);
          editor->setFocus(Qt::OtherFocusReason);
          editor->setTextCursor(cursor);
          editor->ensureCursorVisible();
        }
      }
      ++idx;
    }
  }
  // When nothing loaded, the welcome page (set up below) is shown instead of
  // force-creating an empty file.

  connect(ui_->tabWidget, SIGNAL(currentChanged(int)), SLOT(currentChanged(int)));
  connect(ui_->tabWidget, SIGNAL(tabCloseRequested(int)), SLOT(tabCloseRequested(int)));
  connect(&fileWatcher_, &QFileSystemWatcher::fileChanged,
          this, &MainWindow::fileChangedExternally);
  connect(ui_->functions, SIGNAL(currentRowChanged(int)), SLOT(currentRowChanged(int)));
  connect(ui_->functions, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(itemClicked(QListWidgetItem*)));
  connect(ui_->functions, SIGNAL(itemDoubleClicked(QListWidgetItem*)), SLOT(itemDoubleClicked(QListWidgetItem*)));
  connect(ui_->output, SIGNAL(cursorPositionChanged()), SLOT(errorClicked()));
  QApplication::instance()->installEventFilter(this);

  loadNativeList();

  updateTitle();

  ui_->tabWidget->setCurrentIndex(settings.value("LastViewed", 0).toInt());

  // VS Code-style start page, shown whenever no files are open.
  welcomeWidget_ = buildWelcome();
  ui_->horizontalLayout->addWidget(welcomeWidget_);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  // Follow live OS appearance changes while in "System" mode.
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
          [this](Qt::ColorScheme) { if (themeMode_ == 0) applyTheme(); });
#endif

  applyTheme();
  updateWelcomeVisibility();

  // Footer "Update available" chip — driven entirely by UpdateChecker. We
  // re-render on every signal so the chip clears itself when a newer release
  // matches the locally-installed version.
  if (auto* sb = dynamic_cast<StatusBar*>(statusBar())) {
    auto refreshUpdateChip = [sb] {
      auto* uc = UpdateChecker::instance();
      const auto compRel  = uc->latest(UpdateChecker::Component::Compiler);
      const auto qawnoRel = uc->latest(UpdateChecker::Component::Qawno);
      const QString currentQawno = QString::fromLatin1(QAWNO_VERSION_STRING);
      QStringList parts;
      if (!qawnoRel.version.isEmpty() && qawnoRel.version != currentQawno) {
        parts << tr("Qawno %1").arg(qawnoRel.tag);
      }
      if (!compRel.version.isEmpty()) {
        Compiler c;
        const QString currentComp = c.detectVersion();
        if (!currentComp.isEmpty() && compRel.version != currentComp) {
          parts << tr("compiler %1").arg(compRel.tag);
        }
      }
      sb->setUpdateAvailable(parts.isEmpty() ? QString()
                                             : tr("⤓ Update available: %1").arg(parts.join(", ")));
    };
    connect(UpdateChecker::instance(), &UpdateChecker::releaseFetched, this,
            [refreshUpdateChip](UpdateChecker::Component, UpdateChecker::Release) { refreshUpdateChip(); });
    connect(sb, &StatusBar::updateClicked, this, &MainWindow::promptForUpdate);
  }

  // Restore tabs that were open before a self-update. The installer wrote
  // PendingReopenFiles right before relaunch; we open them then clear the
  // key so subsequent normal launches don't reopen stale paths.
  {
    QSettings s;
    const QStringList reopen = s.value("PendingReopenFiles").toStringList();
    if (!reopen.isEmpty()) {
      s.remove("PendingReopenFiles");
      for (const QString& f : reopen) {
        if (!f.isEmpty() && QFileInfo::exists(f)) tryLoadFile(f);
      }
    }
  }
}

MainWindow::~MainWindow() {
  QSettings settings;

  settings.setValue("Maximized", isMaximized());
  if (!isMaximized()) {
    settings.setValue("WindowSize", size());
  }

  QStringList files {};
  for (auto const& i : fileNames_) {
    files.push_back(i);
  }
  settings.setValue("LastFiles", files);

  QVariantList starts {};
  QVariantList ends {};
  for (auto editor : editors_) {
    QTextCursor cursor = editor->textCursor();
    starts.push_back(cursor.selectionStart());
    ends.push_back(cursor.selectionEnd());
  }
  settings.setValue("LastStarts", starts);
  settings.setValue("LastEnds", ends);

  settings.setValue("LastViewed", getCurrentIndex());

  delete ui_;
}

QString MainWindow::deprototype(QString func) {
  // Strip out some tags - `Float:function()` and `{Float, _}:...` style.
  QRegularExpression ret("^\\s*[a-zA-Z0-9_@]+\\s*:\\s*");
  QRegularExpression brackets("\\s*\\[[^\\]]*\\]\\s*");
  QRegularExpression defval("\\s*=\\s*[^(),]*(\\([^)]+\\))?");
  QRegularExpression trailing(",?\\s*[a-zA-Z0-9_@]+\\s*:\\s*\\.\\.\\.|,?\\s*\\{[a-zA-Z0-9_@, ]+\\}\\s*:\\s*\\.\\.\\.");
  return func.replace("const ", "").replace("&", "").replace(ret, "").replace(trailing, "").replace(brackets, "").replace(defval, "");
}

void MainWindow::loadNativeList() {
  // Declare an invalid symbol for list items that aren't real symbols.
  QListWidgetItem* child;
  // INCLUDES list at ~1.5x the prior sizes.
  QFont* fileFont = new QFont("Sans Serif", 15, QFont::Bold);
  QFont* funcFont = new QFont("Sans Serif", 14);
  QFont* headFont = new QFont("Sans Serif", 14);
  // Loop through all `includes/*.inc` files (ensure they aren't directories).
  QDir includes("./include", "*.inc", QDir::IgnoreCase, QDir::Files | QDir::Readable);
  for (auto const & fileName : includes.entryInfoList()) {
    QFile f{fileName.absoluteFilePath()};
    if (f.open(QFile::ReadOnly | QFile::Text)) {
      child = nullptr;
      // Find every line that starts with `native`.
      while (!f.atEnd()) {
        QByteArray line = f.readLine();
        // Skip leading whitespace.
        int idx = 0, len = line.size();
        char const* data = line.constData();
        while (idx < len) {
          if (data[idx] > ' ') {
            break;
          }
          ++idx;
        }
        if (idx < len) {
          if (strncmp(data + idx, "native ", 7) == 0) {
            idx += 7;

            // Work forwards to the start of text.
            while (data[idx] <= ' ') {
              if (data[idx] == '\n' || data[idx] == '\r') {
                goto not_a_native;
              }
              ++idx;
            }

            // Work back to the end of the parameters (skips `= other;` too).
            do {
              --len;
              if (len == idx) {
                goto not_a_native;
              }
            } while (data[len] != ')');

            // Extract the full name, return, and parameters.
            QString withArgs = QString::fromStdString(std::string(data + idx, (size_t)len - idx + 1));

            // Extract just the name.
            len = idx;
            for ( ; ; ) {
              if (data[len] == '\n' || data[len] == '\r') {
                // Remove this from the list again.  We can't add the name after here because we've
                // clobbered the indexes.
                goto not_a_native;
              }
              if (data[len] == ':') {
                // Skip the return tag.
                idx = len + 1;
              }
              if (data[len] == '(') {
                // Found the parameters start.
                break;
              }
              ++len;
            }
            // Found some function name.
            while (data[idx] <= ' ') {
              ++idx;
            }
            do {
              --len;
            }
            while (data[len] <= ' ');
            ++len;
            if (idx < len) {
              if (!child)
              {
                // first valid entry from this file.  Add the filename too.
                // Extra scope for `name`.
                child = new QListWidgetItem(fileName.fileName(), ui_->functions);
                child->setFont(*fileFont);
                child->setTextAlignment(4);
                child->setFlags(child->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
                // Pad the natives list so indexing works, though we never see this in the status bar.
              }
              if (data[idx] == '#') {
                // Special syntax:
                //
                //   native #Heading();
                //
                // Purely for Qawno titles.
                QString name = QString::fromStdString(std::string(data + idx + 1, (size_t)len - idx - 1));
                child = new QListWidgetItem(name, ui_->functions);
                child->setFont(*headFont);
                child->setTextAlignment(4);
                child->setFlags(child->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
              } else {
                QString name = QString::fromStdString(std::string(data + idx, (size_t)len - idx));
                child = new QListWidgetItem(name, ui_->functions);
                child->setFont(*funcFont);
                child->setData(Qt::ToolTipRole, "native " + withArgs + ";");
                child->setData(Qt::StatusTipRole, withArgs);
                // Add the native to the list of auto-complete predictions with default likelihood.
                predictions_.insert(name, { 1, 1 });
              }
            }
          }
        }
not_a_native:
        (void)0;
      }
    }
  }
}

void MainWindow::itemDoubleClicked(QListWidgetItem* item) {
  // Insert this text in to the current position.
  if (EditorWidget* editor = getCurrentEditor()) {
    // Insert the current function name.
    QTextCursor cursor = editor->textCursor();
    int pos = cursor.selectionStart();
    QString insert = deprototype(item->data(Qt::StatusTipRole).toString());
    cursor.insertText(insert);
    // Jump to the start of the parameters.
    cursor.setPosition(pos + insert.indexOf('(') + 1);
    editor->setFocus(Qt::OtherFocusReason);
    editor->setTextCursor(cursor);
  }
}

void MainWindow::itemClicked(QListWidgetItem* item) {
  statusBar()->showMessage(item->data(Qt::ToolTipRole).toString());
}

void MainWindow::currentChanged(int index) {
  hidePopup();
  startWord();
  if (index != -1) {
    // Remove this index from the MRU list.
    mru_.removeAll(index);
    // And push it to the top.
    mru_.push(index);
  }
  updateTitle();
}

void MainWindow::tabCloseRequested(int index) {
  if (markedIndex_ == index) {
    markedIndex_ = -1;
  } else if (markedIndex_ > index) {
    // Shift the index down.
    --markedIndex_;
  }
  ui_->tabWidget->setCurrentIndex(index);
  on_actionClose_triggered();
  // Keep the file watcher in sync — any path no longer open should be removed
  // so we stop reacting to its external changes.
  if (!fileWatcher_.files().isEmpty()) {
    QStringList stillOpen = fileNames_;
    stillOpen.removeAll(QString());
    QStringList orphans;
    for (const QString& p : fileWatcher_.files()) {
      if (!stillOpen.contains(p)) orphans << p;
    }
    if (!orphans.isEmpty()) {
      fileWatcher_.removePaths(orphans);
    }
  }
}

void MainWindow::currentRowChanged(int index) {
  if (index == -1) {
    if (!getCurrentEditor()) {
      return;
    }
    QTextCursor cursor = getCurrentEditor()->textCursor();
    int line = cursor.blockNumber() + 1;
    int column = cursor.columnNumber() + 1;
    int selected = cursor.selectionEnd() - cursor.selectionStart();
    dynamic_cast<StatusBar*>(statusBar())->setCursorPosition(line, column, selected);
    statusBar()->showMessage("");
  } else {
    statusBar()->showMessage(ui_->functions->currentItem()->data(Qt::ToolTipRole).toString());
  }
}

void MainWindow::hidePopup() {
  if (popup_) {
    popup_->hide();
    delete popup_;
    popup_ = nullptr;
  }
}

void MainWindow::startWord() {
  // Initialise.
  wordStart_ = -1;
  wordEnd_ = -1;
  // We are typing or clicked somewhere new.  Save the extents of the current symbol.
  EditorWidget* editor = getCurrentEditor();
  if (!editor) {
    return;
  }
  QTextCursor cursor = editor->textCursor();
  if (cursor.hasSelection()) {
    return;
  }
  int start = cursor.selectionStart();
  int end = start;
  QString text = editor->toPlainText();
  QChar const* data = text.constData();
  int len = 0;
  while (start) {
    QChar ch = data[--start];
    // Test if the current character is a valid symbol character.
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '@') {
      ++len;
    } else {
      ++start;
      break;
    }
  }
  while (end < text.length()) {
    QChar ch = data[end++];
    // Test if the current character is a valid symbol character.
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '@') {
      ++len;
    } else {
      --end;
      break;
    }
  }
  QChar ch = data[start];
  // Test if the first character is a valid initial symbol character (i.e. not a number).
  if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '@') {
    // Yes.
    wordStart_ = start;
    wordEnd_ = end;
    // Save the current word at this position so that when we edit it we can adjust the predictions list.
    initialWord_ = QString(data + start, end - start);
  } else if ((ch >= '0' && ch <= '9')) {
    // It is a number, which is like a symbol in many ways, but without auto-predict.
    wordStart_ = start;
  }
}

void MainWindow::on_editor_textChanged() {
  updateTitle();

  // Called when the current text changes, every time.  We may need to debounce this a little bit
  // because we are going to be scanning through a long list of strings every keypress otherwise.
  hidePopup();
  // Remove the current word from the predictions list, since we're changing it.
  if (wordEnd_ != -1 && prevEnd_ != -1) {
    if (predictions_.contains(prevWord_)) {
      predictions_s& prediction = predictions_[prevWord_];
      if (prediction.Count < 2) {
        predictions_.remove(prevWord_);
      } else {
        --prediction.Count;
      }
    }
  }
  if (wordEnd_ == -1) {
    // Not in a symbol, don't care.
  }
  // Get the current editor.
  EditorWidget* editor = getCurrentEditor();
  // Get the current cursor position and the current text.
  int pos = editor->textCursor().selectionStart();
  QString text = editor->toPlainText();
  QChar const* data = text.constData();
  // Check if the character typed starts or continues a new word.
  // We now have the extent of the current symbol.  If it is more than three characters and starts
  // with a non-number, search for auto-complete matches.
  int searchLen = pos - wordStart_;
  if (searchLen >= 3) {
    // Loop through all the known symbols.
    suggestions_.clear();
    for (auto it = predictions_.constBegin(), end = predictions_.constEnd(); it != end; ++it) {
      auto const& name = it.key();
      int matchLen = name.length();
      if (matchLen >= searchLen) {
        for (int i = wordStart_, j = 0; j != matchLen; ++j) {
          // Case-insensitive comparison.
          if (name[j].toUpper() == data[i].toUpper()) {
            ++i;
            if (i == pos) {
              // We've found a candidate, all the characters from `data` (the word currently
              // being typed).  We sort the matches by `j`, so that the ones that take the fewest
              // characters to match come first (so `Get` first lists the actual `Get` functions,
              // before things like `TogglePlayerScoresPingsUpdate` which just happen to have `g`,
              // `e`, and `t` somewhere in that order.  We also store "likelihood" metrics with
              // the names, so that those symbols that are used more move up the list quickly.
              // Probably double the likelihood every time a symbol is selected and subtract this
              // value from the length.
              // Get the final sort position.
              suggestions_.push_back({ &name, j - it.value().Count - it.value().Rank });
              break;
            }
          }
        }
      }
    }
    if (suggestions_.size()) {
      // There are some suggestions.  Sort them.
      std::sort(suggestions_.begin(), suggestions_.end());
      // Determine where to draw the suggestions box.
      QRect rect = editor->cursorRect();
      popup_ = new QListWidget(editor);
      popup_->setParent(editor);
      popup_->setObjectName("autoCompletePopup");
      popup_->setFrameShape(QFrame::NoFrame);
      const bool dark = effectiveDarkMode();
      popup_->setStyleSheet(QString(
          "QListWidget#autoCompletePopup {"
          "  background: %1; color: %2;"
          "  border: 1px solid %3; border-radius: 6px;"
          "  padding: 2px; outline: none;"
          "  font-family: 'Menlo', 'Monaco', monospace; font-size: 12px;"
          "}"
          "QListWidget#autoCompletePopup::item {"
          "  padding: 3px 8px; border-radius: 4px;"
          "}"
          "QListWidget#autoCompletePopup::item:selected {"
          "  background: %4; color: %5;"
          "}")
          .arg(dark ? "#21262D" : "#FFFFFF",
               dark ? "#C8CDD6" : "#1A1E25",
               dark ? "#3A3F4B" : "#C8CDD6",
               dark ? "#324054" : "#CCE0F5",
               dark ? "#FFFFFF" : "#1A1E25"));
      popup_->setGeometry(rect.right() + 60, rect.bottom(), 200, 200);
      for (auto const& it : suggestions_) {
        QListWidgetItem* cur = new QListWidgetItem(*it.Name, popup_);
      }
      popup_->show();
    }
  }
  // Add this word to the predictions list.  AFTER showing suggestions so the thing we just typed
  // doesn't affect the results.  This gets called randomly when files load and the cursors are
  // initialised - this is a bug I'm not going to fix, it just means a few random words will be
  // predicted even if you delete them all.  Actually, I can fix this by setting the initial state
  // of the file parser to `NUMBER` to ignore those initial words since they're already added once.
  if (initialWord_.length() < 3) {
    (void)0;
  } else if (predictions_.contains(initialWord_)) {
    ++predictions_[initialWord_].Count;
  } else {
    predictions_.insert(initialWord_, { 1, 1 });
  }
}

void MainWindow::finishSymbol(QString const& symbol, bool add) {
  // Add or remove the symbol to the predictions list.
  if (symbol.length() < 3) {
    (void)0;
  } else if (add) {
    if (predictions_.contains(symbol)) {
      ++predictions_[symbol].Count;
    } else {
      predictions_.insert(symbol, { 1, 1 });
    }
  } else {
    if (predictions_.contains(symbol)) {
      predictions_s& prediction = predictions_[symbol];
      if (prediction.Count < 2) {
        predictions_.remove(symbol);
      } else {
        --prediction.Count;
      }
    }
  }
}

enum file_parse_state_e {
  file_parse_state_number,
  file_parse_state_symbol,
  file_parse_state_unknown,
  file_parse_state_line_comment,
  file_parse_state_block_comment,
  file_parse_state_character,
  file_parse_state_string,
  file_parse_state_preproc,
};

void MainWindow::parseFile(QString const text, bool add) {
  // When adding we start in parse state `number` because any symbols at position 0 are already
  // added to the predictions list by the text edit callback.
  file_parse_state_e state = add ? file_parse_state_number : file_parse_state_unknown;
  QChar const* data = text.data();
  int len = text.length();
  QString symbol = "";
  bool escape = false;
  while (len--) {
    QChar ch = *data++;
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '@') {
      switch (state) {
      case file_parse_state_symbol:
        // Add this character to the current symbol.
        symbol += ch;
        break;
      case file_parse_state_unknown:
        // Start a new symbol.
        symbol = ch;
        state = file_parse_state_symbol;
        break;
      }
    } else if ((ch >= '0' && ch <= '9')) {
      switch (state) {
      case file_parse_state_symbol:
        // Add this character to the current symbol.
        symbol += ch;
        break;
      case file_parse_state_unknown:
        // Start a new number.
        state = file_parse_state_number;
        break;
      }
    } else if (ch == '/') {
      if (len && *data == '/') {
        switch (state) {
        case file_parse_state_symbol:
          finishSymbol(symbol, add);
          // Fallthrough.
        case file_parse_state_number:
        case file_parse_state_unknown:
          state = file_parse_state_line_comment;
          ++data;
          --len;
          break;
        }
      } else if (len && *data == '*') {
        switch (state) {
        case file_parse_state_symbol:
          finishSymbol(symbol, add);
          // Fallthrough.
        case file_parse_state_number:
        case file_parse_state_unknown:
          state = file_parse_state_block_comment;
          ++data;
          --len;
          break;
        }
      } else {
        switch (state) {
        case file_parse_state_symbol:
          finishSymbol(symbol, add);
          // Fallthrough.
        case file_parse_state_number:
          state = file_parse_state_unknown;
          break;
        }
      }
    } else if (ch == '*') {
      if (len && *data == '/') {
        switch (state) {
        case file_parse_state_block_comment:
          state = file_parse_state_unknown;
          ++data;
          --len;
          break;
        case file_parse_state_symbol:
          finishSymbol(symbol, add);
          // Fallthrough.
        case file_parse_state_number:
          state = file_parse_state_unknown;
          break;
        }
      } else {
        switch (state) {
        case file_parse_state_symbol:
          finishSymbol(symbol, add);
          // Fallthrough.
        case file_parse_state_number:
          state = file_parse_state_unknown;
          break;
        }
      }
    } else if (ch == '#') {
      switch (state) {
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
        state = file_parse_state_preproc;
        break;
      }
    } else if (ch == '"') {
      switch (state) {
      case file_parse_state_string:
        if (!escape) {
          state = file_parse_state_unknown;
        }
        break;
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
      case file_parse_state_unknown:
        state = file_parse_state_string;
        break;
      }
    } else if (ch == '\'') {
      switch (state) {
      case file_parse_state_character:
        if (!escape) {
          state = file_parse_state_unknown;
        }
        break;
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
      case file_parse_state_unknown:
        state = file_parse_state_character;
        break;
      }
    } else if (ch == '\\') {
      switch (state) {
      case file_parse_state_string:
      case file_parse_state_character:
        escape = !escape;
        // Don't reset `escape` at the end of the loop.
        continue;
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
      case file_parse_state_unknown:
        state = file_parse_state_character;
        break;
      }
    } else if (ch == '\r' || ch == '\n') {
      switch (state) {
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
      case file_parse_state_line_comment:
      case file_parse_state_character:
      case file_parse_state_preproc:
        state = file_parse_state_unknown;
        break;
      }
    } else {
      switch (state) {
      case file_parse_state_symbol:
        finishSymbol(symbol, add);
        // Fallthrough.
      case file_parse_state_number:
        state = file_parse_state_unknown;
        break;
      }
    }
    escape = false;
  }
}

void MainWindow::replaceSuggestion() {
  QString const& replacement = popup_->currentRow() == -1 ? popup_->item(0)->text() : popup_->currentItem()->text();
  EditorWidget* editor = getCurrentEditor();
  if (!editor) {
    return;
  }
  QTextCursor cursor = editor->textCursor();
  if (cursor.hasSelection()) {
    return;
  }
  // Get the current cursor position and the current text.
  int start = cursor.selectionStart();
  QString text = editor->toPlainText();
  QChar const* data = text.constData();
  int len = 0;
  while (start--) {
    QChar ch = data[start];
    // Test if the current character is a valid symbol character.
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '@') {
      ++len;
    } else {
      break;
    }
  }
  ++start;
  // We now have the extent of the current symbol.  If it is more than three characters and starts
  // with a non-number, search for auto-complete matches.
  cursor.setPosition(start, QTextCursor::MoveAnchor);
  cursor.setPosition(start + len , QTextCursor::KeepAnchor);
  cursor.insertText(replacement);
  // Increase how popular this replacement is.
  predictions_[replacement] = { predictions_[replacement].Rank + 1, predictions_[replacement].Count };
  hidePopup();
  startWord();
}

void MainWindow::on_actionNewGM_triggered() {
  loadFile("./gamemode.new");
}

void MainWindow::on_actionNewFS_triggered() {
  loadFile("./filterscript.new");
}

void MainWindow::on_actionNewInc_triggered() {
  loadFile("./include.new");
}

void MainWindow::on_actionNewBlank_triggered() {
  loadFile("./blank.new");
}

void MainWindow::on_actionOpen_triggered() {
  QSettings settings;
  QString dir = settings.value("LastOpenDir").toString();

  QString caption = tr("Open File");
  QString filter = tr("Pawn scripts (*.pwn *.inc)");
  // Use the native macOS open panel — Qt's fallback dialog looks dated and
  // doesn't honour Documents access prompts the way AppKit does.
  QStringList fileNames =
      QFileDialog::getOpenFileNames(this, caption, dir, filter, nullptr);

  int loaded = 0;
  bool close = fileNames_.count() == 1 && isNewFile() && !isFileModified();
  for (auto const& fileName : fileNames) {
    if (tryLoadFile(fileName) == 1) {
      ++loaded;
    }
  }

  if (loaded == 0) {
    return;
  } else if (close) {
    // Opening a first file replaces the initial new file.
    editors_.remove(0);
    fileNames_.removeAt(0);
    ui_->tabWidget->removeTab(0);
  }

  dir = QFileInfo(fileNames.first()).dir().path();
  settings.setValue("LastOpenDir", dir);
}

void MainWindow::on_actionPaste_triggered() {
  EditorWidget* cur = getCurrentEditor();
  if (cur) {
    cur->paste();
  }
}

void MainWindow::on_actionCopy_triggered() {
  EditorWidget* cur = getCurrentEditor();
  if (cur) {
    cur->copy();
  }
}

void MainWindow::on_actionCut_triggered() {
  EditorWidget* cur = getCurrentEditor();
  if (cur) {
    cur->cut();
  }
}

void MainWindow::on_actionRedo_triggered() {
  EditorWidget* cur = getCurrentEditor();
  if (cur) {
    cur->redo();
  }
}

void MainWindow::on_actionUndo_triggered() {
  EditorWidget* cur = getCurrentEditor();
  if (cur) {
    cur->undo();
  }
}

void MainWindow::scrollByLines(int n) {
  if (auto editor = getCurrentEditor()) {
    editor->verticalScrollBar()->setValue(editor->verticalScrollBar()->value() + n);
  }
}

void MainWindow::on_actionColours_triggered() {
  // Set the standard colours.

  // 23 Game Text Colours, missing white and black (both identical in CGA).
  QColorDialog::setStandardColor(0, QColor(0x90, 0x62, 0x10, 0xAA)); //
  QColorDialog::setStandardColor(6, QColor(0xD8, 0x93, 0x18, 0xAA)); // ~h~
  QColorDialog::setStandardColor(12, QColor(0xFF, 0xFF, 0x36, 0xAA)); // ~h~~h~
  QColorDialog::setStandardColor(18, QColor(0xE2, 0xC0, 0x63, 0xAA)); // ~y~
  QColorDialog::setStandardColor(24, QColor(0xFF, 0xFF, 0x94, 0xAA)); // ~y~~h~
  QColorDialog::setStandardColor(30, QColor(0xFF, 0xFF, 0xDE, 0xAA)); // ~y~~h~~h~ / ~g~~h~~h~~h~~h~
  QColorDialog::setStandardColor(37, QColor(0x36, 0x68, 0x2C, 0xFF)); // ~g~ / Positive Money
  QColorDialog::setStandardColor(43, QColor(0x51, 0x9C, 0x42, 0xFF)); // ~g~~h~
  QColorDialog::setStandardColor(42, QColor(0x79, 0xEA, 0x63, 0xFF)); // ~g~~h~~h~
  QColorDialog::setStandardColor(36, QColor(0xB5, 0xFF, 0x94, 0xFF)); // ~g~~h~~h~~h~
  QColorDialog::setStandardColor(1, QColor(0xB4, 0x19, 0x1D, 0xFF)); // ~r~ / Negative Money
  QColorDialog::setStandardColor(7, QColor(0xFF, 0x25, 0x2B, 0xFF)); // ~r~~h~
  QColorDialog::setStandardColor(13, QColor(0xFF, 0x37, 0x40, 0xFF)); // ~r~~h~~h~
  QColorDialog::setStandardColor(19, QColor(0xFF, 0x52, 0x60, 0xFF)); // ~r~~h~~h~~h~
  QColorDialog::setStandardColor(25, QColor(0xFF, 0x7B, 0x90, 0xFF)); // ~r~~h~~h~~h~~h~
  QColorDialog::setStandardColor(31, QColor(0xFF, 0xB8, 0xD8, 0xFF)); // ~r~~h~~h~~h~~h~~h~
  QColorDialog::setStandardColor(2, QColor(0x32, 0x3C, 0x7F, 0xFF)); // ~b~
  QColorDialog::setStandardColor(8, QColor(0x4B, 0x5A, 0xBE, 0xFF)); // ~b~~h~
  QColorDialog::setStandardColor(14, QColor(0x70, 0x87, 0xFF, 0xFF)); // ~b~~h~~h~
  QColorDialog::setStandardColor(20, QColor(0xA8, 0xCA, 0xFF, 0xFF)); // ~b~~h~~h~~h~
  QColorDialog::setStandardColor(26, QColor(0xA8, 0x6E, 0xFC, 0xFF)); // ~p~
  QColorDialog::setStandardColor(32, QColor(0xFC, 0xA5, 0xFF, 0xFF)); // ~p~~h~
  QColorDialog::setStandardColor(38, QColor(0xFF, 0xF7, 0xFF, 0xFF)); // ~p~~h~~h~

  // Game Text style 2/5 and stunt bonuses.  May be around 0xDDDDDB.
  QColorDialog::setStandardColor( 3, QColor(0x95, 0xB0, 0xD1, 0xFF)); // Game Text style 6
  QColorDialog::setStandardColor( 9, QColor(0xDD, 0xDD, 0xDD, 0xFF)); // GT style 6 ~h~
  QColorDialog::setStandardColor(15, QColor(0xE1, 0xE1, 0xE1, 0xFF)); // Game Text style 2/5, ~w~
  QColorDialog::setStandardColor(21, QColor(0xC3, 0xC3, 0xC3, 0xFF)); // Clock
  QColorDialog::setStandardColor(27, QColor(0x96, 0x96, 0x96, 0xFF)); // Radio switch
  QColorDialog::setStandardColor(33, QColor(0xAC, 0xCB, 0xF1, 0xFF)); // Region names

  // Black and white, the same in game text and CGA.
  QColorDialog::setStandardColor(44, QColor(0xFF, 0xFF, 0xFF, 0xFF));
  QColorDialog::setStandardColor(45, QColor(0x00, 0x00, 0x00, 0xFF));

  // 16 CGA colours (with dark yellow, without black).
  QColorDialog::setStandardColor(10, QColor(0x55, 0x55, 0xFF, 0xFF));
  QColorDialog::setStandardColor(11, QColor(0x00, 0x00, 0xAA, 0xFF));
  QColorDialog::setStandardColor(16, QColor(0x00, 0xAA, 0x00, 0xFF));
  QColorDialog::setStandardColor(17, QColor(0x55, 0xFF, 0x55, 0xFF));
  QColorDialog::setStandardColor(22, QColor(0x00, 0xAA, 0xAA, 0xFF));
  QColorDialog::setStandardColor(23, QColor(0x55, 0xFF, 0xFF, 0xFF));
  QColorDialog::setStandardColor(28, QColor(0xAA, 0x00, 0x00, 0xFF));
  QColorDialog::setStandardColor(29, QColor(0xFF, 0x55, 0x55, 0xFF));
  QColorDialog::setStandardColor(34, QColor(0xAA, 0x00, 0xAA, 0xFF));
  QColorDialog::setStandardColor(35, QColor(0xFF, 0x55, 0xFF, 0xFF));
  QColorDialog::setStandardColor(39, QColor(0xAA, 0x55, 0x00, 0xFF));
  QColorDialog::setStandardColor(40, QColor(0xAA, 0xAA, 0x00, 0xFF));
  QColorDialog::setStandardColor(41, QColor(0xFF, 0xFF, 0x55, 0xFF));
  QColorDialog::setStandardColor(46, QColor(0x55, 0x55, 0x55, 0xFF));
  QColorDialog::setStandardColor(47, QColor(0xAA, 0xAA, 0xAA, 0xFF));

  // Extra colours.
  QColorDialog::setStandardColor( 4, QColor(0x84, 0x77, 0xB7, 0xFF)); // open.mp purple
  QColorDialog::setStandardColor( 5, QColor(0xF0, 0x7B, 0x0F, 0xFF)); // SA-MP orange

  lastColour_ = QColorDialog::getColor(lastColour_, nullptr, QString(), QColorDialog::ShowAlphaChannel);
  if (!lastColour_.isValid()) {
    return;
  }
  if (auto editor = getCurrentEditor()) {
    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
      return;
    }
    int position = cursor.position();
    QString str = lastColour_.name(QColor::HexArgb);
    if (position == 0 || editor->document()->toPlainText()[position - 1] != 'x') {
      // Can't work out what sort of colour we want.  Just put the raw hex.
      cursor.insertText(str.mid(3, 6).toUpper());
    } else {
      // Has `x` before it, use the alpha.
      cursor.insertText((str.mid(3, 6) + str.mid(1, 2)).toUpper());
    }
  }
}

void MainWindow::on_actionDelline_triggered() {
  if (auto editor = getCurrentEditor()) {
    editor->deleteSelection();
  }
}

void MainWindow::on_actionDupline_triggered() {
  if (auto editor = getCurrentEditor()) {
    editor->duplicateSelection(true);
  }
}

void MainWindow::on_actionDupsel_triggered() {
  if (auto editor = getCurrentEditor()) {
    editor->duplicateSelection(false);
  }
}

void MainWindow::on_actionComment_triggered() {
  if (auto editor = getCurrentEditor()) {
    // I was trying to do this in a way with cursors etc.  In the end I just decided to do it with
    // string operations and loops.  Extend the selection to cover the whole of the lines.
    QTextCursor cursor = editor->textCursor();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();
    cursor.setPosition(end);
    int endPosInBlock = cursor.positionInBlock();
    int endBlockLen = cursor.block().length();
    cursor.setPosition(start);
    int startPosInBlock = cursor.positionInBlock();
    cursor.setPosition(start - startPosInBlock, QTextCursor::MoveAnchor);
    cursor.setPosition(end - endPosInBlock + endBlockLen - 1, QTextCursor::KeepAnchor);
    // We have the full lines selected.  Get the text in this area.
    QString text = cursor.selectedText();
    int length = text.length();
    std::string debug = text.toStdString();
    QChar const* data = text.data();
    // Newline, line separator, or paragraph separator.
    QRegularExpression search("(^|\\x{2029})[ \\t]*($|[^ \\t/]|/[^/]|/$)");
    QRegularExpression anything("[^ \\t]");
    if (text.indexOf(search) == -1) {
      // All lines are commented out.
      QRegularExpression comment("(^|\\x{2029})([ \\t]*)// ?");
      // Find where the first comment starts.
      int newStart = text.indexOf('/');
      // Find where the last comment starts.
      int newEnd = text.lastIndexOf(comment);
      if (text[newEnd] == QChar(0x2029)) {
        ++newEnd;
      }
      endBlockLen = text.indexOf(anything, newEnd) - newEnd;
      // Work out where to select again.
      if (startPosInBlock <= newStart) {
        // Start doesn't move.
      } else if (startPosInBlock == newStart + 1) {
        // Move back 1.
        start -= 1;
      } else if (startPosInBlock == newStart + 2 || text[newStart + 2] != ' ') {
        // Move back 2.
        start -= 2;
      } else {
        // Move back 3.
        start -= 3;
      }
      if (text[newEnd + endBlockLen + 2] == ' ') {
        if (endPosInBlock <= endBlockLen) {
          // Move forward 3.
          end += 3;
        } else if (endPosInBlock == endBlockLen + 1) {
          // Move forward 2.
          end += 2;
        } else if (endPosInBlock == endBlockLen + 2) {
          // Move forward 1.
          end += 1;
        } else {
          // End doesn't move.
        }
      } else {
        if (endPosInBlock <= endBlockLen) {
          // Move forward 2.
          end += 2;
        } else if (endPosInBlock == endBlockLen + 1) {
          // Move forward 1.
          end += 1;
        } else {
          // End doesn't move.
        }
      }
      // Do the replacement.
      text.replace(comment, "\\1\\2");
      end -= length - text.length();
    } else {
      // At least one line is uncommented.
      QRegularExpression comment("(^|\\x{2029})([ \\t]*)");
      QRegularExpression other("(^|\\x{2029})");
      // Find where the first line starts.
      int newStart = text.indexOf(anything);
      // Find where the last line starts.
      int newEnd = text.lastIndexOf(comment);
      if (text[newEnd] == QChar(0x2029)) {
        ++newEnd;
      }
      endBlockLen = text.indexOf(anything, newEnd) - newEnd;
      if (startPosInBlock >= newStart) {
        start += 3;
      }
      if (endPosInBlock < endBlockLen) {
        end -= 3;
      }
      text.replace(comment, "\\1\\2// ");
      end += text.length() - length;
    }
    debug = text.toStdString();
    cursor.insertText(text);
    cursor.setPosition(start, QTextCursor::MoveAnchor);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
  }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  int count = ui_->tabWidget->count();
  // Hover-to-close tab affordance: the tab's leading icon morphs into an X
  // when the mouse is over that tab; clicking the icon closes the tab.
  QTabBar* tb = ui_->tabWidget->tabBar();
  if (watched == tb) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest) {
      repositionAddTabBtn();
    }
    auto restoreIcon = [this, tb](int idx) {
      if (idx < 0 || idx >= tb->count()) return;
      if (idx == markedIndex_) {
        // Preserve the run-marker check icon.
        if (auto ed = getCurrentEditor()) {
          tb->setTabIcon(idx, ed->style()->standardIcon(QStyle::SP_DialogApplyButton));
        }
      } else {
        tb->setTabIcon(idx, QIcon(":/assets/images/icons/new.svg"));
      }
    };
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::HoverMove ||
        event->type() == QEvent::Enter || event->type() == QEvent::HoverEnter) {
      QPoint pos;
      if (auto me = dynamic_cast<QMouseEvent*>(event)) pos = me->pos();
      else if (auto he = dynamic_cast<QHoverEvent*>(event)) pos = he->position().toPoint();
      const int idx = tb->tabAt(pos);
      if (idx != hoveredTabIndex_) {
        if (hoveredTabIndex_ >= 0) restoreIcon(hoveredTabIndex_);
        hoveredTabIndex_ = idx;
        if (idx >= 0) tb->setTabIcon(idx, QIcon(":/assets/images/icons/tab-close.svg"));
      }
    } else if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
      if (hoveredTabIndex_ >= 0) {
        restoreIcon(hoveredTabIndex_);
        hoveredTabIndex_ = -1;
      }
    } else if (event->type() == QEvent::MouseButtonPress) {
      auto me = static_cast<QMouseEvent*>(event);
      if (me->button() == Qt::LeftButton) {
        const int idx = tb->tabAt(me->pos());
        if (idx >= 0) {
          const QRect r = tb->tabRect(idx);
          // Icon sits at the leading edge; ~22px hit zone covers icon + padding.
          QRect iconHit(r.left(), r.top(), 22, r.height());
          if (iconHit.contains(me->pos())) {
            tabCloseRequested(idx);
            return true;
          }
        }
      }
    }
  }
#ifdef Q_OS_MACOS
  // Re-show the window when the app is re-activated from the Dock after the
  // window was closed (hidden) on the home screen.
  if (event->type() == QEvent::ApplicationActivate && !isVisible()) {
    show();
    raise();
  }
  // Finder / `open foo.pwn` route here via NSApp → QEvent::FileOpen.
  if (event->type() == QEvent::FileOpen) {
    auto foe = static_cast<QFileOpenEvent*>(event);
    const QString path = foe->file();
    if (!path.isEmpty()) {
      tryLoadFile(path);
      show();
      raise();
      activateWindow();
    }
    return true;
  }
#endif
  // VS Code-style hover tooltip: when the user hovers over an identifier and
  // it matches a known symbol (predictions_ map), show a small popup. Disabled
  // via Settings → General → "Hover tooltips".
  if (event->type() == QEvent::ToolTip &&
      QSettings().value("HoverTooltips", true).toBool()) {
    auto helpEvent = static_cast<QHelpEvent*>(event);
    EditorWidget* ed = nullptr;
    for (auto e : editors_) {
      if (e && e->viewport() == watched) { ed = e; break; }
    }
    if (ed) {
      QTextCursor c = ed->cursorForPosition(helpEvent->pos());
      c.select(QTextCursor::WordUnderCursor);
      const QString word = c.selectedText();
      if (!word.isEmpty() && predictions_.contains(word)) {
        QToolTip::showText(helpEvent->globalPos(),
            QString("<div style='padding:2px 4px;'><b>%1</b><br>"
                    "<span style='color:#888'>known symbol</span></div>")
                .arg(word.toHtmlEscaped()), ed);
        return true;
      } else {
        QToolTip::hideText();
      }
    }
  }
  // Click a recent-file label on the welcome page.
  if (event->type() == QEvent::MouseButtonRelease && watched && watched->isWidgetType()) {
    QVariant rp = watched->property("recentPath");
    if (rp.isValid()) {
      tryLoadFile(rp.toString());
      return true;
    }
  }
  switch (event->type()) {
  case QKeyEvent::KeyPress:
    switch (static_cast<QKeyEvent*>(event)->key()) {
    case Qt::Key_Down:
      if (popup_) {
        popup_->setCurrentRow(popup_->currentRow() + 1);
        return true;
      } else if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ShiftModifier) {
          // Move the selection up.
          if (EditorWidget* editor = getCurrentEditor()) {
            editor->moveSelection(1);
          }
        } else {
          // Scroll up.
          scrollByLines(1);
        }
        return true;
      }
      break;
    case Qt::Key_Up:
      if (popup_) {
        popup_->setCurrentRow(popup_->currentRow() - 1);
        return true;
      } else if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ShiftModifier) {
          // Move the selection down.
          if (EditorWidget* editor = getCurrentEditor()) {
            editor->moveSelection(-1);
          }
        } else {
          // Scroll down.
          scrollByLines(-1);
        }
        return true;
      }
      break;
    case Qt::Key_Enter:
    case Qt::Key_Return:
      if (popup_) {
        replaceSuggestion();
        return true;
      }
      break;
    case Qt::Key_Escape:
      if (popup_) {
        hidePopup();
        return true;
      }
      break;
    case Qt::Key_Tab:
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Tab switcher forwards.
        if (ui_->actionMRU->isChecked()) {
          ++mruIndex_;
          if (mruIndex_ == count) {
            mruIndex_ = 0;
          }
          ui_->tabWidget->setCurrentIndex(mru_[count - 1 - mruIndex_]);
        } else {
          ui_->tabWidget->setCurrentIndex((getCurrentIndex() + 1) % count);
        }
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_1:
      // What is DRY code?
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Jump to tab.
        ui_->tabWidget->setCurrentIndex(0);
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_2:
      // What is DRY code?
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Jump to tab.
        ui_->tabWidget->setCurrentIndex(1);
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_3:
      // What is DRY code?
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Jump to tab.
        ui_->tabWidget->setCurrentIndex(2);
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_4:
      // What is DRY code?
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Jump to tab.
        ui_->tabWidget->setCurrentIndex(3);
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_5:
      // What is DRY code?
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Jump to tab.
        ui_->tabWidget->setCurrentIndex(4);
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_6:
      // What is DRY code?
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Jump to tab.
        ui_->tabWidget->setCurrentIndex(5);
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_7:
      // What is DRY code?
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Jump to tab.
        ui_->tabWidget->setCurrentIndex(6);
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_8:
      // What is DRY code?
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Jump to tab.
        ui_->tabWidget->setCurrentIndex(7);
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_9:
      // What is DRY code?
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Jump to tab.
        ui_->tabWidget->setCurrentIndex(8);
        // Stop propagation.
        return true;
      }
      break;
    case Qt::Key_0:
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        return zoomFocused(0);
      }
      break;
    case Qt::Key_Equal:
    case Qt::Key_Plus:
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        return zoomFocused(+1);
      }
      break;
    case Qt::Key_Minus:
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        return zoomFocused(-1);
      }
      break;
    case Qt::Key_Backtab:
      if (static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) {
        // Tab switcher backwards.
        if (ui_->actionMRU->isChecked()) {
          --mruIndex_;
          if (mruIndex_ == -1) {
            mruIndex_ = count - 1;
          }
          ui_->tabWidget->setCurrentIndex(mru_[count - 1 - mruIndex_]);
        } else {
          ui_->tabWidget->setCurrentIndex((getCurrentIndex() - 1 + count) % count);
        }
        // Stop propagation.
        return true;
      }
      break;
    }
    break;
  case QKeyEvent::KeyRelease:
    switch (static_cast<QKeyEvent*>(event)->key()) {
    case Qt::Key_Control:
      // If they release ctrl, reset the MRU switcher, regardless of what else is happening.
      mruIndex_ = 0;
      break;
    }
    break;
  }
  // Pass it on.
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::on_actionClose_triggered() {
  hidePopup();

  bool canClose = true;
  int cur = getCurrentIndex();
  if (cur < 0) {
    return;
  }

  if (isFileModified() && !isFileEmpty()) {
    QString message = (!isNewFile())
      ? tr("Save changes to %1?").arg(fileNames_.at(cur))
      : tr("Save changes to a new file?");
    int result = QMessageBox::question(this,
                                       QCoreApplication::applicationName(),
                                       message,
                                       QMessageBox::Yes | QMessageBox::No
                                                        | QMessageBox::Cancel);
    switch (result) {
      case QMessageBox::Yes:
        on_actionSave_triggered();
        canClose = !isFileModified();
        break;
      case QMessageBox::No:
        canClose = true;
        break;
      case QMessageBox::Cancel:
        canClose = false;
        break;
    }
  }

  if (canClose) {
    parseFile(getCurrentEditor()->toPlainText(), false);
    editors_.remove(cur);
    fileNames_.removeAt(cur);
    ui_->tabWidget->removeTab(cur);
    // Closing the last tab drops to the welcome page (no forced new file).
    updateWelcomeVisibility();
  }

  updateTitle();
  startWord();
}

void MainWindow::on_actionQuit_triggered() {
  bool canClose = true;
  int cur = getCurrentIndex();

  for (int i = 0, j = ui_->tabWidget->count(); i != j; ++i) {
    ui_->tabWidget->setCurrentIndex(i);
    if (isFileModified() && !isFileEmpty()) {
      QString message = (!isNewFile())
        ? tr("Save changes to %1?").arg(fileNames_.at(i))
        : tr("Save changes to a new file?");
      int result = QMessageBox::question(this,
                                         QCoreApplication::applicationName(),
                                         message,
                                         QMessageBox::Yes | QMessageBox::No
                                                          | QMessageBox::Cancel);
      switch (result) {
        case QMessageBox::Yes:
          on_actionSave_triggered();
          canClose = canClose && !isFileModified();
          break;
        case QMessageBox::No:
          break;
        case QMessageBox::Cancel:
          canClose = false;
          break;
      }
    }
  }

  if (canClose) {
    QApplication::instance()->quit();
  } else if (cur != -1) {
    ui_->tabWidget->setCurrentIndex(cur);
    updateTitle();
  }
}

void MainWindow::on_actionSave_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  if (isNewFile()) {
    on_actionSaveAs_triggered();
    return;
  }

  QFile file(fileNames_[getCurrentIndex()]);
  if (!file.open(QIODevice::WriteOnly)) {
    QString message =
      tr("Could not save to %1: %2.").arg(fileNames_[getCurrentIndex()], file.errorString());
    QMessageBox::critical(this,
                          QCoreApplication::applicationName(),
                          message,
                          QMessageBox::Ok);
    return;
  }

  QString text = getCurrentEditor()->toPlainText();
  QTextCodec *codec = QTextCodec::codecForName("Windows-1251");
  file.write(codec ? codec->fromUnicode(text) : text.toUtf8());
  getCurrentEditor()->textChanged();
  setFileModified(false);
  // Note our own save so the QFileSystemWatcher echo doesn't trigger a reload
  // dialog. Also re-add the path; some filesystems drop the watch after write.
  lastSelfSavedPath_ = fileNames_[getCurrentIndex()];
  lastSelfSaveMs_ = QDateTime::currentMSecsSinceEpoch();
  file.close();
  if (!fileWatcher_.files().contains(lastSelfSavedPath_)) {
    fileWatcher_.addPath(lastSelfSavedPath_);
  }
}

void MainWindow::on_actionSaveAs_triggered() {
  QSettings settings;
  QString dir = settings.value("LastSaveDir", "../gamemodes").toString();

  QString caption = tr("Save File As");
  QString filter = tr("Pawn scripts (*.pwn *.inc)");
  // Native save panel — same reasoning as Open above.
  QString fileName =
      QFileDialog::getSaveFileName(this, caption, dir, filter, nullptr);

  if (fileName.isEmpty()) {
    return;
  }

  // Qt's non-native dialog does not append the filter's extension, so a name
  // typed without one (e.g. "test1") would be saved extensionless and the
  // compiler then fails ("cannot read ... test1.p"). Default to .pwn.
  if (QFileInfo(fileName).suffix().isEmpty()) {
    fileName += ".pwn";
  }

  QFileInfo fileInfo(fileName);
  dir = fileInfo.dir().path();
  settings.setValue("LastSaveDir", dir);
  addRecentFile(fileName);

  ui_->tabWidget->setTabText(getCurrentIndex(), QFileInfo(fileInfo).fileName());
  fileNames_[getCurrentIndex()] = fileName;
  return on_actionSave_triggered();
}

void MainWindow::ensureFindDialog() {
  if (findDialog_) {
    return;
  }
  findDialog_ = new FindDialog(this);
  connect(findDialog_, &FindDialog::findRequested, this, [this]() {
    findStart_ = getCurrentEditor() ? getCurrentEditor()->textCursor().position() : 0;
    findRound_ = 0;
    on_actionFindNext_triggered();
  });
  connect(findDialog_, &FindDialog::replaceRequested, this, [this]() {
    findStart_ = getCurrentEditor() ? getCurrentEditor()->textCursor().position() : 0;
    findRound_ = 0;
    on_actionReplaceNext_triggered();
  });
  connect(findDialog_, &FindDialog::replaceAllRequested, this, [this]() {
    findStart_ = getCurrentEditor() ? getCurrentEditor()->textCursor().position() : 0;
    findRound_ = 0;
    on_actionReplaceAll_triggered();
  });
  findDialog_->applyTheme(effectiveDarkMode());
}

void MainWindow::on_actionFind_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  ensureFindDialog();
  findDialog_->clearStatus();
  findDialog_->show();
  findDialog_->raise();
  findDialog_->activateWindow();
  findDialog_->focusFindField();
}

void MainWindow::on_actionFindNext_triggered() {
  if (!getCurrentEditor() || !findDialog_) {
    if (findDialog_) {
      findDialog_->setStatus(tr("Nothing to search."), /*error=*/true);
    }
    return;
  }
  QTextDocument::FindFlags flags;
  if (findDialog_->matchCase())       flags |= QTextDocument::FindCaseSensitively;
  if (findDialog_->matchWholeWords()) flags |= QTextDocument::FindWholeWords;
  if (findDialog_->searchBackwards()) flags |= QTextDocument::FindBackward;

  const QString needle = findDialog_->findWhatText();
  if (needle.isEmpty()) {
    findDialog_->setStatus(tr("Enter text to search."), /*error=*/true);
    return;
  }

  QTextCursor current = getCurrentEditor()->textCursor();
  QTextCursor next;

  if (findDialog_->useRegExp()) {
    auto sens = findDialog_->matchCase() ? QRegularExpression::NoPatternOption
                                         : QRegularExpression::CaseInsensitiveOption;
    QRegularExpression regexp(needle, sens);
    next = getCurrentEditor()->document()->find(regexp, current, flags);
  } else {
    next = getCurrentEditor()->document()->find(needle, current, flags);
  }

  bool found = !next.isNull() &&
    (findRound_ == 0 || next.position() < findStart_);

  if (!found && findRound_ > 0) {
    findDialog_->setStatus(tr("No matches for \"%1\".").arg(needle), /*error=*/true);
    findRound_ = 0;
    return;
  }

  if (!found && findRound_ == 0) {
    // Wrap once.
    next = current;
    if (findDialog_->searchBackwards()) {
      next.movePosition(QTextCursor::End);
    } else {
      next.movePosition(QTextCursor::Start);
    }
    findRound_++;
    getCurrentEditor()->setTextCursor(next);
    on_actionFindNext_triggered();
    return;
  }

  getCurrentEditor()->setTextCursor(next);
  findDialog_->setStatus(tr("Match found."));
}

void MainWindow::on_actionReplaceNext_triggered() {
  if (!getCurrentEditor() || !findDialog_) {
    return;
  }
  QTextCursor current = getCurrentEditor()->textCursor();
  // If the current selection matches the search term, replace it in place;
  // otherwise jump to the next match without replacing.
  const QString needle = findDialog_->findWhatText();
  if (needle.isEmpty()) {
    findDialog_->setStatus(tr("Enter text to search."), /*error=*/true);
    return;
  }
  if (current.hasSelection() && current.selectedText() == needle) {
    current.insertText(findDialog_->replaceText());
    findDialog_->setStatus(tr("Replaced one occurrence."));
  }
  findStart_ = getCurrentEditor()->textCursor().position();
  findRound_ = 0;
  on_actionFindNext_triggered();
}

void MainWindow::on_actionReplaceAll_triggered() {
  if (!getCurrentEditor() || !findDialog_) {
    return;
  }
  QTextDocument::FindFlags flags;
  if (findDialog_->matchCase())       flags |= QTextDocument::FindCaseSensitively;
  if (findDialog_->matchWholeWords()) flags |= QTextDocument::FindWholeWords;
  // Replace-all always sweeps forward from the document start.
  flags &= ~QTextDocument::FindBackward;

  const QString needle = findDialog_->findWhatText();
  const QString replacement = findDialog_->replaceText();
  if (needle.isEmpty()) {
    findDialog_->setStatus(tr("Enter text to search."), /*error=*/true);
    return;
  }

  auto* editor = getCurrentEditor();
  QTextCursor sweep(editor->document());
  sweep.movePosition(QTextCursor::Start);

  int count = 0;
  bool inEditBlock = false;
  while (true) {
    QTextCursor hit;
    if (findDialog_->useRegExp()) {
      auto sens = findDialog_->matchCase() ? QRegularExpression::NoPatternOption
                                           : QRegularExpression::CaseInsensitiveOption;
      QRegularExpression regexp(needle, sens);
      hit = editor->document()->find(regexp, sweep, flags);
    } else {
      hit = editor->document()->find(needle, sweep, flags);
    }
    if (hit.isNull()) {
      break;
    }
    if (!inEditBlock) {
      hit.beginEditBlock();
      inEditBlock = true;
    } else {
      hit.joinPreviousEditBlock();
    }
    hit.insertText(replacement);
    hit.endEditBlock();
    sweep = hit;
    ++count;
  }

  if (count == 0) {
    findDialog_->setStatus(tr("No matches for \"%1\".").arg(needle), /*error=*/true);
  } else {
    findDialog_->setStatus(
        tr("All \"%1\" replaced with \"%2\" successfully — %3 occurrence(s).")
            .arg(needle, replacement)
            .arg(count));
  }
}

void MainWindow::on_actionGoToLine_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  GoToDialog dialog;
  dialog.exec();
  getCurrentEditor()->jumpToLine(dialog.targetLineNumber());
}

void MainWindow::on_actionEditorFont_triggered() {
  if (!getCurrentEditor()) {
    return;
  }
  QFontDialog fontDialog(this);

  bool ok = false;
  QFont newFont = fontDialog.getFont(&ok, getCurrentEditor()->font(), this,
                                     tr("Editor Font"));

  if (ok) {
    getCurrentEditor()->setFont(newFont);
  }
}

void MainWindow::on_actionOutputFont_triggered() {
  QFontDialog fontDialog(this);

  bool ok = false;
  QFont newFont = fontDialog.getFont(&ok, ui_->output->font(), this,
  tr("Output Font"));

  if (ok) {
    ui_->output->setFont(newFont);
  }
}

void MainWindow::on_actionDarkMode_triggered() {
  // The Settings-menu item is a plain light/dark toggle; the toolbar button
  // offers the full System/Light/Dark choice.
  setThemeMode(effectiveDarkMode() ? 1 : 2);
}

void MainWindow::on_actionMRU_triggered() {
  QSettings settings;
  bool useDarkMode = !settings.value("MRU", false).toBool();
  settings.setValue("MRU", useDarkMode);
}

void MainWindow::on_actionCompiler_triggered() {
  Compiler compiler;
  CompilerSettingsDialog dialog;

  dialog.setCompilerPath(compiler.path());
  dialog.setCompilerOptions(compiler.options().join(" "));

  dialog.exec();

  if (dialog.result() == QDialog::Accepted) {
    compiler.setPath(dialog.compilerPath());
    compiler.setOptions(dialog.compilerOptions());
  }
}

void MainWindow::on_actionServer_triggered() {
  ServerSettingsDialog dialog;

  dialog.setServerPath(server_.path());
  dialog.setServerOptions(server_.options().join(" "));
  dialog.setServerExtras(server_.extras().join(" "));

  dialog.exec();

  if (dialog.result() == QDialog::Accepted) {
    server_.setPath(dialog.serverPath());
    server_.setOptions(dialog.serverOptions());
    server_.setExtras(dialog.serverExtras());
  }
}

void MainWindow::on_actionSaveAll_triggered() {
  int cur = getCurrentIndex(), count = fileNames_.count();
  if (count == 0) {
    return;
  }
  // Loop over all the tabs and save them all.  There could be include dependencies.
  for (int i = 0; i != count; ++i) {
    ui_->tabWidget->setCurrentIndex(i);
    if (isNewFile()) {
      on_actionSaveAs_triggered();
    } else {
      on_actionSave_triggered();
    }
  }
  // Return to the originally selected tab.
  ui_->tabWidget->setCurrentIndex(cur);
}

void MainWindow::runCompile(bool alsoRun) {
  if (fileNames_.isEmpty()) {
    return;
  }
  on_actionSaveAll_triggered();
  int idx = markedIndex_ == -1 ? getCurrentIndex() : markedIndex_;
  if (idx < 0 || idx >= fileNames_.size()) {
    return;
  }
  const QString fileName = fileNames_[idx];
  // New/unsaved files have an empty name; compiling them yields bogus paths.
  if (fileName.isEmpty()) {
    QMessageBox::warning(this, QCoreApplication::applicationName(),
        tr("Save the file before compiling."), QMessageBox::Ok);
    return;
  }

#ifdef Q_OS_MACOS
  // Ensure the native pawncc is present in the project's qawno/native/ folder.
  if (!macCompilePreflight(fileName)) {
    return;
  }
#endif

  // Deploy the bundled native pawncc into <projectDir>/qawno/native/ so the
  // same compiler is reusable from a terminal/CI, and the project stays
  // self-contained. No Wine/CrossOver is involved.
  const QString qawnoDir = projectQawnoDir(fileName);
  deployNativeCompiler(qawnoDir);

  Compiler compiler;
  // Pin the compiler to the discovered absolute native binary so commandFor
  // doesn't synthesise a non-existent sibling path. projectQawnoDir walks up
  // to find the project's qawno/ folder (parent of gamemodes/, includes/, …).
  const QString deployedCompiler = qawnoDir + "/native/pawncc";
  if (QFile::exists(deployedCompiler)) {
    compiler.setPath(deployedCompiler);
  }
  const QString command = compiler.commandFor(fileName);
  const QString shortName = QFileInfo(fileName).fileName();

  ui_->output->clear();
  ui_->output->resetErrorCounter();
  ui_->output->appendPlainText(tr("Compiling %1…").arg(shortName));
  clearDiagnostics();

  // Disable the Run/Compile actions for the duration.
  setCompileActionsEnabled(false);

  QProcess *proc = new QProcess(this);
  proc->setProcessChannelMode(QProcess::MergedChannels);
  proc->setWorkingDirectory(QDir::currentPath());

  // The native compiler needs no special environment. It links libpawnc.dylib
  // from its own folder via @loader_path, so it runs straight from qawno/native/.
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  proc->setProcessEnvironment(env);

  auto canceled = std::make_shared<bool>(false);

  // When the bottom output panel is visible, drive the compile inline there
  // (progress bar + Cancel as a strip above the output, log streamed into the
  // panel). When it's hidden, fall back to a modal popup so the user still sees
  // progress. inline_ == nullptr means "popup mode".
  const bool inlineMode = ui_->output->isVisible();

  QProgressBar *bar = new QProgressBar;
  bar->setRange(0, 100);
  bar->setValue(0);
  bar->setFormat(tr("%p%"));
  bar->setStyleSheet(
      "QProgressBar { border: 1px solid #C8722A; border-radius: 4px; text-align: center; height: 16px; }"
      "QProgressBar::chunk { background-color: #F0843C; border-radius: 3px; }");
  QPushButton *cancelBtn = new QPushButton(
      QIcon(":/assets/images/icons/close.svg"), tr("Cancel"));

  // log: where streamed output goes. Inline -> the bottom panel; popup -> the
  // dialog's own text view. strip_/dlg are the host widgets, freed in finish().
  QPlainTextEdit *log = nullptr;
  QWidget *strip = nullptr; // inline progress strip in the splitter
  QDialog *dlg = nullptr;   // popup host

  // Drop any older compile strips still sitting above the output panel —
  // applies in both inline AND popup modes so a hidden output panel never
  // hangs onto a stale "Compile failed" strip.
  for (int i = ui_->splitter->count() - 1; i >= 0; --i) {
    QWidget* w = ui_->splitter->widget(i);
    if (w && w->objectName() == "compileStrip") w->deleteLater();
  }

  if (inlineMode) {
    // A compact strip (progress + Cancel) inserted above the output panel.
    strip = new QWidget;
    strip->setObjectName("compileStrip");
    QHBoxLayout *sl = new QHBoxLayout(strip);
    sl->setContentsMargins(6, 4, 6, 4);
    sl->setSpacing(8);
    sl->addWidget(bar, 1);
    sl->addWidget(cancelBtn, 0);
    int outIdx = ui_->splitter->indexOf(ui_->output);
    ui_->splitter->insertWidget(outIdx < 0 ? ui_->splitter->count() : outIdx,
                                strip);
    log = ui_->output; // stream straight into the bottom panel
    log->appendPlainText(command + "\n");
  } else {
    // Popup mode: vertical stack — header row (title + wrap toggle), progress
    // bar, rounded log area, full-width close/cancel at the bottom. Stays
    // on top via Qt::Tool so it doesn't slip behind the editor window.
    dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Qawno Compiler"));
    dlg->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint
                        | Qt::WindowCloseButtonHint);
    // Stay-on-top keeps the popup visible while the user clicks back into
    // the editor to look at a flagged line.
    dlg->setWindowFlag(Qt::WindowStaysOnTopHint, true);
    dlg->setWindowModality(Qt::NonModal);
    dlg->setMinimumSize(640, 380);
    QVBoxLayout *dl = new QVBoxLayout(dlg);
    dl->setSpacing(10);
    dl->setContentsMargins(14, 12, 14, 12);

    // Header row: "Qawno Compiler — <filename.pwn>" left, "Wrap" toggle right.
    QHBoxLayout *hdrRow = new QHBoxLayout;
    QLabel* hdrTitle = new QLabel(tr("Qawno Compiler — %1").arg(shortName));
    hdrTitle->setStyleSheet("font-weight: 600; font-size: 13px;");
    hdrRow->addWidget(hdrTitle, 1);
    auto* wrapLabel = new QLabel(tr("Wrap"));
    wrapLabel->setStyleSheet("color:#B6BDC7;font-size:11px;");
    hdrRow->addWidget(wrapLabel, 0);
    auto* wrapToggle = new IosToggle;
    wrapToggle->setFixedSize(34, 20);
    // Wrap preference is persisted (Settings → Compiler exposes the same key).
    wrapToggle->setChecked(QSettings().value("CompilerLogWrap", true).toBool());
    hdrRow->addWidget(wrapToggle, 0);
    dl->addLayout(hdrRow);

    // Progress bar directly under the header — indeterminate while running.
    dl->addWidget(bar);

    // Rounded log area.
    log = new QPlainTextEdit(dlg);
    log->setReadOnly(true);
    log->setLineWrapMode(QPlainTextEdit::WidgetWidth);  // matches wrap=on default
    log->setFont(ui_->output->font());
    log->setStyleSheet(
        "QPlainTextEdit { background: #1B1F25; color: #C8CDD6; "
        "border: 1px solid #3A3F4B; border-radius: 8px; padding: 6px; }");
    log->appendPlainText(command + "\n");
    dl->addWidget(log, 1);
    log->setLineWrapMode(wrapToggle->isChecked() ? QPlainTextEdit::WidgetWidth
                                                  : QPlainTextEdit::NoWrap);
    QObject::connect(wrapToggle, &IosToggle::toggled, log, [log](bool on) {
      log->setLineWrapMode(on ? QPlainTextEdit::WidgetWidth
                              : QPlainTextEdit::NoWrap);
      QSettings().setValue("CompilerLogWrap", on);
    });

    // Bottom row: Close (left) + Recompile (right). cancelBtn morphs into
    // Close at finish(); recompile triggers the same runCompile path.
    QHBoxLayout* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(8);
    cancelBtn->setMinimumHeight(34);
    bottomRow->addWidget(cancelBtn, 1);
    auto* recompileBtn = new QPushButton(QIcon(":/assets/images/icons/run.svg"),
                                          tr("Recompile"));
    recompileBtn->setMinimumHeight(34);
    recompileBtn->setCursor(Qt::PointingHandCursor);
    recompileBtn->setStyleSheet(
        "QPushButton { border-radius: 8px; padding: 6px 16px; "
        "background: #1F6E3D; color: white; font-weight: 600; border: none; }"
        "QPushButton:hover { background: #258049; }");
    bottomRow->addWidget(recompileBtn, 1);
    dl->addLayout(bottomRow);
    connect(recompileBtn, &QPushButton::clicked, this, [this, alsoRun, dlg] {
      // Close the existing popup first so finish() doesn't fight with the
      // new compile's strip/dialog setup.
      if (dlg) { dlg->close(); dlg->deleteLater(); }
      runCompile(alsoRun);
    });
  }
  // In popup mode the bottom inline strip is redundant — drop it so the
  // user only sees one progress indicator.
  if (dlg && strip) {
    strip->deleteLater();
    strip = nullptr;
  }

  connect(cancelBtn, &QPushButton::clicked, proc, [proc, canceled]() {
    *canceled = true;
    proc->kill();
  });

  // Bar runs in indeterminate mode (no crawl timer needed). On completion we
  // flip the range back to 0..100 + setValue(100) so the "Compiled" / failed
  // state shows a fully-filled chunk instead of the busy animation.
  QTimer *timer = new QTimer(this);
  timer->setInterval(1000);  // kept as a heartbeat-only hook for finish()

  // Safety timeout: a hung pawncc (e.g. a bad include tree) would otherwise
  // freeze forever. Kill it after 90 s.
  QTimer *killTimer = new QTimer(this);
  killTimer->setSingleShot(true);
  killTimer->setInterval(90000);
  connect(killTimer, &QTimer::timeout, this, [proc, log, canceled]() {
    *canceled = true;
    log->appendPlainText(tr("\n[timed out after 90s — compiler killed]"));
    proc->kill();
  });

  // Stream output live to the active log (bottom panel inline, or popup view).
  // Rewrite "Z:\…\foo.pwn(321) : warning" lines so they show the short path
  // the user asked for ("foo.pwn(321) : warning" or with the user-prefix
  // stripped when full paths are enabled in Settings).
  auto rewriteChunk = [this](QString chunk) {
    // Strip MoltenVK / Vulkan extension dump that wine pollutes stderr with
    // on macOS — none of it relates to the Pawn compile and it swamps the
    // actual diagnostic lines the user cares about.
    static QRegularExpression vkNoise(
        R"(((^|\n)\[mvk-[^\n]*\n(?:[ \t][^\n]*\n)*)|((^|\n)\s*(VK_|Metal Shading|GPU Family|Read-Write|GPU memory|GPU device|model:|type:|vendorID:|deviceID:|pipelineCacheUUID:|supports the following|Created VkInstance|The following \d+ Vulkan extensions are supported)[^\n]*))");
    chunk.remove(vkNoise);
    static QRegularExpression rx("^(.+?\\.(?:pwn|inc))\\((\\d+)\\)",
                                  QRegularExpression::MultilineOption);
    QString out;
    int last = 0;
    auto it = rx.globalMatch(chunk);
    while (it.hasNext()) {
      QRegularExpressionMatch m = it.next();
      out += chunk.mid(last, m.capturedStart() - last);
      out += shortenPath(m.captured(1));
      out += "(";
      out += m.captured(2);
      out += ")";
      last = m.capturedEnd();
    }
    out += chunk.mid(last);
    return out;
  };
  connect(proc, &QProcess::readyRead, this, [this, proc, log, bar, rewriteChunk]() {
    QString chunk = QString::fromLocal8Bit(proc->readAll());
    if (chunk.isEmpty()) {
      return;
    }
    // Diagnostics: parse "<path>(line) : error|warning" lines and decorate
    // the matching editor with a wavy underline. Gated by the LintSquiggles
    // setting (Settings → General).
    if (QSettings().value("LintSquiggles", true).toBool()) {
      static QRegularExpression diagRx(
          R"((^|\n)\s*(.+?\.(?:pwn|inc))\((\d+)\)\s*:\s*(error|warning))",
          QRegularExpression::CaseInsensitiveOption);
      auto it = diagRx.globalMatch(chunk);
      while (it.hasNext()) {
        auto m = it.next();
        applyDiagnostic(m.captured(2), m.captured(3).toInt(),
                        m.captured(4).compare("error", Qt::CaseInsensitive) == 0);
      }
    }
    chunk = rewriteChunk(chunk);
    log->moveCursor(QTextCursor::End);
    log->insertPlainText(chunk);
    log->moveCursor(QTextCursor::End);

    // Stage-based progress estimator. pawncc emits no percentage, so we map
    // recognisable banner lines to fixed waypoints (10 / 50 / 80 / 95) and
    // bump the bar a touch per diagnostic in between for a sense of motion.
    int next = bar->value();
    if (chunk.contains("Compiling ") && next < 10) next = 10;
    static QRegularExpression diagInc(
        R"(\.(pwn|inc)\(\d+\)\s*:\s*(error|warning|fatal))",
        QRegularExpression::CaseInsensitiveOption);
    auto it = diagInc.globalMatch(chunk);
    while (it.hasNext()) { it.next(); if (next < 75) next += 2; }
    if (chunk.contains("Pawn compiler ") && next < 80) next = 80;
    if (chunk.contains("Header size:") && next < 90) next = 90;
    if (chunk.contains("Total requirements:") && next < 95) next = 95;
    if (chunk.contains("Done.") && next < 98) next = 98;
    if (next > bar->value()) bar->setValue(next);
  });

  // Single cleanup, guarded so finished/errorOccurred can't run it twice. On
  // failure the popup stays open (so the error is readable) and is given a
  // Close button; the inline strip is always removed but the panel keeps its
  // log. The compiler actions are always re-enabled.
  auto guard = std::make_shared<bool>(false);
  auto finish = [this, proc, dlg, strip, bar, cancelBtn, log, timer, killTimer,
                 alsoRun, guard, canceled](bool failedToStart) {
    if (*guard) {
      return;
    }
    *guard = true;
    timer->stop();
    killTimer->stop();
    QString tail = QString::fromLocal8Bit(proc->readAll());
    if (failedToStart) {
      tail += "\n" + proc->errorString();
    }
    if (!tail.trimmed().isEmpty()) {
      // Rewrite paths in the tail too — same logic as the streaming chunk.
      static QRegularExpression tx("^(.+?\\.(?:pwn|inc))\\((\\d+)\\)",
                                    QRegularExpression::MultilineOption);
      QString out;
      int last = 0;
      auto it = tx.globalMatch(tail);
      while (it.hasNext()) {
        auto m = it.next();
        out += tail.mid(last, m.capturedStart() - last);
        out += shortenPath(m.captured(1));
        out += "(";
        out += m.captured(2);
        out += ")";
        last = m.capturedEnd();
      }
      out += tail.mid(last);
      log->moveCursor(QTextCursor::End);
      log->insertPlainText(out);
      log->moveCursor(QTextCursor::End);
    }
    bar->setValue(100);
    const bool ok = !*canceled && !failedToStart &&
                    proc->exitStatus() == QProcess::NormalExit &&
                    proc->exitCode() == 0;
    // Status label inside the progress bar ("Compiled" / "Failed" / "Cancelled")
    // instead of "100%". Tint the chunk by outcome so the user can read the
    // status without scanning the log.
    bar->setFormat(*canceled ? tr("Cancelled")
                              : (ok ? tr("Compiled") : tr("Compile failed")));
    bar->setStyleSheet(QString(
        "QProgressBar { border: 1px solid %1; border-radius: 4px; "
        "text-align: center; height: 16px; color: %2; font-weight: 600; }"
        "QProgressBar::chunk { background-color: %3; border-radius: 3px; }")
        .arg(ok ? "#2E8B5A" : (*canceled ? "#8B6F2E" : "#9A2A2A"),
             ok ? "#FFFFFF" : "#FFFFFF",
             ok ? "#3CB371" : (*canceled ? "#D8A23C" : "#D04848")));
    log->appendPlainText(*canceled ? tr("Cancelled.")
                                    : (ok ? tr("Done.") : tr("Compile failed.")));

    // Inline strip stays visible (with "Compiled" label) so the result is
    // readable; clicking the button after completion dismisses the strip.
    if (strip) {
      cancelBtn->disconnect();
      cancelBtn->setText(tr("Dismiss"));
      cancelBtn->setIcon(QIcon(":/assets/images/icons/close.svg"));
      cancelBtn->setEnabled(true);
      connect(cancelBtn, &QPushButton::clicked, strip, [strip]() {
        strip->deleteLater();
      });
    }

    if (dlg) {
      // Popup stays on screen regardless of outcome. The bottom button
      // switches from Cancel to Close so the user dismisses it manually.
      cancelBtn->disconnect();
      cancelBtn->setText(tr("Close"));
      cancelBtn->setIcon(QIcon(":/assets/images/icons/close.svg"));
      cancelBtn->setEnabled(true);
      connect(cancelBtn, &QPushButton::clicked, dlg, [dlg]() {
        dlg->close();
        dlg->deleteLater();
      });
    }

    proc->deleteLater();
    timer->deleteLater();
    killTimer->deleteLater();
    setCompileActionsEnabled(true);
    // The legacy alsoRun path launched the open.mp server after compile.
    // Qawno is just a Pawn editor/compiler — server launch lives elsewhere.
    (void)alsoRun;
  };

  connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [finish](int, QProcess::ExitStatus) { finish(false); });
  connect(proc, &QProcess::errorOccurred, this,
          [finish](QProcess::ProcessError) { finish(true); });

  timer->start();
  killTimer->start();
  if (dlg) {
    dlg->show();
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  proc->startCommand(command, QProcess::ReadOnly);
#else
  proc->start(command, QProcess::ReadOnly);
#endif
  // Close stdin so a compiler that prompts gets EOF instead of blocking.
  proc->closeWriteChannel();
}

void MainWindow::setCompileActionsEnabled(bool on) {
  ui_->actionCompile->setEnabled(on);
  ui_->actionCompileRun->setEnabled(on);
  ui_->actionRun->setEnabled(on);
}

void MainWindow::on_actionCompile_triggered() {
  runCompile(false);
}

void MainWindow::errorClicked() {
  QTextCursor cursor = ui_->output->textCursor();
  if (!cursor.hasSelection()) {
    return;
  }
  cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
  cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
  // Example formats:
  //   D:\open.mp\gamemodes\independence.pwn(9) : warning 203: …
  //   zombie.pwn(9) : warning 203: …          (after path shortening)
  //   /Documents/Projects/foo/bar.pwn(9) : …  (user-prefix stripped)
  QRegularExpression message("^(.*?)\\((\\d+)\\)");
  QString text = cursor.selectedText();
  QRegularExpressionMatch match = message.match(text);
  if (!match.hasMatch()) {
    return;
  }
  QString fileName = match.captured(1);
  QString line = match.captured(2);
  ui_->output->setTextCursor(cursor);

  // Try open tabs first: shortened forms ("zombie.pwn" or "/Documents/…")
  // won't resolve via absoluteFilePath but should match the basename or a
  // trailing-substring of an open file's path.
  QString matched;
  QString needleBase = QFileInfo(fileName).fileName();
  for (const QString& open : fileNames_) {
    if (open.isEmpty()) continue;
    if (open == fileName) { matched = open; break; }
    if (QFileInfo(open).fileName() == needleBase) {
      // Prefer an exact tail match over a basename collision.
      if (matched.isEmpty() || open.endsWith(fileName)) {
        matched = open;
      }
    }
  }
  if (matched.isEmpty()) {
    matched = QFileInfo(fileName).absoluteFilePath();
  }
  jumpToLine(matched, line.toInt());
}

void MainWindow::on_actionMark_triggered() {
  if (markedIndex_ == -1) {
    markedIndex_ = getCurrentIndex();
    if (markedIndex_ != -1) {
      ui_->tabWidget->setTabIcon(markedIndex_, getCurrentEditor()->style()->standardIcon(QStyle::SP_DialogApplyButton));
    }
  } else {
    ui_->tabWidget->setTabIcon(markedIndex_, QIcon(""));
    markedIndex_ = -1;
  }
}

void MainWindow::on_actionRun_triggered() {
  if (fileNames_.isEmpty()) {
    return;
  }
  server_.run(fileNames_[markedIndex_ == -1 ? getCurrentIndex() : markedIndex_]);
}

void MainWindow::promptForUpdate() {
  auto* uc = UpdateChecker::instance();
  const auto qawnoRel = uc->latest(UpdateChecker::Component::Qawno);
  if (qawnoRel.version.isEmpty()) return;  // nothing to install

  // Collect dirty buffers — tab title (with file name) is enough context
  // for the user; we'll iterate the same indices to save/discard later.
  QStringList unsaved;
  for (int i = 0; i < editors_.size(); ++i) {
    if (editors_[i]->document()->isModified()) {
      unsaved << ui_->tabWidget->tabText(i);
    }
  }

  UpdatePromptDialog dlg(unsaved, qawnoRel.tag, this);
  dlg.exec();
  switch (dlg.outcome()) {
    case UpdatePromptDialog::Postpone:
      return;
    case UpdatePromptDialog::ProceedSave:
      on_actionSaveAll_triggered();
      break;
    case UpdatePromptDialog::ProceedDiscard:
      // Mark every dirty doc clean so the close path doesn't re-prompt.
      for (auto* e : editors_) e->document()->setModified(false);
      break;
    case UpdatePromptDialog::ProceedNoUnsaved:
      break;
  }

  Installer::Plan plan;
  plan.qawnoAssetPath    = uc->downloadedPath(UpdateChecker::Component::Qawno);
  plan.compilerAssetPath = uc->downloadedPath(UpdateChecker::Component::Compiler);

  // Resolve the project's qawno/ dir for compiler replacement. Compiler::path
  // is "%p/qawno/pawn-cc.sh" by default; the parent of that resolved path is
  // the directory we want to drop the new pawncc into.
  if (!plan.compilerAssetPath.isEmpty()) {
    Compiler c;
    QString cmdPath = c.path();
    // We need a real project context (%p), so use the currently active file.
    if (!fileNames_.isEmpty()) {
      const QString in = fileNames_[getCurrentIndex()];
      const QString p  = QFileInfo(in).absolutePath();
      const QString o  = QFileInfo(in).baseName();
      cmdPath.replace("%p", p).replace("%o", o)
             .replace("%q", QCoreApplication::applicationDirPath());
      plan.compilerTargetDir = QFileInfo(cmdPath).absolutePath();
    } else {
      // No open project: skip compiler install rather than guessing.
      plan.compilerAssetPath.clear();
    }
  }

  // Nothing to install — kick off downloads instead of quitting.
  if (plan.qawnoAssetPath.isEmpty() && plan.compilerAssetPath.isEmpty()) {
    uc->downloadAsset(UpdateChecker::Component::Qawno);
    uc->downloadAsset(UpdateChecker::Component::Compiler);
    QMessageBox::information(this, tr("Update"),
        tr("Downloading the update in the background. Try again in a moment."));
    return;
  }

  if (SettingsDialog::reopenFilesAfterUpdate()) {
    for (const QString& f : fileNames_) {
      if (!f.isEmpty()) plan.reopenFiles << f;
    }
  }

  if (!Installer::startUpdate(plan)) {
    QMessageBox::warning(this, tr("Update"),
        tr("Couldn't start the installer. The download is saved here:\n%1")
            .arg(plan.qawnoAssetPath.isEmpty() ? plan.compilerAssetPath
                                                : plan.qawnoAssetPath));
    return;
  }
  QCoreApplication::quit();
}

void MainWindow::on_actionCompileRun_triggered() {
  runCompile(true);
}

void MainWindow::on_actionNextErr_triggered() {
  ui_->output->advanceErrorCounter();
}

void MainWindow::on_actionAbout_triggered() {
  AboutDialog dialog;
  dialog.exec();
}

void MainWindow::on_actionAboutQt_triggered() {
  QMessageBox::aboutQt(this);
}

void MainWindow::on_editor_cursorPositionChanged() {
  if (!getCurrentEditor()) {
    return;
  }
  hidePopup();
  prevWord_ = initialWord_;
  prevEnd_ = wordEnd_;
  startWord();
  QTextCursor cursor = getCurrentEditor()->textCursor();
  int line = cursor.blockNumber() + 1;
  int column = cursor.columnNumber() + 1;
  int selected = cursor.selectionEnd() - cursor.selectionStart();
  int docLen = getCurrentEditor()->document()->characterCount() - 1;
  bool allSelected = selected > 0 && selected >= docLen;
  dynamic_cast<StatusBar*>(statusBar())->setCursorPosition(line, column, selected, allSelected);
  updateFileInfo();
}

void MainWindow::updateFileInfo() {
  StatusBar* sb = dynamic_cast<StatusBar*>(statusBar());
  if (!sb) {
    return;
  }
  EditorWidget* editor = getCurrentEditor();
  if (!editor) {
    sb->setFileInfo(0, 0, false);
    return;
  }
  QString text = editor->toPlainText();
  int lines = editor->document()->blockCount();
  qint64 bytes = text.toUtf8().size();
  sb->setFileInfo(lines, bytes, true);
}

void MainWindow::closeEvent(QCloseEvent *event) {
#ifdef Q_OS_MACOS
  // macOS: closing the window when no files are open hides the app but keeps
  // it alive in the Dock (standard mac behavior). Quit is Cmd+Q / Dock menu.
  if (editors_.isEmpty()) {
    hide();
    event->ignore();
    return;
  }
#endif
  on_actionQuit_triggered();
  if (isFileEmpty()) {
    event->accept();
  } else {
    event->ignore();
  }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  // Auto-collapse the INCLUDES sidebar when the window gets too narrow to hold
  // both it and a usable editor (mirrors how the log can be hidden).
  if (sidebarPane_ && !editors_.isEmpty()) {
    const bool tooNarrow = width() < 820;
    if (tooNarrow && sidebarPane_->isVisible()) {
      sidebarPane_->setVisible(false);
    } else if (!tooNarrow && !sidebarPane_->isVisible() && sidebarAutoHidden_) {
      sidebarPane_->setVisible(true);
    }
    if (tooNarrow) {
      sidebarAutoHidden_ = true;
    } else if (sidebarPane_->isVisible()) {
      sidebarAutoHidden_ = false;
    }
  }
  // Responsive toolbar: collapse labels when the window is narrow.
  applyToolbarStyle();
}

// Only accept .pwn / .inc files (pawn source + include). Other types (images,
// arbitrary text, binaries) are not openable by this editor and dragging them
// in by mistake used to land them in the Recents list — block them up-front.
static bool isPawnFile(const QString& path) {
  const QString lower = path.toLower();
  return lower.endsWith(".pwn") || lower.endsWith(".inc");
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
  if (!event->mimeData()->hasUrls()) return;
  for (const QUrl& u : event->mimeData()->urls()) {
    if (u.isLocalFile() && isPawnFile(u.toLocalFile())) {
      event->acceptProposedAction();
      return;
    }
  }
}

void MainWindow::dropEvent(QDropEvent *event) {
  QList<QUrl> urls = event->mimeData()->urls();
  foreach (QUrl url, urls) {
    if (url.isLocalFile() && isPawnFile(url.toLocalFile())) {
      loadFile(url.toLocalFile());
    }
  }
}

const QString& MainWindow::getCurrentName() const {
  static QString none = "";
  if (fileNames_.isEmpty()) {
    return none;
  }
  return fileNames_[getCurrentIndex()];
}

EditorWidget* MainWindow::getCurrentEditor() const {
  if (editors_.isEmpty()) {
    return 0;
  }
  return editors_[getCurrentIndex()];
}

int MainWindow::getCurrentIndex() const {
  return ui_->tabWidget->currentIndex();
}

void MainWindow::updateTitle() {
  QString title;

  if (fileNames_.isEmpty()) {
    title = "No File";
  } else if (isNewFile()) {
    if (isFileModified()) {
      title = "Untitled File *";
    } else {
      title = "Untitled File";
    }
  } else {
    title = QFileInfo(fileNames_[getCurrentIndex()]).fileName();
    if (isFileModified()) {
      title.append(" *");
    }
    ui_->tabWidget->setTabText(getCurrentIndex(), title);
  }
  title.append(" - ");
  title.append(QCoreApplication::applicationName());

  setWindowTitle(title);
}

int MainWindow::tryLoadFile(const QString &fileName) {
  for (int i = fileNames_.count(); i--; ) {
    if (fileNames_[i] == fileName) {
      ui_->tabWidget->setCurrentIndex(i);
      // It wasn't loaded (but is open).
      return -1;
    }
  }

  return loadFile(fileName) ? 1 : 0;
}

void MainWindow::jumpToLine(const QString& fileName, int line) {
  if (tryLoadFile(fileName)) {
    // Was just opened, or was already open.  Either way, we're now on the correct tab.
    if (EditorWidget* editor = getCurrentEditor()) {
      editor->jumpToLine(line);
    }
  }
}

bool MainWindow::loadFile(const QString &fileName, const char* encoding) {
  bool nu = fileName.startsWith(".");
  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly)) {
    QString message = tr("Could not open %1: %2.").arg(fileName)
                                                  .arg(file.errorString());
    QMessageBox::critical(this,
                          QCoreApplication::applicationName(),
                          message,
                          QMessageBox::Ok);
    return false;
  }

  QByteArray raw = file.readAll();
  QTextCodec *codec = QTextCodec::codecForName(encoding);
  QString contents = codec ? codec->toUnicode(raw) : QString::fromUtf8(raw);

  fileNames_.push_back(nu ? "" : fileName);
  if (!nu) {
    addRecentFile(fileName);
    if (!fileWatcher_.files().contains(fileName)) {
      fileWatcher_.addPath(fileName);
    }
  }
  QString path = nu ? QString("New %1").arg(++newCount_) : fileName;
  createTab(nu ? path : file.fileName(), path);
  editors_.last()->setPlainText(contents);
  parseFile(editors_.last()->toPlainText(), true);
  setFileModified(false);
  file.close();

  return true;
}

void MainWindow::fileChangedExternally(const QString& path) {
  // Some editors (and our own save) write atomically (write-temp-then-rename),
  // which causes QFileSystemWatcher to drop the path. Re-add it after a beat
  // so we keep getting notifications.
  QTimer::singleShot(150, this, [this, path] {
    if (QFile::exists(path) && !fileWatcher_.files().contains(path)) {
      fileWatcher_.addPath(path);
    }
  });

  // Ignore the watcher echo of our own save (within ~600 ms).
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (path == lastSelfSavedPath_ && now - lastSelfSaveMs_ < 600) {
    return;
  }

  // Find the tab(s) hosting this path. fileNames_ is parallel to editors_.
  for (int i = 0; i < fileNames_.size(); ++i) {
    if (fileNames_[i] != path) continue;
    EditorWidget* editor = (i < editors_.size()) ? editors_[i] : nullptr;
    if (!editor) continue;

    auto reload = [this, editor, path] {
      QFile f(path);
      if (!f.open(QIODevice::ReadOnly)) return;
      QByteArray raw = f.readAll();
      QTextCodec* codec = QTextCodec::codecForName("Windows-1251");
      QString contents = codec ? codec->toUnicode(raw) : QString::fromUtf8(raw);
      // Preserve cursor position roughly.
      int pos = editor->textCursor().position();
      QTextCursor cur = editor->textCursor();
      cur.select(QTextCursor::Document);
      cur.insertText(contents);
      QTextCursor restore = editor->textCursor();
      restore.setPosition(qMin(pos, (int)contents.length()));
      editor->setTextCursor(restore);
      editor->document()->setModified(false);
      updateTitle();
    };

    const bool dirty = editor->document()->isModified();
    if (!dirty) {
      reload();
    } else {
      // Conflict: ask before discarding local edits.
      QMessageBox::StandardButton choice = QMessageBox::question(this,
          tr("File changed on disk"),
          tr("\"%1\" was modified outside Qawno but you have unsaved edits.\n\n"
             "Reload from disk (discard your edits)?").arg(QFileInfo(path).fileName()),
          QMessageBox::Yes | QMessageBox::No);
      if (choice == QMessageBox::Yes) {
        reload();
      }
    }
  }
}

bool MainWindow::isNewFile() const {
  return fileNames_.isEmpty() || fileNames_[getCurrentIndex()].isEmpty();
}

bool MainWindow::isFileModified() const {
  return !editors_.isEmpty() && editors_[getCurrentIndex()]->document()->isModified();
}

void MainWindow::setFileModified(bool isModified) {
  if (!editors_.isEmpty()) {
    editors_[getCurrentIndex()]->document()->setModified(isModified);
    updateTitle();
  }
}

bool MainWindow::isFileEmpty() const {
  if (editors_.isEmpty()) {
    return true;
  }
  QTextDocument *document = editors_[getCurrentIndex()]->document();
  return document->isEmpty()
    || (isNewFile() && !document->toPlainText().contains(QRegularExpression("\\S")));
}

void MainWindow::createTab(const QString& title, const QString& tooltip) {
  mru_.push(ui_->tabWidget->count());
  QWidget* tab = new QWidget();
  QHBoxLayout* horizontalLayout = new QHBoxLayout(tab);
  horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  EditorWidget* editor = new EditorWidget(tab);
  editor->setObjectName(QString::fromUtf8("editor"));
  QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Expanding);
  sizePolicy2.setHorizontalStretch(0);
  sizePolicy2.setVerticalStretch(0);
  sizePolicy2.setHeightForWidth(editor->sizePolicy().hasHeightForWidth());
  editor->setSizePolicy(sizePolicy2);
  editor->setAcceptDrops(false);
  horizontalLayout->addWidget(editor);
  ui_->tabWidget->addTab(tab, QString());
  int idx = ui_->tabWidget->indexOf(tab);
  ui_->tabWidget->setTabText(idx, title);
  ui_->tabWidget->setTabToolTip(idx, tooltip);
  ui_->tabWidget->setTabIcon(idx, QIcon(":/assets/images/icons/new.svg"));
  editor->toggleDarkMode(effectiveDarkMode());
  editors_.push_back(editor);
  editor->focusWidget();
  connect(editor, SIGNAL(textChanged()), SLOT(on_editor_textChanged()));
  connect(editor, SIGNAL(cursorPositionChanged()), SLOT(on_editor_cursorPositionChanged()));
  ui_->tabWidget->setCurrentIndex(ui_->tabWidget->count() - 1);
  editor->setFocus(Qt::OtherFocusReason);
  updateWelcomeVisibility();
}

void MainWindow::zoomEditor(int delta) {
  static constexpr int kDefaultFontSize = 13;
  for (auto editor : editors_) {
    QFont f = editor->font();
    int newSize = delta == 0 ? kDefaultFontSize
                             : qBound(6, f.pointSize() + delta, 72);
    f.setPointSize(newSize);
    editor->setFont(f);
  }
}

// Zoom whichever pane has focus: the code editor or the output log. Returns
// true if it handled the key (so the shortcut is swallowed). Does nothing when
// focus is elsewhere / no editor is open.
bool MainWindow::zoomFocused(int delta) {
  QWidget* fw = QApplication::focusWidget();
  // Output log focused?
  if (fw && (fw == ui_->output || ui_->output->isAncestorOf(fw))) {
    static constexpr int kOutDefault = 11;
    QFont f = ui_->output->font();
    int sz = delta == 0 ? kOutDefault : qBound(6, f.pointSize() + delta, 72);
    f.setPointSize(sz);
    ui_->output->setFont(f);
    return true;
  }
  // Code editor focused?
  EditorWidget* editor = getCurrentEditor();
  if (editor && (fw == editor || editor->isAncestorOf(fw))) {
    zoomEditor(delta);
    return true;
  }
  // Focus elsewhere: do nothing (but still swallow so it doesn't misfire).
  return false;
}

void MainWindow::buildTopControls() {
  // "TABS" section label in the tab bar's top-left corner.
  QWidget* leftCorner = new QWidget(ui_->tabWidget);
  // Min height matches the tab strip so the inner layout has room to centre
  // the icon + label vertically instead of hugging the top edge.
  leftCorner->setMinimumHeight(42);
  QHBoxLayout* leftLay = new QHBoxLayout(leftCorner);
  leftLay->setContentsMargins(10, 0, 8, 0);
  leftLay->setSpacing(6);
  leftLay->setAlignment(Qt::AlignVCenter);
  QLabel* tabsIcon = new QLabel(leftCorner);
  tabsIcon->setPixmap(QIcon(":/assets/images/icons/panel.svg").pixmap(14, 14));
  leftLay->addWidget(tabsIcon, 0, Qt::AlignVCenter);
  QLabel* tabsLabel = new QLabel(tr("TABS"), leftCorner);
  tabsLabel->setObjectName("tabsLabel");
  leftLay->addWidget(tabsLabel, 0, Qt::AlignVCenter);
  ui_->tabWidget->setCornerWidget(leftCorner, Qt::TopLeftCorner);

  // Action buttons live in a full-width strip mounted as a top QToolBar on
  // the main window. They sit above the splitter, flush against the right
  // edge of the WINDOW (not just the editor pane).
  topToolbar_ = new QWidget(this);
  topToolbar_->setObjectName("topToolbar");
  topToolbar_->setMinimumHeight(42);
  topToolbar_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  QHBoxLayout* lay = new QHBoxLayout(topToolbar_);
  lay->setContentsMargins(8, 4, 8, 4);
  lay->setSpacing(4);
  lay->setAlignment(Qt::AlignVCenter);

  // "+" new-tab button is parented to the tabBar itself and manually
  // repositioned (see repositionAddTabBtn) so it sits flush right of the
  // rightmost tab — Qt's corner widgets anchor to the far edge, which would
  // leave a gap when tabs don't fill the bar.
  addTabBtn_ = new QToolButton(ui_->tabWidget->tabBar());
  addTabBtn_->setObjectName("addTabBtn");
  addTabBtn_->setText("+");
  addTabBtn_->setCursor(Qt::PointingHandCursor);
  addTabBtn_->setToolTip(tr("New blank script"));
  connect(addTabBtn_, &QToolButton::clicked, this, [this] { on_actionNewBlank_triggered(); });
  addTabBtn_->show();
  repositionAddTabBtn();

  // Build every toolbar button through a single factory so the global
  // "show text" toggle (in Settings) can flip them all between text+icon and
  // icon-only. Each button keeps its objectName "toolBtn" for stylesheet
  // targeting (rounded border, hover state).
  auto makeButton = [this](const QString& label, const QString& iconPath,
                            const QString& tip,
                            std::function<void()> onClick,
                            QAction* act = nullptr) {
    QToolButton* b = new QToolButton(this);
    b->setObjectName("toolBtn");
    if (act) {
      act->setIcon(QIcon(iconPath));
      b->setDefaultAction(act);
    } else if (!iconPath.isEmpty()) {
      b->setIcon(QIcon(iconPath));
    }
    b->setText(label);
    b->setIconSize(QSize(16, 16));
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    if (onClick) {
      connect(b, &QToolButton::clicked, this, onClick);
    }
    toolbarButtons_.append(b);
    return b;
  };

  // Left-side menu buttons: File + Edit (replaces the native macOS menu bar
  // entries). Each one opens a popup menu mirroring the original menubar
  // structure (File: new/open/save/quit, Edit: undo/redo/cut/copy/paste/etc).
  auto makeMenuButton = [this](const QString& label, const QString& iconPath,
                                 QMenu* menu) {
    QToolButton* b = new QToolButton(this);
    // Dual objectName: still picked up by the existing `toolBtn` QSS rules,
    // PLUS a `menuBtn` marker so we can override menu-arrow visibility in
    // the toolbar stylesheet (we WANT the chevron here, unlike action btns).
    b->setObjectName("menuBtn");
    b->setProperty("class", "toolBtn menuBtn");
    b->setText(label);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setIconSize(QSize(16, 16));
    if (!iconPath.isEmpty()) b->setIcon(QIcon(iconPath));
    b->setCursor(Qt::PointingHandCursor);
    b->setPopupMode(QToolButton::MenuButtonPopup);  // shows the chevron
    b->setMenu(menu);
    toolbarButtons_.append(b);
    return b;
  };
  auto* fileBtn = makeMenuButton(tr("File"), ":/assets/images/icons/file-menu.svg",
                                  ui_->menuFile);
  auto* editBtn = makeMenuButton(tr("Edit"), ":/assets/images/icons/edit.svg",
                                  ui_->menuEdit);
  // File + Edit are the user's main menu access; collapsing them to icons is
  // confusing (no obvious "menu" affordance), so opt them out of the responsive
  // narrow-window collapse handled by applyToolbarStyle().
  fileBtn->setProperty("keepLabel", true);
  editBtn->setProperty("keepLabel", true);
  lay->addWidget(fileBtn);
  lay->addWidget(editBtn);

  auto actionButton = [this, &makeButton](QAction* act, const QString& label,
                             const QString& iconPath) {
    QToolButton* b = makeButton(label, iconPath, label, nullptr, act);
    return b;
  };

  // Hide the native menubar entries we just promoted into the toolbar. The
  // app menu (Qawno / Help) stays as macOS requires it.
  menuBar()->removeAction(ui_->menuFile->menuAction());
  menuBar()->removeAction(ui_->menuEdit->menuAction());
  menuBar()->removeAction(ui_->menuBuild->menuAction());
  menuBar()->removeAction(ui_->menuSettings->menuAction());

  // Stretch BEFORE the right-side controls so File/Edit hug the left edge.
  lay->addStretch(1);

  // Compile (right side).
  lay->addWidget(actionButton(ui_->actionCompile, tr("Compile"),
                              ":/assets/images/icons/run.svg"));

  // Search button — toggles the inline find bar. Sits to the right of
  // Compile per the new layout request.
  lay->addWidget(makeButton(tr("Search"), ":/assets/images/icons/search.svg",
                             tr("Find (inline bar)"),
                             [this] { toggleInlineFindBar(); }));

  // Appearance toggle: single click cycles System → Light → Dark. The button
  // shows the current mode's icon + name (no dropdown anymore).
  themeButton_ = new QToolButton(this);
  themeButton_->setObjectName("toolBtn");
  themeButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  themeButton_->setIconSize(QSize(16, 16));
  themeButton_->setCursor(Qt::PointingHandCursor);
  themeButton_->setToolTip(tr("Click to cycle System / Light / Dark"));
  toolbarButtons_.append(themeButton_);
  // Keep these QActions around so menuTheme still works (System/Light/Dark
  // entries in the menu bar).
  themeSystemAct_ = new QAction(QIcon(":/assets/images/icons/theme-system.svg"), tr("System"), this);
  themeLightAct_  = new QAction(QIcon(":/assets/images/icons/theme-light.svg"),  tr("Light"),  this);
  themeDarkAct_   = new QAction(QIcon(":/assets/images/icons/theme-dark.svg"),   tr("Dark"),   this);
  for (QAction* a : { themeSystemAct_, themeLightAct_, themeDarkAct_ }) {
    a->setCheckable(true);
  }
  connect(themeSystemAct_, &QAction::triggered, this, [this] { setThemeMode(0); });
  connect(themeLightAct_,  &QAction::triggered, this, [this] { setThemeMode(1); });
  connect(themeDarkAct_,   &QAction::triggered, this, [this] { setThemeMode(2); });
  connect(themeButton_, &QToolButton::clicked, this, [this] {
    setThemeMode((themeMode_ + 1) % 3);
  });
  updateThemeButton();
  lay->addWidget(themeButton_);

  // Mirror the System/Light/Dark choice in the menu bar.
  ui_->menuTheme->addAction(themeSystemAct_);
  ui_->menuTheme->addAction(themeLightAct_);
  ui_->menuTheme->addAction(themeDarkAct_);

  // Settings button — opens the unified Settings window.
  lay->addWidget(makeButton(tr("Settings"), ":/assets/images/icons/settings.svg",
                             tr("Settings"),
                             [this] { on_actionSettings_triggered(); }));

  // Sidebar + Output panel toggles. Icon-only (no text) regardless of the
  // global show-text setting — they're spatial controls, the icon is enough.
  QToolButton* sidebarBtn = new QToolButton(this);
  sidebarBtn->setObjectName("toolBtn");
  sidebarBtn->setIcon(QIcon(":/assets/images/icons/sidebar.svg"));
  sidebarBtn->setIconSize(QSize(16, 16));
  sidebarBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
  sidebarBtn->setCheckable(true);
  sidebarBtn->setChecked(true);
  sidebarBtn->setCursor(Qt::PointingHandCursor);
  sidebarBtn->setToolTip(tr("Toggle sidebar"));
  connect(sidebarBtn, &QToolButton::toggled, this, [this](bool on) {
    if (sidebarPane_) {
      sidebarPane_->setVisible(on);
    }
  });
  lay->addWidget(sidebarBtn);

  QToolButton* panelBtn = new QToolButton(this);
  panelBtn->setObjectName("toolBtn");
  panelBtn->setIcon(QIcon(":/assets/images/icons/panel.svg"));
  panelBtn->setIconSize(QSize(16, 16));
  panelBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
  panelBtn->setCheckable(true);
  panelBtn->setChecked(true);
  panelBtn->setCursor(Qt::PointingHandCursor);
  panelBtn->setToolTip(tr("Toggle output panel"));
  connect(panelBtn, &QToolButton::toggled, this, [this](bool on) {
    ui_->output->setVisible(on);
  });
  lay->addWidget(panelBtn);

  QToolBar* mainBar = addToolBar(tr("Main"));
  mainBar->setObjectName("mainToolBar");
  mainBar->setMovable(false);
  mainBar->setFloatable(false);
  mainBar->setStyleSheet("QToolBar#mainToolBar { border: none; padding: 0; spacing: 0; }");
  mainBar->addWidget(topToolbar_);

  // Reposition "+" whenever tabs change.
  connect(ui_->tabWidget, &QTabWidget::currentChanged, this, [this](int){ repositionAddTabBtn(); });
  connect(ui_->tabWidget->tabBar(), &QTabBar::tabMoved, this, [this](int,int){ repositionAddTabBtn(); });

  applyToolbarStyle();
}

void MainWindow::repositionAddTabBtn() {
  if (!addTabBtn_) return;
  QTabBar* tb = ui_->tabWidget->tabBar();
  const int count = tb->count();
  const int btnW = 26, btnH = 22;
  int x = 4;
  int y = qMax(0, (tb->height() - btnH) / 2);
  if (count > 0) {
    QRect last = tb->tabRect(count - 1);
    x = last.right() + 6;
    y = last.top() + qMax(0, (last.height() - btnH) / 2);
  }
  addTabBtn_->setGeometry(x, y, btnW, btnH);
  addTabBtn_->raise();
}

void MainWindow::applyToolbarStyle() {
  // Setting: "ShowToolbarText" (default true). The toolbar also collapses to
  // icons when the window is too narrow to fit all labels — even with the
  // user preference on. ~1100px is roughly the width at which the labels
  // start crowding the right corner / overflowing.
  const bool prefersText = QSettings().value("ShowToolbarText", true).toBool();
  const bool fitsText = width() >= 1100;
  const auto style = (prefersText && fitsText) ? Qt::ToolButtonTextBesideIcon
                                                : Qt::ToolButtonIconOnly;
  for (QToolButton* b : toolbarButtons_) {
    if (!b) continue;
    if (b->property("keepLabel").toBool()) {
      b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    } else {
      b->setToolButtonStyle(style);
    }
  }
}

bool MainWindow::effectiveDarkMode() const {
  if (themeMode_ == 1) {
    return false;
  }
  if (themeMode_ == 2) {
    return true;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
  return kDefaultDarkMode;
#endif
}

void MainWindow::setThemeMode(int mode) {
  themeMode_ = mode;
  QSettings settings;
  settings.setValue("ThemeMode", mode);
  applyTheme();
}

void MainWindow::applyThemedActionIcons(bool dark) {
  // Centralised re-tint for every QAction icon shipped with the app — runs
  // on every theme switch so dark icons swap to light tints (and vice versa)
  // without leaving stale colours behind.
  struct Entry { QAction* action; const char* path; int size; };
  const Entry entries[] = {
    { ui_->actionNewGM,      ":/assets/images/icons/new.svg",                 16 },
    { ui_->actionNewFS,      ":/assets/images/icons/new.svg",                 16 },
    { ui_->actionNewInc,     ":/assets/images/icons/new.svg",                 16 },
    { ui_->actionNewBlank,   ":/assets/images/icons/new.svg",                 16 },
    { ui_->actionOpen,       ":/assets/images/icons/open.svg",                16 },
    { ui_->actionSave,       ":/assets/images/icons/save.svg",                16 },
    { ui_->actionSaveAs,     ":/assets/images/icons/save.svg",                16 },
    { ui_->actionSaveAll,    ":/assets/images/icons/save.svg",                16 },
    { ui_->actionCompile,    ":/assets/images/icons/run.svg",                 16 },
    { ui_->actionCompileRun, ":/assets/images/icons/run.svg",                 16 },
    { ui_->actionRun,        ":/assets/images/icons/run.svg",                 16 },
    { ui_->actionClose,      ":/assets/images/icons/close.svg",               16 },
    { ui_->actionQuit,       ":/assets/images/icons/quit.svg",                16 },
    { ui_->actionUndo,       ":/assets/images/icons/undo.svg",                16 },
    { ui_->actionRedo,       ":/assets/images/icons/redo.svg",                16 },
    { ui_->actionCut,        ":/assets/images/icons/cut.svg",                 16 },
    { ui_->actionCopy,       ":/assets/images/icons/copy.svg",                16 },
    { ui_->actionPaste,      ":/assets/images/icons/paste.svg",               16 },
    { ui_->actionFind,       ":/assets/images/icons/find.svg",                16 },
    { ui_->actionFindNext,   ":/assets/images/icons/find-next.svg",           16 },
    { ui_->actionGoToLine,   ":/assets/images/icons/goto-line.svg",           16 },
    { ui_->actionDupsel,     ":/assets/images/icons/duplicate-selection.svg", 16 },
    { ui_->actionDupline,    ":/assets/images/icons/duplicate-line.svg",      16 },
    { ui_->actionDelline,    ":/assets/images/icons/delete-line.svg",         16 },
    { ui_->actionComment,    ":/assets/images/icons/comment.svg",             16 },
    { ui_->actionColours,    ":/assets/images/icons/color-picker.svg",        16 },
  };
  for (const Entry& e : entries) {
    if (e.action) e.action->setIcon(IconLoader::themed(e.path, dark, QSize(e.size, e.size)));
  }
  // Themed icons on the File/Edit menu buttons (objectName "menuBtn").
  const QList<QPair<const char*, const char*>> menuBtnIcons = {
    {"File", ":/assets/images/icons/file-menu.svg"},
    {"Edit", ":/assets/images/icons/edit.svg"},
  };
  for (QToolButton* b : toolbarButtons_) {
    if (!b || b->objectName() != "menuBtn") continue;
    for (const auto& mi : menuBtnIcons) {
      if (b->text() == QString::fromLatin1(mi.first)) {
        b->setIcon(IconLoader::themed(mi.second, dark, QSize(16, 16)));
        break;
      }
    }
  }
}

void MainWindow::applyTheme() {
  bool dark = effectiveDarkMode();
  applyThemedActionIcons(dark);
  ui_->outerWidget->setPalette(dark ? darkModePalette : defaultPalette);
  for (auto i : editors_) {
    i->toggleDarkMode(dark);
  }
  ui_->actionDarkMode->setChecked(dark);
  updateThemeButton();
  if (findDialog_) {
    findDialog_->applyTheme(dark);
  }
  if (settingsDialog_) {
    settingsDialog_->applyTheme(dark);
  }

  // Window-level theming for the cornerBtn text/icons and sidebar header.
  const char* barText  = dark ? "#C8CDD6" : "#1A1E25";
  const char* sepCol   = dark ? "#3A3F4B" : "#A8B0BC";
  const char* btnBorder= dark ? "rgba(160,170,190,0.45)" : "rgba(60,68,80,0.55)";
  const char* btnHover = dark ? "rgba(127,140,160,0.18)" : "rgba(0,0,0,0.10)";
  const char* winBg    = dark ? "#21262D" : "#D8DEE5";
  setStyleSheet(QString(R"(
    QMainWindow { background: %5; }
    QWidget#topToolbar { background: %5; }
    QToolButton#toolBtn {
      border: 1px solid %3; border-radius: 6px; padding: 4px 10px; font-size: 12px; color: %1;
      min-height: 22px;
    }
    QToolButton#toolBtn:hover   { background: %4; border: 1px solid %3; }
    QToolButton#toolBtn:pressed { background: %4; border: 1px solid %3; }
    QToolButton#toolBtn:checked { background: %4; border: 1px solid %3; }
    QToolButton#toolBtn::menu-button { border: none; width: 0; }
    QToolButton#toolBtn::menu-arrow { image: none; width: 0; height: 0; }
    QToolButton#toolBtn::menu-indicator { image: none; width: 0; height: 0; }
    /* File / Edit drop-down buttons keep the same chrome but show a chevron
       so users know they open a menu. */
    QToolButton#menuBtn {
      border: 1px solid %3; border-radius: 6px; padding: 4px 8px 4px 10px;
      font-size: 12px; color: %1; min-height: 22px;
    }
    QToolButton#menuBtn:hover, QToolButton#menuBtn:pressed { background: %4; border: 1px solid %3; }
    QToolButton#menuBtn::menu-button { border: none; width: 12px; }
    QToolButton#menuBtn::menu-arrow {
      image: url(:/assets/images/icons/chevron-down.svg);
      width: 10px; height: 10px; margin-right: 4px;
    }
    QToolButton#menuBtn::menu-indicator { image: none; width: 0; height: 0; }
    QFrame#cornerSep { color: %2; max-width: 1px; margin: 6px 3px; }
    QLabel#tabsLabel { color: %1; font-weight: bold; font-size: 11px; letter-spacing: 1px; padding: 0; }
    QTabBar QToolButton { border: none; background: transparent; }
    QTabBar::scroller { width: 28px; }
    QTabBar::close-button {
      subcontrol-position: right; subcontrol-origin: padding;
      margin: 0 4px 0 10px;
      image: url(:/assets/images/icons/tab-close.svg);
      width: 18px; height: 18px;
      padding: 2px;
    }
    QTabBar::close-button:hover {
      image: url(:/assets/images/icons/tab-close-hover.svg);
      background: rgba(127,140,160,0.25); border-radius: 4px;
    }
    QWidget#sidebarBox { border-radius: 0; }
    QListWidget#functions { border: none; padding: 4px; }
    QLabel#sidebarHeader {
      font-weight: bold; letter-spacing: 1px; font-size: 11px; padding: 4px;
    }
  )").arg(barText, sepCol, btnBorder, btnHover, winBg));

  // VS Code-style tab strip: tabs sit flush against the editor pane (square
  // bottom, no gap), the active tab's background matches the pane so they
  // visually merge. Text is vertically centred.
  if (dark) {
    ui_->outerWidget->setStyleSheet(R"(
      QToolButton#cornerBtn { color: #C8CDD6; }
      QTabWidget::pane { border: 1px solid #3A3F4B; border-top: none; background: #1B1F25; top: 0; }
      QMainWindow { background: #21262D; }
      QWidget#outerWidget { background: #21262D; }
      QTabBar { qproperty-drawBase: 0; background: #21262D; }
      QTabBar::tab {
        background: #21262D; color: #8B93A2;
        padding: 6px 10px 6px 14px; margin: 0; min-width: 80px; min-height: 24px;
        border: none; border-right: 1px solid #3A3F4B;
      }
      QTabBar::tab:selected { background: #1B1F25; color: #FFFFFF; border-bottom: 2px solid #5B9BD5; }
      QTabBar::tab:hover:!selected { background: #2D333D; }
      QTabBar::close-button {
        subcontrol-position: right; subcontrol-origin: padding;
        margin: 0 4px 0 10px;
        image: url(:/assets/images/icons/tab-close.svg);
        width: 18px; height: 18px;
        padding: 2px;
      }
      QTabBar::close-button:hover {
        image: url(:/assets/images/icons/tab-close-hover.svg);
        background: rgba(127,140,160,0.25); border-radius: 4px;
      }
      QWidget#topToolbar { background: #21262D; }
      QToolButton#addTabBtn {
        color: #C8CDD6; background: transparent;
        border: 1px solid rgba(160,170,190,0.45); border-radius: 6px;
        padding: 0; font-size: 16px; font-weight: bold;
      }
      QToolButton#addTabBtn:hover { background: rgba(127,140,160,0.22); }
      QWidget#sidebarBox {
        border: 1px solid #3A3F4B; background: #1B1F25;
        border-top-left-radius: 8px; border-top-right-radius: 8px;
      }
      QListWidget#functions {
        border: none; border-top: 1px solid #3A3F4B; background: #1B1F25; color: #C8CDD6;
        border-bottom-left-radius: 8px; border-bottom-right-radius: 8px;
      }
      QLabel#sidebarHeader {
        color: #9DA5B4; background: #2D333D; border-bottom: 1px solid #3A3F4B;
        border-top-left-radius: 8px; border-top-right-radius: 8px;
      }
      QWidget#sidebarTopSpacer { background: #21262D; border-bottom: 1px solid #3A3F4B; }
      OutputWidget#output {
        border: 1px solid #3A3F4B; background: #1B1F25; color: #C8CDD6;
        border-top-left-radius: 8px; border-top-right-radius: 8px;
      }
      QWidget#welcomePage { background: #282C34; }
      QWidget#inlineFindBar { background: #21262D; border-bottom: 1px solid #3A3F4B; }
      QWidget#inlineFindBar QLineEdit { background: #1B1F25; border: 1px solid #3A3F4B; border-radius: 4px; padding: 4px 8px; color: #C8CDD6; font-size: 13px; min-height: 22px; }
      QWidget#inlineFindBar QToolButton#inlineFindBtn,
      QWidget#inlineFindBar QToolButton#inlineFindOpt {
        color: #C8CDD6; background: #2D333D; border: 1px solid #3A3F4B; border-radius: 5px;
        padding: 4px 8px; min-width: 28px; min-height: 22px; font-size: 13px;
      }
      QWidget#inlineFindBar QToolButton#inlineFindBtn:hover,
      QWidget#inlineFindBar QToolButton#inlineFindOpt:hover { background: rgba(127,140,160,0.22); }
      QWidget#inlineFindBar QToolButton#inlineFindOpt:checked { background: #324054; border-color: #5B9BD5; color: #FFFFFF; }
      QWidget#inlineFindBar QToolButton#inlineFindExpand {
        color: #C8CDD6; background: #2D333D; border: 1px solid #3A3F4B;
        border-radius: 6px; font-size: 18px; font-weight: bold; padding: 0;
      }
      QWidget#inlineFindBar QToolButton#inlineFindExpand:hover { background: rgba(127,140,160,0.22); }
      QLabel#inlineFindStatus { color: #9AA0AB; font-size: 12px; padding: 0 8px; }
      QPushButton#recentItem, QLabel#recentItem { border: none; text-align: left; color: #C8CDD6; font-size: 14px; padding: 2px 0; }
      QPushButton#recentItem:hover, QLabel#recentItem:hover { color: #5B9BD5; }
      QPushButton#welcomeTile { text-align: left; padding: 8px 12px; border: 1px solid #3A3F4B; border-radius: 10px; font-size: 14px; color: #C8CDD6; }
      QPushButton#welcomeTile:hover { background: rgba(127,140,160,0.14); }
      QPushButton#welcomeLink { border: none; color: #5B9BD5; text-align: left; font-size: 14px; }
      QPushButton#welcomeLink:hover { text-decoration: underline; }
      QLabel#welcomeName { color: #FFFFFF; }
      QLabel#recentHeader { color: #9DA5B4; }
      QLabel#startHeader  { color: #9DA5B4; }
      QFrame#toolingCard { border: 1px solid #3A3F4B; border-radius: 8px; background: #21262D; }
      QLabel#toolingTitle { color: #FFFFFF; font-size: 16px; font-weight: 600; }
      QLabel#toolingDetail { color: #9AA1AD; font-size: 12px; }
    )");
  } else {
    ui_->outerWidget->setStyleSheet(R"(
      QToolButton#cornerBtn { color: #3A4252; }
      QTabWidget::pane { border: 1px solid #C8CDD6; border-top: none; background: #FBFCFD; top: 0; }
      QMainWindow { background: #D8DEE5; }
      QWidget#outerWidget { background: #D8DEE5; }
      QTabBar { qproperty-drawBase: 0; background: #D8DEE5; }
      QTabBar::tab {
        background: #D8DEE5; color: #2A3340;
        padding: 6px 10px 6px 14px; margin: 0; min-width: 80px; min-height: 24px;
        border: none; border-right: 1px solid #B6BDC7;
      }
      QTabBar::tab:selected { background: #FBFCFD; color: #1A1E25; border-bottom: 2px solid #1E6BB8; }
      QTabBar::tab:hover:!selected { background: #C5CCD6; color: #1A1E25; }
      QTabBar::close-button {
        subcontrol-position: right; subcontrol-origin: padding;
        margin: 0 4px 0 10px;
        image: url(:/assets/images/icons/tab-close.svg);
        width: 18px; height: 18px;
        padding: 2px;
      }
      QTabBar::close-button:hover {
        image: url(:/assets/images/icons/tab-close-hover.svg);
        background: rgba(0,0,0,0.10); border-radius: 4px;
      }
      QWidget#topToolbar { background: #D8DEE5; }
      QToolButton#addTabBtn {
        color: #1A1E25; background: transparent;
        border: 1px solid rgba(60,68,80,0.55); border-radius: 6px;
        padding: 0; font-size: 16px; font-weight: bold;
      }
      QToolButton#addTabBtn:hover { background: rgba(0,0,0,0.10); }
      QWidget#sidebarBox {
        border: 1px solid #C8CDD6; background: #FBFCFD;
        border-top-left-radius: 8px; border-top-right-radius: 8px;
      }
      QListWidget#functions {
        border: none; border-top: 1px solid #C8CDD6; background: #FBFCFD; color: #3A4252;
        border-bottom-left-radius: 8px; border-bottom-right-radius: 8px;
      }
      QLabel#sidebarHeader {
        color: #2A3340; background: #E0E4EA; border-bottom: 1px solid #C8CDD6;
        border-top-left-radius: 8px; border-top-right-radius: 8px;
      }
      QWidget#sidebarTopSpacer { background: #D8DEE5; border-bottom: 1px solid #B6BDC7; }
      OutputWidget#output {
        border: 1px solid #C8CDD6; background: #FBFCFD; color: #1A1E25;
        border-top-left-radius: 8px; border-top-right-radius: 8px;
      }
      QWidget#welcomePage { background: #F4F6F8; }
      QWidget#inlineFindBar { background: #E8ECF0; border-bottom: 1px solid #C8CDD6; }
      QWidget#inlineFindBar QLineEdit { background: #FFFFFF; border: 1px solid #C8CDD6; border-radius: 4px; padding: 4px 8px; color: #1A1E25; font-size: 13px; min-height: 22px; }
      QWidget#inlineFindBar QToolButton#inlineFindBtn,
      QWidget#inlineFindBar QToolButton#inlineFindOpt {
        color: #3A4252; background: #FFFFFF; border: 1px solid #C8CDD6; border-radius: 5px;
        padding: 4px 8px; min-width: 28px; min-height: 22px; font-size: 13px;
      }
      QWidget#inlineFindBar QToolButton#inlineFindBtn:hover,
      QWidget#inlineFindBar QToolButton#inlineFindOpt:hover { background: rgba(0,0,0,0.05); }
      QWidget#inlineFindBar QToolButton#inlineFindOpt:checked { background: #CCE0F5; border-color: #1E6BB8; color: #1A1E25; }
      QWidget#inlineFindBar QToolButton#inlineFindExpand {
        color: #1A1E25; background: #FFFFFF; border: 1px solid #C8CDD6;
        border-radius: 6px; font-size: 18px; font-weight: bold; padding: 0;
      }
      QWidget#inlineFindBar QToolButton#inlineFindExpand:hover { background: rgba(0,0,0,0.06); }
      QLabel#inlineFindStatus { color: #5A6270; font-size: 12px; padding: 0 8px; }
      QPushButton#recentItem, QLabel#recentItem { border: none; text-align: left; color: #1A1E25; font-size: 14px; padding: 2px 0; }
      QPushButton#recentItem:hover, QLabel#recentItem:hover { color: #1E6BB8; }
      QPushButton#welcomeTile { text-align: left; padding: 8px 12px; border: 1px solid #C8CDD6; border-radius: 10px; font-size: 14px; color: #3A4252; }
      QPushButton#welcomeTile:hover { background: rgba(0,0,0,0.05); }
      QPushButton#welcomeLink { border: none; color: #1E6BB8; text-align: left; font-size: 14px; }
      QPushButton#welcomeLink:hover { text-decoration: underline; }
      QLabel#welcomeName { color: #1A1E25; }
      QLabel#recentHeader { color: #1A1E25; font-weight: 600; }
      QLabel#startHeader  { color: #1A1E25; font-weight: 600; }
      QFrame#toolingCard { border: 1px solid #C8CDD6; border-radius: 8px; background: #FFFFFF; }
      QLabel#toolingTitle { color: #1A1E25; font-size: 16px; font-weight: 600; }
      QLabel#toolingDetail { color: #5A6270; font-size: 12px; }
    )");
  }
  repositionAddTabBtn();
}

void MainWindow::clearDiagnostics() {
  // Reset every editor's extraSelections — wipes any wavy-underline markers
  // left over from a previous compile.
  for (EditorWidget* e : editors_) {
    if (e) e->setExtraSelections({});
  }
}

void MainWindow::applyDiagnostic(const QString& filePath, int line, bool isError) {
  // Find the open tab matching this path (compare by basename to be tolerant
  // of the path-shortening rewrites).
  EditorWidget* target = nullptr;
  const QString base = QFileInfo(filePath).fileName();
  for (int i = 0; i < fileNames_.size(); ++i) {
    if (fileNames_[i].isEmpty()) continue;
    if (fileNames_[i] == filePath ||
        QFileInfo(fileNames_[i]).fileName() == base) {
      target = (i < editors_.size()) ? editors_[i] : nullptr;
      if (target) break;
    }
  }
  if (!target || line <= 0) return;
  QTextBlock block = target->document()->findBlockByLineNumber(line - 1);
  if (!block.isValid()) return;
  QTextEdit::ExtraSelection sel;
  sel.cursor = QTextCursor(block);
  sel.cursor.select(QTextCursor::LineUnderCursor);
  QTextCharFormat fmt;
  fmt.setUnderlineStyle(QTextCharFormat::WaveUnderline);
  fmt.setUnderlineColor(isError ? QColor(0xE5, 0x48, 0x4A) : QColor(0xE0, 0xA8, 0x3C));
  sel.format = fmt;
  auto existing = target->extraSelections();
  existing.append(sel);
  target->setExtraSelections(existing);
}

QString MainWindow::stripUserPrefix(const QString& path) const {
  // macOS / Linux native paths.
  const QString home = QDir::homePath();
  if (!home.isEmpty() && path.startsWith(home)) {
    return path.mid(home.length());
  }
  return path;
}

QString MainWindow::shortenPath(const QString& path) const {
  if (SettingsDialog::fullPathInOutput()) {
    return stripUserPrefix(path);
  }
  // Just the filename — "Z:\…\zombie.pwn" → "zombie.pwn".
  QString p = path;
  p.replace('\\', '/');
  int slash = p.lastIndexOf('/');
  return slash < 0 ? path : p.mid(slash + 1);
}

void MainWindow::updateThemeButton() {
  if (!themeButton_) return;
  const char* icon = themeMode_ == 1 ? ":/assets/images/icons/theme-light.svg"
                   : themeMode_ == 2 ? ":/assets/images/icons/theme-dark.svg"
                                     : ":/assets/images/icons/theme-system.svg";
  const QString name = themeMode_ == 1 ? tr("Light")
                     : themeMode_ == 2 ? tr("Dark")
                                       : tr("System");
  themeButton_->setIcon(QIcon(icon));
  themeButton_->setText(name);
  if (themeSystemAct_) themeSystemAct_->setChecked(themeMode_ == 0);
  if (themeLightAct_)  themeLightAct_->setChecked(themeMode_ == 1);
  if (themeDarkAct_)   themeDarkAct_->setChecked(themeMode_ == 2);
}

void MainWindow::on_actionSettings_triggered() {
  // Lazy-create a persistent SettingsDialog (frameless, in-window). Same
  // pattern as FindDialog — single instance lives across open/close cycles.
  if (!settingsDialog_) {
    settingsDialog_ = new SettingsDialog(this);
    connect(settingsDialog_, &SettingsDialog::applied, this, [this] {
      setThemeMode(SettingsDialog::uiThemeMode());
      QFont ef = settingsDialog_->editorFont();
      for (auto e : editors_) {
        e->setFont(ef);
      }
      ui_->output->setFont(settingsDialog_->outputFont());
      applyToolbarStyle();
      settingsDialog_->applyTheme(effectiveDarkMode());
    });
    connect(settingsDialog_, &SettingsDialog::updateRequested, this, &MainWindow::promptForUpdate);
  }
  if (auto e = getCurrentEditor()) {
    settingsDialog_->setEditorFont(e->font());
  } else {
    settingsDialog_->setEditorFont(font());
  }
  settingsDialog_->setOutputFont(ui_->output->font());
  settingsDialog_->applyTheme(effectiveDarkMode());
  settingsDialog_->show();
  settingsDialog_->raise();
  settingsDialog_->activateWindow();
}

void MainWindow::ensureInlineFindBar() {
  if (inlineFindBar_) return;
  // VS Code-style two-row find/replace bar that pushes the editor content
  // down (no overlay) — lives in the central widget's verticalLayout above
  // the splitter so it always reserves its own vertical space.
  inlineFindBar_ = new QWidget(this);
  inlineFindBar_->setObjectName("inlineFindBar");
  inlineFindBar_->setAutoFillBackground(true);
  // Horizontal split: expand chevron column + stacked rows column. Keeping
  // the chevron in its own column lets it sit centered between Find and
  // Replace rows when expanded, with a proper button boundary.
  auto outer = new QHBoxLayout(inlineFindBar_);
  outer->setContentsMargins(8, 4, 6, 4);
  outer->setSpacing(6);

  // Expand-replace chevron (▸ collapsed, ▾ expanded). Bordered button,
  // vertically centered against the whole bar so it tracks both rows.
  auto expandBtn = new QToolButton;
  expandBtn->setObjectName("inlineFindExpand");
  expandBtn->setText(QStringLiteral("▸"));
  expandBtn->setCheckable(true);
  expandBtn->setToolTip(tr("Toggle replace"));
  expandBtn->setCursor(Qt::PointingHandCursor);
  expandBtn->setFixedSize(28, 28);
  outer->addWidget(expandBtn, 0, Qt::AlignVCenter);

  auto v = new QVBoxLayout;
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(4);
  outer->addLayout(v, 1);

  // ---- Find row ----
  auto findRow = new QHBoxLayout;
  findRow->setSpacing(4);

  inlineFindEdit_ = new QLineEdit;
  inlineFindEdit_->setPlaceholderText(tr("Find"));
  inlineFindEdit_->setClearButtonEnabled(true);
  inlineFindEdit_->setMinimumWidth(280);
  findRow->addWidget(inlineFindEdit_, 1);

  inlineMatchCase_ = new QToolButton;
  inlineMatchCase_->setText(QStringLiteral("Aa"));
  inlineMatchCase_->setCheckable(true);
  inlineMatchCase_->setToolTip(tr("Match case"));
  inlineWholeWord_ = new QToolButton;
  inlineWholeWord_->setText(QStringLiteral("ab"));
  inlineWholeWord_->setCheckable(true);
  inlineWholeWord_->setToolTip(tr("Match whole word"));
  inlineRegex_ = new QToolButton;
  inlineRegex_->setText(QStringLiteral(".*"));
  inlineRegex_->setCheckable(true);
  inlineRegex_->setToolTip(tr("Regular expression"));
  for (auto b : { inlineMatchCase_, inlineWholeWord_, inlineRegex_ }) {
    b->setObjectName("inlineFindOpt");
    b->setCursor(Qt::PointingHandCursor);
    findRow->addWidget(b);
  }

  auto prev = new QToolButton; prev->setText(QStringLiteral("↑")); prev->setToolTip(tr("Previous"));
  auto next = new QToolButton; next->setText(QStringLiteral("↓")); next->setToolTip(tr("Next"));
  auto close = new QToolButton; close->setText(QStringLiteral("×")); close->setToolTip(tr("Close"));
  for (auto b : { prev, next, close }) {
    b->setObjectName("inlineFindBtn");
    b->setCursor(Qt::PointingHandCursor);
  }
  inlineFindStatus_ = new QLabel(tr("No results"));
  inlineFindStatus_->setObjectName("inlineFindStatus");
  findRow->addWidget(inlineFindStatus_);
  findRow->addWidget(prev);
  findRow->addWidget(next);
  findRow->addWidget(close);
  v->addLayout(findRow);

  // ---- Replace row (hidden until expanded) ----
  inlineReplaceRow_ = new QWidget;
  auto replaceLay = new QHBoxLayout(inlineReplaceRow_);
  replaceLay->setContentsMargins(0, 0, 0, 0);
  replaceLay->setSpacing(4);
  inlineReplaceEdit_ = new QLineEdit;
  inlineReplaceEdit_->setPlaceholderText(tr("Replace"));
  inlineReplaceEdit_->setClearButtonEnabled(true);
  inlineReplaceEdit_->setMinimumWidth(280);
  replaceLay->addWidget(inlineReplaceEdit_, 1);
  auto repOne = new QToolButton; repOne->setText(tr("Replace"));
  auto repAll = new QToolButton; repAll->setText(tr("All"));
  for (auto b : { repOne, repAll }) {
    b->setObjectName("inlineFindBtn");
    b->setCursor(Qt::PointingHandCursor);
    replaceLay->addWidget(b);
  }
  inlineReplaceRow_->hide();
  v->addWidget(inlineReplaceRow_);

  connect(expandBtn, &QToolButton::toggled, this, [this, expandBtn](bool on) {
    inlineReplaceRow_->setVisible(on);
    expandBtn->setText(on ? QStringLiteral("▾") : QStringLiteral("▸"));
  });

  connect(inlineFindEdit_, &QLineEdit::returnPressed, this, [this] { runInlineFind(false); });
  connect(next, &QToolButton::clicked, this, [this] { runInlineFind(false); });
  connect(prev, &QToolButton::clicked, this, [this] { runInlineFind(true); });
  connect(close, &QToolButton::clicked, this, [this] { inlineFindBar_->hide(); });
  connect(repOne, &QToolButton::clicked, this, [this] { runInlineReplaceOne(); });
  connect(repAll, &QToolButton::clicked, this, [this] { runInlineReplaceAll(); });

  // Insert at the top of the central widget's vertical stack so the bar
  // reserves layout space (editor content shifts down — no overlay).
  ui_->verticalLayout->insertWidget(0, inlineFindBar_);
  inlineFindBar_->hide();
}

void MainWindow::toggleInlineFindBar() {
  ensureInlineFindBar();
  if (inlineFindBar_->isVisible()) {
    inlineFindBar_->hide();
    if (auto e = getCurrentEditor()) e->setFocus();
  } else {
    inlineFindBar_->show();
    inlineFindEdit_->setFocus();
    inlineFindEdit_->selectAll();
    if (inlineFindStatus_) inlineFindStatus_->clear();
  }
}

void MainWindow::runInlineFind(bool backward) {
  auto e = getCurrentEditor();
  if (!e || !inlineFindEdit_) return;
  const QString needle = inlineFindEdit_->text();
  if (needle.isEmpty()) return;
  QTextDocument::FindFlags flags;
  if (backward) flags |= QTextDocument::FindBackward;
  if (inlineMatchCase_->isChecked()) flags |= QTextDocument::FindCaseSensitively;
  if (inlineWholeWord_->isChecked()) flags |= QTextDocument::FindWholeWords;

  auto findOne = [this, e, needle, &flags](QTextCursor from) {
    if (inlineRegex_->isChecked()) {
      auto sens = inlineMatchCase_->isChecked()
                      ? QRegularExpression::NoPatternOption
                      : QRegularExpression::CaseInsensitiveOption;
      return e->document()->find(QRegularExpression(needle, sens), from, flags);
    }
    return e->document()->find(needle, from, flags);
  };

  QTextCursor cur = e->textCursor();
  QTextCursor hit = findOne(cur);
  if (hit.isNull()) {
    QTextCursor wrap(e->document());
    if (backward) wrap.movePosition(QTextCursor::End);
    hit = findOne(wrap);
  }
  if (hit.isNull()) {
    inlineFindStatus_->setText(tr("No results"));
  } else {
    e->setTextCursor(hit);
    inlineFindStatus_->setText(QString());
  }
}

void MainWindow::runInlineReplaceOne() {
  auto e = getCurrentEditor();
  if (!e || !inlineFindEdit_ || !inlineReplaceEdit_) return;
  QTextCursor cur = e->textCursor();
  const QString needle = inlineFindEdit_->text();
  // Replace current selection if it matches the search term, then advance.
  if (cur.hasSelection() && cur.selectedText() == needle) {
    cur.insertText(inlineReplaceEdit_->text());
  }
  runInlineFind(false);
}

void MainWindow::runInlineReplaceAll() {
  auto e = getCurrentEditor();
  if (!e || !inlineFindEdit_ || !inlineReplaceEdit_) return;
  const QString needle = inlineFindEdit_->text();
  const QString replacement = inlineReplaceEdit_->text();
  if (needle.isEmpty()) return;
  QTextDocument::FindFlags flags;
  if (inlineMatchCase_->isChecked()) flags |= QTextDocument::FindCaseSensitively;
  if (inlineWholeWord_->isChecked()) flags |= QTextDocument::FindWholeWords;

  QTextCursor sweep(e->document());
  sweep.movePosition(QTextCursor::Start);
  int count = 0;
  bool inBlock = false;
  while (true) {
    QTextCursor hit;
    if (inlineRegex_->isChecked()) {
      auto sens = inlineMatchCase_->isChecked()
                      ? QRegularExpression::NoPatternOption
                      : QRegularExpression::CaseInsensitiveOption;
      hit = e->document()->find(QRegularExpression(needle, sens), sweep, flags);
    } else {
      hit = e->document()->find(needle, sweep, flags);
    }
    if (hit.isNull()) break;
    if (!inBlock) { hit.beginEditBlock(); inBlock = true; }
    else { hit.joinPreviousEditBlock(); }
    hit.insertText(replacement);
    hit.endEditBlock();
    sweep = hit;
    ++count;
  }
  inlineFindStatus_->setText(count == 0
                                  ? tr("No results")
                                  : tr("%1 replaced").arg(count));
}

void MainWindow::updateWelcomeVisibility() {
  if (!welcomeWidget_) {
    return;
  }
  bool empty = editors_.isEmpty();
  ui_->divider->setVisible(!empty);
  welcomeWidget_->setVisible(empty);
  // Hide the action toolbar on the welcome screen — those buttons act on an
  // open editor, so they're not meaningful before any file is loaded.
  if (topToolbar_) {
    if (QWidget* tb = topToolbar_->parentWidget()) {
      tb->setVisible(!empty);
    }
  }
  if (empty) {
    refreshRecents();
  } else {
    repositionAddTabBtn();
  }
  updateFileInfo();
}

void MainWindow::addRecentFile(const QString& path) {
  if (path.isEmpty() || !isPawnFile(path)) {
    return;
  }
  QSettings settings;
  QStringList recents = settings.value("RecentFiles").toStringList();
  // Purge any stale non-pawn entries that may have been recorded before the
  // extension filter was added.
  recents.erase(std::remove_if(recents.begin(), recents.end(),
                                 [](const QString& p) { return !isPawnFile(p); }),
                  recents.end());
  recents.removeAll(path);
  recents.prepend(path);
  while (recents.size() > 15) {
    recents.removeLast();
  }
  settings.setValue("RecentFiles", recents);
}

void MainWindow::refreshRecents() {
  if (!recentsLayout_) {
    return;
  }
  QLayoutItem* item;
  while ((item = recentsLayout_->takeAt(0)) != nullptr) {
    if (item->widget()) {
      item->widget()->deleteLater();
    }
    delete item;
  }
  QSettings settings;
  QStringList recents = settings.value("RecentFiles").toStringList();
  if (recents.isEmpty()) {
    QLabel* none = new QLabel(tr("No recent files"));
    none->setStyleSheet("color:#6B7280; font-size:13px;");
    recentsLayout_->addWidget(none);
    return;
  }
  int shown = 0;
  for (const QString& path : recents) {
    if (shown >= 5) {
      break;
    }
    QFileInfo fi(path);
    // QLabel supports rich text, so filename can be normal and the path dimmed.
    // Wrap it in a clickable button-like label.
    const QString dim = effectiveDarkMode() ? "#6B7280" : "#9AA1AC";
    QLabel* b = new QLabel;
    b->setObjectName("recentItem");
    b->setCursor(Qt::PointingHandCursor);
    b->setTextFormat(Qt::RichText);
    // Strip the user's home prefix from the recent path so the list shows
    // "/Documents/…" instead of "/Users/<user>/Documents/…". Tooltip still
    // carries the absolute path so it's recoverable on hover.
    const QString displayPath = stripUserPrefix(fi.absolutePath());
    b->setText(QString("<img src=':/assets/images/icons/new.svg' width='14' height='14'>"
                       "&nbsp;<span>%1</span>&nbsp;&nbsp;"
                       "<span style='color:%2'>%3</span>")
                   .arg(fi.fileName().toHtmlEscaped(), dim, displayPath.toHtmlEscaped()));
    b->setToolTip(path);
    b->installEventFilter(this);
    b->setProperty("recentPath", path);
    recentsLayout_->addWidget(b);
    ++shown;
  }
}

QWidget* MainWindow::buildWelcome() {
  QWidget* page = new QWidget;
  page->setObjectName("welcomePage");

  // Outer layout: vertical centering + horizontal centering.
  QVBoxLayout* vOuter = new QVBoxLayout(page);
  vOuter->addStretch(1);
  QHBoxLayout* outer = new QHBoxLayout;
  vOuter->addLayout(outer);
  vOuter->addStretch(1);

  outer->addStretch(1);
  QWidget* centerBox = new QWidget;
  centerBox->setMinimumWidth(620);
  centerBox->setMaximumWidth(900);
  QVBoxLayout* center = new QVBoxLayout(centerBox);
  center->setContentsMargins(0, 0, 0, 0);
  center->setSpacing(28);
  outer->addWidget(centerBox, 0);
  outer->addStretch(1);

  // Header: logo + name.
  QHBoxLayout* header = new QHBoxLayout;
  QLabel* logo = new QLabel;
  QPixmap pm(":/assets/images/icon_128x128.png");
  if (!pm.isNull()) {
    logo->setPixmap(pm.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  header->addWidget(logo);
  QLabel* name = new QLabel(tr("Qawno"));
  name->setObjectName("welcomeName");
  name->setStyleSheet("font-size: 34px; font-weight: 600;");
  header->addWidget(name);
  header->addStretch(1);
  center->addLayout(header);

  // Two columns: recents (left) | actions (right), with a divider.
  QHBoxLayout* cols = new QHBoxLayout;
  cols->setSpacing(40);
  center->addLayout(cols, 0);

  // Left column.
  QVBoxLayout* left = new QVBoxLayout;
  left->setSpacing(8);
  QLabel* recentHdr = new QLabel(tr("Recent"));
  recentHdr->setObjectName("recentHeader");
  recentHdr->setStyleSheet("font-size: 16px; font-weight: 600;");
  left->addWidget(recentHdr);
  recentsLayout_ = new QVBoxLayout;
  recentsLayout_->setSpacing(4);
  left->addLayout(recentsLayout_);
  left->addStretch(1);
  cols->addLayout(left, 1);

  // Divider.
  QFrame* vline = new QFrame;
  vline->setFrameShape(QFrame::VLine);
  vline->setStyleSheet("color:#3A3F4B;");
  cols->addWidget(vline);

  // Right column: action tiles.
  QVBoxLayout* right = new QVBoxLayout;
  right->setSpacing(12);
  QLabel* startHdr = new QLabel(tr("Start"));
  startHdr->setObjectName("startHeader");
  startHdr->setStyleSheet("font-size: 16px; font-weight: 600;");
  right->addWidget(startHdr);

  auto tile = [this](const QString& iconPath, const QString& title,
                     std::function<void()> onClick) {
    QPushButton* b = new QPushButton;
    b->setObjectName("welcomeTile");
    b->setCursor(Qt::PointingHandCursor);
    b->setIcon(QIcon(iconPath));
    b->setIconSize(QSize(24, 24));
    b->setText("  " + title);
    b->setMinimumHeight(58);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(b, &QPushButton::clicked, this, onClick);
    return b;
  };

  QGridLayout* tiles = new QGridLayout;
  tiles->setSpacing(12);
  tiles->setColumnStretch(0, 1);
  tiles->setColumnStretch(1, 1);
  tiles->addWidget(tile(":/assets/images/icons/new.svg", tr("New Gamemode"),
                        [this] { on_actionNewGM_triggered(); }), 0, 0);
  tiles->addWidget(tile(":/assets/images/icons/new.svg", tr("New Filterscript"),
                        [this] { on_actionNewFS_triggered(); }), 0, 1);
  tiles->addWidget(tile(":/assets/images/icons/new.svg", tr("New Include"),
                        [this] { on_actionNewInc_triggered(); }), 1, 0);
  tiles->addWidget(tile(":/assets/images/icons/new.svg", tr("New Blank"),
                        [this] { on_actionNewBlank_triggered(); }), 1, 1);
  tiles->addWidget(tile(":/assets/images/icons/open.svg", tr("Open…"),
                        [this] { on_actionOpen_triggered(); }), 2, 0, 1, 2);
  right->addLayout(tiles);
  right->addStretch(1);
  cols->addLayout(right, 1);

  // The app ships and deploys a native compiler, so there is no compiler/Wine
  // detection to show on the start page.

  return page;
}

QString MainWindow::projectQawnoDir(const QString& pwnFile) {
  if (pwnFile.isEmpty()) {
    return QString();
  }
  // Walk upward from the .pwn looking for a qawno/ folder. Typical open.mp
  // layouts put it at the project root next to gamemodes/, includes/, … — not
  // beside every .pwn. Fall back to <pwnDir>/qawno so error messages still
  // point at a sensible path.
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

void MainWindow::deployNativeCompiler(const QString& qawnoDir) {
  if (qawnoDir.isEmpty()) {
    return;
  }
  const QString destDir = qawnoDir + "/native";
  QDir().mkpath(destDir);

  // pawncc + its shared library are copied as a pair: the binary resolves
  // libpawnc.dylib from its own directory via an @loader_path rpath.
  const QStringList artefacts = {"pawncc", "libpawnc.dylib"};
  for (const QString& name : artefacts) {
    const QString dest = destDir + "/" + name;
    if (QFile::exists(dest)) {
      continue;
    }
    // Source candidates: app bundle Resources/native, then repo bin/ when
    // running from a build tree.
    QStringList sources;
    sources << QCoreApplication::applicationDirPath() + "/native/" + name;
#ifdef Q_OS_MACOS
    sources << QCoreApplication::applicationDirPath() + "/../Resources/native/" + name;
#endif
    sources << QCoreApplication::applicationDirPath() + "/../bin/darwin/" + name;
    for (const QString& src : sources) {
      if (QFile::exists(src)) {
        if (QFile::copy(src, dest) && name == "pawncc") {
          QFile::setPermissions(dest,
              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
              QFileDevice::ReadGroup | QFileDevice::ExeGroup |
              QFileDevice::ReadOther | QFileDevice::ExeOther);
        }
        break;
      }
    }
  }
}

#ifdef Q_OS_MACOS
bool MainWindow::macCompilePreflight(const QString& pwnFile) {
  // The native compiler is bundled and deployed into the project's
  // qawno/native/ folder — no Wine/CrossOver. The only requirement is that the
  // project has (or can receive) the compiler. deployNativeCompiler handles
  // copying it in; here we just confirm the project's qawno/ folder resolved.
  const QString qawnoDir = projectQawnoDir(pwnFile);
  if (qawnoDir.isEmpty()) {
    QMessageBox::warning(
        this, tr("Cannot compile"),
        tr("Could not locate a qawno/ folder for this project. Place your "
           ".pwn inside a project that has a qawno/ directory."));
    return false;
  }
  return true;
}
#endif // Q_OS_MACOS

