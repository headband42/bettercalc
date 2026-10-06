#include "backend.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHash>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QSize>
#include <cmath>
#include <limits>

using Engine::Token;
using Engine::TokenKind;

namespace {
const auto modeSetting = QStringLiteral("mode");
const auto angleSetting = QStringLiteral("scientific/angle");
const auto baseSetting = QStringLiteral("programmer/base");
const auto wordSizeSetting = QStringLiteral("programmer/wordSize");
const auto historySetting = QStringLiteral("history");

constexpr int historyLimit = 100;
constexpr int undoLimit = 200;

const QString xorSign(QChar(0x2295));
const QString nandSign(QChar(0x22BC));
const QString norSign(QChar(0x22BD));
const QString rolSign(QChar(0x21BA));
const QString rorSign(QChar(0x21BB));

const QStringList modeNames{QStringLiteral("basic"), QStringLiteral("scientific"),
                            QStringLiteral("programmer")};

bool isAsciiDigit(QChar c) {
    return c >= QLatin1Char('0') && c <= QLatin1Char('9');
}

int hexDigitValue(QChar c) {
    if (isAsciiDigit(c))
        return c.unicode() - '0';
    if (c >= QLatin1Char('A') && c <= QLatin1Char('F'))
        return c.unicode() - 'A' + 10;
    if (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
        return c.unicode() - 'a' + 10;
    return -1;
}

bool endsOperand(TokenKind kind) {
    return kind == TokenKind::Number || kind == TokenKind::Constant
        || kind == TokenKind::Close || kind == TokenKind::Postfix;
}

bool isSingleLiteral(const QList<Token> &tokens) {
    if (tokens.size() == 1)
        return tokens.first().kind == TokenKind::Number;
    return tokens.size() == 2 && tokens.first().kind == TokenKind::Prefix
        && tokens.first().text == QLatin1String("-") && tokens.last().kind == TokenKind::Number;
}

// An operand that a power can take as it stands, without parentheses: a
// single value, a parenthesized group or a function call — but not a signed
// value or another power.
bool simpleOperand(const QList<Token> &tokens, int start) {
    if (tokens.at(start).kind == TokenKind::Prefix)
        return false;
    int depth = 0;
    for (int i = start; i < tokens.size(); ++i) {
        const Token &token = tokens.at(i);
        if (token.kind == TokenKind::Open || token.kind == TokenKind::Function)
            ++depth;
        else if (token.kind == TokenKind::Close)
            --depth;
        else if (depth == 0 && token.kind == TokenKind::Binary)
            return false;
    }
    return true;
}

// The operand is exactly one parenthesized group, "(3+4)", with nothing
// after its closing parenthesis.
bool wholeGroup(const QList<Token> &tokens, int start) {
    if (tokens.at(start).kind != TokenKind::Open || tokens.last().kind != TokenKind::Close)
        return false;
    int depth = 0;
    for (int i = start; i < tokens.size(); ++i) {
        const TokenKind kind = tokens.at(i).kind;
        if (kind == TokenKind::Open || kind == TokenKind::Function)
            ++depth;
        else if (kind == TokenKind::Close && --depth == 0)
            return i == tokens.size() - 1;
    }
    return false;
}
}

bool Backend::Snapshot::operator==(const Snapshot &other) const {
    return expression == other.expression && justEvaluated == other.justEvaluated
        && evaluated == other.evaluated && result == other.result
        && integerResult == other.integerResult && error == other.error
        && base == other.base && wordSize == other.wordSize;
}

Backend::Backend(QObject *parent) : QObject(parent) {
    const QSettings settings;
    m_mode = qBound(int(Basic), settings.value(modeSetting, int(Basic)).toInt(), int(Programmer));

    const QString angle = settings.value(angleSetting).toString();
    if (angle == QLatin1String("RAD"))
        m_angle = Engine::AngleUnit::Radians;
    else if (angle == QLatin1String("GRAD"))
        m_angle = Engine::AngleUnit::Gradians;

    const int base = settings.value(baseSetting, 10).toInt();
    if (base == 2 || base == 8 || base == 10 || base == 16)
        m_base = base;
    const int wordSize = settings.value(wordSizeSetting, 64).toInt();
    if (wordSize == 8 || wordSize == 16 || wordSize == 32 || wordSize == 64)
        m_wordSize = wordSize;

    m_history = settings.value(historySetting).toList();
    refresh();
}

void Backend::setMode(int mode) {
    if (mode < Basic || mode > Programmer || mode == m_mode)
        return;

    // Basic and scientific share one engine, so the expression simply stays.
    // Crossing into or out of programmer mode carries the current value over
    // instead, as an integer one way and a decimal the other.
    if ((mode == Programmer) != programmer()) {
        QString literal;
        if (m_justEvaluated || (!m_expression.isEmpty() && m_hasCurrent)) {
            literal = mode == Programmer
                ? Engine::integerLiteral(toInteger(m_currentValue), m_base, m_wordSize)
                : QString::number(Engine::signExtend(m_currentInteger, m_wordSize));
        }
        m_expression = literal;
        m_justEvaluated = false;
        m_evaluated.clear();
        m_error.clear();
        m_lastPreview = QStringLiteral("0");
        m_undo.clear();
        m_redo.clear();
    }

    m_mode = mode;
    QSettings().setValue(modeSetting, m_mode);
    refresh();
    emit modeChanged();
}

QString Backend::angleUnit() const {
    switch (m_angle) {
    case Engine::AngleUnit::Radians:
        return QStringLiteral("RAD");
    case Engine::AngleUnit::Gradians:
        return QStringLiteral("GRAD");
    case Engine::AngleUnit::Degrees:
        break;
    }
    return QStringLiteral("DEG");
}

void Backend::setBase(int base) {
    if ((base != 2 && base != 8 && base != 10 && base != 16) || base == m_base)
        return;

    const Snapshot before = snapshot();
    m_expression = Engine::convertLiterals(m_expression, m_base, base, m_wordSize);
    m_evaluated = Engine::convertLiterals(m_evaluated, m_base, base, m_wordSize);
    m_base = base;
    m_lastPreview = Engine::formatInteger(m_currentInteger, m_base, m_wordSize, true);
    QSettings().setValue(baseSetting, m_base);
    record(before);
    emit settingsChanged();
    refresh();
}

void Backend::setWordSize(int wordSize) {
    if ((wordSize != 8 && wordSize != 16 && wordSize != 32 && wordSize != 64)
            || wordSize == m_wordSize)
        return;

    // Narrowing truncates every value to the new width, the way a cast would.
    const Snapshot before = snapshot();
    m_expression = Engine::convertLiterals(m_expression, m_base, m_base, wordSize);
    m_evaluated = Engine::convertLiterals(m_evaluated, m_base, m_base, wordSize);
    const quint64 mask = Engine::wordMask(wordSize);
    m_integerResult &= mask;
    m_integerAns &= mask;
    m_currentInteger &= mask;
    m_wordSize = wordSize;
    m_lastPreview = Engine::formatInteger(m_currentInteger, m_base, m_wordSize, true);
    QSettings().setValue(wordSizeSetting, m_wordSize);
    record(before);
    emit settingsChanged();
    refresh();
}

void Backend::cycleWordSize() {
    setWordSize(m_wordSize == 8 ? 64 : m_wordSize / 2);
}

void Backend::cycleAngleUnit() {
    switch (m_angle) {
    case Engine::AngleUnit::Degrees:
        m_angle = Engine::AngleUnit::Radians;
        break;
    case Engine::AngleUnit::Radians:
        m_angle = Engine::AngleUnit::Gradians;
        break;
    case Engine::AngleUnit::Gradians:
        m_angle = Engine::AngleUnit::Degrees;
        break;
    }
    QSettings().setValue(angleSetting, angleUnit());
    emit settingsChanged();
    refresh();
}

QString Backend::wordName() const {
    switch (m_wordSize) {
    case 8:
        return QStringLiteral("BYTE");
    case 16:
        return QStringLiteral("WORD");
    case 32:
        return QStringLiteral("DWORD");
    default:
        return QStringLiteral("QWORD");
    }
}

QString Backend::bits() const {
    QString text(64, QLatin1Char('0'));
    for (int bit = 0; bit < 64; ++bit) {
        if (m_currentInteger & (quint64(1) << bit))
            text[63 - bit] = QLatin1Char('1');
    }
    return text;
}

void Backend::setTextScale(qreal textScale) {
    if (qFuzzyCompare(m_textScale, textScale))
        return;
    m_textScale = textScale;
    emit textScaleChanged();
}

Engine::Syntax Backend::syntax() const {
    return programmer() ? Engine::Syntax::Programmer : Engine::Syntax::Scientific;
}

QList<Token> Backend::tokens() const {
    return Engine::tokenize(m_expression, syntax());
}

void Backend::pressKey(const QString &key) {
    const Snapshot before = snapshot();
    if (!applyKey(key))
        return;
    record(before);
    refresh();
}

void Backend::typeText(const QString &text) {
    const Snapshot before = snapshot();
    for (const QChar character : text)
        applyCharacter(character);
    record(before);
    refresh();
}

bool Backend::applyKey(const QString &key) {
    if (key == QLatin1String("clear")) {
        clearAll();
    } else if (key == QLatin1String("backspace")) {
        backspace();
    } else if (key == QLatin1String("=")) {
        equals();
    } else if (key == QLatin1String("sign")) {
        toggleUnary(QStringLiteral("-"));
    } else if (key == QLatin1String("(")) {
        beginEntry();
        m_expression += QLatin1Char('(');
    } else if (key == QLatin1String(")")) {
        closeParenthesis();
    } else if (key.size() == 1
               && (isAsciiDigit(key.at(0)) || (programmer() && hexDigitValue(key.at(0)) >= 0))) {
        appendDigit(key.at(0));
    } else if (key == QLatin1String(".")) {
        if (!programmer())
            appendDecimal();
    } else if (key == QLatin1String("+")) {
        appendBinary(QStringLiteral("+"));
    } else if (key == QLatin1String("-") || key == QStringLiteral("−")) {
        appendBinary(QStringLiteral("-"));
    } else if (key == QLatin1String("*") || key == QStringLiteral("×")) {
        appendBinary(QStringLiteral("*"));
    } else if (key == QLatin1String("/") || key == QStringLiteral("÷")) {
        appendBinary(QStringLiteral("/"));
    } else if (key == QLatin1String("mod")) {
        appendBinary(programmer() ? QStringLiteral("%") : QStringLiteral("mod"));
    } else if (key == QLatin1String("ans")) {
        appendConstant(programmer() ? QStringLiteral("@") : QStringLiteral("ans"));
    } else {
        return programmer() ? applyProgrammerKey(key) : applyScientificKey(key);
    }
    return true;
}

bool Backend::applyScientificKey(const QString &key) {
    if (key == QLatin1String("%") || key == QLatin1String("!")) {
        appendPostfix(key);
    } else if (key == QLatin1String("square")) {
        appendPostfix(m_second ? QStringLiteral("^3") : QStringLiteral("^2"));
    } else if (key == QLatin1String("inverse")) {
        appendPostfix(QStringLiteral("^-1"));
    } else if (key == QLatin1String("^")) {
        appendBinary(QStringLiteral("^"));
    } else if (key == QLatin1String("root")) {
        // y√x is x^(1/y): the radicand comes first, then the degree.
        appendBinary(QStringLiteral("^(1/"));
    } else if (key == QLatin1String("sqrt")) {
        applyFunction(m_second ? QStringLiteral("cbrt") : QStringLiteral("sqrt"));
    } else if (key == QLatin1String("sin") || key == QLatin1String("cos")
               || key == QLatin1String("tan")) {
        QString name = key;
        if (m_hyperbolic)
            name += QLatin1Char('h');
        if (m_second)
            name.prepend(QLatin1Char('a'));
        applyFunction(name);
    } else if (key == QLatin1String("ln") || key == QLatin1String("abs")) {
        applyFunction(key);
    } else if (key == QLatin1String("log")) {
        applyFunction(m_second ? QStringLiteral("log2") : QStringLiteral("log"));
    } else if (key == QLatin1String("exp")) {
        applyPowerPrefix(QStringLiteral("e^"));
    } else if (key == QLatin1String("pow10")) {
        applyPowerPrefix(m_second ? QStringLiteral("2^") : QStringLiteral("10^"));
    } else if (key == QLatin1String("pi") || key == QLatin1String("e")) {
        appendConstant(key);
    } else if (key == QLatin1String("ee")) {
        appendExponent();
    } else if (key == QLatin1String("rand")) {
        insertValue(Engine::formatNumber(QRandomGenerator::global()->generateDouble()));
    } else if (key == QLatin1String("mc") || key == QLatin1String("mr")
               || key == QLatin1String("ms") || key == QLatin1String("mplus")
               || key == QLatin1String("mminus")) {
        memory(key);
    } else if (key == QLatin1String("2nd")) {
        m_second = !m_second;
        emit settingsChanged();
    } else if (key == QLatin1String("hyp")) {
        m_hyperbolic = !m_hyperbolic;
        emit settingsChanged();
    } else if (key == QLatin1String("angle")) {
        cycleAngleUnit();
    } else {
        return false;
    }
    return true;
}

bool Backend::applyProgrammerKey(const QString &key) {
    static const QHash<QString, QString> operators{
        {QStringLiteral("and"), QStringLiteral("&")},
        {QStringLiteral("or"), QStringLiteral("|")},
        {QStringLiteral("xor"), xorSign},
        {QStringLiteral("nand"), nandSign},
        {QStringLiteral("nor"), norSign},
        {QStringLiteral("shl"), QStringLiteral("<<")},
        {QStringLiteral("shr"), QStringLiteral(">>")},
        {QStringLiteral("rol"), rolSign},
        {QStringLiteral("ror"), rorSign},
        {QStringLiteral("%"), QStringLiteral("%")},
    };

    if (operators.contains(key))
        appendBinary(operators.value(key));
    else if (key == QLatin1String("not"))
        toggleUnary(QStringLiteral("~"));
    else if (key == QLatin1String("word"))
        cycleWordSize();
    else
        return false;
    return true;
}

// Characters typed on the keyboard. Letters spell function names in the
// decimal modes and are hex digits in programmer mode.
void Backend::applyCharacter(QChar c) {
    if (c == QLatin1Char('(') || c == QLatin1Char(')') || c == QLatin1Char('=')) {
        applyKey(QString(c));
        return;
    }
    if (c == QLatin1Char('+') || c == QLatin1Char('-') || c == QLatin1Char('*')
            || c == QLatin1Char('/') || c == QChar(0x2212) || c == QChar(0x00D7)
            || c == QChar(0x00F7)) {
        applyKey(QString(c));
        return;
    }

    if (programmer()) {
        if (hexDigitValue(c) >= 0)
            appendDigit(c);
        else if (c == QLatin1Char('%'))
            appendBinary(QStringLiteral("%"));
        else if (c == QLatin1Char('^'))
            appendBinary(xorSign);
        else if (c == QLatin1Char('&') || c == QLatin1Char('|'))
            appendBinary(QString(c));
        else if (c == QLatin1Char('<'))
            appendBinary(QStringLiteral("<<"));
        else if (c == QLatin1Char('>'))
            appendBinary(QStringLiteral(">>"));
        else if (c == QLatin1Char('~'))
            toggleUnary(QStringLiteral("~"));
        return;
    }

    if (isAsciiDigit(c))
        appendDigit(c);
    else if (c == QLatin1Char('.') || c == QLatin1Char(','))
        appendDecimal();
    else if (c == QLatin1Char('^'))
        appendBinary(QStringLiteral("^"));
    else if (c == QLatin1Char('%') || c == QLatin1Char('!'))
        appendPostfix(QString(c));
    else if (c == QLatin1Char('E'))
        appendExponent();
    else if (c == QChar(0x03C0))
        appendConstant(QStringLiteral("pi"));
    else if (c == QChar(0x221A))
        applyFunction(QStringLiteral("sqrt"));
    else if ((c >= QLatin1Char('a') && c <= QLatin1Char('z'))
             || (c >= QLatin1Char('A') && c <= QLatin1Char('Z')))
        appendLetter(c.toLower());
}

void Backend::clearAll() {
    m_expression.clear();
    m_justEvaluated = false;
    m_evaluated.clear();
    m_error.clear();
    m_lastPreview = QStringLiteral("0");
}

// Typing a fresh value after = starts a new calculation rather than
// appending to the old one.
void Backend::beginEntry() {
    if (m_justEvaluated)
        clearAll();
    m_error.clear();
}

void Backend::appendDigit(QChar digit) {
    digit = digit.toUpper();
    if (programmer()) {
        const int value = hexDigitValue(digit);
        if (value < 0 || value >= m_base)
            return;
    } else if (!isAsciiDigit(digit)) {
        return;
    }

    beginEntry();
    const QList<Token> tokens = this->tokens();
    if (tokens.isEmpty() || tokens.last().kind != TokenKind::Number) {
        m_expression += digit;
        return;
    }

    const Token &last = tokens.last();
    QString candidate;
    if (programmer()) {
        candidate = last.text == QLatin1String("0") ? QString(digit) : last.text + digit;
        if (!literalFits(candidate, tokens))
            return;
    } else {
        const int e = last.text.indexOf(QLatin1Char('E'));
        if (e >= 0) {
            int exponentDigits = 0;
            for (int i = e + 1; i < last.text.size(); ++i)
                exponentDigits += isAsciiDigit(last.text.at(i)) ? 1 : 0;
            if (exponentDigits >= 3)
                return;
            candidate = last.text + digit;
        } else if (last.text == QLatin1String("0")) {
            candidate = QString(digit);
        } else {
            // Fifteen significant digits is what the display can show; typing
            // past that would only be silently rounded away.
            candidate = last.text + digit;
            if (Engine::significantDigits(candidate) > 15)
                return;
        }
    }
    m_expression = m_expression.left(last.position) + candidate;
}

bool Backend::literalFits(const QString &literal, const QList<Token> &tokens) const {
    quint64 value = 0;
    if (!Engine::parseInteger(literal, m_base, &value))
        return false;
    if (m_base != 10)
        return value <= Engine::wordMask(m_wordSize);

    // Decimal literals are signed. Right after a minus there is room for
    // one more, so the most negative value can be typed directly.
    const quint64 largest = Engine::wordMask(m_wordSize) >> 1;
    const bool negated = tokens.size() >= 2
        && tokens.at(tokens.size() - 2).kind == TokenKind::Prefix
        && tokens.at(tokens.size() - 2).text == QLatin1String("-");
    return value <= largest + (negated ? 1 : 0);
}

void Backend::appendDecimal() {
    beginEntry();
    const QList<Token> tokens = this->tokens();
    if (!tokens.isEmpty() && tokens.last().kind == TokenKind::Number) {
        const QString &literal = tokens.last().text;
        if (!literal.contains(QLatin1Char('.')) && !literal.contains(QLatin1Char('E')))
            m_expression += QLatin1Char('.');
        return;
    }
    m_expression += QStringLiteral("0.");
}

void Backend::appendExponent() {
    if (m_justEvaluated) {
        const QString literal = editableResult();
        m_justEvaluated = false;
        m_expression = literal.contains(QLatin1Char('E')) ? literal : literal + QLatin1Char('E');
        return;
    }
    m_error.clear();
    const QList<Token> tokens = this->tokens();
    if (!tokens.isEmpty() && tokens.last().kind == TokenKind::Number) {
        if (!tokens.last().text.contains(QLatin1Char('E')))
            m_expression += QLatin1Char('E');
        return;
    }
    if (!Engine::endsWithOperand(tokens))
        m_expression += QStringLiteral("1E");
}

void Backend::appendBinary(const QString &op) {
    if (m_justEvaluated) {
        // Chain from the exact result rather than its rounded display.
        QString literal = resultLiteral();
        if (op.startsWith(QLatin1Char('^')) && literal.startsWith(QLatin1Char('-')))
            literal = QLatin1Char('(') + literal + QLatin1Char(')');
        m_expression = literal;
        m_justEvaluated = false;
    }
    m_error.clear();

    const QList<Token> tokens = this->tokens();
    const bool minus = op == QLatin1String("-");
    if (tokens.isEmpty()) {
        if (minus)
            m_expression = op;
        else if (op != QLatin1String("+"))
            m_expression = QLatin1Char('0') + op;
        return;
    }

    const Token &last = tokens.last();
    if (last.kind == TokenKind::Open || last.kind == TokenKind::Function) {
        if (minus)
            m_expression += op;
        return;
    }

    if (last.kind == TokenKind::Binary || last.kind == TokenKind::Prefix) {
        // A minus after ×, ÷ or ^ starts a negative operand; any other
        // operator replaces the ones still waiting for an operand.
        if (minus && last.kind == TokenKind::Binary && last.text != QLatin1String("+")
                && last.text != QLatin1String("-")) {
            m_expression += op;
            return;
        }
        int cut = tokens.size();
        while (cut > 0
               && (tokens.at(cut - 1).kind == TokenKind::Binary
                   || tokens.at(cut - 1).kind == TokenKind::Prefix))
            --cut;
        if (cut == 0 || tokens.at(cut - 1).kind == TokenKind::Open
                || tokens.at(cut - 1).kind == TokenKind::Function)
            return;
        m_expression = m_expression.left(tokens.at(cut).position) + op;
        return;
    }

    m_expression += op;
}

void Backend::appendPostfix(const QString &op) {
    if (m_justEvaluated) {
        m_expression = resultLiteral();
        m_justEvaluated = false;
    }
    m_error.clear();

    const QList<Token> tokens = this->tokens();
    if (!Engine::endsWithOperand(tokens))
        return;

    // Squaring −3 means (−3)², the way a calculator squares what it shows,
    // and squaring 5² means (5²)² rather than a tower that reads like 5²².
    if (op.startsWith(QLatin1Char('^'))) {
        const int start = Engine::operandStart(tokens, true);
        if (start >= 0 && !simpleOperand(tokens, start)) {
            const int position = tokens.at(start).position;
            m_expression = m_expression.left(position) + QLatin1Char('(')
                + m_expression.mid(position) + QLatin1Char(')');
        }
    }
    m_expression += op;
}

void Backend::appendConstant(const QString &name) {
    beginEntry();
    const QList<Token> tokens = this->tokens();
    // Two names side by side would read as one (pi then e as "pie"), so a
    // constant after a name multiplies explicitly.
    if (!programmer() && !tokens.isEmpty()
            && (tokens.last().kind == TokenKind::Constant || tokens.last().kind == TokenKind::Name))
        m_expression += QLatin1Char('*');
    m_expression += name;
}

void Backend::appendLetter(QChar letter) {
    beginEntry();
    m_expression += letter;
}

// A function key applies to the operand the expression ends with, the way a
// calculator applies √ to the number on its display; with no operand yet it
// opens the function for one to be typed.
void Backend::applyFunction(const QString &name) {
    const QString opener = name + QLatin1Char('(');
    if (m_justEvaluated) {
        m_expression = opener + resultLiteral() + QLatin1Char(')');
        m_justEvaluated = false;
        m_error.clear();
        return;
    }
    m_error.clear();

    const QList<Token> tokens = this->tokens();
    const int start = Engine::operandStart(tokens, true);
    if (start < 0) {
        m_expression += opener;
        return;
    }
    // A group already has its parentheses: √ on (3+4) gives √(3+4).
    const int position = tokens.at(start).position;
    if (wholeGroup(tokens, start))
        m_expression.insert(position, name);
    else
        m_expression = m_expression.left(position) + opener + m_expression.mid(position)
            + QLatin1Char(')');
}

void Backend::applyPowerPrefix(const QString &prefix) {
    if (m_justEvaluated) {
        const QString literal = resultLiteral();
        m_expression = prefix
            + (literal.startsWith(QLatin1Char('-')) ? QLatin1Char('(') + literal + QLatin1Char(')')
                                                    : literal);
        m_justEvaluated = false;
        m_error.clear();
        return;
    }
    m_error.clear();

    const QList<Token> tokens = this->tokens();
    const int start = Engine::operandStart(tokens, true);
    if (start < 0) {
        m_expression += prefix;
        return;
    }
    const int position = tokens.at(start).position;
    const QString operand = m_expression.mid(position);
    m_expression = m_expression.left(position) + prefix
        + (simpleOperand(tokens, start) ? operand
                                        : QLatin1Char('(') + operand + QLatin1Char(')'));
}

void Backend::closeParenthesis() {
    if (m_justEvaluated)
        return;
    const QList<Token> tokens = this->tokens();
    if (Engine::openParentheses(tokens) == 0 || !Engine::endsWithOperand(tokens))
        return;
    m_error.clear();
    m_expression += QLatin1Char(')');
}

// ± and NOT toggle a sign in front of the operand the expression ends with,
// or start the next operand with one.
void Backend::toggleUnary(const QString &sign) {
    if (m_justEvaluated) {
        m_expression = resultLiteral();
        m_justEvaluated = false;
    }
    m_error.clear();

    const QList<Token> tokens = this->tokens();
    int start = Engine::operandStart(tokens, false);
    if (start < 0) {
        if (!tokens.isEmpty() && tokens.last().kind == TokenKind::Prefix
                && tokens.last().text == sign) {
            m_expression.chop(1);
            return;
        }
        if (tokens.isEmpty() || !endsOperand(tokens.last().kind))
            m_expression += sign;
        return;
    }

    // The other kind of sign belongs to the operand: ± on NOT 5 negates the
    // whole of it, and NOT on −5 inverts −5.
    while (start > 0 && tokens.at(start - 1).kind == TokenKind::Prefix
           && tokens.at(start - 1).text != sign)
        --start;

    if (start > 0 && tokens.at(start - 1).kind == TokenKind::Prefix
            && tokens.at(start - 1).text == sign) {
        m_expression.remove(tokens.at(start - 1).position, 1);
        return;
    }

    // After an implicit multiplication like 2(3), a bare minus would turn
    // into subtraction.
    QString insertion = sign;
    if (start > 0 && endsOperand(tokens.at(start - 1).kind))
        insertion.prepend(QLatin1Char('*'));
    m_expression.insert(tokens.at(start).position, insertion);
}

// Values from the history, memory or Rand replace the operand being typed,
// like typing a new number would.
void Backend::insertValue(const QString &literal) {
    if (m_justEvaluated) {
        clearAll();
        m_expression = literal;
        return;
    }
    m_error.clear();

    const QList<Token> tokens = this->tokens();
    const int start = Engine::operandStart(tokens, true);
    if (start >= 0)
        m_expression = m_expression.left(tokens.at(start).position) + literal;
    else
        m_expression += literal;
}

void Backend::backspace() {
    if (m_justEvaluated) {
        // Editing after = picks up from the result's digits.
        m_expression = editableResult();
        m_justEvaluated = false;
        m_evaluated.clear();
    }
    m_error.clear();

    const QList<Token> tokens = this->tokens();
    if (tokens.isEmpty()) {
        m_expression.clear();
        return;
    }

    // What a key put in whole comes out whole: sin(, π, mod, <<, Ans.
    const Token &last = tokens.last();
    const bool whole = last.kind == TokenKind::Function || last.kind == TokenKind::Constant
        || (last.kind == TokenKind::Binary && last.text.size() > 1);
    if (whole)
        m_expression.truncate(last.position);
    else
        m_expression.chop(1);

    if (m_expression == QLatin1String("-"))
        m_expression.clear();
}

void Backend::equals() {
    if (m_justEvaluated || m_expression.isEmpty())
        return;

    QString used;
    if (programmer()) {
        const Engine::IntegerResult result = evaluateIntegerLeniently(m_expression, &used);
        if (!result.ok) {
            m_error = result.error;
            return;
        }
        m_integerResult = result.value;
        m_integerAns = result.value;
    } else {
        const Engine::Result result = evaluateLeniently(m_expression, false, &used);
        if (!result.ok) {
            m_error = result.error;
            return;
        }
        m_result = result.value;
        m_ans = result.value;
    }

    const int open = Engine::openParentheses(Engine::tokenize(used, syntax()));
    m_evaluated = used + QString(open, QLatin1Char(')'));
    m_justEvaluated = true;
    m_error.clear();
    addHistory();
    emit evaluated();
}

void Backend::memory(const QString &key) {
    if (key == QLatin1String("mr")) {
        if (m_hasMemory)
            insertValue(Engine::literalFor(m_memory));
        return;
    }

    if (key == QLatin1String("mc")) {
        m_memory = 0;
        m_hasMemory = false;
    } else {
        const double value = m_hasCurrent ? m_currentValue : 0;
        if (key == QLatin1String("ms"))
            m_memory = value;
        else if (key == QLatin1String("mplus"))
            m_memory += value;
        else
            m_memory -= value;
        m_hasMemory = true;
    }
    emit settingsChanged();
}

QString Backend::resultLiteral() const {
    return programmer() ? Engine::integerLiteral(m_integerResult, m_base, m_wordSize)
                        : Engine::literalFor(m_result);
}

QString Backend::editableResult() const {
    return programmer() ? Engine::integerLiteral(m_integerResult, m_base, m_wordSize)
                        : Engine::formatNumber(m_result);
}

QString Backend::pretty(const QString &expression) const {
    return programmer() ? Engine::prettyProgrammer(expression)
                        : Engine::prettyScientific(expression);
}

QString Backend::formatValue(double value, quint64 integer) const {
    return programmer() ? Engine::formatInteger(integer, m_base, m_wordSize, true)
                        : Engine::formatDisplay(value);
}

// Evaluates what has been typed so far, forgiving a dangling tail: "2 +"
// is worth 2 and "3 × sin(" is worth 3. The preview also forgives a name
// still being typed; = does not, so a misspelling is reported.
Engine::Result Backend::evaluateLeniently(QString expression, bool preview, QString *used) const {
    Engine::Result first;
    for (int attempt = 0; !expression.isEmpty(); ++attempt) {
        const Engine::Result result = Engine::evaluate(expression, m_angle, m_ans);
        if (attempt == 0)
            first = result;
        if (result.ok) {
            if (used)
                *used = expression;
            return result;
        }
        if (!result.incomplete)
            return attempt == 0 ? result : first;

        const QList<Token> tokens = Engine::tokenize(expression, Engine::Syntax::Scientific);
        const TokenKind kind = tokens.last().kind;
        const bool dangling = kind == TokenKind::Binary || kind == TokenKind::Prefix
            || kind == TokenKind::Open || kind == TokenKind::Function
            || (preview && kind == TokenKind::Name);
        if (!dangling)
            break;
        expression.truncate(tokens.last().position);
    }
    return first;
}

Engine::IntegerResult Backend::evaluateIntegerLeniently(QString expression, QString *used) const {
    Engine::IntegerResult first;
    for (int attempt = 0; !expression.isEmpty(); ++attempt) {
        const Engine::IntegerResult result =
            Engine::evaluateInteger(expression, m_base, m_wordSize, m_integerAns);
        if (attempt == 0)
            first = result;
        if (result.ok) {
            if (used)
                *used = expression;
            return result;
        }
        if (!result.incomplete)
            return attempt == 0 ? result : first;

        const QList<Token> tokens = Engine::tokenize(expression, Engine::Syntax::Programmer);
        const TokenKind kind = tokens.last().kind;
        if (kind != TokenKind::Binary && kind != TokenKind::Prefix && kind != TokenKind::Open)
            break;
        expression.truncate(tokens.last().position);
    }
    return first;
}

quint64 Backend::toInteger(double value) const {
    // Decimals become integers the way a C cast would, truncating toward
    // zero, with anything beyond 64 bits pinned to the limits.
    if (!std::isfinite(value))
        return 0;
    const double truncated = std::trunc(value);
    qint64 integer = 0;
    if (truncated >= 9223372036854775807.0)
        integer = std::numeric_limits<qint64>::max();
    else if (truncated <= -9223372036854775808.0)
        integer = std::numeric_limits<qint64>::min();
    else
        integer = qint64(truncated);
    return quint64(integer) & Engine::wordMask(m_wordSize);
}

void Backend::refresh() {
    const QList<Token> tokens = this->tokens();
    m_pending = 0;
    m_hasCurrent = true;

    if (m_justEvaluated) {
        m_expressionLine = pretty(m_evaluated) + QStringLiteral(" =");
        m_display = formatValue(m_result, m_integerResult);
        m_displayState = QStringLiteral("result");
        m_currentValue = m_result;
        m_currentInteger = m_integerResult;
    } else if (tokens.isEmpty()) {
        m_expressionLine.clear();
        m_display = QStringLiteral("0");
        m_displayState = QStringLiteral("entry");
        m_lastPreview = QStringLiteral("0");
        m_currentValue = 0;
        m_currentInteger = 0;
    } else if (isSingleLiteral(tokens)) {
        // A lone number shows on the big line exactly as typed, trailing
        // decimal point and all.
        m_expressionLine.clear();
        m_displayState = QStringLiteral("entry");
        if (programmer()) {
            const QString &digits = tokens.last().text;
            QString grouped = m_base == 10 ? Engine::groupDigits(digits, 3, QStringLiteral(","))
                                           : Engine::groupDigits(digits, m_base == 8 ? 3 : 4,
                                                                 QStringLiteral(" "));
            if (tokens.size() == 2)
                grouped.prepend(QChar(0x2212));
            m_display = grouped;
            const Engine::IntegerResult result =
                Engine::evaluateInteger(m_expression, m_base, m_wordSize, m_integerAns);
            m_hasCurrent = result.ok;
            if (result.ok)
                m_currentInteger = result.value;
        } else {
            m_display = Engine::prettyScientific(m_expression);
            const Engine::Result result = Engine::evaluate(m_expression, m_angle, m_ans);
            m_hasCurrent = result.ok;
            if (result.ok)
                m_currentValue = result.value;
        }
        m_lastPreview = m_display;
    } else {
        m_expressionLine = pretty(m_expression);
        m_pending = Engine::openParentheses(tokens);
        m_displayState = QStringLiteral("preview");
        bool ok = false;
        if (programmer()) {
            const Engine::IntegerResult result =
                evaluateIntegerLeniently(m_expression, nullptr);
            ok = result.ok;
            if (ok)
                m_currentInteger = result.value;
        } else {
            const Engine::Result result = evaluateLeniently(m_expression, true, nullptr);
            ok = result.ok;
            if (ok)
                m_currentValue = result.value;
        }
        // While the expression is mid-word or mid-error, the last good
        // preview stays up rather than flickering away.
        if (ok)
            m_lastPreview = formatValue(m_currentValue, m_currentInteger);
        m_hasCurrent = ok;
        m_display = m_lastPreview;
    }

    if (!m_error.isEmpty()) {
        m_expressionLine = pretty(m_expression);
        m_pending = Engine::openParentheses(tokens);
        m_display = m_error;
        m_displayState = QStringLiteral("error");
    }

    emit stateChanged();
}

Backend::Snapshot Backend::snapshot() const {
    Snapshot state;
    state.expression = m_expression;
    state.justEvaluated = m_justEvaluated;
    state.evaluated = m_evaluated;
    state.result = m_result;
    state.integerResult = m_integerResult;
    state.error = m_error;
    state.base = m_base;
    state.wordSize = m_wordSize;
    return state;
}

void Backend::restore(const Snapshot &state) {
    const bool settingsMoved = state.base != m_base || state.wordSize != m_wordSize;
    m_expression = state.expression;
    m_justEvaluated = state.justEvaluated;
    m_evaluated = state.evaluated;
    m_result = state.result;
    m_integerResult = state.integerResult;
    m_error = state.error;
    m_base = state.base;
    m_wordSize = state.wordSize;
    m_lastPreview = QStringLiteral("0");
    if (settingsMoved) {
        QSettings settings;
        settings.setValue(baseSetting, m_base);
        settings.setValue(wordSizeSetting, m_wordSize);
        emit settingsChanged();
    }
}

void Backend::record(const Snapshot &before) {
    if (before == snapshot())
        return;
    m_undo.append(before);
    if (m_undo.size() > undoLimit)
        m_undo.removeFirst();
    m_redo.clear();
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    m_redo.append(snapshot());
    restore(m_undo.takeLast());
    refresh();
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo.append(snapshot());
    restore(m_redo.takeLast());
    refresh();
}

void Backend::toggleBit(int index) {
    if (!programmer() || index < 0 || index >= m_wordSize)
        return;

    // The bits show the value on the display; flipping one edits that value.
    const Snapshot before = snapshot();
    const quint64 value = (m_currentInteger ^ (quint64(1) << index)) & Engine::wordMask(m_wordSize);
    m_expression = Engine::integerLiteral(value, m_base, m_wordSize);
    m_justEvaluated = false;
    m_evaluated.clear();
    m_error.clear();
    record(before);
    refresh();
}

// Only programmer mode has digits to grey out; its hex keys are A to F.
bool Backend::digitEnabled(const QString &digit) const {
    if (!programmer() || digit.size() != 1)
        return true;
    const QChar c = digit.at(0);
    if (!isAsciiDigit(c) && (c < QLatin1Char('A') || c > QLatin1Char('F')))
        return true;
    return hexDigitValue(c) < m_base;
}

QString Backend::copyText() const {
    if (m_displayState == QLatin1String("error") || !m_hasCurrent)
        return {};
    return programmer() ? Engine::integerLiteral(m_currentInteger, m_base, m_wordSize)
                        : Engine::formatNumber(m_currentValue);
}

void Backend::copyResult() const {
    const QString text = copyText();
    if (text.isEmpty())
        return;
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

void Backend::paste() {
    if (const QClipboard *clipboard = QGuiApplication::clipboard())
        pasteText(clipboard->text());
}

void Backend::pasteText(const QString &text) {
    const QString cleaned = programmer() ? sanitizeProgrammer(text) : sanitizeScientific(text);
    if (cleaned.isEmpty())
        return;

    const Snapshot before = snapshot();
    if (m_justEvaluated)
        clearAll();
    m_error.clear();
    m_expression += cleaned;
    record(before);
    refresh();
}

// Pasted text may come from anywhere: this calculator's own display, a
// spreadsheet, a web page. Typographic symbols become the engine's, digit
// grouping goes, and a lone decimal comma becomes a point.
QString Backend::sanitizeScientific(const QString &text) const {
    QString s = text.trimmed();
    s.replace(QChar(0x2212), QLatin1Char('-'));
    s.replace(QChar(0x00D7), QLatin1Char('*'));
    s.replace(QChar(0x00F7), QLatin1Char('/'));
    s.replace(QChar(0x03C0), QStringLiteral("pi"));
    s.replace(QChar(0x221A), QStringLiteral("sqrt"));
    s.replace(QChar(0x00B2), QStringLiteral("^2"));
    s.replace(QChar(0x00B3), QStringLiteral("^3"));
    static const QRegularExpression space(QStringLiteral("\\s"),
                                          QRegularExpression::UseUnicodePropertiesOption);
    s.remove(space);

    static const QRegularExpression grouped(QStringLiteral("^-?\\d{1,3}(,\\d{3})+(\\.\\d*)?$"));
    if (s.contains(QLatin1Char(',')) && (s.contains(QLatin1Char('.')) || grouped.match(s).hasMatch()))
        s.remove(QLatin1Char(','));
    else
        s.replace(QLatin1Char(','), QLatin1Char('.'));

    // 1.5e+20 is an exponent; on its own, e is Euler's number.
    static const QRegularExpression exponent(QStringLiteral("([0-9.])[eE]\\+?(-?[0-9])"));
    s.replace(exponent, QStringLiteral("\\1E\\2"));

    QString cleaned;
    for (const QChar c : s) {
        if (c >= QLatin1Char('A') && c <= QLatin1Char('Z') && c != QLatin1Char('E'))
            cleaned += c.toLower();
        else if (isAsciiDigit(c) || (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
                 || QStringLiteral(".E+-*/^%!()").contains(c))
            cleaned += c;
    }
    return cleaned;
}

QString Backend::sanitizeProgrammer(const QString &text) const {
    QString s = text.trimmed();
    s.replace(QChar(0x2212), QLatin1Char('-'));
    s.replace(QChar(0x00D7), QLatin1Char('*'));
    s.replace(QChar(0x00F7), QLatin1Char('/'));
    static const QRegularExpression separators(QStringLiteral("[\\s_',]"),
                                               QRegularExpression::UseUnicodePropertiesOption);
    s.remove(separators);

    // C-style 0x, 0o and 0b literals convert into the current base.
    static const QRegularExpression prefixed(QStringLiteral("0([xXoObB])([0-9a-fA-F]+)"));
    QString converted;
    int cursor = 0;
    auto matches = prefixed.globalMatch(s);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        converted += s.mid(cursor, match.capturedStart() - cursor);
        cursor = match.capturedEnd();
        const QChar kind = match.captured(1).at(0).toLower();
        const int base = kind == QLatin1Char('x') ? 16 : kind == QLatin1Char('o') ? 8 : 2;
        quint64 value = 0;
        if (Engine::parseInteger(match.captured(2), base, &value))
            converted += Engine::integerLiteral(value & Engine::wordMask(m_wordSize), m_base,
                                                m_wordSize);
        else
            converted += match.captured(0);
    }
    converted += s.mid(cursor);

    QString cleaned;
    for (int i = 0; i < converted.size(); ++i) {
        const QChar c = converted.at(i).toUpper();
        if (hexDigitValue(c) >= 0 || QStringLiteral("+-*/%&|~()").contains(c)) {
            cleaned += c;
        } else if ((c == QLatin1Char('<') || c == QLatin1Char('>')) && i + 1 < converted.size()
                   && converted.at(i + 1) == c) {
            cleaned += QString(2, c);
            ++i;
        }
    }
    return cleaned;
}

void Backend::addHistory() {
    QVariantMap entry;
    entry.insert(QStringLiteral("expression"), pretty(m_evaluated));
    entry.insert(QStringLiteral("result"), formatValue(m_result, m_integerResult));
    entry.insert(QStringLiteral("programmer"), programmer());
    if (programmer()) {
        static const QHash<int, QString> baseNames{{2, QStringLiteral("BIN")},
                                                   {8, QStringLiteral("OCT")},
                                                   {10, QStringLiteral("DEC")},
                                                   {16, QStringLiteral("HEX")}};
        entry.insert(QStringLiteral("tag"), baseNames.value(m_base));
        entry.insert(QStringLiteral("value"),
                     QString::number(Engine::signExtend(m_integerResult, m_wordSize)));
    } else {
        entry.insert(QStringLiteral("tag"), QString());
        entry.insert(QStringLiteral("value"), Engine::literalFor(m_result));
    }

    m_history.prepend(entry);
    while (m_history.size() > historyLimit)
        m_history.removeLast();
    saveHistory();
    emit historyChanged();
}

void Backend::recallHistory(int index) {
    if (index < 0 || index >= m_history.size())
        return;

    const QVariantMap entry = m_history.at(index).toMap();
    const QString value = entry.value(QStringLiteral("value")).toString();
    const bool fromProgrammer = entry.value(QStringLiteral("programmer")).toBool();
    QString literal = value;
    if (programmer()) {
        const quint64 integer = fromProgrammer ? quint64(value.toLongLong()) & Engine::wordMask(m_wordSize)
                                               : toInteger(value.toDouble());
        literal = Engine::integerLiteral(integer, m_base, m_wordSize);
    }

    const Snapshot before = snapshot();
    insertValue(literal);
    record(before);
    refresh();
}

void Backend::clearHistory() {
    if (m_history.isEmpty())
        return;
    m_history.clear();
    saveHistory();
    emit historyChanged();
}

void Backend::saveHistory() {
    QSettings().setValue(historySetting, m_history);
}

QVariantMap Backend::windowSize(int mode) const {
    const QSettings settings;
    const QString name = modeNames.value(mode, modeNames.first());
    const QSize size = settings.value(QStringLiteral("window/%1/size").arg(name)).toSize();
    QVariantMap map;
    map.insert(QStringLiteral("valid"), size.isValid());
    map.insert(QStringLiteral("width"), size.width());
    map.insert(QStringLiteral("height"), size.height());
    map.insert(QStringLiteral("maximized"),
               settings.value(QStringLiteral("window/maximized"), false).toBool());
    return map;
}

void Backend::saveWindowSize(int mode, int width, int height, bool maximized) {
    QSettings settings;
    const QString name = modeNames.value(mode, modeNames.first());
    settings.setValue(QStringLiteral("window/%1/size").arg(name), QSize(width, height));
    settings.setValue(QStringLiteral("window/maximized"), maximized);
}
