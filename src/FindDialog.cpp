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

#include "FindDialog.h"
#include "IosToggle.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

FindDialog::FindDialog(QWidget* parent) : QDialog(parent) {
  // Frameless + translucent: native-macOS-style rounded popup, stays-on-top
  // of the parent so it doesn't slip behind the editor.
  setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, false);
  setObjectName("findPopup");

  auto root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  // ----- title bar (macOS traffic lights + centred title) -----
  titleBar_ = new QWidget(this);
  titleBar_->setObjectName("findTitleBar");
  titleBar_->setFixedHeight(30);
  titleBar_->installEventFilter(this);
  auto tlay = new QHBoxLayout(titleBar_);
  tlay->setContentsMargins(12, 0, 12, 0);
  tlay->setSpacing(6);

  closeBtn_ = new QPushButton(titleBar_);
  closeBtn_->setObjectName("findCloseBtn");
  closeBtn_->setFixedSize(12, 12);
  closeBtn_->setCursor(Qt::PointingHandCursor);
  closeBtn_->setToolTip(tr("Close"));
  connect(closeBtn_, &QPushButton::clicked, this, &QDialog::hide);
  tlay->addWidget(closeBtn_);
  auto* dotY = new QLabel(titleBar_); dotY->setObjectName("findTrafficYellow");
  dotY->setFixedSize(12, 12); tlay->addWidget(dotY);
  auto* dotG = new QLabel(titleBar_); dotG->setObjectName("findTrafficGreen");
  dotG->setFixedSize(12, 12); tlay->addWidget(dotG);

  titleLabel_ = new QLabel(tr("Search and Replace"), titleBar_);
  titleLabel_->setObjectName("findTitleLabel");
  titleLabel_->setAlignment(Qt::AlignCenter);
  tlay->addSpacing(8);
  tlay->addWidget(titleLabel_, 1);
  tlay->addSpacing(12 + 12 + 6 + 12 + 6 + 12);  // mirror left cluster
  root->addWidget(titleBar_);

  // ----- body -----
  auto body = new QWidget(this);
  body->setObjectName("findBody");
  auto v = new QVBoxLayout(body);
  v->setContentsMargins(14, 12, 14, 12);
  v->setSpacing(8);

  v->addWidget(new QLabel(tr("Find what:")));
  findEdit_ = new QLineEdit;
  findEdit_->setClearButtonEnabled(true);
  v->addWidget(findEdit_);

  v->addWidget(new QLabel(tr("Replace:")));
  replaceEdit_ = new QLineEdit;
  replaceEdit_->setClearButtonEnabled(true);
  v->addWidget(replaceEdit_);

  v->addSpacing(2);
  // iOS-style toggles per the user spec — easier to scan than checkboxes
  // and matches the rest of Qawno's Settings UI.
  auto addToggleRow = [v](const QString& label, IosToggle*& target) {
    auto row = new QHBoxLayout;
    row->setSpacing(10);
    auto* l = new QLabel(label);
    l->setObjectName("findOptLabel");
    row->addWidget(l, 1);
    target = new IosToggle;
    target->setFixedSize(34, 20);
    row->addWidget(target, 0, Qt::AlignVCenter);
    v->addLayout(row);
  };
  addToggleRow(tr("Match case"),              matchCase_);
  addToggleRow(tr("Match whole words only"),  matchWhole_);
  addToggleRow(tr("Use regular expressions"), useRegexp_);
  addToggleRow(tr("Search backwards"),        searchBack_);

  // Buttons (Find / Replace / Replace All — no Close: titlebar X handles that).
  auto brow = new QHBoxLayout;
  brow->addStretch(1);
  findBtn_ = new QPushButton(tr("Find"));
  findBtn_->setDefault(true);
  replaceBtn_ = new QPushButton(tr("Replace"));
  replaceAllBtn_ = new QPushButton(tr("Replace All"));
  brow->addWidget(findBtn_);
  brow->addWidget(replaceBtn_);
  brow->addWidget(replaceAllBtn_);
  v->addLayout(brow);

  // Status banner (dotted border, info icon, shows last action result).
  statusLabel_ = new QLabel;
  statusLabel_->setObjectName("findStatus");
  statusLabel_->setWordWrap(true);
  statusLabel_->setVisible(false);
  v->addWidget(statusLabel_);

  root->addWidget(body, 1);

  // Restore persisted state.
  QSettings settings;
  findEdit_->setText(settings.value("FindText").toString());
  replaceEdit_->setText(settings.value("ReplaceText").toString());
  matchCase_->setChecked(settings.value("FindMatchCase").toBool());
  matchWhole_->setChecked(settings.value("FindMatchWholeWords").toBool());
  searchBack_->setChecked(settings.value("FindSearchBackwards").toBool());
  useRegexp_->setChecked(settings.value("FindUseRegexp").toBool());

  connect(findBtn_,       &QPushButton::clicked, this, [this] {
    clearStatus();
    emit findRequested();
  });
  connect(replaceBtn_,    &QPushButton::clicked, this, [this] {
    clearStatus();
    emit replaceRequested();
  });
  connect(replaceAllBtn_, &QPushButton::clicked, this, [this] {
    clearStatus();
    emit replaceAllRequested();
  });
  // Enter on the find/replace field also triggers Find — same as the button.
  connect(findEdit_,    &QLineEdit::returnPressed, this, [this] {
    clearStatus();
    emit findRequested();
  });
  connect(replaceEdit_, &QLineEdit::returnPressed, this, [this] {
    clearStatus();
    emit findRequested();
  });
  // Esc inside the popup hides it (mirrors the X button).
  auto esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(esc, &QShortcut::activated, this, &QDialog::hide);

  setMinimumWidth(380);
  applyTheme(palette().window().color().lightness() < 128);
}

FindDialog::~FindDialog() {
  QSettings settings;
  settings.setValue("FindText", findEdit_->text());
  settings.setValue("ReplaceText", replaceEdit_->text());
  settings.setValue("FindMatchCase", matchCase_->isChecked());
  settings.setValue("FindMatchWholeWords", matchWhole_->isChecked());
  settings.setValue("FindSearchBackwards", searchBack_->isChecked());
  settings.setValue("FindUseRegexp", useRegexp_->isChecked());
}

QString FindDialog::findWhatText() const { return findEdit_->text(); }
QString FindDialog::replaceText()  const { return replaceEdit_->text(); }
bool FindDialog::matchCase()       const { return matchCase_->isChecked(); }
bool FindDialog::matchWholeWords() const { return matchWhole_->isChecked(); }
bool FindDialog::useRegExp()       const { return useRegexp_->isChecked(); }
bool FindDialog::searchBackwards() const { return searchBack_->isChecked(); }

void FindDialog::focusFindField() {
  findEdit_->setFocus();
  findEdit_->selectAll();
}

void FindDialog::setStatus(const QString& message, bool error) {
  // Info-style "(i)" prefix matches the dotted notification look the user asked
  // for. Errors get a warning tint.
  statusLabel_->setText(QStringLiteral("ⓘ  ") + message);
  statusLabel_->setProperty("error", error);
  statusLabel_->style()->unpolish(statusLabel_);
  statusLabel_->style()->polish(statusLabel_);
  statusLabel_->setVisible(true);
  adjustSize();
}

void FindDialog::clearStatus() {
  statusLabel_->clear();
  statusLabel_->setVisible(false);
  adjustSize();
}

void FindDialog::applyTheme(bool dark) {
  const char* bodyBg     = dark ? "#21262D" : "#FFFFFF";
  const char* bodyBorder = dark ? "#3A3F4B" : "#C8CDD6";
  const char* titleBg    = dark ? "#2D333D" : "#E8ECF0";
  const char* titleFg    = dark ? "#C8CDD6" : "#3A4252";
  const char* inputBg    = dark ? "#1B1F25" : "#FBFCFD";
  const char* inputBd    = dark ? "#3A3F4B" : "#C8CDD6";
  const char* labelFg    = dark ? "#E5E9EF" : "#1A1E25";
  const char* statusBg   = dark ? "#1F242B" : "#F1F5FA";
  const char* statusFg   = dark ? "#C8CDD6" : "#3A4252";
  const char* statusBd   = dark ? "#5A6678" : "#9AA5B8";
  const char* statusErrFg= dark ? "#FFB4A8" : "#A33A2A";
  const char* statusErrBd= dark ? "#A33A2A" : "#A33A2A";
  const char* btnBg      = dark ? "#3D7CB8" : "#1E6BB8";
  setStyleSheet(QString(R"(
    QDialog#findPopup { background: %1; border: 1px solid %2; border-radius: 12px; }
    QWidget#findTitleBar {
      background: %3;
      border-top-left-radius: 12px; border-top-right-radius: 12px;
      border-bottom: 1px solid %2;
    }
    QLabel#findTitleLabel { color: %4; font-weight: 600; font-size: 13px; }
    QLabel#findOptLabel { color: %7; font-size: 12px; }
    QPushButton#findCloseBtn {
      background: #FF5F57; border: 1px solid #E0443E; border-radius: 6px;
    }
    QPushButton#findCloseBtn:hover { background: #FF7268; }
    QLabel#findTrafficYellow {
      background: #FEBC2E; border: 1px solid #DEA123; border-radius: 6px;
    }
    QLabel#findTrafficGreen {
      background: #28C840; border: 1px solid #1AAA29; border-radius: 6px;
    }
    QWidget#findBody { background: %1; border-bottom-left-radius: 12px; border-bottom-right-radius: 12px; }
    QLabel { color: %7; }
    QLineEdit {
      background: %5; color: %7; border: 1px solid %6;
      border-radius: 6px; padding: 6px 10px; font-size: 13px;
    }
    QLineEdit:focus { border: 1px solid %11; }
    QPushButton {
      background: %5; color: %7; border: 1px solid %6;
      border-radius: 6px; padding: 6px 14px; font-size: 12px;
    }
    QPushButton:hover { background: rgba(127,140,160,0.16); }
    QPushButton:default {
      background: %11; color: white; border: none;
    }
    QPushButton:default:hover { background: #5093D3; }
    QLabel#findStatus { background: %8; color: %9; border: 1px dotted %10; border-radius: 6px; padding: 6px 10px; font-size: 12px; }
    QLabel#findStatus[error="true"] { color: %12; border-color: %13; }
  )").arg(bodyBg, bodyBorder, titleBg, titleFg, inputBg, inputBd, labelFg,
          statusBg, statusFg, statusBd, btnBg, statusErrFg, statusErrBd));
}

bool FindDialog::eventFilter(QObject* obj, QEvent* ev) {
  if (obj != titleBar_) {
    return QDialog::eventFilter(obj, ev);
  }
  // Drag the popup by its title bar — keeps it inside the parent's visible
  // area when the parent is the MainWindow.
  if (ev->type() == QEvent::MouseButtonPress) {
    auto me = static_cast<QMouseEvent*>(ev);
    if (me->button() == Qt::LeftButton) {
      dragging_ = true;
      dragOffset_ = me->globalPosition().toPoint() - frameGeometry().topLeft();
      return true;
    }
  } else if (ev->type() == QEvent::MouseMove && dragging_) {
    auto me = static_cast<QMouseEvent*>(ev);
    QPoint target = me->globalPosition().toPoint() - dragOffset_;
    if (auto p = parentWidget()) {
      QRect bounds = p->geometry();
      // Convert parent-local bounds to global. Allow the popup's bottom edge to
      // touch the parent's bottom edge but keep the title bar visible.
      QPoint topLeft = p->mapToGlobal(QPoint(0, 0));
      QPoint bottomRight = p->mapToGlobal(QPoint(p->width(), p->height()));
      int minX = topLeft.x();
      int maxX = bottomRight.x() - width();
      int minY = topLeft.y();
      int maxY = bottomRight.y() - titleBar_->height();
      target.setX(qBound(minX, target.x(), maxX));
      target.setY(qBound(minY, target.y(), maxY));
      (void)bounds;
    }
    move(target);
    return true;
  } else if (ev->type() == QEvent::MouseButtonRelease) {
    dragging_ = false;
    return true;
  }
  return QDialog::eventFilter(obj, ev);
}

void FindDialog::showEvent(QShowEvent* ev) {
  QDialog::showEvent(ev);
  // Centre over the parent on first show.
  if (auto p = parentWidget()) {
    QRect pg = p->geometry();
    QPoint topLeft = p->mapToGlobal(QPoint(0, 0));
    int x = topLeft.x() + (pg.width() - width()) / 2;
    int y = topLeft.y() + 100;
    move(qMax(topLeft.x(), x), qMax(topLeft.y(), y));
  }
  focusFindField();
}
