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

#ifndef FINDDIALOG_H
#define FINDDIALOG_H

#include <QDialog>
#include <QPoint>
#include <QString>

class QLineEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class QWidget;
class IosToggle;

// Frameless, non-modal Find & Replace popup. Single persistent instance per
// MainWindow: clicking Find / Replace / Replace All emits signals without
// closing the popup, and a status banner reports the outcome inline
// ("Replaced N occurrences"). Draggable from its custom title bar; the X
// button hides the popup.
class FindDialog : public QDialog {
  Q_OBJECT

 public:
  explicit FindDialog(QWidget* parent = nullptr);
  ~FindDialog() override;

  QString findWhatText() const;
  QString replaceText() const;
  bool matchCase() const;
  bool matchWholeWords() const;
  bool useRegExp() const;
  bool searchBackwards() const;

  void focusFindField();
  void setStatus(const QString& message, bool error = false);
  void clearStatus();
  void applyTheme(bool dark);

 signals:
  void findRequested();
  void replaceRequested();
  void replaceAllRequested();

 protected:
  bool eventFilter(QObject* obj, QEvent* ev) override;
  void showEvent(QShowEvent* ev) override;

 private:
  QWidget* titleBar_ = nullptr;
  QLabel* titleLabel_ = nullptr;
  QPushButton* closeBtn_ = nullptr;
  QLineEdit* findEdit_ = nullptr;
  QLineEdit* replaceEdit_ = nullptr;
  IosToggle* matchCase_ = nullptr;
  IosToggle* matchWhole_ = nullptr;
  IosToggle* useRegexp_ = nullptr;
  IosToggle* searchBack_ = nullptr;
  QPushButton* findBtn_ = nullptr;
  QPushButton* replaceBtn_ = nullptr;
  QPushButton* replaceAllBtn_ = nullptr;
  QLabel* statusLabel_ = nullptr;

  bool dragging_ = false;
  QPoint dragOffset_;
};

#endif // FINDDIALOG_H
