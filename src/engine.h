#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

// The calculator's arithmetic, kept free of Qt Quick so it can be tested on
// its own. Expressions are plain strings in an internal notation the keypad
// builds up ("2*sqrt(9)+pi", or "FF&@" in programmer mode); the backend shows
// them to people through the pretty printers at the bottom of this file.
namespace Engine {

enum class Syntax { Scientific, Programmer };
enum class AngleUnit { Degrees, Radians, Gradians };

enum class TokenKind {
    Number,    // 3.14, 1.5E3, FF
    Constant,  // pi, e, ans — or @, Ans in programmer mode
    Name,      // letters that name nothing yet, usually a function half typed
    Function,  // "sin(": the name and its opening parenthesis travel together
    Open,      // (
    Close,     // )
    Binary,    // + - * / ^ mod, and the programmer operators
    Prefix,    // unary - + and programmer NOT (~)
    Postfix,   // ! and scientific %
    Invalid    // a character the syntax has no use for
};

struct Token {
    TokenKind kind;
    QString text;
    int position;
};

QList<Token> tokenize(const QString &source, Syntax syntax);

// The last token closes off a complete operand, so a postfix operator or a
// binary operator can follow it.
bool endsWithOperand(const QList<Token> &tokens);

// Index of the first token of the operand the expression ends with — the
// thing a function key like √ or a sign toggle applies to — or -1 when the
// expression ends in an operator. Power chains count as one operand, so √
// after 2^3 wraps the whole power. With includeSign, a unary minus directly
// in front of the operand is part of it.
int operandStart(const QList<Token> &tokens, bool includeSign = false);

// Opening parentheses still waiting for their closing partner.
int openParentheses(const QList<Token> &tokens);

// Machine-inserted literals (chained results, recalled values) carry 17
// significant digits so they round-trip exactly; anything typed is capped at
// 15, so this tells the two apart.
int significantDigits(const QString &literal);

struct Result {
    bool ok = false;
    // The input ran out before the expression was whole ("2 +", "sin(").
    // Dropping the dangling tail and trying again can still give an answer.
    bool incomplete = false;
    double value = 0;
    QString error;
};

Result evaluate(const QString &expression, AngleUnit angle = AngleUnit::Degrees, double ans = 0);

struct IntegerResult {
    bool ok = false;
    bool incomplete = false;
    quint64 value = 0;  // The bit pattern, masked to the word size.
    QString error;
};

IntegerResult evaluateInteger(const QString &expression, int base, int wordSize,
                              quint64 ans = 0);

quint64 wordMask(int wordSize);
qint64 signExtend(quint64 value, int wordSize);

// Reads a programmer literal in the given base. Fails on digits the base
// doesn't have or on values that don't fit in 64 bits.
bool parseInteger(const QString &digits, int base, quint64 *value);

// Decimal results for copying and for editing: fifteen significant digits,
// no grouping, an E exponent.
QString formatNumber(double value);
// Decimal results for reading: grouped thousands, a typographic minus, and
// ×10ⁿ exponents.
QString formatDisplay(double value);
// Full round-trip precision, for putting a result back into an expression.
QString literalFor(double value);

// Programmer values: signed in decimal, the raw two's complement bit
// pattern in the other bases.
QString formatInteger(quint64 value, int base, int wordSize, bool grouped);
QString integerLiteral(quint64 value, int base, int wordSize);

QString groupDigits(const QString &digits, int size, const QString &separator);
QString superscript(const QString &text);

QString prettyScientific(const QString &expression);
QString prettyProgrammer(const QString &expression);

// Rewrites every literal of a programmer expression from one base and word
// size to another, so switching from HEX to DEC keeps the same values.
QString convertLiterals(const QString &expression, int fromBase, int toBase, int toWordSize);
}
