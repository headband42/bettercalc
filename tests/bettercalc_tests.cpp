#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "engine.h"
#include "theme.h"

namespace {
void press(Backend &calculator, const QString &keys) {
    const QStringList sequence = keys.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &key : sequence)
        calculator.pressKey(key);
}

double value(const QString &expression,
             Engine::AngleUnit angle = Engine::AngleUnit::Degrees) {
    const Engine::Result result = Engine::evaluate(expression, angle);
    if (!result.ok)
        qWarning() << expression << "failed:" << result.error;
    return result.value;
}

QString shown(const QString &expression,
              Engine::AngleUnit angle = Engine::AngleUnit::Degrees) {
    return Engine::formatNumber(value(expression, angle));
}

QString error(const QString &expression) {
    return Engine::evaluate(expression).error;
}

quint64 integer(const QString &expression, int base = 16, int wordSize = 64) {
    const Engine::IntegerResult result = Engine::evaluateInteger(expression, base, wordSize);
    if (!result.ok)
        qWarning() << expression << "failed:" << result.error;
    return result.value;
}

const QString xorSign(QChar(0x2295));
const QString rolSign(QChar(0x21BA));
const QString rorSign(QChar(0x21BB));
}

class BettercalcTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDirectory.path());
    }

    // Every backend starts from a clean slate rather than the last test's
    // saved mode and history.
    void init() { QSettings().clear(); }

    // Engine: scientific

    void followsPrecedence() {
        QCOMPARE(shown("2+3*4"), QStringLiteral("14"));
        QCOMPARE(shown("10-4/2"), QStringLiteral("8"));
        QCOMPARE(shown("2^3^2"), QStringLiteral("512"));
        QCOMPARE(shown("-2^2"), QStringLiteral("-4"));
        QCOMPARE(shown("2^-1"), QStringLiteral("0.5"));
    }

    void multipliesImplicitly() {
        QCOMPARE(shown("2(3+4)"), QStringLiteral("14"));
        QCOMPARE(shown("(1+2)(3+4)"), QStringLiteral("21"));
        QCOMPARE(shown("2pi"), Engine::formatNumber(2 * 3.14159265358979323846));
        QCOMPARE(shown("3sqrt(16)"), QStringLiteral("12"));
    }

    void closesParenthesesAtTheEnd() {
        QCOMPARE(shown("2*(3+4"), QStringLiteral("14"));
        QCOMPARE(shown("sqrt(sqrt(16"), QStringLiteral("2"));
    }

    void takesPercentOfTheRunningTotal() {
        QCOMPARE(shown("200+10%"), QStringLiteral("220"));
        QCOMPARE(shown("200-10%"), QStringLiteral("180"));
        QCOMPARE(shown("200*10%"), QStringLiteral("20"));
        QCOMPARE(shown("50%"), QStringLiteral("0.5"));
    }

    void givesExactQuarterTurns() {
        QCOMPARE(shown("sin(30)"), QStringLiteral("0.5"));
        QCOMPARE(shown("sin(180)"), QStringLiteral("0"));
        QCOMPARE(shown("cos(90)"), QStringLiteral("0"));
        QCOMPARE(shown("tan(45)"), QStringLiteral("1"));
        QCOMPARE(shown("sin(pi)", Engine::AngleUnit::Radians), QStringLiteral("0"));
        QCOMPARE(shown("sin(100)", Engine::AngleUnit::Gradians), QStringLiteral("1"));
        QCOMPARE(error("tan(90)"), QStringLiteral("tan is undefined here"));
        // Tiny angles are not mistaken for zero.
        QCOMPARE(shown("sin(1E-13)", Engine::AngleUnit::Radians), QStringLiteral("1E-13"));
    }

    void invertsTrigInTheAngleUnit() {
        QCOMPARE(shown("asin(1)"), QStringLiteral("90"));
        QCOMPARE(shown("atan(1)"), QStringLiteral("45"));
        QCOMPARE(shown("acos(-1)", Engine::AngleUnit::Radians),
                 Engine::formatNumber(3.14159265358979323846));
        QCOMPARE(error("asin(2)"), QStringLiteral("Outside the domain of sin⁻¹"));
    }

    void evaluatesFunctions() {
        QCOMPARE(shown("sqrt(16)"), QStringLiteral("4"));
        QCOMPARE(shown("cbrt(-27)"), QStringLiteral("-3"));
        QCOMPARE(shown("ln(e)"), QStringLiteral("1"));
        QCOMPARE(shown("log(1000)"), QStringLiteral("3"));
        QCOMPARE(shown("log2(8)"), QStringLiteral("3"));
        QCOMPARE(shown("abs(-3)"), QStringLiteral("3"));
        QCOMPARE(shown("sinh(0)+cosh(0)"), QStringLiteral("1"));
        QCOMPARE(error("sqrt(-1)"), QStringLiteral("Square root of a negative number"));
        QCOMPARE(error("ln(0)"), QStringLiteral("Outside the domain of ln"));
    }

    void computesFactorials() {
        QCOMPARE(shown("5!"), QStringLiteral("120"));
        QCOMPARE(shown("0!"), QStringLiteral("1"));
        QCOMPARE(shown("0.5!"), QStringLiteral("0.886226925452758"));
        QCOMPARE(error("(-1)!"), QStringLiteral("Undefined for negative integers"));
        QCOMPARE(error("171!"), QStringLiteral("Number too large"));
    }

    void takesOddRootsOfNegatives() {
        QCOMPARE(shown("(-8)^(1/3)"), QStringLiteral("-2"));
        QCOMPARE(error("(-8)^0.5"), QStringLiteral("Not a real number"));
    }

    void reportsWhatWentWrong() {
        QCOMPARE(error("1/0"), QStringLiteral("Can't divide by zero"));
        QCOMPARE(error("5mod0"), QStringLiteral("Can't divide by zero"));
        QCOMPARE(error("0^-1"), QStringLiteral("Can't divide by zero"));
        QCOMPARE(error("foo(2)"), QStringLiteral("Unknown function “foo”"));
        QCOMPARE(error("2+bar"), QStringLiteral("Unknown name “bar”"));
        QCOMPARE(error("2)"), QStringLiteral("Unmatched “)”"));
        QCOMPARE(error("10^400"), QStringLiteral("Number too large"));

        QVERIFY(Engine::evaluate("2+").incomplete);
        QVERIFY(Engine::evaluate("si").incomplete);
        QVERIFY(!Engine::evaluate("1/0").incomplete);
    }

    void usesFlooredModulo() {
        QCOMPARE(shown("7mod3"), QStringLiteral("1"));
        QCOMPARE(shown("-7mod3"), QStringLiteral("2"));
    }

    void hidesFloatNoise() {
        QCOMPARE(shown("0.1+0.2"), QStringLiteral("0.3"));
        QCOMPARE(shown("sqrt(2)^2"), QStringLiteral("2"));
    }

    void formatsNumbers() {
        QCOMPARE(Engine::formatDisplay(1234567.5), QStringLiteral("1,234,567.5"));
        QCOMPARE(Engine::formatDisplay(-1e20), QStringLiteral("−1×10²⁰"));
        QCOMPARE(Engine::formatDisplay(1.5e-7), QStringLiteral("1.5×10⁻⁷"));
        QCOMPARE(Engine::formatNumber(1e20), QStringLiteral("1E20"));
        QCOMPARE(Engine::formatNumber(-0.0), QStringLiteral("0"));
        QCOMPARE(value(Engine::literalFor(1.0 / 3)) * 3, 1.0);
    }

    void prettyPrintsExpressions() {
        QCOMPARE(Engine::prettyScientific("2*sqrt(9)+pi"), QStringLiteral("2 × √(9) + π"));
        QCOMPARE(Engine::prettyScientific("2^10"), QStringLiteral("2¹⁰"));
        QCOMPARE(Engine::prettyScientific("-5^-1"), QStringLiteral("−5^−1"));
        QCOMPARE(Engine::prettyScientific("1234.5-ans"), QStringLiteral("1,234.5 − Ans"));
        QCOMPARE(Engine::prettyScientific("asin(1)/cbrt(8)"), QStringLiteral("sin⁻¹(1) ÷ ³√(8)"));
        QCOMPARE(Engine::prettyScientific("0.33333333333333331"),
                 QStringLiteral("0.333333333333333"));
        QCOMPARE(Engine::prettyProgrammer("FF&@<<2"), QStringLiteral("FF AND Ans << 2"));
        QCOMPARE(Engine::prettyProgrammer("~5%3"), QStringLiteral("NOT 5 mod 3"));
    }

    // Engine: programmer

    void calculatesWithIntegers() {
        QCOMPARE(integer("FF+1"), quint64(0x100));
        QCOMPARE(integer("FF&F0"), quint64(0xF0));
        QCOMPARE(integer("F0|0F"), quint64(0xFF));
        QCOMPARE(integer("FF" + xorSign + "0F"), quint64(0xF0));
        QCOMPARE(integer("1<<3"), quint64(8));
        // Shifts and arithmetic bind tighter than AND, AND tighter than XOR.
        QCOMPARE(integer("1+2&3", 10), quint64(3));
        QCOMPARE(integer("1|2" + xorSign + "3", 10), quint64(1));
    }

    void wrapsToTheWordSize() {
        QCOMPARE(integer("-1", 16, 8), quint64(0xFF));
        QCOMPARE(integer("~0", 16, 8), quint64(0xFF));
        QCOMPARE(integer("FF+1", 16, 8), quint64(0));
        QCOMPARE(Engine::signExtend(0xFF, 8), qint64(-1));
        QCOMPARE(Engine::signExtend(0x7F, 8), qint64(127));
    }

    void dividesWithSign() {
        QCOMPARE(qint64(integer("-7/2", 10)), qint64(-3));
        QCOMPARE(qint64(integer("-7%2", 10)), qint64(-1));
        QCOMPARE(qint64(integer("-8>>1", 10)), qint64(-4));
        QCOMPARE(integer("-9223372036854775808/-1", 10), quint64(0x8000000000000000));
        QCOMPARE(Engine::evaluateInteger("1/0", 10, 64).error, QStringLiteral("Can't divide by zero"));
    }

    void rotatesWithinTheWord() {
        QCOMPARE(integer("80" + rolSign + "1", 16, 8), quint64(0x01));
        QCOMPARE(integer("01" + rorSign + "1", 16, 8), quint64(0x80));
        QCOMPARE(integer("1" + rolSign + "-1", 16, 8), quint64(0x80));
    }

    void rejectsDigitsOutsideTheBase() {
        QCOMPARE(Engine::evaluateInteger("19", 8, 64).error,
                 QStringLiteral("“9” isn't an octal digit"));
        QCOMPARE(Engine::evaluateInteger("1FFFFFFFFFFFFFFFF", 16, 64).error,
                 QStringLiteral("Number too large"));
        QCOMPARE(Engine::evaluateInteger("1<<-1", 10, 64).error,
                 QStringLiteral("Can't shift by a negative amount"));
    }

    void formatsIntegers() {
        QCOMPARE(Engine::formatInteger(0xFF, 2, 8, true), QStringLiteral("1111 1111"));
        QCOMPARE(Engine::formatInteger(~quint64(0), 10, 64, true), QStringLiteral("−1"));
        QCOMPARE(Engine::formatInteger(~quint64(0), 16, 64, true),
                 QStringLiteral("FFFF FFFF FFFF FFFF"));
        QCOMPARE(Engine::formatInteger(1234567, 10, 64, true), QStringLiteral("1,234,567"));
        QCOMPARE(Engine::formatInteger(8, 8, 64, true), QStringLiteral("10"));
    }

    void convertsLiteralsBetweenBases() {
        QCOMPARE(Engine::convertLiterals("FF+10", 16, 10, 64), QStringLiteral("255+16"));
        QCOMPARE(Engine::convertLiterals("FF", 16, 10, 8), QStringLiteral("-1"));
        QCOMPARE(Engine::convertLiterals("5*FF", 16, 10, 8), QStringLiteral("5*-1"));
        QCOMPARE(Engine::convertLiterals("-FF", 16, 10, 8), QStringLiteral("-(-1)"));
        QCOMPARE(Engine::convertLiterals("255", 10, 2, 64), QStringLiteral("11111111"));
    }

    // Backend: typing

    void calculatesWithPrecedence() {
        Backend calculator;
        press(calculator, "4 2 * 3 + 7 =");
        QCOMPARE(calculator.display(), QStringLiteral("133"));
        QCOMPARE(calculator.expression(), QStringLiteral("42 × 3 + 7 ="));
        QCOMPARE(calculator.displayState(), QStringLiteral("result"));
    }

    void previewsWhileTyping() {
        Backend calculator;
        press(calculator, "1 2 3 4");
        QCOMPARE(calculator.display(), QStringLiteral("1,234"));
        QCOMPARE(calculator.expression(), QString());
        QCOMPARE(calculator.displayState(), QStringLiteral("entry"));

        press(calculator, "+ 6");
        QCOMPARE(calculator.expression(), QStringLiteral("1,234 + 6"));
        QCOMPARE(calculator.display(), QStringLiteral("1,240"));
        QCOMPARE(calculator.displayState(), QStringLiteral("preview"));

        // A dangling operator keeps the last good preview.
        press(calculator, "*");
        QCOMPARE(calculator.display(), QStringLiteral("1,240"));
    }

    void chainsFromTheExactResult() {
        Backend calculator;
        press(calculator, "1 / 3 = * 3 =");
        QCOMPARE(calculator.display(), QStringLiteral("1"));
        QCOMPARE(calculator.expression(), QStringLiteral("0.333333333333333 × 3 ="));
    }

    void startsFreshAfterAResult() {
        Backend calculator;
        press(calculator, "2 + 3 = 7");
        QCOMPARE(calculator.display(), QStringLiteral("7"));
        QCOMPARE(calculator.rawExpression(), QStringLiteral("7"));

        press(calculator, "clear 1 2 + 3 = backspace");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("1"));
    }

    void replacesPendingOperators() {
        Backend calculator;
        press(calculator, "5 + * 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("10"));

        press(calculator, "clear 5 * - 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("−10"));
    }

    void recoversFromErrors() {
        Backend calculator;
        press(calculator, "1 / 0 =");
        QCOMPARE(calculator.displayState(), QStringLiteral("error"));
        QCOMPARE(calculator.display(), QStringLiteral("Can't divide by zero"));
        QCOMPARE(calculator.expression(), QStringLiteral("1 ÷ 0"));

        // The expression stays editable, so the mistake can be fixed.
        press(calculator, "backspace 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("0.5"));
    }

    void togglesTheSign() {
        Backend calculator;
        press(calculator, "4 + 2 sign");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("4+-2"));
        QCOMPARE(calculator.display(), QStringLiteral("2"));
        press(calculator, "sign");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("4+2"));

        press(calculator, "clear sign 5");
        QCOMPARE(calculator.display(), QStringLiteral("−5"));

        press(calculator, "clear 3 = sign =");
        QCOMPARE(calculator.display(), QStringLiteral("−3"));
    }

    void limitsEntryToFifteenDigits() {
        Backend calculator;
        press(calculator, "1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("123456789012345"));

        press(calculator, "clear 0 . 0 0 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6");
        QCOMPARE(Engine::significantDigits(calculator.rawExpression()), 15);
    }

    void handlesDecimals() {
        Backend calculator;
        press(calculator, ". 5 + . 2 5 =");
        QCOMPARE(calculator.display(), QStringLiteral("0.75"));

        press(calculator, "clear 1 . 5 . 5");
        QCOMPARE(calculator.display(), QStringLiteral("1.55"));

        press(calculator, "clear 0 .");
        QCOMPARE(calculator.display(), QStringLiteral("0."));
    }

    void showsPendingParentheses() {
        Backend calculator;
        press(calculator, "( 2 + 3");
        QCOMPARE(calculator.pendingParentheses(), 1);
        QCOMPARE(calculator.display(), QStringLiteral("5"));

        press(calculator, ") * 4 =");
        QCOMPARE(calculator.display(), QStringLiteral("20"));

        press(calculator, "clear ( 1 + 2 =");
        QCOMPARE(calculator.expression(), QStringLiteral("(1 + 2) ="));
    }

    void takesPercentages() {
        Backend calculator;
        press(calculator, "2 0 0 + 1 0 % =");
        QCOMPARE(calculator.display(), QStringLiteral("220"));
    }

    // Backend: scientific keys

    void appliesFunctionsToTheOperand() {
        Backend calculator;
        calculator.setMode(Backend::Scientific);
        press(calculator, "9 sqrt");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("sqrt(9)"));
        QCOMPARE(calculator.display(), QStringLiteral("3"));

        press(calculator, "clear 1 6 = sqrt =");
        QCOMPARE(calculator.display(), QStringLiteral("4"));

        press(calculator, "clear 2 + sqrt 9 =");
        QCOMPARE(calculator.display(), QStringLiteral("5"));

        press(calculator, "clear 2 + 3 sign sqrt");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("2+sqrt(-3)"));

        // A group lends the function its parentheses.
        press(calculator, "clear ( 3 square + 4 square ) sqrt");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("sqrt(3^2+4^2)"));
        QCOMPARE(calculator.expression(), QStringLiteral("√(3² + 4²)"));
        QCOMPARE(calculator.display(), QStringLiteral("5"));
        press(calculator, "clear ( 3 ) ! sqrt");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("sqrt((3)!)"));
    }

    void switchesToSecondFunctions() {
        Backend calculator;
        calculator.setMode(Backend::Scientific);
        press(calculator, "2nd sin 1 =");
        QCOMPARE(calculator.display(), QStringLiteral("90"));

        press(calculator, "clear hyp sin");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("asinh("));
        // 2nd stays on until pressed again; hyp is off again here.
        press(calculator, "clear hyp 2 7 sqrt =");
        QCOMPARE(calculator.display(), QStringLiteral("3"));
    }

    void raisesPowers() {
        Backend calculator;
        calculator.setMode(Backend::Scientific);
        press(calculator, "3 sign square =");
        QCOMPARE(calculator.display(), QStringLiteral("9"));

        press(calculator, "clear 5 ^ 2 square");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("(5^2)^2"));
        QCOMPARE(calculator.display(), QStringLiteral("625"));

        press(calculator, "clear 3 pow10 =");
        QCOMPARE(calculator.display(), QStringLiteral("1,000"));

        press(calculator, "clear 2 sign pow10 =");
        QCOMPARE(calculator.display(), QStringLiteral("0.01"));

        press(calculator, "clear 2 7 root 3 =");
        QCOMPARE(calculator.display(), QStringLiteral("3"));

        press(calculator, "clear 4 inverse =");
        QCOMPARE(calculator.display(), QStringLiteral("0.25"));

        press(calculator, "clear 1 exp =");
        QCOMPARE(calculator.display(), QStringLiteral("2.71828182845905"));

        // After a negative result, x² squares the whole value.
        press(calculator, "clear 3 sign = square =");
        QCOMPARE(calculator.display(), QStringLiteral("9"));
    }

    void insertsConstants() {
        Backend calculator;
        calculator.setMode(Backend::Scientific);
        press(calculator, "2 pi");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("2pi"));

        press(calculator, "clear pi e");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("pi*e"));

        press(calculator, "clear 1 . 5 ee 3 =");
        QCOMPARE(calculator.display(), QStringLiteral("1,500"));

        press(calculator, "clear 2 + 3 = ans * 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("10"));
    }

    void followsTheAngleUnit() {
        Backend calculator;
        calculator.setMode(Backend::Scientific);
        QCOMPARE(calculator.angleUnit(), QStringLiteral("DEG"));
        calculator.cycleAngleUnit();
        QCOMPARE(calculator.angleUnit(), QStringLiteral("RAD"));
        press(calculator, "pi sin");
        QCOMPARE(calculator.display(), QStringLiteral("0"));

        // The unit is remembered.
        Backend reopened;
        QCOMPARE(reopened.angleUnit(), QStringLiteral("RAD"));
    }

    void remembersInMemory() {
        Backend calculator;
        calculator.setMode(Backend::Scientific);
        press(calculator, "5 ms clear mr + 1 =");
        QCOMPARE(calculator.display(), QStringLiteral("6"));
        QVERIFY(calculator.hasMemory());

        press(calculator, "mplus clear mr");
        QCOMPARE(calculator.display(), QStringLiteral("11"));

        press(calculator, "mc");
        QVERIFY(!calculator.hasMemory());
    }

    void typesExpressionsFromTheKeyboard() {
        Backend calculator;
        calculator.setMode(Backend::Scientific);
        calculator.typeText("sin(30)=");
        QCOMPARE(calculator.display(), QStringLiteral("0.5"));

        press(calculator, "clear");
        calculator.typeText("2^10");
        QCOMPARE(calculator.expression(), QStringLiteral("2¹⁰"));
        QCOMPARE(calculator.display(), QStringLiteral("1,024"));

        // A half-typed name keeps the preview steady instead of erroring.
        press(calculator, "clear");
        calculator.typeText("2+sq");
        QCOMPARE(calculator.displayState(), QStringLiteral("preview"));
        QCOMPARE(calculator.display(), QStringLiteral("2"));
        calculator.typeText("rt(9)=");
        QCOMPARE(calculator.display(), QStringLiteral("5"));

        press(calculator, "clear");
        calculator.typeText("2+sq=");
        QCOMPARE(calculator.display(), QStringLiteral("Unknown name “sq”"));
    }

    void deletesNamesWhole() {
        Backend calculator;
        calculator.setMode(Backend::Scientific);
        press(calculator, "2 + sin backspace");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("2+"));
        press(calculator, "pi backspace");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("2+"));
    }

    // Backend: undo, history, clipboard

    void undoesAndRedoes() {
        Backend calculator;
        press(calculator, "1 2 +");
        QVERIFY(calculator.canUndo());
        calculator.undo();
        QCOMPARE(calculator.rawExpression(), QStringLiteral("12"));
        calculator.redo();
        QCOMPARE(calculator.rawExpression(), QStringLiteral("12+"));

        press(calculator, "3 = clear");
        calculator.undo();
        QCOMPARE(calculator.display(), QStringLiteral("15"));
        QCOMPARE(calculator.displayState(), QStringLiteral("result"));
    }

    void keepsHistory() {
        {
            Backend calculator;
            press(calculator, "2 + 2 = clear 3 * 3 =");
            QCOMPARE(calculator.history().size(), 2);
            const QVariantMap latest = calculator.history().first().toMap();
            QCOMPARE(latest.value("expression").toString(), QStringLiteral("3 × 3"));
            QCOMPARE(latest.value("result").toString(), QStringLiteral("9"));

            press(calculator, "clear 1 +");
            calculator.recallHistory(1);
            QCOMPARE(calculator.rawExpression(), QStringLiteral("1+4"));
        }

        Backend reopened;
        QCOMPARE(reopened.history().size(), 2);
        reopened.clearHistory();
        QVERIFY(reopened.history().isEmpty());
    }

    void pastesNumbersAndExpressions() {
        Backend calculator;
        calculator.pasteText(" 1,234.5 ");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("1234.5"));

        press(calculator, "clear");
        calculator.pasteText("3,5");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("3.5"));

        press(calculator, "clear");
        calculator.pasteText("2 × (3 − 1)");
        QCOMPARE(calculator.display(), QStringLiteral("4"));

        press(calculator, "clear");
        calculator.pasteText("1.5e+3");
        QCOMPARE(calculator.display(), QStringLiteral("1.5E3"));
        QCOMPARE(calculator.copyText(), QStringLiteral("1500"));
    }

    void copiesPlainNumbers() {
        Backend calculator;
        press(calculator, "1 / 3 =");
        QCOMPARE(calculator.copyText(), QStringLiteral("0.333333333333333"));
        press(calculator, "clear 1 2 3 4 * 1 0 0 0 =");
        QCOMPARE(calculator.copyText(), QStringLiteral("1234000"));
        press(calculator, "clear 1 / 0 =");
        QCOMPARE(calculator.copyText(), QString());
    }

    // Backend: programmer mode

    void carriesValuesBetweenModes() {
        Backend calculator;
        press(calculator, "2 . 7 =");
        calculator.setMode(Backend::Programmer);
        QCOMPARE(calculator.rawExpression(), QStringLiteral("2"));

        calculator.setBase(16);
        press(calculator, "clear F F");
        calculator.setMode(Backend::Basic);
        QCOMPARE(calculator.rawExpression(), QStringLiteral("255"));
    }

    void showsEveryBase() {
        Backend calculator;
        calculator.setMode(Backend::Programmer);
        calculator.setBase(16);
        press(calculator, "F F + 1 =");
        QCOMPARE(calculator.display(), QStringLiteral("100"));
        QCOMPARE(calculator.hexValue(), QStringLiteral("100"));
        QCOMPARE(calculator.decValue(), QStringLiteral("256"));
        QCOMPARE(calculator.octValue(), QStringLiteral("400"));
        QCOMPARE(calculator.binValue(), QStringLiteral("1 0000 0000"));
        QVERIFY(calculator.bits().endsWith(QStringLiteral("100000000")));
        QCOMPARE(calculator.bits().size(), 64);
    }

    void acceptsOnlyDigitsOfTheBase() {
        Backend calculator;
        calculator.setMode(Backend::Programmer);
        calculator.setBase(2);
        press(calculator, "1 2 0");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("10"));
        QVERIFY(!calculator.digitEnabled("2"));
        QVERIFY(calculator.digitEnabled("1"));

        calculator.setBase(16);
        QVERIFY(calculator.digitEnabled("F"));
        calculator.typeText("a");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("2A"));
    }

    void convertsTheExpressionWithTheBase() {
        Backend calculator;
        calculator.setMode(Backend::Programmer);
        calculator.setBase(16);
        press(calculator, "F F + 1");
        calculator.setBase(10);
        QCOMPARE(calculator.rawExpression(), QStringLiteral("255+1"));
        QCOMPARE(calculator.display(), QStringLiteral("256"));
        calculator.undo();
        QCOMPARE(calculator.base(), 16);
        QCOMPARE(calculator.rawExpression(), QStringLiteral("FF+1"));
    }

    void truncatesToTheWordSize() {
        Backend calculator;
        calculator.setMode(Backend::Programmer);
        calculator.setBase(16);
        calculator.setWordSize(8);
        QCOMPARE(calculator.wordName(), QStringLiteral("BYTE"));
        press(calculator, "F F");
        QCOMPARE(calculator.decValue(), QStringLiteral("−1"));
        press(calculator, "F");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("FF"));

        calculator.cycleWordSize();
        QCOMPARE(calculator.wordSize(), 64);

        calculator.setBase(10);
        calculator.setWordSize(8);
        press(calculator, "clear 1 2 8");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("12"));
        press(calculator, "clear - 1 2 8");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("-128"));
    }

    void togglesBits() {
        Backend calculator;
        calculator.setMode(Backend::Programmer);
        calculator.setBase(16);
        press(calculator, "F");
        calculator.toggleBit(4);
        QCOMPARE(calculator.rawExpression(), QStringLiteral("1F"));
        calculator.toggleBit(0);
        QCOMPARE(calculator.rawExpression(), QStringLiteral("1E"));

        calculator.setWordSize(8);
        calculator.toggleBit(12);
        QCOMPARE(calculator.rawExpression(), QStringLiteral("1E"));
    }

    void runsBitwiseOperators() {
        Backend calculator;
        calculator.setMode(Backend::Programmer);
        calculator.setBase(16);
        calculator.setWordSize(8);
        press(calculator, "0 not =");
        QCOMPARE(calculator.display(), QStringLiteral("FF"));

        press(calculator, "clear 5 not");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("~5"));
        press(calculator, "not");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("5"));

        press(calculator, "clear 8 0 rol 1 =");
        QCOMPARE(calculator.display(), QStringLiteral("1"));

        calculator.setBase(10);
        calculator.setWordSize(64);
        press(calculator, "clear 1 shl 4 =");
        QCOMPARE(calculator.display(), QStringLiteral("16"));
        press(calculator, "clear 1 6 shr 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("4"));

        press(calculator, "clear");
        calculator.typeText("6^3=");
        QCOMPARE(calculator.display(), QStringLiteral("5"));

        press(calculator, "clear 5 = ans + 1 =");
        QCOMPARE(calculator.display(), QStringLiteral("6"));

        // NOT inverts a negative value whole.
        press(calculator, "clear 5 sign not =");
        QCOMPARE(calculator.display(), QStringLiteral("4"));
    }

    void pastesPrefixedLiterals() {
        Backend calculator;
        calculator.setMode(Backend::Programmer);
        calculator.setBase(10);
        calculator.pasteText("0x1F + 0b11");
        QCOMPARE(calculator.rawExpression(), QStringLiteral("31+3"));
        QCOMPARE(calculator.display(), QStringLiteral("34"));

        calculator.setBase(16);
        press(calculator, "=");
        QCOMPARE(calculator.copyText(), QStringLiteral("22"));
    }

    void remembersModeAndBase() {
        {
            Backend calculator;
            calculator.setMode(Backend::Programmer);
            calculator.setBase(2);
            calculator.setWordSize(16);
        }
        Backend reopened;
        QCOMPARE(reopened.mode(), int(Backend::Programmer));
        QCOMPARE(reopened.base(), 2);
        QCOMPARE(reopened.wordSize(), 16);
    }

    // Theme

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QStringLiteral("#FFD60A"));
        QVERIFY(theme.dark());

        theme.setSystemDark(false);
        QVERIFY(!theme.dark());
        QCOMPARE(theme.background(), QStringLiteral("#fafafa"));
    }

    void themeReadsColors() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("mode = \"light\"\n"
                     "accent = \"#1e66f5\"\n"
                     "background = \"#eff1f5\"\n"
                     "foreground = \"#4c4f69\"\n"
                     "red = \"#d20f39\"\n");
        colors.close();

        Theme theme(dir.path());
        QVERIFY(!theme.dark());
        QCOMPARE(theme.accent(), QStringLiteral("#1e66f5"));
        QCOMPARE(theme.danger(), QStringLiteral("#d20f39"));
        // The light background reads better on the deep blue accent.
        QCOMPARE(theme.accentForeground(), QStringLiteral("#eff1f5"));
    }

    void themeGuessesModeFromTheBackground() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = '#7aa2f7'\nbackground = '#1a1b26'\nforeground = '#a9b1d6'\n");
        colors.close();

        Theme theme(dir.path());
        theme.setSystemDark(false);
        QVERIFY(theme.dark());
        QCOMPARE(theme.accentForeground(), QStringLiteral("#1a1b26"));
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(BettercalcTests)
#include "bettercalc_tests.moc"
