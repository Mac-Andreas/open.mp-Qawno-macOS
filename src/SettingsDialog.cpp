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

#include "SettingsDialog.h"

#include "Compiler.h"
#include "IconLoader.h"
#include "IosToggle.h"
#include "LocaleManager.h"
#include "TelemetryClient.h"
#include "UpdateChecker.h"
#include "qawno.h"

#include <QApplication>
#include <QBrush>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QUrl>

#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  // Native macOS chrome — real traffic lights, native window shadow + corner
  // rounding. Frameless/translucent variants were brittle on Qt6 and looked
  // like a cheap re-skin. Letting AppKit draw the window gives the genuine
  // System Settings look.
  setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                 | Qt::WindowStaysOnTopHint);
  setWindowTitle(tr("Qawno Settings"));
  setModal(false);
  setObjectName("settingsPopup");
  setFixedSize(820, 580);
  buildUi();
  loadFromSettings();
  applyTheme(palette().window().color().lightness() < 128);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setEditorFont(const QFont& f) {
  editorFont_ = f;
  if (editorFontCombo_) {
    int idx = editorFontCombo_->findText(f.family());
    if (idx >= 0) editorFontCombo_->setCurrentIndex(idx);
  }
}

void SettingsDialog::setOutputFont(const QFont& f) {
  outputFont_ = f;
  if (outputFontCombo_) {
    int idx = outputFontCombo_->findText(f.family());
    if (idx >= 0) outputFontCombo_->setCurrentIndex(idx);
  }
}

QFont SettingsDialog::editorFont() const { return editorFont_; }
QFont SettingsDialog::outputFont() const { return outputFont_; }

bool SettingsDialog::fullPathInOutput() {
  return QSettings().value("OutputFullPath", false).toBool();
}
QString SettingsDialog::syntaxTheme() {
  return QSettings().value("SyntaxTheme", "classic").toString();
}
int SettingsDialog::uiThemeMode() {
  return QSettings().value("ThemeMode", 0).toInt();
}
bool SettingsDialog::showToolbarText() {
  return QSettings().value("ShowToolbarText", true).toBool();
}
bool SettingsDialog::autoDownloadUpdates() {
  return QSettings().value("AutoDownloadUpdates", true).toBool();
}
bool SettingsDialog::reopenFilesAfterUpdate() {
  return QSettings().value("ReopenFilesAfterUpdate", true).toBool();
}

QStringList SettingsDialog::monospaceFontsForPicker() const {
  // Match the original Windows fork: default = "Courier New". On platforms
  // without it, fall back to system monospace. Subsequent entries are the
  // common macOS monospace families.
  QStringList list;
  list << "Courier New (Default)";
  list << "Menlo (VS Code)";
  QFontDatabase fdb;
  const QStringList prefer = {
      "SF Mono", "Monaco", "Andale Mono", "Courier",
      "Helvetica Neue", "Helvetica", "Lucida Grande", "PT Mono",
  };
  for (const QString& f : prefer) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (QFontDatabase::families().contains(f, Qt::CaseInsensitive)) {
#else
    if (fdb.families().contains(f, Qt::CaseInsensitive)) {
#endif
      list << f;
    }
  }
  return list;
}

void SettingsDialog::buildUi() {
  auto root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  // Native window chrome handles traffic lights + title; no custom titlebar.

  // Body: sidebar nav (left) + stacked pages (right). VS Code / macOS
  // Preferences-style modern layout instead of the old top tab strip.
  auto body = new QWidget(this);
  body->setObjectName("settingsBody");
  auto bodyLay = new QHBoxLayout(body);
  bodyLay->setContentsMargins(0, 0, 0, 0);
  bodyLay->setSpacing(0);

  // Nav column: scrollable list on top, "Reset to defaults" pinned at the
  // bottom so it stays out of the per-page content area.
  auto* navCol = new QWidget(body);
  navCol->setObjectName("settingsNavCol");
  navCol->setFixedWidth(180);
  auto* navColLay = new QVBoxLayout(navCol);
  navColLay->setContentsMargins(0, 0, 0, 0);
  navColLay->setSpacing(0);

  nav_ = new QListWidget(navCol);
  nav_->setObjectName("settingsNav");
  nav_->setFrameShape(QFrame::NoFrame);
  nav_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  nav_->setFocusPolicy(Qt::NoFocus);
  nav_->setIconSize(QSize(18, 18));
  navColLay->addWidget(nav_, 1);

  resetBtn_ = new QPushButton(tr("Reset to defaults"), navCol);
  resetBtn_->setObjectName("settingsResetBtn");
  resetBtn_->setCursor(Qt::PointingHandCursor);
  connect(resetBtn_, &QPushButton::clicked, this, &SettingsDialog::resetDefaults);
  navColLay->addWidget(resetBtn_, 0);
  navColLay->addSpacing(8);

  bodyLay->addWidget(navCol);

  pages_ = new QStackedWidget(body);
  pages_->setObjectName("settingsPages");
  bodyLay->addWidget(pages_, 1);
  root->addWidget(body, 1);

  // Card builder — a titled container with a 1px border, used to group
  // related settings inside a page. Returns a child layout to fill.
  auto makeCard = [this](const QString& title, const QString& subtitle = QString()) {
    auto card = new QFrame;
    card->setObjectName("settingsCard");
    card->setFrameShape(QFrame::NoFrame);
    // QFrame ignores `background:` shorthand unless styled-background is on.
    // Without this the card paints with the system's default light surface
    // (very visible in dark mode) instead of the themed cardBg colour.
    card->setAttribute(Qt::WA_StyledBackground, true);
    cards_.append(card);
    auto cl = new QVBoxLayout(card);
    cl->setContentsMargins(22, 18, 22, 18);
    cl->setSpacing(14);
    if (!title.isEmpty()) {
      auto t = new QLabel(title, card);
      t->setObjectName("settingsCardTitle");
      cl->addWidget(t);
    }
    if (!subtitle.isEmpty()) {
      auto s = new QLabel(subtitle, card);
      s->setObjectName("settingsCardSubtitle");
      s->setWordWrap(true);
      cl->addWidget(s);
    }
    return std::make_pair(card, cl);
  };

  // Row helper: label (+ optional hint) on the left, control on the right.
  auto makeRow = [](const QString& label, const QString& hint, QWidget* ctrl) {
    auto row = new QHBoxLayout;
    row->setSpacing(12);
    auto col = new QVBoxLayout;
    col->setSpacing(2);
    auto lbl = new QLabel(label);
    lbl->setObjectName("settingsRowLabel");
    col->addWidget(lbl);
    if (!hint.isEmpty()) {
      auto h = new QLabel(hint);
      h->setObjectName("settingsRowHint");
      h->setWordWrap(true);
      col->addWidget(h);
    }
    row->addLayout(col, 1);
    row->addWidget(ctrl, 0, Qt::AlignVCenter);
    return row;
  };

  auto addPage = [this](const QString& title, const QString& iconPath = QString()) {
    auto page = new QWidget;
    auto pv = new QVBoxLayout(page);
    pv->setContentsMargins(32, 28, 32, 28);
    pv->setSpacing(20);
    // Header row: icon + title. Icon is themed (re-tinted on theme switch
    // by applyTheme via the pageHeaderIcons_ map).
    auto hdrRow = new QHBoxLayout;
    hdrRow->setSpacing(12);
    if (!iconPath.isEmpty()) {
      auto* icon = new QLabel;
      icon->setObjectName("settingsPageHeaderIcon");
      const bool dark = palette().window().color().lightness() < 128;
      icon->setPixmap(IconLoader::themed(iconPath, dark, QSize(28, 28)).pixmap(28, 28));
      hdrRow->addWidget(icon, 0, Qt::AlignVCenter);
      pageHeaderIcons_.append({icon, iconPath});
    }
    auto hdr = new QLabel(title);
    hdr->setObjectName("settingsPageHeader");
    hdrRow->addWidget(hdr, 1, Qt::AlignVCenter);
    pv->addLayout(hdrRow);
    pages_->addWidget(page);
    auto navItem = new QListWidgetItem(title, nav_);
    navItem->setSizeHint(QSize(0, 38));
    if (!iconPath.isEmpty()) {
      const bool dark = palette().window().color().lightness() < 128;
      navItem->setIcon(IconLoader::themed(iconPath, dark, QSize(16, 16)));
    }
    return pv;
  };

  // ----- General -----
  {
    auto v = addPage(tr("General"), ":/assets/images/icons/settings.svg");
    auto [card, cl] = makeCard(tr("Editor behaviour"));
    fullPath_       = new IosToggle;
    showToolbarText_= new IosToggle;
    hoverTooltips_  = new IosToggle;
    lintSquiggles_  = new IosToggle;
    cl->addLayout(makeRow(tr("Show full paths in compiler output"),
                           tr("Display absolute paths instead of just the file name."),
                           fullPath_));
    cl->addLayout(makeRow(tr("Show text alongside toolbar icons"),
                           tr("Collapse to icon-only when the window is narrow."),
                           showToolbarText_));
    cl->addLayout(makeRow(tr("Hover tooltips on identifiers"),
                           tr("Show the symbol's prototype when you hover over it."),
                           hoverTooltips_));
    cl->addLayout(makeRow(tr("Underline compiler errors / warnings"),
                           tr("Wavy underline on the offending line in the editor."),
                           lintSquiggles_));
    v->addWidget(card);

    // Language picker — pulls the locale list from LocaleManager and shows
    // each option's native name. Changing it re-installs the .qm immediately
    // (no restart) and persists to QSettings("Language").
    auto [langCard, lc] = makeCard(tr("Language"),
        tr("Same order as the open.mp launcher. Restart isn't required."));
    auto* langCombo = new QComboBox;
    langCombo->setMinimumWidth(260);
    auto* lm = LocaleManager::instance();
    const QString cur = lm->currentCode();
    int curIdx = 0;
    for (int i = 0; i < lm->locales().size(); ++i) {
      const auto& loc = lm->locales()[i];
      // No flag icon — just the native language name. Translation completeness
      // is what users actually care about, and the placeholder flags read as
      // noise more than information.
      const bool isEnglish = (loc.code == "en");
      const bool hasQm = isEnglish ||
          QFileInfo::exists(QString(":/i18n/qawno_%1.qm").arg(loc.code));
      QString label = loc.nativeName;
      if (!hasQm) label += tr("  (coming soon)");
      langCombo->addItem(label, loc.code);
      auto* model = qobject_cast<QStandardItemModel*>(langCombo->model());
      if (model) {
        auto* item = model->item(i);
        if (item) {
          item->setEnabled(hasQm);
          if (!hasQm) {
            QBrush dim = item->foreground();
            dim.setColor(QColor(0x7E, 0x86, 0x94));
            item->setForeground(dim);
          }
        }
      }
      if (loc.code == cur) curIdx = i;
    }
    langCombo->setCurrentIndex(curIdx);
    lc->addLayout(makeRow(tr("Display language"), QString(), langCombo));
    v->addWidget(langCard);
    connect(langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [langCombo](int) {
              LocaleManager::instance()->setCurrent(langCombo->currentData().toString());
            });

    v->addStretch(1);

    connect(fullPath_,        &IosToggle::toggled, this, [this](bool){ save(); });
    connect(showToolbarText_, &IosToggle::toggled, this, [this](bool){ save(); });
    connect(hoverTooltips_,   &IosToggle::toggled, this, [this](bool){ save(); });
    connect(lintSquiggles_,   &IosToggle::toggled, this, [this](bool){ save(); });
  }

  // ----- Appearance -----
  {
    auto v = addPage(tr("Appearance"), ":/assets/images/icons/theme-light.svg");

    auto [themeCard, tl] = makeCard(tr("Application theme"),
        tr("Choose how Qawno looks. System mirrors your macOS appearance."));

    // Icon-based theme picker (replaces the previous IosToggle row). Each
    // tile is a checkable QToolButton with the icon above its label, grouped
    // exclusively so picking one clears the others.
    auto makeThemeTile = [this](const QString& name, const QString& iconPath) {
      auto* b = new QToolButton;
      b->setObjectName("settingsThemeTile");
      b->setCheckable(true);
      b->setAutoExclusive(false);  // managed manually so save() runs uniformly
      b->setText(name);
      b->setIcon(QIcon(iconPath));
      b->setIconSize(QSize(36, 36));
      b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
      b->setCursor(Qt::PointingHandCursor);
      b->setMinimumSize(96, 96);
      return b;
    };
    themeSystem_ = makeThemeTile(tr("System"), ":/assets/images/icons/theme-system.svg");
    themeLight_  = makeThemeTile(tr("Light"),  ":/assets/images/icons/theme-light.svg");
    themeDark_   = makeThemeTile(tr("Dark"),   ":/assets/images/icons/theme-dark.svg");

    auto themeRow = new QHBoxLayout;
    themeRow->setSpacing(12);
    themeRow->addWidget(themeSystem_);
    themeRow->addWidget(themeLight_);
    themeRow->addWidget(themeDark_);
    themeRow->addStretch(1);
    tl->addLayout(themeRow);
    v->addWidget(themeCard);

    auto exclusiveTheme = [this](QToolButton* picked) {
      blockSave_ = true;
      themeSystem_->setChecked(picked == themeSystem_);
      themeLight_->setChecked(picked == themeLight_);
      themeDark_->setChecked(picked == themeDark_);
      blockSave_ = false;
      save();
    };
    connect(themeSystem_, &QToolButton::clicked, this, [this, exclusiveTheme] { exclusiveTheme(themeSystem_); });
    connect(themeLight_,  &QToolButton::clicked, this, [this, exclusiveTheme] { exclusiveTheme(themeLight_); });
    connect(themeDark_,   &QToolButton::clicked, this, [this, exclusiveTheme] { exclusiveTheme(themeDark_); });

    auto [synCard, sl] = makeCard(tr("Syntax colours"),
        tr("Color scheme used by the code editor."));
    syntaxTheme_ = new QComboBox;
    syntaxTheme_->setMinimumWidth(220);
    syntaxTheme_->addItem(tr("Classic"), "classic");
    syntaxTheme_->addItem(tr("VS Code"), "vscode");
    sl->addLayout(makeRow(tr("Scheme"), QString(), syntaxTheme_));
    v->addWidget(synCard);
    v->addStretch(1);

    connect(syntaxTheme_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ save(); });
  }

  // ----- Fonts -----
  {
    auto v = addPage(tr("Fonts"), ":/assets/images/icons/edit.svg");
    auto [card, cl] = makeCard(tr("Typography"),
        tr("Monospace families installed on this system."));
    const QStringList families = monospaceFontsForPicker();
    editorFontCombo_ = new QComboBox;
    editorFontCombo_->addItems(families);
    editorFontCombo_->setMinimumWidth(240);
    outputFontCombo_ = new QComboBox;
    outputFontCombo_->addItems(families);
    outputFontCombo_->setMinimumWidth(240);
    // Generous row gap + spacer so the two combos don't collide vertically.
    cl->setSpacing(24);
    cl->addLayout(makeRow(tr("Editor font"), tr("Code editor surface."), editorFontCombo_));
    cl->addSpacing(8);
    cl->addLayout(makeRow(tr("Output font"), tr("Compiler and run log."),  outputFontCombo_));
    v->addWidget(card);

    // App-wide UI font: applied to menus, dialogs, sidebar — anything that
    // isn't the code editor / log surfaces handled above.
    auto [appCard, ac] = makeCard(tr("Application font"),
        tr("Used for menus, dialogs, the tab strip and the sidebar."));
    auto* appFontCombo = new QComboBox;
    appFontCombo->setMinimumWidth(260);
    appFontCombo->setEditable(true);
    appFontCombo->addItem("SF Pro Text (Default)");
    appFontCombo->addItems(QFontDatabase::families());
    const QString currentAppFont = QSettings().value("AppFont").toString();
    if (!currentAppFont.isEmpty()) {
      int i = appFontCombo->findText(currentAppFont);
      if (i >= 0) appFontCombo->setCurrentIndex(i);
      else appFontCombo->setEditText(currentAppFont);
    }
    ac->addLayout(makeRow(tr("Family"), QString(), appFontCombo));
    v->addWidget(appCard);
    connect(appFontCombo, &QComboBox::currentTextChanged, this,
            [this](const QString& text) {
              QString family = text;
              int parens = family.indexOf(" (");
              if (parens > 0) family = family.left(parens);
              QSettings().setValue("AppFont", family);
              QFont f = qApp->font();
              f.setFamily(family);
              qApp->setFont(f);
              save();
            });

    // Syntax colour scheme — per-language presets that auto-invert with the
    // app theme.
    auto [synCard, sc] = makeCard(tr("Syntax colours"),
        tr("Per-language palette. Light/dark variant follows the app theme."));
    auto* langCombo = new QComboBox;
    langCombo->setMinimumWidth(200);
    langCombo->addItem(tr("Pawn"),       "Pawn");
    langCombo->addItem(tr("C / C++"),    "Cpp");
    langCombo->addItem(tr("Python"),     "Python");
    langCombo->addItem(tr("JavaScript"), "JavaScript");
    langCombo->addItem(tr("Rust"),       "Rust");
    const QString curLang = QSettings().value("SyntaxLanguage", "Pawn").toString();
    int li = langCombo->findData(curLang);
    if (li >= 0) langCombo->setCurrentIndex(li);
    sc->addLayout(makeRow(tr("Language"), QString(), langCombo));
    v->addWidget(synCard);
    connect(langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [langCombo](int) {
              QSettings().setValue("SyntaxLanguage", langCombo->currentData().toString());
            });

    v->addStretch(1);

    auto onFontPicked = [this](QComboBox* combo, QFont& target) {
      const QString text = combo->currentText();
      QString family = text;
      int parens = family.indexOf(" (");
      if (parens > 0) family = family.left(parens);
      target.setFamily(family);
      save();
    };
    connect(editorFontCombo_, &QComboBox::currentTextChanged, this,
            [this, onFontPicked](const QString&) { onFontPicked(editorFontCombo_, editorFont_); });
    connect(outputFontCombo_, &QComboBox::currentTextChanged, this,
            [this, onFontPicked](const QString&) { onFontPicked(outputFontCombo_, outputFont_); });
  }

  // ----- Compiler -----
  {
    auto v = addPage(tr("Compiler"), ":/assets/images/icons/run.svg");
    auto [card, cl] = makeCard(tr("Pawn compiler"),
        tr("Path to the pawncc binary and command-line options."));
    auto pathRow = new QHBoxLayout;
    pathRow->setSpacing(6);
    compilerPath_ = new QLineEdit;
    compilerPath_->setObjectName("settingsLineEdit");
    pathRow->addWidget(compilerPath_, 1);
    auto browse = new QPushButton(tr("Browse…"));
    browse->setObjectName("settingsBrowseBtn");
    browse->setCursor(Qt::PointingHandCursor);
    connect(browse, &QPushButton::clicked, this, [this] {
      QString p = QFileDialog::getOpenFileName(this, tr("Compiler executable"),
                                                compilerPath_->text());
      if (!p.isEmpty()) compilerPath_->setText(p);
    });
    pathRow->addWidget(browse);
    auto pathWrap = new QWidget; pathWrap->setLayout(pathRow);
    cl->addLayout(makeRow(tr("Compiler path"), QString(), pathWrap));

    compilerOptions_ = new QLineEdit;
    compilerOptions_->setObjectName("settingsLineEdit");
    compilerOptions_->setMinimumWidth(320);
    cl->addLayout(makeRow(tr("Options"),
        tr("Tokens: %p = project dir, %i = input file, %o = base name, %q = qawno dir."),
        compilerOptions_));
    v->addWidget(card);
    v->addStretch(1);

    connect(compilerPath_, &QLineEdit::textEdited, this, [this](const QString&){ save(); });
    connect(compilerOptions_, &QLineEdit::textEdited, this, [this](const QString&){ save(); });
  }

  // ----- Folder Access -----
  {
    auto v = addPage(tr("Folder Access"), ":/assets/images/icons/open.svg");
    auto [card, cl] = makeCard(tr("Granted folders"),
        tr("macOS only: Qawno reads/writes from the folders below. Revoke via System Settings → Privacy & Security → Files and Folders."));

    auto* listLabel = new QLabel;
    listLabel->setObjectName("settingsRowLabel");
    listLabel->setWordWrap(true);
    auto refreshFolderList = [listLabel] {
      QStringList rows;
      const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
      const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
      const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
      for (const QString& p : {docs, downloads, desktop}) {
        if (!p.isEmpty()) {
          const bool readable = QFileInfo(p).isReadable();
          rows << QString("%1  —  %2").arg(p, readable ? tr("readable") : tr("no access yet"));
        }
      }
      listLabel->setText(rows.join("\n"));
    };
    refreshFolderList();
    cl->addWidget(listLabel);

    auto* btnRow = new QHBoxLayout;
    auto* grantBtn = new QPushButton(tr("Grant Documents access"));
    grantBtn->setObjectName("settingsBrowseBtn");
    grantBtn->setCursor(Qt::PointingHandCursor);
    connect(grantBtn, &QPushButton::clicked, this, [this, refreshFolderList] {
      // Triggering a read prompts macOS to ask the user for permission the
      // first time. Subsequent runs reuse the granted scope.
      const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
      QDir(docs).entryList();
      refreshFolderList();
    });
    btnRow->addWidget(grantBtn);

    auto* sysBtn = new QPushButton(tr("Open System Settings"));
    sysBtn->setObjectName("settingsBrowseBtn");
    sysBtn->setCursor(Qt::PointingHandCursor);
    connect(sysBtn, &QPushButton::clicked, this, [] {
      QDesktopServices::openUrl(QUrl("x-apple.systempreferences:com.apple.preference.security?Privacy_Files"));
    });
    btnRow->addWidget(sysBtn);
    btnRow->addStretch(1);
    cl->addLayout(btnRow);
    v->addWidget(card);
    v->addStretch(1);
  }

  // ----- Privacy -----
  {
    auto v = addPage(tr("Privacy"), ":/assets/images/icons/privacy.svg");
    // Title built manually so a backend status dot can sit beside it: green =
    // telemetry backend reachable, red = not.
    auto [card, cl] = makeCard(QString(),
        tr("Help us prioritise fixes. Nothing identifying you, your projects, "
           "or your code is ever sent. A random per-install ID counts unique "
           "users without revealing who they are."));

    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel(tr("Anonymous usage data"), card);
    title->setObjectName("settingsCardTitle");
    auto* statusDot = new QLabel(card);
    statusDot->setFixedSize(10, 10);
    statusDot->setStyleSheet("border-radius:5px; background:#8A8595;");  // unknown
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    titleRow->addWidget(statusDot);
    cl->insertLayout(0, titleRow);

    connect(TelemetryClient::instance(), &TelemetryClient::backendStatus,
            statusDot, [statusDot](bool ok) {
      statusDot->setStyleSheet(QStringLiteral("border-radius:5px; background:%1;")
          .arg(ok ? "#3FB950" : "#E5534B"));
    });
    TelemetryClient::instance()->checkBackend();

    auto* telToggle = new IosToggle;
    telToggle->setChecked(TelemetryClient::instance()->consent()
                          == TelemetryClient::Consent::Granted);
    cl->addLayout(makeRow(tr("Share anonymous usage data"),
                           tr("Toggle anytime; takes effect immediately."),
                           telToggle));
    connect(telToggle, &IosToggle::toggled, this, [](bool on) {
      TelemetryClient::instance()->setConsent(
          on ? TelemetryClient::Consent::Granted
             : TelemetryClient::Consent::Denied);
    });
    v->addWidget(card);
    v->addStretch(1);
  }

  // ----- Updates -----
  {
    auto v = addPage(tr("Updates"), ":/assets/images/icons/update.svg");
    auto [card, cl] = makeCard(tr("Components"),
        tr("Check for new releases of the Pawn compiler and Qawno itself."));

    auto buildRow = [this](const QString& name, QLabel*& verLabel, QPushButton*& btn) {
      auto row = new QHBoxLayout;
      row->setSpacing(10);
      auto nameLbl = new QLabel(name);
      nameLbl->setObjectName("settingsRowLabel");
      row->addWidget(nameLbl, 1);

      btn = new QPushButton(tr("Up to date"));
      btn->setObjectName("settingsUpdateBtn");
      btn->setCursor(Qt::PointingHandCursor);
      btn->setEnabled(false);
      row->addWidget(btn, 0, Qt::AlignVCenter);

      verLabel = new QLabel(tr("—"));
      verLabel->setObjectName("settingsVersionChip");
      verLabel->setAlignment(Qt::AlignCenter);
      verLabel->setMinimumWidth(70);
      row->addWidget(verLabel, 0, Qt::AlignVCenter);
      return row;
    };

    cl->addLayout(buildRow(tr("Pawn compiler"), compilerCurrentVer_, compilerUpdateBtn_));
    cl->addLayout(buildRow(tr("Qawno"),         qawnoCurrentVer_,    qawnoUpdateBtn_));
    v->addWidget(card);

    auto [prefCard, pl] = makeCard(tr("Preferences"));
    autoDownload_      = new IosToggle;
    reopenAfterUpdate_ = new IosToggle;
    pl->addLayout(makeRow(tr("Download updates automatically"),
                           tr("Fetch the release in the background so you only need to confirm the install."),
                           autoDownload_));
    pl->addLayout(makeRow(tr("Re-open files after update"),
                           tr("Restore the editor tabs that were open before the update."),
                           reopenAfterUpdate_));
    v->addWidget(prefCard);
    v->addStretch(1);

    connect(autoDownload_,      &IosToggle::toggled, this, [this](bool){ save(); });
    connect(reopenAfterUpdate_, &IosToggle::toggled, this, [this](bool){ save(); });

    // Either tool's Update button funnels to the same MainWindow-level prompt
    // (both update streams share the install/relaunch path).
    connect(compilerUpdateBtn_, &QPushButton::clicked, this, [this] { emit updateRequested(); });
    connect(qawnoUpdateBtn_,    &QPushButton::clicked, this, [this] { emit updateRequested(); });

    auto* uc = UpdateChecker::instance();
    auto applyState = [this](UpdateChecker::Component which) {
      auto* btn = which == UpdateChecker::Component::Compiler ? compilerUpdateBtn_ : qawnoUpdateBtn_;
      auto* lbl = which == UpdateChecker::Component::Compiler ? compilerCurrentVer_ : qawnoCurrentVer_;
      if (!btn || !lbl) return;
      if (UpdateChecker::instance()->isChecking(which)) {
        btn->setText(tr("Checking…"));
        btn->setEnabled(false);
        return;
      }
      const auto rel = UpdateChecker::instance()->latest(which);
      const QString current = lbl->text();
      // Compiler is "installed" when pawncc.exe sits beside Qawno.app (the
      // bundled-install layout). Matches the welcome card's detection so
      // the two surfaces don't disagree.
      bool installedLocal = false;
      if (which == UpdateChecker::Component::Compiler) {
        QDir appSibling(QCoreApplication::applicationDirPath());
        appSibling.cdUp(); appSibling.cdUp(); appSibling.cdUp();
        installedLocal = QFileInfo::exists(appSibling.absoluteFilePath("pawncc.exe"));
      }
      if (rel.version.isEmpty()) {
        btn->setText(installedLocal ? tr("Up to date") : tr("Check"));
        btn->setEnabled(!installedLocal);
        return;
      }
      if (installedLocal) {
        btn->setText(tr("Up to date"));
        btn->setEnabled(false);
        if (current == tr("—") || current.isEmpty()) lbl->setText(rel.version);
        return;
      }
      // Surface the remote version on the chip when we have no local one —
      // the user still wants to see what's available even if we can't read
      // the installed pawncc (e.g. project not open yet).
      if (current == tr("—") || current.isEmpty()) {
        lbl->setText(rel.version);
        btn->setText(tr("Install %1").arg(rel.tag));
        btn->setEnabled(true);
      } else if (rel.version == current) {
        btn->setText(tr("Up to date"));
        btn->setEnabled(false);
      } else {
        btn->setText(tr("Update available %1").arg(rel.tag));
        btn->setEnabled(true);
      }
    };
    connect(uc, &UpdateChecker::checkStarted, this, applyState);
    connect(uc, &UpdateChecker::releaseFetched, this,
            [applyState](UpdateChecker::Component which, UpdateChecker::Release) { applyState(which); });
    connect(uc, &UpdateChecker::fetchFailed, this,
            [this](UpdateChecker::Component which, const QString&) {
              auto* btn = which == UpdateChecker::Component::Compiler ? compilerUpdateBtn_ : qawnoUpdateBtn_;
              if (btn) { btn->setText(tr("Recheck")); btn->setEnabled(true); }
            });
    // Render initial state from whatever's cached so the UI never starts
    // stuck on the placeholder "Up to date" / "Checking…" state.
    applyState(UpdateChecker::Component::Compiler);
    applyState(UpdateChecker::Component::Qawno);
  }

  connect(nav_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
  // Refresh remote release info every time the user opens the Updates pane —
  // the chip shows "Checking…" immediately, then resolves to "Up to date" or
  // "Update available <ver>" via the UpdateChecker signals already wired up.
  const int kUpdatesPageIndex = pages_->count() - 1;
  connect(nav_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row != pages_->count() - 1) return;
    auto* uc = UpdateChecker::instance();
    uc->checkAll();
  });
  (void)kUpdatesPageIndex;
  nav_->setCurrentRow(0);

  // Esc closes (mirrors the × button).
  auto esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(esc, &QShortcut::activated, this, &QDialog::hide);
}

void SettingsDialog::loadFromSettings() {
  blockSave_ = true;
  QSettings s;
  fullPath_->setChecked(s.value("OutputFullPath", false).toBool());
  showToolbarText_->setChecked(s.value("ShowToolbarText", true).toBool());
  hoverTooltips_->setChecked(s.value("HoverTooltips", true).toBool());
  lintSquiggles_->setChecked(s.value("LintSquiggles", true).toBool());
  const int mode = s.value("ThemeMode", 0).toInt();
  themeSystem_->setChecked(mode == 0);
  themeLight_->setChecked(mode == 1);
  themeDark_->setChecked(mode == 2);
  const QString syn = s.value("SyntaxTheme", "classic").toString();
  int idx = syntaxTheme_->findData(syn);
  syntaxTheme_->setCurrentIndex(idx < 0 ? 0 : idx);
  Compiler c;
  compilerPath_->setText(c.path());
  compilerOptions_->setText(c.options().join(" "));
  if (qawnoCurrentVer_) {
    qawnoCurrentVer_->setText(QString::fromLatin1(QAWNO_VERSION_STRING));
  }
  if (autoDownload_)      autoDownload_->setChecked(s.value("AutoDownloadUpdates", true).toBool());
  if (reopenAfterUpdate_) reopenAfterUpdate_->setChecked(s.value("ReopenFilesAfterUpdate", true).toBool());
  if (compilerCurrentVer_) {
    const QString v = c.detectVersion();
    compilerCurrentVer_->setText(v.isEmpty() ? tr("—") : v);
  }
  blockSave_ = false;
}

void SettingsDialog::save() {
  if (blockSave_) return;
  QSettings s;
  s.setValue("OutputFullPath", fullPath_->isChecked());
  s.setValue("ShowToolbarText", showToolbarText_->isChecked());
  s.setValue("HoverTooltips", hoverTooltips_->isChecked());
  s.setValue("LintSquiggles", lintSquiggles_->isChecked());
  int mode = themeLight_->isChecked() ? 1 : themeDark_->isChecked() ? 2 : 0;
  s.setValue("ThemeMode", mode);
  s.setValue("SyntaxTheme", syntaxTheme_->currentData().toString());
  if (autoDownload_)      s.setValue("AutoDownloadUpdates", autoDownload_->isChecked());
  if (reopenAfterUpdate_) s.setValue("ReopenFilesAfterUpdate", reopenAfterUpdate_->isChecked());

  Compiler c;
  c.setPath(compilerPath_->text());
  c.setOptions(compilerOptions_->text());

  emit applied();
}

void SettingsDialog::resetDefaults() {
  blockSave_ = true;
  QSettings s;
  s.remove("OutputFullPath");
  s.remove("ShowToolbarText");
  s.remove("HoverTooltips");
  s.remove("LintSquiggles");
  s.remove("ThemeMode");
  s.remove("SyntaxTheme");
  s.remove("CompilerPath");
  s.remove("CompilerOptions");
  s.remove("AutoDownloadUpdates");
  s.remove("ReopenFilesAfterUpdate");
  loadFromSettings();
  blockSave_ = false;
  emit applied();
}

bool SettingsDialog::eventFilter(QObject* obj, QEvent* ev) {
  if (obj != titleBar_) return QDialog::eventFilter(obj, ev);
  if (ev->type() == QEvent::MouseButtonPress) {
    auto me = static_cast<QMouseEvent*>(ev);
    if (me->button() == Qt::LeftButton) {
      dragging_ = true;
      dragOffset_ = me->globalPosition().toPoint() - frameGeometry().topLeft();
      return true;
    }
  } else if (ev->type() == QEvent::MouseMove && dragging_) {
    auto me = static_cast<QMouseEvent*>(ev);
    move(me->globalPosition().toPoint() - dragOffset_);
    return true;
  } else if (ev->type() == QEvent::MouseButtonRelease) {
    dragging_ = false;
    return true;
  }
  return QDialog::eventFilter(obj, ev);
}

void SettingsDialog::showEvent(QShowEvent* ev) {
  QDialog::showEvent(ev);
  if (auto p = parentWidget()) {
    QPoint topLeft = p->mapToGlobal(QPoint(0, 0));
    int x = topLeft.x() + (p->width() - width()) / 2;
    int y = topLeft.y() + 80;
    move(qMax(topLeft.x(), x), qMax(topLeft.y(), y));
  }
}

void SettingsDialog::applyTheme(bool dark) {
  // Re-tint nav icons to match the current theme. The list items were given
  // themed icons at build time; theme switches need a fresh pass.
  if (nav_) {
    static const char* pageIcons[] = {
      ":/assets/images/icons/settings.svg",      // General
      ":/assets/images/icons/theme-light.svg",   // Appearance
      ":/assets/images/icons/edit.svg",          // Fonts
      ":/assets/images/icons/run.svg",           // Compiler
      ":/assets/images/icons/open.svg",          // Folder Access
      ":/assets/images/icons/privacy.svg",       // Privacy
      ":/assets/images/icons/update.svg",        // Updates
    };
    const int n = qMin<int>(nav_->count(), int(sizeof(pageIcons) / sizeof(pageIcons[0])));
    for (int i = 0; i < n; ++i) {
      nav_->item(i)->setIcon(IconLoader::themed(pageIcons[i], dark, QSize(16, 16)));
    }
  }
  // Re-tint each page's header icon.
  for (const auto& h : pageHeaderIcons_) {
    if (h.label) {
      h.label->setPixmap(IconLoader::themed(h.path, dark, QSize(28, 28)).pixmap(28, 28));
    }
  }
  // Per-card stylesheet scoped via `#settingsCard` ID — Qt's QSS cascade
  // doesn't replace child rules when the selector is ID-scoped, so QLabel
  // colours from the dialog-level sheet still apply to labels inside the
  // card. Plain `QFrame { ... }` here would override every child QFrame's
  // styling, including the labels' implicit ones — visible as the "greyed
  // text" the user reported.
  const char* cardBg   = dark ? "#21262D" : "#FFFFFF";
  const char* cardText = dark ? "#E5E9EF" : "#1A1E25";
  const char* cardSub  = dark ? "#9AA0AB" : "#5A6270";
  const char* cardHint = dark ? "#7E8694" : "#7A8290";
  for (auto* card : cards_) {
    if (!card) continue;
    card->setAutoFillBackground(false);
    card->setStyleSheet(QString(
        "QFrame#settingsCard { background-color: %1; border: none; border-radius: 10px; }"
        "QFrame#settingsCard QLabel { color: %2; background: transparent; }"
        "QFrame#settingsCard QLabel#settingsCardSubtitle { color: %3; }"
        "QFrame#settingsCard QLabel#settingsRowHint     { color: %4; }")
        .arg(cardBg, cardText, cardSub, cardHint));
  }

  // Modernised palette — sidebar nav + card-based pages.
  struct C { QString bg, panelBg, navBg, navHover, navActive, navText, navActiveText,
                  cardBg, cardBd, titleBg, text, sub, hint, inputBg, inputBd,
                  focusBd, btnBg, btnHover; };
  // Palette tuned for contrast in dark mode — inputs are LIGHTER than the
  // card bg (not equal/darker, which made them disappear). Buttons are
  // distinctly lighter than the card bg too, with light text always forced.
  C c = dark ? C{
    "#1B1F25", "#21262D", "#1B1F25", "#262B33", "#3D7CB8", "#C8CDD6", "#FFFFFF",
    "#21262D", "#3A3F4B", "#2D333D", "#E5E9EF", "#9AA0AB", "#7E8694",
    "#2A313A", "#4A5363", "#5B9BD5", "#3A4250", "#4A5566"
  } : C{
    "#F4F6F8", "#FFFFFF", "#EEF1F4", "#E0E5EC", "#CCE0F5", "#3A4252", "#1A1E25",
    "#FFFFFF", "#D8DEE5", "#E8ECF0", "#1A1E25", "#5A6270", "#7A8290",
    "#FBFCFD", "#C8CDD6", "#1E6BB8", "#E8ECF0", "#DCE2EA"
  };
  setStyleSheet(QString(R"(
    QDialog#settingsPopup { background: %1; }

    QWidget#settingsBody { background: %1; }
    QWidget#settingsNavCol {
      background: %3; border-right: 1px solid %9;
    }

    QListWidget#settingsNav {
      background: %3; border: none;
      outline: none; padding: 14px 6px; color: %6; font-size: 13px;
    }
    QStackedWidget#settingsPages { background: %1; }
    QListWidget#settingsNav::item {
      padding: 9px 12px; margin: 1px 6px; border-radius: 7px;
    }
    QListWidget#settingsNav::item:hover { background: %4; color: %7; }
    QListWidget#settingsNav::item:selected { background: %5; color: %7; }

    QLabel#settingsPageHeader {
      color: %11; font-size: 26px; font-weight: 700;
      padding: 0 0 6px 0; letter-spacing: -0.2px;
    }
    QFrame#settingsCard {
      background-color: %8; border: none; border-radius: 10px;
    }
    QLabel#settingsCardTitle { color: %11; font-size: 14px; font-weight: 600; }
    QLabel#settingsCardSubtitle { color: %12; font-size: 12px; padding-bottom: 4px; }
    QLabel#settingsRowLabel { color: %11; font-size: 13px; }
    QLabel#settingsRowHint  { color: %13; font-size: 11px; }
    QLabel#settingsThemeName { color: %11; font-size: 12px; }
    QToolButton#settingsThemeTile {
      background-color: transparent; color: %11; border: 1px solid %15;
      border-radius: 12px; padding: 14px 8px; font-size: 12px; font-weight: 500;
    }
    QToolButton#settingsThemeTile:hover {
      background-color: rgba(127,140,160,0.10);
    }
    QToolButton#settingsThemeTile:checked {
      background-color: rgba(91,155,213,0.18); border: 2px solid %16; color: %11;
    }
    QLabel#settingsHint { color: %12; font-size: 11px; }
    QLabel { color: %11; }

    QLineEdit#settingsLineEdit, QLineEdit {
      background: %14; color: %11; border: 1px solid %15; border-radius: 10px;
      padding: 7px 12px; min-height: 22px; font-size: 13px;
      selection-background-color: %16; selection-color: white;
    }
    QLineEdit:focus { border: 1px solid %16; }
    QComboBox {
      background: %14; color: %11; border: 1px solid %15; border-radius: 10px;
      padding: 7px 12px; min-height: 22px; font-size: 13px;
    }
    QComboBox:focus { border: 1px solid %16; }
    QComboBox:disabled { color: %12; }
    QComboBox::drop-down {
      width: 24px; border: none; subcontrol-origin: padding; subcontrol-position: right center;
    }
    QComboBox::down-arrow {
      width: 12px; height: 12px; margin-right: 4px;
      image: url(:/assets/images/icons/chevron-down.svg);
    }
    QComboBox QAbstractItemView {
      background: %14; color: %11; border: 1px solid %15;
      selection-background-color: %16; selection-color: white;
    }
    /* Outline-only buttons: transparent fill, themed border, themed text.
       Reads as "subtle action" in both themes and stays legible without
       fighting the card bg. */
    QPushButton#settingsBrowseBtn, QPushButton#settingsResetBtn {
      background-color: transparent; color: %11; border: 1px solid %15;
      border-radius: 8px; padding: 7px 16px; font-size: 12px; font-weight: 600;
    }
    QPushButton#settingsResetBtn { margin: 0 12px 4px 12px; min-height: 28px; }
    QPushButton#settingsBrowseBtn:hover, QPushButton#settingsResetBtn:hover {
      background-color: rgba(127,140,160,0.12); border-color: %16;
    }
    QPushButton#settingsBrowseBtn:enabled, QPushButton#settingsResetBtn:enabled { color: %11; }
    QPushButton#settingsBrowseBtn:focus, QPushButton#settingsResetBtn:focus { color: %11; outline: none; }
    QPushButton#settingsUpdateBtn {
      background-color: transparent; color: %11; border: 1px solid %15;
      border-radius: 14px; padding: 6px 16px; font-size: 12px; font-weight: 600;
      min-height: 22px;
    }
    QPushButton#settingsUpdateBtn:hover {
      background-color: rgba(127,140,160,0.12); border-color: %16;
    }
    QPushButton#settingsUpdateBtn:disabled {
      color: %12; background-color: transparent; border: 1px solid %15;
    }
    QLabel#settingsVersionChip {
      background: %14; color: %11; border: 1px solid %15; border-radius: 12px;
      padding: 4px 12px; font-size: 11px; font-weight: 500; min-height: 18px;
    }
  )").arg(c.bg, c.panelBg, c.navBg, c.navHover, c.navActive, c.navText, c.navActiveText,
          c.cardBg, c.cardBd, c.titleBg, c.text, c.sub, c.hint,
          c.inputBg, c.inputBd, c.focusBd, c.btnBg, c.btnHover));
}
