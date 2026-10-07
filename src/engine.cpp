#include "engine.h"

#include <QHash>
#include <QLocale>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Engine {
namespace {

constexpr double pi = 3.14159265358979323846;
constexpr double euler = 2.71828182845904523536;

// Programmer operators get single characters of their own so they can never
// be mistaken for hex digits; the pretty printer spells them out.
constexpr char16_t xorSign = 0x2295;   // ⊕
constexpr char16_t nandSign = 0x22BC;  // ⊼
constexpr char16_t norSign = 0x22BD;   // ⊽
constexpr char16_t rolSign = 0x21BA;   // ↺
constexpr char16_t rorSign = 0x21BB;   // ↻

const QStringList scientificConstants{QStringLiteral("pi"), QStringLiteral("e"),
                                      QStringLiteral("ans")};

const QStringList knownNames{
    QStringLiteral("sin"), QStringLiteral("cos"), QStringLiteral("tan"),
    QStringLiteral("asin"), QStringLiteral("acos"), QStringLiteral("atan"),
    QStringLiteral("sinh"), QStringLiteral("cosh"), QStringLiteral("tanh"),
    QStringLiteral("asinh"), QStringLiteral("acosh"), QStringLiteral("atanh"),
    QStringLiteral("ln"), QStringLiteral("log"), QStringLiteral("log2"),
    QStringLiteral("exp"), QStringLiteral("sqrt"), QStringLiteral("cbrt"),
    QStringLiteral("abs"), QStringLiteral("floor"), QStringLiteral("ceil"),
    QStringLiteral("round"), QStringLiteral("mod"), QStringLiteral("pi"),
    QStringLiteral("e"), QStringLiteral("ans")};

bool isAsciiDigit(QChar c) {
    return c >= QLatin1Char('0') && c <= QLatin1Char('9');
}

bool isAsciiLetter(QChar c) {
    return c >= QLatin1Char('a') && c <= QLatin1Char('z');
}

int digitValue(QChar c) {
    if (isAsciiDigit(c))
        return c.unicode() - '0';
    if (c >= QLatin1Char('A') && c <= QLatin1Char('F'))
        return c.unicode() - 'A' + 10;
    if (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
        return c.unicode() - 'a' + 10;
    return -1;
}

// A + or - is a sign rather than an operator when nothing complete stands to
// its left.
bool inPrefixPosition(const QList<Token> &tokens) {
    if (tokens.isEmpty())
        return true;
    const TokenKind kind = tokens.last().kind;
    return kind == TokenKind::Binary || kind == TokenKind::Prefix
        || kind == TokenKind::Open || kind == TokenKind::Function;
}

bool startsOperand(const Token &token) {
    return token.kind == TokenKind::Number || token.kind == TokenKind::Constant
        || token.kind == TokenKind::Name || token.kind == TokenKind::Function
        || token.kind == TokenKind::Open;
}

int matchingOpen(const QList<Token> &tokens, int closeIndex) {
    int depth = 0;
    for (int i = closeIndex; i >= 0; --i) {
        const TokenKind kind = tokens.at(i).kind;
        if (kind == TokenKind::Close)
            ++depth;
        else if ((kind == TokenKind::Open || kind == TokenKind::Function) && --depth == 0)
            return i;
    }
    return -1;
}

// The first token of the operand that ends at `end`, skipping back over
// postfix operators and through a parenthesized group to its opener.
int primaryStart(const QList<Token> &tokens, int end) {
    int i = end;
    while (i >= 0 && tokens.at(i).kind == TokenKind::Postfix)
        --i;
    if (i < 0)
        return -1;
    switch (tokens.at(i).kind) {
    case TokenKind::Close:
        return matchingOpen(tokens, i);
    case TokenKind::Number:
    case TokenKind::Constant:
        return i;
    default:
        return -1;
    }
}

QString exponentForm(QString text) {
    // Qt writes 1e+20 and 1e-07; the calculator reads and writes 1E20 and
    // 1E-7, keeping a lowercase e free to mean Euler's number.
    const int e = text.indexOf(QLatin1Char('e'));
    if (e < 0)
        return text;
    return text.left(e) + QLatin1Char('E') + QString::number(text.mid(e + 1).toInt());
}

QString prettyNumber(const QString &literal) {
    QString text = literal;
    if (significantDigits(text) > 15) {
        bool ok = false;
        const double value = QLocale::c().toDouble(text, &ok);
        if (ok)
            text = formatNumber(value);
    }

    QString exponent;
    const int e = text.indexOf(QLatin1Char('E'));
    if (e >= 0) {
        exponent = text.mid(e + 1);
        exponent.replace(QLatin1Char('-'), QChar(0x2212));
        exponent.prepend(QLatin1Char('E'));
        text.truncate(e);
    }

    const int point = text.indexOf(QLatin1Char('.'));
    const QString whole = point < 0 ? text : text.left(point);
    const QString fraction = point < 0 ? QString() : text.mid(point);
    return groupDigits(whole, 3, QStringLiteral(",")) + fraction + exponent;
}

QString prettyFunction(const QString &opener) {
    static const QHash<QString, QString> names{
        {QStringLiteral("asin("), QStringLiteral("sin⁻¹(")},
        {QStringLiteral("acos("), QStringLiteral("cos⁻¹(")},
        {QStringLiteral("atan("), QStringLiteral("tan⁻¹(")},
        {QStringLiteral("asinh("), QStringLiteral("sinh⁻¹(")},
        {QStringLiteral("acosh("), QStringLiteral("cosh⁻¹(")},
        {QStringLiteral("atanh("), QStringLiteral("tanh⁻¹(")},
        {QStringLiteral("sqrt("), QStringLiteral("√(")},
        {QStringLiteral("cbrt("), QStringLiteral("³√(")},
        {QStringLiteral("log2("), QStringLiteral("log₂(")},
    };
    return names.value(opener, opener);
}

QString prettyFunctionName(const QString &name) {
    return prettyFunction(name + QLatin1Char('(')).chopped(1);
}

bool isAllDigits(const QString &text) {
    if (text.isEmpty())
        return false;
    for (const QChar c : text) {
        if (!isAsciiDigit(c))
            return false;
    }
    return true;
}

class ScientificParser {
public:
    ScientificParser(const QList<Token> &tokens, AngleUnit angle, double ans)
        : m_tokens(tokens), m_angle(angle), m_ans(ans) {}

    Result run() {
        Result result;
        if (m_tokens.isEmpty()) {
            result.incomplete = true;
            result.error = QStringLiteral("Nothing to calculate");
            return result;
        }

        const double value = additive().number;
        if (!m_failed && !atEnd()) {
            const Token &token = peek();
            fail(token.kind == TokenKind::Close
                     ? QStringLiteral("Unmatched “)”")
                     : QStringLiteral("Unexpected “%1”").arg(token.text));
        }
        if (!m_failed && !std::isfinite(value))
            fail(QStringLiteral("Number too large"));

        if (m_failed) {
            result.error = m_error;
            result.incomplete = m_incomplete;
            return result;
        }
        result.ok = true;
        result.value = value == 0 ? 0 : value;  // No negative zero.
        return result;
    }

private:
    // A value remembers whether it was written as a percentage, so that
    // 200 + 10% can mean ten percent of 200.
    struct Value {
        double number = 0;
        bool percent = false;
    };

    bool atEnd() const { return m_pos >= m_tokens.size(); }
    const Token &peek() const { return m_tokens.at(m_pos); }
    bool peekBinary(const QString &text) const {
        return !atEnd() && peek().kind == TokenKind::Binary && peek().text == text;
    }

    void fail(const QString &message, bool incomplete = false) {
        if (m_failed)
            return;
        m_failed = true;
        m_error = message;
        m_incomplete = incomplete;
    }

    Value additive() {
        Value left = term();
        while (!m_failed && (peekBinary(QStringLiteral("+")) || peekBinary(QStringLiteral("-")))) {
            const bool plus = peek().text == QLatin1String("+");
            ++m_pos;
            const Value right = term();
            if (m_failed)
                break;
            // iOS-style percent: with + or −, x% is x percent of what came
            // before, so 200 + 10% gives 220. Elsewhere it is just x ÷ 100.
            const double amount = right.percent ? left.number * right.number : right.number;
            left = {plus ? left.number + amount : left.number - amount, false};
        }
        return left;
    }

    Value term() {
        Value left = unary();
        while (!m_failed && !atEnd()) {
            const Token &token = peek();
            if (token.kind == TokenKind::Binary
                    && (token.text == QLatin1String("*") || token.text == QLatin1String("/")
                        || token.text == QLatin1String("mod"))) {
                const QString op = token.text;
                ++m_pos;
                const Value right = unary();
                if (m_failed)
                    break;
                if (op == QLatin1String("*")) {
                    left = {left.number * right.number, false};
                } else if (right.number == 0) {
                    fail(QStringLiteral("Can't divide by zero"));
                } else if (op == QLatin1String("/")) {
                    left = {left.number / right.number, false};
                } else {
                    // Floored modulo, so the answer takes the divisor's sign
                    // the way it does on paper: −7 mod 3 is 2.
                    left = {left.number - right.number * std::floor(left.number / right.number),
                            false};
                }
            } else if (startsOperand(token)) {
                // Implicit multiplication: 2π, 3(4 + 1), 2sin(30).
                const Value right = unary();
                if (m_failed)
                    break;
                left = {left.number * right.number, false};
            } else {
                break;
            }
        }
        return left;
    }

    Value unary() {
        if (!atEnd() && peek().kind == TokenKind::Prefix) {
            const bool negate = peek().text == QLatin1String("-");
            ++m_pos;
            Value value = unary();
            if (negate)
                value.number = -value.number;
            return value;
        }
        return power();
    }

    // Powers bind tighter than a leading minus (−2² is −4) and associate to
    // the right (2^3^2 is 2^9), and the exponent may carry its own sign.
    Value power() {
        const Value base = postfix();
        if (m_failed || !peekBinary(QStringLiteral("^")))
            return base;
        ++m_pos;
        const Value exponent = unary();
        if (m_failed)
            return {};
        return {raise(base.number, exponent.number), false};
    }

    Value postfix() {
        Value value = primary();
        while (!m_failed && !atEnd() && peek().kind == TokenKind::Postfix) {
            const bool factorial = peek().text == QLatin1String("!");
            ++m_pos;
            if (factorial)
                value = {this->factorial(value.number), false};
            else
                value = {value.number / 100, true};
        }
        return value;
    }

    Value primary() {
        if (atEnd()) {
            fail(QStringLiteral("Incomplete expression"), true);
            return {};
        }

        const Token token = m_tokens.at(m_pos++);
        switch (token.kind) {
        case TokenKind::Number:
            return {number(token.text), false};
        case TokenKind::Constant:
            if (token.text == QLatin1String("pi"))
                return {pi, false};
            if (token.text == QLatin1String("e"))
                return {euler, false};
            return {m_ans, false};
        case TokenKind::Function:
        case TokenKind::Open: {
            const Value inner = additive();
            if (m_failed)
                return {};
            // A missing closing parenthesis at the very end is implied.
            if (!atEnd()) {
                if (peek().kind != TokenKind::Close) {
                    fail(QStringLiteral("Unexpected “%1”").arg(peek().text));
                    return {};
                }
                ++m_pos;
            }
            if (token.kind == TokenKind::Open)
                return {inner.number, false};
            return {call(token.text.chopped(1), inner.number), false};
        }
        case TokenKind::Name: {
            // Still typing a name the calculator knows counts as incomplete,
            // so the preview waits instead of flashing an error.
            bool partial = false;
            if (atEnd()) {
                for (const QString &name : knownNames)
                    partial = partial || name.startsWith(token.text);
            }
            if (knownNames.contains(token.text))
                fail(QStringLiteral("%1 needs “(”").arg(prettyFunctionName(token.text)), partial);
            else
                fail(QStringLiteral("Unknown name “%1”").arg(token.text), partial);
            return {};
        }
        case TokenKind::Close:
            fail(QStringLiteral("Unexpected “)”"));
            return {};
        case TokenKind::Binary:
        case TokenKind::Postfix:
        case TokenKind::Prefix:
            fail(QStringLiteral("Missing a number"));
            return {};
        case TokenKind::Invalid:
            fail(QStringLiteral("Unexpected “%1”").arg(token.text));
            return {};
        }
        return {};
    }

    double number(QString text) {
        // Typing leaves loose ends like "5." or "2E" behind; read those
        // as the number they are on their way to being.
        if (text.endsWith(QLatin1String("E+")) || text.endsWith(QLatin1String("E-")))
            text.chop(2);
        else if (text.endsWith(QLatin1Char('E')))
            text.chop(1);
        if (text.endsWith(QLatin1Char('.')))
            text.chop(1);
        if (text.isEmpty())
            return 0;

        bool ok = false;
        const double value = QLocale::c().toDouble(text, &ok);
        if (!ok) {
            fail(QStringLiteral("Unreadable number “%1”").arg(text));
            return 0;
        }
        if (!std::isfinite(value))
            fail(QStringLiteral("Number too large"));
        return value;
    }

    double raise(double base, double exponent) {
        if (base == 0 && exponent < 0) {
            fail(QStringLiteral("Can't divide by zero"));
            return 0;
        }
        const double result = std::pow(base, exponent);
        if (!std::isnan(result))
            return result;

        // pow() refuses every fractional power of a negative number, but odd
        // roots of one are real: (−8)^(1/3) is −2.
        if (base < 0) {
            const double root = 1 / exponent;
            const double nearest = std::round(root);
            if (std::fabs(root - nearest) < 1e-9 && std::fmod(nearest, 2) != 0)
                return -std::pow(-base, exponent);
        }
        fail(QStringLiteral("Not a real number"));
        return 0;
    }

    double factorial(double x) {
        if (x != std::floor(x)) {
            // The gamma function carries the factorial between the integers.
            const double result = std::tgamma(x + 1);
            if (!std::isfinite(result))
                fail(QStringLiteral("Number too large"));
            return result;
        }
        if (x < 0) {
            fail(QStringLiteral("Undefined for negative integers"));
            return 0;
        }
        if (x > 170) {
            fail(QStringLiteral("Number too large"));
            return 0;
        }
        double result = 1;
        for (int i = 2; i <= int(x); ++i)
            result *= i;
        return result;
    }

    double fullTurn() const {
        switch (m_angle) {
        case AngleUnit::Degrees:
            return 360;
        case AngleUnit::Gradians:
            return 400;
        case AngleUnit::Radians:
            break;
        }
        return 2 * pi;
    }

    double toRadians(double angle) const {
        return m_angle == AngleUnit::Radians ? angle : angle / fullTurn() * 2 * pi;
    }

    double fromRadians(double radians) const {
        return m_angle == AngleUnit::Radians ? radians : radians / (2 * pi) * fullTurn();
    }

    double trig(const QString &name, double angle) {
        // Whole quarter turns get exact answers: sin 180° is 0, not the
        // 1.2e-16 that the nearest double to π leaves behind.
        const double quarters = angle / fullTurn() * 4;
        const double nearest = std::round(quarters);
        const double tolerance = 8 * std::numeric_limits<double>::epsilon()
            * std::max(1.0, std::fabs(nearest));
        if (std::fabs(quarters - nearest) <= tolerance) {
            const int quadrant = int(std::fmod(std::fmod(nearest, 4) + 4, 4));
            static const double sines[] = {0, 1, 0, -1};
            static const double cosines[] = {1, 0, -1, 0};
            if (name == QLatin1String("sin"))
                return sines[quadrant];
            if (name == QLatin1String("cos"))
                return cosines[quadrant];
            if (quadrant % 2 != 0) {
                fail(QStringLiteral("tan is undefined here"));
                return 0;
            }
            return 0;
        }

        const double radians = toRadians(angle);
        if (name == QLatin1String("sin"))
            return std::sin(radians);
        if (name == QLatin1String("cos"))
            return std::cos(radians);
        return std::tan(radians);
    }

    double domainError(const QString &name) {
        fail(QStringLiteral("Outside the domain of %1").arg(prettyFunctionName(name)));
        return 0;
    }

    double call(const QString &name, double x) {
        if (name == QLatin1String("sin") || name == QLatin1String("cos")
                || name == QLatin1String("tan"))
            return trig(name, x);
        if (name == QLatin1String("asin"))
            return x < -1 || x > 1 ? domainError(name) : fromRadians(std::asin(x));
        if (name == QLatin1String("acos"))
            return x < -1 || x > 1 ? domainError(name) : fromRadians(std::acos(x));
        if (name == QLatin1String("atan"))
            return fromRadians(std::atan(x));
        if (name == QLatin1String("sinh"))
            return std::sinh(x);
        if (name == QLatin1String("cosh"))
            return std::cosh(x);
        if (name == QLatin1String("tanh"))
            return std::tanh(x);
        if (name == QLatin1String("asinh"))
            return std::asinh(x);
        if (name == QLatin1String("acosh"))
            return x < 1 ? domainError(name) : std::acosh(x);
        if (name == QLatin1String("atanh"))
            return x <= -1 || x >= 1 ? domainError(name) : std::atanh(x);
        if (name == QLatin1String("ln"))
            return x <= 0 ? domainError(name) : std::log(x);
        if (name == QLatin1String("log"))
            return x <= 0 ? domainError(name) : std::log10(x);
        if (name == QLatin1String("log2"))
            return x <= 0 ? domainError(name) : std::log2(x);
        if (name == QLatin1String("exp"))
            return std::exp(x);
        if (name == QLatin1String("sqrt")) {
            if (x < 0) {
                fail(QStringLiteral("Square root of a negative number"));
                return 0;
            }
            return std::sqrt(x);
        }
        if (name == QLatin1String("cbrt"))
            return std::cbrt(x);
        if (name == QLatin1String("abs"))
            return std::fabs(x);
        if (name == QLatin1String("floor"))
            return std::floor(x);
        if (name == QLatin1String("ceil"))
            return std::ceil(x);
        if (name == QLatin1String("round"))
            return std::round(x);

        fail(QStringLiteral("Unknown function “%1”").arg(name));
        return 0;
    }

    const QList<Token> &m_tokens;
    AngleUnit m_angle;
    double m_ans;
    int m_pos = 0;
    bool m_failed = false;
    bool m_incomplete = false;
    QString m_error;
};

class IntegerParser {
public:
    IntegerParser(const QList<Token> &tokens, int base, int wordSize, quint64 ans)
        : m_tokens(tokens), m_base(base), m_wordSize(wordSize),
          m_mask(wordMask(wordSize)), m_ans(ans & m_mask) {}

    IntegerResult run() {
        IntegerResult result;
        if (m_tokens.isEmpty()) {
            result.incomplete = true;
            result.error = QStringLiteral("Nothing to calculate");
            return result;
        }

        const quint64 value = bitOr();
        if (!m_failed && !atEnd()) {
            const Token &token = peek();
            fail(token.kind == TokenKind::Close
                     ? QStringLiteral("Unmatched “)”")
                     : QStringLiteral("Unexpected “%1”").arg(token.text));
        }
        if (m_failed) {
            result.error = m_error;
            result.incomplete = m_incomplete;
            return result;
        }
        result.ok = true;
        result.value = value & m_mask;
        return result;
    }

private:
    bool atEnd() const { return m_pos >= m_tokens.size(); }
    const Token &peek() const { return m_tokens.at(m_pos); }
    bool peekBinary(QStringView text) const {
        return !atEnd() && peek().kind == TokenKind::Binary && peek().text == text;
    }
    bool peekBinary(char16_t sign) const { return peekBinary(QStringView(&sign, 1)); }

    void fail(const QString &message, bool incomplete = false) {
        if (m_failed)
            return;
        m_failed = true;
        m_error = message;
        m_incomplete = incomplete;
    }

    quint64 masked(quint64 value) const { return value & m_mask; }
    qint64 signedValue(quint64 value) const { return signExtend(value, m_wordSize); }

    // Precedence follows C, lowest first: OR and NOR, XOR, AND and NAND, the
    // shifts and rotates, then ordinary arithmetic.
    quint64 bitOr() {
        quint64 left = bitXor();
        while (!m_failed && (peekBinary(u"|") || peekBinary(norSign))) {
            const bool nor = peek().text.at(0) == QChar(norSign);
            ++m_pos;
            const quint64 right = bitXor();
            left = masked(nor ? ~(left | right) : left | right);
        }
        return left;
    }

    quint64 bitXor() {
        quint64 left = bitAnd();
        while (!m_failed && peekBinary(xorSign)) {
            ++m_pos;
            left = masked(left ^ bitAnd());
        }
        return left;
    }

    quint64 bitAnd() {
        quint64 left = shift();
        while (!m_failed && (peekBinary(u"&") || peekBinary(nandSign))) {
            const bool nand = peek().text.at(0) == QChar(nandSign);
            ++m_pos;
            const quint64 right = shift();
            left = masked(nand ? ~(left & right) : left & right);
        }
        return left;
    }

    quint64 shift() {
        quint64 left = additive();
        while (!m_failed
               && (peekBinary(u"<<") || peekBinary(u">>") || peekBinary(rolSign)
                   || peekBinary(rorSign))) {
            const QString op = peek().text;
            ++m_pos;
            const quint64 right = additive();
            if (m_failed)
                break;

            const qint64 count = signedValue(right);
            if (op == QLatin1String("<<") || op == QLatin1String(">>")) {
                if (count < 0) {
                    fail(QStringLiteral("Can't shift by a negative amount"));
                    break;
                }
                if (op == QLatin1String("<<")) {
                    left = count >= m_wordSize ? 0 : masked(left << count);
                } else {
                    // Arithmetic shift: the sign bit fills in from the left.
                    const qint64 value = signedValue(left);
                    left = count >= m_wordSize ? masked(value < 0 ? ~quint64(0) : 0)
                                               : masked(quint64(value >> count));
                }
            } else {
                const int turns = int(((count % m_wordSize) + m_wordSize) % m_wordSize);
                const bool leftward = op.at(0) == QChar(rolSign);
                if (turns != 0) {
                    left = leftward ? masked((left << turns) | (left >> (m_wordSize - turns)))
                                    : masked((left >> turns) | (left << (m_wordSize - turns)));
                }
            }
        }
        return left;
    }

    quint64 additive() {
        quint64 left = term();
        while (!m_failed && (peekBinary(u"+") || peekBinary(u"-"))) {
            const bool plus = peek().text == QLatin1String("+");
            ++m_pos;
            const quint64 right = term();
            left = masked(plus ? left + right : left - right);
        }
        return left;
    }

    quint64 term() {
        quint64 left = unary();
        while (!m_failed && !atEnd()) {
            const Token &token = peek();
            const bool explicitOp = token.kind == TokenKind::Binary
                && (token.text == QLatin1String("*") || token.text == QLatin1String("/")
                    || token.text == QLatin1String("%"));
            if (!explicitOp && !startsOperand(token))
                break;

            const QString op = explicitOp ? token.text : QStringLiteral("*");
            if (explicitOp)
                ++m_pos;
            const quint64 right = unary();
            if (m_failed)
                break;

            if (op == QLatin1String("*")) {
                left = masked(left * right);
                continue;
            }
            if (right == 0) {
                fail(QStringLiteral("Can't divide by zero"));
                break;
            }
            // Signed division truncating toward zero, like C. Dividing by −1
            // is negation, which also sidesteps the one overflowing case.
            const qint64 a = signedValue(left);
            const qint64 b = signedValue(right);
            if (op == QLatin1String("/"))
                left = b == -1 ? masked(0 - left) : masked(quint64(a / b));
            else
                left = b == -1 ? 0 : masked(quint64(a % b));
        }
        return left;
    }

    quint64 unary() {
        if (!atEnd() && peek().kind == TokenKind::Prefix) {
            const QString op = peek().text;
            ++m_pos;
            const quint64 value = unary();
            if (op == QLatin1String("-"))
                return masked(0 - value);
            if (op == QLatin1String("~"))
                return masked(~value);
            return value;
        }
        return primary();
    }

    quint64 primary() {
        if (atEnd()) {
            fail(QStringLiteral("Incomplete expression"), true);
            return 0;
        }

        const Token token = m_tokens.at(m_pos++);
        switch (token.kind) {
        case TokenKind::Number: {
            for (const QChar c : token.text) {
                if (digitValue(c) >= m_base) {
                    static const QHash<int, QString> names{
                        {2, QStringLiteral("a binary")}, {8, QStringLiteral("an octal")},
                        {10, QStringLiteral("a decimal")}, {16, QStringLiteral("a hex")}};
                    fail(QStringLiteral("“%1” isn't %2 digit").arg(c).arg(names.value(m_base)));
                    return 0;
                }
            }
            quint64 value = 0;
            if (!parseInteger(token.text, m_base, &value)) {
                fail(QStringLiteral("Number too large"));
                return 0;
            }
            return masked(value);
        }
        case TokenKind::Constant:
            return m_ans;
        case TokenKind::Open: {
            const quint64 inner = bitOr();
            if (m_failed)
                return 0;
            if (!atEnd()) {
                if (peek().kind != TokenKind::Close) {
                    fail(QStringLiteral("Unexpected “%1”").arg(peek().text));
                    return 0;
                }
                ++m_pos;
            }
            return inner;
        }
        case TokenKind::Close:
            fail(QStringLiteral("Unexpected “)”"));
            return 0;
        case TokenKind::Binary:
        case TokenKind::Postfix:
        case TokenKind::Prefix:
            fail(QStringLiteral("Missing a number"));
            return 0;
        default:
            fail(QStringLiteral("Unexpected “%1”").arg(token.text));
            return 0;
        }
    }

    const QList<Token> &m_tokens;
    int m_base;
    int m_wordSize;
    quint64 m_mask;
    quint64 m_ans;
    int m_pos = 0;
    bool m_failed = false;
    bool m_incomplete = false;
    QString m_error;
};

}  // namespace

QList<Token> tokenize(const QString &source, Syntax syntax) {
    QList<Token> tokens;
    const int length = source.size();
    int i = 0;
    while (i < length) {
        const QChar c = source.at(i);
        const int start = i;
        if (c.isSpace()) {
            ++i;
            continue;
        }

        if (syntax == Syntax::Scientific) {
            if (isAsciiDigit(c) || c == QLatin1Char('.')) {
                bool seenPoint = false;
                while (i < length
                       && (isAsciiDigit(source.at(i))
                           || (source.at(i) == QLatin1Char('.') && !seenPoint))) {
                    seenPoint = seenPoint || source.at(i) == QLatin1Char('.');
                    ++i;
                }
                if (i < length && source.at(i) == QLatin1Char('E')) {
                    ++i;
                    if (i < length
                            && (source.at(i) == QLatin1Char('+') || source.at(i) == QLatin1Char('-')))
                        ++i;
                    while (i < length && isAsciiDigit(source.at(i)))
                        ++i;
                }
                tokens.append({TokenKind::Number, source.mid(start, i - start), start});
                continue;
            }

            if (isAsciiLetter(c)) {
                while (i < length && isAsciiLetter(source.at(i)))
                    ++i;
                QString name = source.mid(start, i - start);
                // log2 is the one name with a digit in it.
                if (name == QLatin1String("log") && source.mid(i, 2) == QLatin1String("2(")) {
                    name += QLatin1Char('2');
                    ++i;
                }

                if (name == QLatin1String("mod")) {
                    tokens.append({TokenKind::Binary, name, start});
                } else if (scientificConstants.contains(name)) {
                    tokens.append({TokenKind::Constant, name, start});
                } else if (i < length && source.at(i) == QLatin1Char('(')) {
                    ++i;
                    tokens.append({TokenKind::Function, name + QLatin1Char('('), start});
                } else {
                    tokens.append({TokenKind::Name, name, start});
                }
                continue;
            }

            ++i;
            TokenKind kind = TokenKind::Invalid;
            switch (c.unicode()) {
            case '+':
            case '-':
                kind = inPrefixPosition(tokens) ? TokenKind::Prefix : TokenKind::Binary;
                break;
            case '*':
            case '/':
            case '^':
                kind = TokenKind::Binary;
                break;
            case '!':
            case '%':
                kind = TokenKind::Postfix;
                break;
            case '(':
                kind = TokenKind::Open;
                break;
            case ')':
                kind = TokenKind::Close;
                break;
            default:
                break;
            }
            tokens.append({kind, QString(c), start});
            continue;
        }

        if (digitValue(c) >= 0) {
            while (i < length && digitValue(source.at(i)) >= 0)
                ++i;
            tokens.append({TokenKind::Number, source.mid(start, i - start).toUpper(), start});
            continue;
        }

        if ((c == QLatin1Char('<') || c == QLatin1Char('>')) && i + 1 < length
                && source.at(i + 1) == c) {
            i += 2;
            tokens.append({TokenKind::Binary, QString(2, c), start});
            continue;
        }

        ++i;
        TokenKind kind = TokenKind::Invalid;
        switch (c.unicode()) {
        case '+':
        case '-':
            kind = inPrefixPosition(tokens) ? TokenKind::Prefix : TokenKind::Binary;
            break;
        case '~':
            kind = TokenKind::Prefix;
            break;
        case '*':
        case '/':
        case '%':
        case '&':
        case '|':
        case xorSign:
        case nandSign:
        case norSign:
        case rolSign:
        case rorSign:
            kind = TokenKind::Binary;
            break;
        case '@':
            kind = TokenKind::Constant;
            break;
        case '(':
            kind = TokenKind::Open;
            break;
        case ')':
            kind = TokenKind::Close;
            break;
        default:
            break;
        }
        tokens.append({kind, QString(c), start});
    }
    return tokens;
}

bool endsWithOperand(const QList<Token> &tokens) {
    if (tokens.isEmpty())
        return false;
    const TokenKind kind = tokens.last().kind;
    return kind == TokenKind::Number || kind == TokenKind::Constant
        || kind == TokenKind::Close || kind == TokenKind::Postfix;
}

int operandStart(const QList<Token> &tokens, bool includeSign) {
    if (!endsWithOperand(tokens))
        return -1;

    int start = primaryStart(tokens, tokens.size() - 1);
    if (start < 0)
        return -1;

    // Walk back through power chains, including a signed exponent: the
    // operand of 2^-3 is all of it, not just the 3.
    for (;;) {
        int k = start - 1;
        if (k >= 1 && tokens.at(k).kind == TokenKind::Prefix
                && tokens.at(k - 1).kind == TokenKind::Binary
                && tokens.at(k - 1).text == QLatin1String("^"))
            --k;
        if (k >= 1 && tokens.at(k).kind == TokenKind::Binary
                && tokens.at(k).text == QLatin1String("^")) {
            const int base = primaryStart(tokens, k - 1);
            if (base < 0)
                break;
            start = base;
            continue;
        }
        break;
    }

    if (includeSign && start >= 1 && tokens.at(start - 1).kind == TokenKind::Prefix
            && tokens.at(start - 1).text == QLatin1String("-"))
        --start;
    return start;
}

int openParentheses(const QList<Token> &tokens) {
    int depth = 0;
    for (const Token &token : tokens) {
        if (token.kind == TokenKind::Open || token.kind == TokenKind::Function)
            ++depth;
        else if (token.kind == TokenKind::Close && depth > 0)
            --depth;
    }
    return depth;
}

int significantDigits(const QString &literal) {
    QString mantissa = literal;
    const int e = mantissa.indexOf(QLatin1Char('E'));
    if (e >= 0)
        mantissa.truncate(e);
    mantissa.remove(QLatin1Char('.'));
    mantissa.remove(QLatin1Char('-'));
    int leading = 0;
    while (leading < mantissa.size() && mantissa.at(leading) == QLatin1Char('0'))
        ++leading;
    return mantissa.size() - leading;
}

Result evaluate(const QString &expression, AngleUnit angle, double ans) {
    const QList<Token> tokens = tokenize(expression, Syntax::Scientific);
    return ScientificParser(tokens, angle, ans).run();
}

IntegerResult evaluateInteger(const QString &expression, int base, int wordSize, quint64 ans) {
    const QList<Token> tokens = tokenize(expression, Syntax::Programmer);
    return IntegerParser(tokens, base, wordSize, ans).run();
}

quint64 wordMask(int wordSize) {
    return wordSize >= 64 ? ~quint64(0) : (quint64(1) << wordSize) - 1;
}

qint64 signExtend(quint64 value, int wordSize) {
    if (wordSize >= 64)
        return qint64(value);
    const quint64 sign = quint64(1) << (wordSize - 1);
    value &= wordMask(wordSize);
    return qint64((value ^ sign) - sign);
}

bool parseInteger(const QString &digits, int base, quint64 *value) {
    if (digits.isEmpty())
        return false;
    quint64 result = 0;
    for (const QChar c : digits) {
        const int digit = digitValue(c);
        if (digit < 0 || digit >= base)
            return false;
        if (result > (std::numeric_limits<quint64>::max() - quint64(digit)) / quint64(base))
            return false;
        result = result * quint64(base) + quint64(digit);
    }
    *value = result;
    return true;
}

QString formatNumber(double value) {
    if (value == 0)
        value = 0;  // Collapse negative zero.

    // Fifteen significant digits keeps binary-float noise like
    // 0.1 + 0.2 = 0.30000000000000004 out of sight while showing every
    // integer the 15-digit entry limit can produce exactly.
    return exponentForm(QString::number(value, 'g', 15));
}

QString formatDisplay(double value) {
    QString mantissa = formatNumber(value);
    QString exponent;
    const int e = mantissa.indexOf(QLatin1Char('E'));
    if (e >= 0) {
        exponent = mantissa.mid(e + 1);
        mantissa.truncate(e);
    }

    const bool negative = mantissa.startsWith(QLatin1Char('-'));
    if (negative)
        mantissa.remove(0, 1);
    const int point = mantissa.indexOf(QLatin1Char('.'));
    const QString whole = point < 0 ? mantissa : mantissa.left(point);
    const QString fraction = point < 0 ? QString() : mantissa.mid(point);

    QString text = (negative ? QString(QChar(0x2212)) : QString())
        + groupDigits(whole, 3, QStringLiteral(",")) + fraction;
    if (!exponent.isEmpty())
        text += QStringLiteral("×10") + superscript(exponent);
    return text;
}

QString literalFor(double value) {
    if (value == 0)
        value = 0;
    // Seventeen significant digits round-trip any double, so 1 ÷ 3 = × 3
    // comes back as exactly 1.
    return exponentForm(QString::number(value, 'g', 17));
}

QString formatInteger(quint64 value, int base, int wordSize, bool grouped) {
    value &= wordMask(wordSize);
    if (base == 10) {
        const qint64 number = signExtend(value, wordSize);
        const quint64 magnitude = number < 0 ? 0 - quint64(number) : quint64(number);
        QString digits = QString::number(magnitude);
        if (grouped)
            digits = groupDigits(digits, 3, QStringLiteral(","));
        if (number < 0)
            digits.prepend(grouped ? QChar(0x2212) : QLatin1Char('-'));
        return digits;
    }

    QString digits = QString::number(value, base).toUpper();
    if (grouped)
        digits = groupDigits(digits, base == 8 ? 3 : 4, QStringLiteral(" "));
    return digits;
}

QString integerLiteral(quint64 value, int base, int wordSize) {
    return formatInteger(value, base, wordSize, false);
}

QString groupDigits(const QString &digits, int size, const QString &separator) {
    QString grouped;
    int count = 0;
    for (int i = digits.size() - 1; i >= 0; --i) {
        grouped.prepend(digits.at(i));
        if (++count % size == 0 && i > 0)
            grouped.prepend(separator);
    }
    return grouped;
}

QString superscript(const QString &text) {
    static const QString digits = QStringLiteral("⁰¹²³⁴⁵⁶⁷⁸⁹");
    QString raised;
    for (const QChar c : text) {
        if (isAsciiDigit(c))
            raised += digits.at(c.unicode() - '0');
        else if (c == QLatin1Char('-') || c == QChar(0x2212))
            raised += QChar(0x207B);
        else if (c != QLatin1Char('+'))
            raised += c;
    }
    return raised;
}

QString prettyScientific(const QString &expression) {
    const QList<Token> tokens = tokenize(expression, Syntax::Scientific);
    QString text;
    for (int i = 0; i < tokens.size(); ++i) {
        const Token &token = tokens.at(i);
        switch (token.kind) {
        case TokenKind::Number:
            text += prettyNumber(token.text);
            break;
        case TokenKind::Constant:
            if (token.text == QLatin1String("pi"))
                text += QStringLiteral("π");
            else if (token.text == QLatin1String("ans"))
                text += QStringLiteral("Ans");
            else
                text += token.text;
            break;
        case TokenKind::Function:
            text += prettyFunction(token.text);
            break;
        case TokenKind::Binary:
            if (token.text == QLatin1String("^")) {
                // Small whole-number powers read best raised: 2¹⁰, x².
                if (i + 1 < tokens.size() && tokens.at(i + 1).kind == TokenKind::Number
                        && isAllDigits(tokens.at(i + 1).text)) {
                    text += superscript(tokens.at(i + 1).text);
                    ++i;
                } else {
                    text += QLatin1Char('^');
                }
            } else if (token.text == QLatin1String("+")) {
                text += QStringLiteral(" + ");
            } else if (token.text == QLatin1String("-")) {
                text += QStringLiteral(" − ");
            } else if (token.text == QLatin1String("*")) {
                text += QStringLiteral(" × ");
            } else if (token.text == QLatin1String("/")) {
                text += QStringLiteral(" ÷ ");
            } else {
                text += QLatin1Char(' ') + token.text + QLatin1Char(' ');
            }
            break;
        case TokenKind::Prefix:
            text += token.text == QLatin1String("-") ? QStringLiteral("−") : token.text;
            break;
        default:
            text += token.text;
            break;
        }
    }
    return text.trimmed();
}

QString prettyProgrammer(const QString &expression) {
    static const QHash<QString, QString> operators{
        {QStringLiteral("+"), QStringLiteral("+")},
        {QStringLiteral("-"), QStringLiteral("−")},
        {QStringLiteral("*"), QStringLiteral("×")},
        {QStringLiteral("/"), QStringLiteral("÷")},
        {QStringLiteral("%"), QStringLiteral("mod")},
        {QStringLiteral("&"), QStringLiteral("AND")},
        {QStringLiteral("|"), QStringLiteral("OR")},
        {QString(QChar(xorSign)), QStringLiteral("XOR")},
        {QString(QChar(nandSign)), QStringLiteral("NAND")},
        {QString(QChar(norSign)), QStringLiteral("NOR")},
        {QStringLiteral("<<"), QStringLiteral("<<")},
        {QStringLiteral(">>"), QStringLiteral(">>")},
        {QString(QChar(rolSign)), QStringLiteral("ROL")},
        {QString(QChar(rorSign)), QStringLiteral("ROR")},
    };

    const QList<Token> tokens = tokenize(expression, Syntax::Programmer);
    QString text;
    for (const Token &token : tokens) {
        switch (token.kind) {
        case TokenKind::Constant:
            text += QStringLiteral("Ans");
            break;
        case TokenKind::Binary:
            text += QLatin1Char(' ') + operators.value(token.text, token.text) + QLatin1Char(' ');
            break;
        case TokenKind::Prefix:
            if (token.text == QLatin1String("-"))
                text += QStringLiteral("−");
            else if (token.text == QLatin1String("~"))
                text += QStringLiteral("NOT ");
            else
                text += token.text;
            break;
        default:
            text += token.text;
            break;
        }
    }
    return text.trimmed();
}

QString convertLiterals(const QString &expression, int fromBase, int toBase, int toWordSize) {
    const QList<Token> tokens = tokenize(expression, Syntax::Programmer);
    QString converted;
    int cursor = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        const Token &token = tokens.at(i);
        converted += expression.mid(cursor, token.position - cursor);
        cursor = token.position + token.text.size();

        quint64 value = 0;
        if (token.kind != TokenKind::Number || !parseInteger(token.text, fromBase, &value)) {
            converted += token.text;
            continue;
        }

        QString literal = integerLiteral(value & wordMask(toWordSize), toBase, toWordSize);
        // A negative decimal needs parentheses wherever its minus could be
        // read as subtraction or would stack on another sign.
        if (literal.startsWith(QLatin1Char('-')) && i > 0
                && tokens.at(i - 1).kind != TokenKind::Binary
                && tokens.at(i - 1).kind != TokenKind::Open)
            literal = QLatin1Char('(') + literal + QLatin1Char(')');
        converted += literal;
    }
    converted += expression.mid(cursor);
    return converted;
}

}  // namespace Engine
