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

#include <QPainter>
#include <QSettings>
#include <QTextEdit>
#include <QTextBlock>
#include <QTextCursor>

#include "EditorWidget.h"
#include "SyntaxHighlighter.h"

EditorLineNumberWidget::EditorLineNumberWidget(EditorWidget *editor)
  : QWidget(editor)
{
  connect(editor, SIGNAL(updateRequest(QRect, int)), SLOT(update(QRect, int)));
  connect(editor, SIGNAL(blockCountChanged(int)), SLOT(updateWidth(int)));
  connect(editor, SIGNAL(blockCountChanged(int)), SLOT(updateGeometry()));
}

EditorLineNumberWidget::~EditorLineNumberWidget() {
  // nothing
}

EditorWidget *EditorLineNumberWidget::editor() const {
  return static_cast<EditorWidget*>(parent());
}

QSize EditorLineNumberWidget::sizeHint() const {
  int lineCount = editor()->blockCount();
  QFont f = editor()->font();
  // Force tabular-numerals layout so a proportional fallback font (e.g. the
  // system UI font when "Courier New" is missing) still gives each digit the
  // same advance — keeps the gutter numbers right-aligned to a fixed column.
  f.setStyleHint(QFont::Monospace);
  f.setFixedPitch(true);
  QFontMetrics fm(f);
  // Measure the widest digit (8 is widest in most fonts) so we never crop
  // numbers like 1888.
  int digitWidth = 0;
  for (QChar c : QStringLiteral("0123456789")) {
    digitWidth = qMax(digitWidth, fm.horizontalAdvance(c));
  }
  int numDigits = QString::number(lineCount).length();
  int width = digitWidth * qMax(numDigits, 2) + 16;
  return QSize(width, 0);
}

void EditorLineNumberWidget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.fillRect(event->rect(), palette().window().color());

  // Use the editor's own (possibly zoomed) font so the numbers line up exactly
  // with each text row and stay crisp. Force fixed-pitch so the digits land
  // on a consistent grid even if the family falls back to a proportional one.
  QFont f = editor()->font();
  f.setStyleHint(QFont::Monospace);
  f.setFixedPitch(true);
  painter.setFont(f);
  const QColor fg = palette().windowText().color();
  const QColor fgCurrent = editor()->currentLineNumberColor_;
  const int currentLine = editor()->textCursor().blockNumber();

  QTextBlock block = editor()->firstVisibleBlock();
  QPointF contentOffset = editor()->contentOffset();
  qreal top = editor()->blockBoundingGeometry(block).translated(contentOffset).top();
  qreal bottom = top + editor()->blockBoundingRect(block).height();
  const int rightPad = 8;

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      const int n = block.blockNumber();
      painter.setPen(n == currentLine ? fgCurrent : fg);
      QRect rect(0, static_cast<int>(top),
                 width() - rightPad, static_cast<int>(bottom - top));
      painter.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, QString::number(n + 1));
    }
    block = block.next();
    top = bottom;
    bottom = top + editor()->blockBoundingRect(block).height();
  }
}

void EditorLineNumberWidget::resizeEvent(QResizeEvent *event) {
  Q_UNUSED(event);
  updateGeometry();
  updateWidth();
}

void EditorLineNumberWidget::update(const QRect &rect, int dy) {
  if (dy != 0) {
    scroll(0, dy);
  } else {
    QWidget::update(0, rect.y(), sizeHint().width(), rect.height());
  }
}

void EditorLineNumberWidget::updateWidth(int) {
  editor()->setViewportMargins(sizeHint().width(), 0, 0, 0);
}

void EditorLineNumberWidget::updateGeometry() {
  QRect cr = editor()->contentsRect();
  setGeometry(cr.left(), cr.top(), sizeHint().width(), cr.height());
}

static QFont defaultFont() {
  #ifdef Q_OS_WIN32
    QFont font("Courier New");
  #elif defined(Q_OS_MAC)
    QFont font("Menlo");
  #else
    QFont font("Monospace");
  #endif
  font.setStyleHint(QFont::Monospace);
  font.setPointSize(13);
  return font;
}

EditorWidget::EditorWidget(QWidget *parent)
  : QPlainTextEdit(parent),
    lineNumberArea_(this),
    highlighter_(this)
{
  QSettings settings;

  QFont font = defaultFont();
  font.fromString(settings.value("EditorFont", font).toString());
  setFont(font);

  setTabWidth(tabWidth_);
  setIndentWidth(indentWidth_);

  setLineWrapMode(NoWrap);
  setUndoRedoEnabled(true);
  setCursorWidth(2);

  QPalette palette;
  palette.setColor(lineNumberArea_.backgroundRole(), Qt::lightGray);
  palette.setColor(lineNumberArea_.foregroundRole(), Qt::black);
  lineNumberArea_.setPalette(palette);

  highlighter_.setDocument(document());

  connect(this, SIGNAL(cursorPositionChanged()), SLOT(highlightCurrentLine()));
  highlightCurrentLine();
}

EditorWidget::~EditorWidget() {
  QSettings settings;
}

int EditorWidget::tabWidth() const {
  return tabWidth_;
}

void EditorWidget::setTabWidth(int width) {
  tabWidth_ = width;
  setTabStopDistance(static_cast<double>(fontMetrics().horizontalAdvance(' ')) * static_cast<double>(width));
}

int EditorWidget::indentWidth() const {
  return indentWidth_;
}

void EditorWidget::setIndentWidth(int width) {
  indentWidth_ = width;
}

void EditorWidget::toggleDarkMode(bool toggle) {
  QColor base, text, gutterBg, gutterFg;
  // Resolve the user's chosen language → scheme for the active theme. Falls
  // back to Pawn when the setting hasn't been touched yet (matches the
  // historical default).
  const QString lang = QSettings().value("SyntaxLanguage", "Pawn").toString();
  auto resolveLang = [](const QString& s) {
    if (s == "Cpp")        return SyntaxHighlighter::Language::Cpp;
    if (s == "Python")     return SyntaxHighlighter::Language::Python;
    if (s == "JavaScript") return SyntaxHighlighter::Language::JavaScript;
    if (s == "Rust")       return SyntaxHighlighter::Language::Rust;
    return SyntaxHighlighter::Language::Pawn;
  };
  highlighter_.setColorScheme(
      SyntaxHighlighter::colorSchemeFor(resolveLang(lang), toggle));
  if (toggle) {
    base = QColor(0x282C34);
    text = QColor(0xABB2BF);
    gutterBg = QColor(0x282C34);
    gutterFg = QColor(0x858585);
    currentLineNumberColor_ = QColor(0xFFFFFF);
  } else {
    base = QColor(0xFFFFFF);
    text = QColor(0x1A1E25);
    gutterBg = QColor(0xFFFFFF);
    gutterFg = QColor(0x6E7681);
    currentLineNumberColor_ = QColor(0x1A1E25);
  }
  usingDarkMode = toggle;
  // Editor surface: stylesheet on the viewport only so it doesn't cascade
  // into the line-number child widget.
  setStyleSheet(QString());
  QPalette ep = palette();
  ep.setColor(QPalette::Base, base);
  ep.setColor(QPalette::Text, text);
  setPalette(ep);
  viewport()->setStyleSheet(
      QString("QWidget { background-color: %1; color: %2; }")
          .arg(base.name(), text.name()));
  // Gutter: drive colors through both the palette AND a direct stylesheet so
  // Qt repaints reliably across theme switches (palette-only sometimes
  // leaves stale pixels until the next resize).
  QPalette gp;
  gp.setColor(lineNumberArea_.backgroundRole(), gutterBg);
  gp.setColor(lineNumberArea_.foregroundRole(), gutterFg);
  lineNumberArea_.setAutoFillBackground(true);
  lineNumberArea_.setPalette(gp);
  lineNumberArea_.setStyleSheet(
      QString("QWidget { background-color: %1; color: %2; }")
          .arg(gutterBg.name(), gutterFg.name()));
  highlighter_.rehighlight();
  lineNumberArea_.QWidget::update();
  highlightCurrentLine();
  viewport()->update();
}

void EditorWidget::jumpToLine(long line) {
  if (line > 0 && line <= blockCount()) {
    QTextCursor cursor = textCursor();
    int pos = document()->findBlockByLineNumber(line - 1).position();
    cursor.setPosition(pos);
    setTextCursor(cursor);
  }
}

void EditorWidget::resizeEvent(QResizeEvent *event) {
  Q_UNUSED(event);
  // FIXME: This should be done somwhere inside EditorLineNumberWidget.
  lineNumberArea_.updateGeometry();
}

void EditorWidget::keyPressEvent(QKeyEvent *event) {
  QTextCursor cursor = textCursor();
  bool removePrevChar = false;

  switch (event->key()) {
    case Qt::Key_Tab:
      if (cursor.hasSelection()) {
        indentSelectedText(cursor);
      } else {
        indentBlock(cursor);
      }
      event->accept();
      return;
    case Qt::Key_Backtab:
      //if (cursor.hasSelection()) {
        unindentSelectedText(cursor);
      //}
      event->accept();
      return;
    case Qt::Key_Enter:
    case Qt::Key_Return: {
      autoIndentOnNewLineInsertion(cursor);
      removePrevChar = true;
      event->accept();
      break;
    }
    case Qt::Key_BraceRight: {
      autoUnindentOnClosingBraceInsertion(cursor);
      event->accept();
      return;
    }
  }

  QPlainTextEdit::keyPressEvent(event);
  if (removePrevChar) {
    textCursor().deletePreviousChar();
  }
}

void EditorWidget::highlightCurrentLine() {
  // VS Code-style: no full-line highlight, just the thin caret. Clear any
  // existing extra selections so the current line is not painted over.
  setExtraSelections({});
}

void EditorWidget::editSelectedText(QTextCursor cursor,
                             std::function<void(QTextCursor cursor)> callback) {
  int start = cursor.selectionStart();
  int end = cursor.selectionEnd();

  cursor.setPosition(start);
  int startBlock = cursor.blockNumber();

  cursor.setPosition(end);
  int endBlock = cursor.blockNumber();

  cursor.setPosition(start);
  cursor.beginEditBlock();

  for (int i = 0; i <= endBlock - startBlock; i++) {
    cursor.movePosition(QTextCursor::StartOfBlock);
    callback(cursor);
    cursor.movePosition(QTextCursor::NextBlock);
  }

  cursor.endEditBlock();
}

void EditorWidget::moveSelection(int distance) {
  QTextCursor cursor = textCursor();
  int start = cursor.selectionStart();
  int end = cursor.selectionEnd();
  cursor.setPosition(start, QTextCursor::MoveAnchor);
  cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
  int offset = cursor.selectionStart();
  cursor.setPosition(end, QTextCursor::KeepAnchor);
  cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
  cursor.beginEditBlock();
  QString text = cursor.selectedText();
  cursor.insertText("");
  while (distance < 0) {
    cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor);
    ++distance;
  }
  while (distance > 0) {
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
    --distance;
  }
  cursor.insertText(text);
  offset = cursor.position() - text.length() - offset;
  cursor.setPosition(offset + start, QTextCursor::MoveAnchor);
  cursor.setPosition(offset + end, QTextCursor::KeepAnchor);
  cursor.endEditBlock();
  setTextCursor(cursor);
}

void EditorWidget::duplicateSelection(bool lines) {
  // Duplicate the line.
  QTextCursor cursor = textCursor();
  int start = cursor.selectionStart();
  int end = cursor.selectionEnd();
  cursor.beginEditBlock();
  if (lines || !cursor.hasSelection())
  {
    cursor.setPosition(start, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
  }
  QString text = cursor.selectedText();
  cursor.setPosition(end);
  cursor.insertText(text);
  cursor.setPosition(start, QTextCursor::MoveAnchor);
  cursor.setPosition(end, QTextCursor::KeepAnchor);
  cursor.endEditBlock();
  setTextCursor(cursor);
}

void EditorWidget::deleteSelection() {
  // Extend the selection to cover the whole of the lines.
  QTextCursor cursor = textCursor();
  int start = cursor.selectionStart();
  int end = cursor.selectionEnd();
  cursor.setPosition(start, QTextCursor::MoveAnchor);
  cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
  cursor.setPosition(end, QTextCursor::KeepAnchor);
  cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
  cursor.insertText("");
}

void EditorWidget::indentBlock(QTextCursor cursor) {
  cursor.insertText("\t");
}

void EditorWidget::indentSelectedText(QTextCursor cursor) {
  editSelectedText(cursor, [this](QTextCursor cursor) {
    indentBlock(cursor);
  });
}

void EditorWidget::unindentSelectedText(QTextCursor cursor) {
  editSelectedText(cursor, [this](QTextCursor cursor) {
    int removedChars = 0;
    int blockStart = cursor.position();
    do {
      cursor.setPosition(blockStart + 1, QTextCursor::KeepAnchor);
      QString text = cursor.selectedText();
      if (text == "\t") {
        cursor.removeSelectedText();
        removedChars += tabWidth_;
      } else {
        break;
      }
    } while (removedChars < indentWidth_);
  });
}

void EditorWidget::autoIndentOnNewLineInsertion(QTextCursor cursor) {
  int currPos = cursor.position();
  cursor.setPosition(currPos - 1, QTextCursor::KeepAnchor);
  QString charBefore = cursor.selectedText();
  cursor.setPosition(currPos - cursor.positionInBlock() - 1, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();
  int indents = 0;
  if (charBefore == "{") {
    indents++;
  }
  indents += countIndents(line);
  cursor.setPosition(currPos);
  cursor.insertText("\r\n");
  while (indents) {
    indentBlock(cursor);
    indents--;
  }
}

void EditorWidget::autoUnindentOnClosingBraceInsertion(QTextCursor cursor) {
  int currPos = cursor.position();
  cursor.setPosition(currPos - cursor.positionInBlock(), QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();
  bool allTabs = isLineAllTabs(line);
  if (allTabs) {
    int indents = 0;
    indents += countIndents(line);
    if (indents != 0) {
      indents--;
    }
    while (indents) {
      indentBlock(cursor);
      indents--;
    }
  }
  else {
    cursor.setPosition(currPos);
  }
  cursor.insertText("}");
}

int EditorWidget::countIndents(QString line) {
  int indents = 0;
  for (int i = 0; i < line.length(); i++) {
    if (line[i].toLatin1() == '\t') {
      indents++;
    } else {
      break;
    }
  }
  return indents;
}

bool EditorWidget::isLineAllTabs(QString line) {
  bool allTabs = true;
  for (int i = 0; i < line.length(); i++) {
    if (line[i].toLatin1() != '\t') {
      allTabs = false;
      break;
    }
  }
  return allTabs;
}
