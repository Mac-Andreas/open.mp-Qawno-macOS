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

#include "SyntaxHighlighter.h"

SyntaxHighlighter::ColorScheme SyntaxHighlighter::defaultColorScheme = {
  Qt::darkBlue,
  Qt::blue,
  Qt::darkGreen,
  Qt::darkGreen,
  Qt::black,
  Qt::darkMagenta,
  Qt::darkMagenta,
  Qt::darkRed,
  QColor(0x906040)
};

SyntaxHighlighter::ColorScheme SyntaxHighlighter::darkModeColorScheme = {
  QColor(0xABB2BF),
  QColor(0x10B1FE),
  QColor(0x636D83),
  QColor(0x636D83),
  QColor(0xCCCCCC),
  QColor(0xFF78F8),
  QColor(0xFF78F8),
  QColor(0xF9C859),
  QColor(0x9F71CA)
};

SyntaxHighlighter::ColorScheme SyntaxHighlighter::colorSchemeFor(Language lang, bool dark) {
  // Per-language palettes — each one returns light/dark variants so the
  // chosen colour theme follows the app's appearance setting automatically.
  // `QColor(int)` is ambiguous between QRgb / const char*, so use string hex
  // literals to keep the call site unambiguous.
  auto C = [](const char* s) { return QColor(QString::fromLatin1(s)); };
  switch (lang) {
    case Language::Pawn:
      return dark ? darkModeColorScheme : defaultColorScheme;
    case Language::Cpp:
      return dark
          ? ColorScheme{ C("#D4D4D4"), C("#569CD6"), C("#6A9955"), C("#6A9955"),
                         C("#9CDCFE"), C("#B5CEA8"), C("#CE9178"), C("#CE9178"),
                         C("#C586C0") }
          : ColorScheme{ C("#000000"), C("#0000FF"), C("#008000"), C("#008000"),
                         C("#001080"), C("#098658"), C("#A31515"), C("#A31515"),
                         C("#AF00DB") };
    case Language::Python:
      return dark
          ? ColorScheme{ C("#E6E6E6"), C("#FF7B72"), C("#8B949E"), C("#8B949E"),
                         C("#79C0FF"), C("#79C0FF"), C("#A5D6FF"), C("#A5D6FF"),
                         C("#FFA657") }
          : ColorScheme{ C("#24292F"), C("#CF222E"), C("#6E7781"), C("#6E7781"),
                         C("#0550AE"), C("#0550AE"), C("#0A3069"), C("#0A3069"),
                         C("#953800") };
    case Language::JavaScript:
      return dark
          ? ColorScheme{ C("#EEFFFF"), C("#C792EA"), C("#546E7A"), C("#546E7A"),
                         C("#82AAFF"), C("#F78C6C"), C("#C3E88D"), C("#C3E88D"),
                         C("#FFCB6B") }
          : ColorScheme{ C("#383A42"), C("#A626A4"), C("#A0A1A7"), C("#A0A1A7"),
                         C("#4078F2"), C("#986801"), C("#50A14F"), C("#50A14F"),
                         C("#C18401") };
    case Language::Rust:
      return dark
          ? ColorScheme{ C("#E6E6E6"), C("#FF7733"), C("#8B949E"), C("#8B949E"),
                         C("#DCDCAA"), C("#B5CEA8"), C("#CE9178"), C("#CE9178"),
                         C("#4EC9B0") }
          : ColorScheme{ C("#24292F"), C("#D2991D"), C("#6E7781"), C("#6E7781"),
                         C("#795E26"), C("#098658"), C("#A31515"), C("#A31515"),
                         C("#267F99") };
  }
  return dark ? darkModeColorScheme : defaultColorScheme;
}

const char* SyntaxHighlighter::languageName(Language lang) {
  switch (lang) {
    case Language::Pawn:       return "Pawn";
    case Language::Cpp:        return "C/C++";
    case Language::Python:     return "Python";
    case Language::JavaScript: return "JavaScript";
    case Language::Rust:       return "Rust";
  }
  return "Pawn";
}

SyntaxHighlighter::SyntaxHighlighter(QObject *parent)
  : QSyntaxHighlighter(parent)
{
  colorScheme_ = defaultColorScheme;

  keywords_
    << "@"
    << "@foreign"
    << "@global"
    << "@hook"
    << "@ptask"
    << "@remote"
    << "@return"
    << "@task"
    << "@test"
    << "@timer"
    << "_"
    << "__addressof"
    << "__emit"
    << "__nameof"
    << "__pragma"
    << "assert"
    << "break"
    << "case"
    << "char"
    << "const"
    << "continue"
    << "decl"
    << "default"
    << "defer"
    << "defined"
    << "do"
    << "else"
    << "enum"
    << "exit"
    << "false"
    << "final"
    << "for"
    << "foreach"
    << "foreign"
    << "forward"
    << "global"
    << "goto"
    << "if"
    << "inline"
    << "hook"
    << "native"
    << "new"
    << "operator"
    << "ptask"
    << "public"
    << "repeat"
    << "return"
    << "sizeof"
    << "sleep"
    << "state"
    << "static"
    << "stock"
    << "stop"
    << "switch"
    << "tagof"
    << "task"
    << "timer"
    << "true"
    << "using"
    << "while"
    << "yield";
}

SyntaxHighlighter::~SyntaxHighlighter() {
  // nothing
}

const SyntaxHighlighter::ColorScheme &SyntaxHighlighter::colorScheme() const {
  return colorScheme_;
}

void SyntaxHighlighter::setColorScheme(const ColorScheme &scheme) {
  colorScheme_ = scheme;
}

bool SyntaxHighlighter::isIdentifierFirstChar(QChar c) {
  return c.isLetter() || c == '_' || c == '@';
}

bool SyntaxHighlighter::isIdentifierChar(QChar c) {
  return isIdentifierFirstChar(c) || c.isDigit();
}

bool SyntaxHighlighter::isHexDigit(QChar c) {
  if (c.isDigit()) {
    return true;
  }
  char cc = c.toLatin1();
  return (cc >= 'a' && cc <= 'f') || (cc >= 'A' && cc <= 'F') || cc == 'x';
}

bool SyntaxHighlighter::isKeyword(const QString &s) {
  return keywords_.contains(s);
}

void SyntaxHighlighter::highlightBlock(const QString &text) {
  setFormat(0, text.length(), colorScheme_.defaultColor);

  enum State {
    Unknown = -1,
    CommentBegin,
    Comment,
    CommentEnd,
    Identifier,
    NumericLiteral,
    CharacterLiteral,
    StringLiteral,
    Preprocessor,
    PreprocessorNextLine
  };

  State state = (State)previousBlockState();

  for (int i = 0; i < text.length(); ++i) {
    switch (state) {
    case Comment:
      if (text[i] == '*') {
        state = CommentEnd;
      }
      setFormat(i, 1, colorScheme_.cComment);
      break;
    case CommentBegin:
      if (text[i] == '/') {
        setFormat(i - 1, text.length() - i + 1, colorScheme_.cppComment);
        goto end;
      } else if (text[i] == '*') {
        setFormat(i - 1, 2, colorScheme_.cComment);
        state = Comment;
      } else {
        state = Unknown;
      }
      break;
    case CommentEnd:
      setFormat(i, 1, colorScheme_.cComment);
      if (text[i] == '/')
      {
        state = Unknown;
      } else if (text[i] == '*') {
        state = CommentEnd;
      } else {
        state = Comment;
      }
      break;
    case Identifier:
      if (isIdentifierChar(text[i])) {
        setFormat(i, 1, colorScheme_.identifier);
      } else {
        --i;
        QString ident;
        int start = i;
        while (start >= 0 && isIdentifierChar(text[start])) {
          ident.prepend(text[start--]);
        }
        if (isKeyword(ident)) {
          setFormat(start + 1, ident.length(), colorScheme_.keyword);
        }
        state = Unknown;
      }
      break;
    case NumericLiteral:
      if (text[i].isDigit() || isHexDigit(text[i]) || text[i] == '.' || text[i] == '_') {
        setFormat(i, 1, colorScheme_.number);
      } else {
        --i;
        state = Unknown;
      }
      break;
    case CharacterLiteral:
      setFormat(i, 1, colorScheme_.character);
      if (text[i] == '\'') {
        state = Unknown;
      }
      break;
    case StringLiteral:
      setFormat(i, 1, colorScheme_.string);
      if (text[i] == '\"') {
        state = Unknown;
      }
      break;
    case Preprocessor:
      if (text[i] == '\\') {
        state = PreprocessorNextLine;
        goto end;
      } else {
        if (text[i].isSpace()) {
          state = Unknown;
        } else {
          setFormat(i, 1, colorScheme_.preprocessor);
        }
      }
      break;
    case PreprocessorNextLine:
      break;
    default:
      if (text[i] == '\'') {
        state = CharacterLiteral;
        setFormat(i, 1, colorScheme_.character);
      } else if (isIdentifierFirstChar(text[i])) {
        state = Identifier;
        setFormat(i, 1, colorScheme_.identifier);
      } else if (text[i].isDigit()) {
        setFormat(i, 1, colorScheme_.number);
        state = NumericLiteral;
      } else if (text[i] == '/') {
        state = CommentBegin;
      } else if (text[i] == '\"') {
        state = StringLiteral;
        setFormat(i, 1, colorScheme_.string);
      } else if (text[i] == '#') {
        state = Preprocessor;
        setFormat(i, 1, colorScheme_.preprocessor);
      } else if (text[i] == '<') {
        if (text.contains("#include") || text.contains("#tryinclude")) {
          int count = 0;
          while (count < text.length()) {
            ++count;
          }
          setFormat(i, count, colorScheme_.string);
          i += count;
        }
      }
      break;
    }
  }

end:
  // Some styles automatically end at the end of a line.
  switch (state) {
  case Identifier: {
    QString ident;
    int start = text.length() - 1;
    while (start >= 0 && isIdentifierChar(text[start])) {
      ident.prepend(text[start--]);
    }
    if (isKeyword(ident)) {
      setFormat(start + 1, ident.length(), colorScheme_.keyword);
    }
    setCurrentBlockState((int)Unknown);
    break;
  }
  case CommentBegin:
  case NumericLiteral:
  case Preprocessor:
  case CharacterLiteral:
    setCurrentBlockState((int)Unknown);
    break;
  case Unknown:
  case Comment:
  case CommentEnd:
  case StringLiteral:
  case PreprocessorNextLine:
    setCurrentBlockState((int)state);
    break;
  }
}
