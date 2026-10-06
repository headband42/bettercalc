#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "engine.h"

// The calculator's state, exposed to QML as `backend`. Keys build up an
// expression in the engine's notation; the display shows it pretty-printed
// along with a live preview of its value, and = commits that value to the
// history.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)

    // The display: the expression line above, the big line below, and what
    // the big line is showing — "entry", "preview", "result" or "error".
    Q_PROPERTY(QString expression READ expression NOTIFY stateChanged)
    Q_PROPERTY(int pendingParentheses READ pendingParentheses NOTIFY stateChanged)
    Q_PROPERTY(QString display READ display NOTIFY stateChanged)
    Q_PROPERTY(QString displayState READ displayState NOTIFY stateChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY stateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY stateChanged)

    Q_PROPERTY(QString angleUnit READ angleUnit NOTIFY settingsChanged)
    Q_PROPERTY(bool second READ second NOTIFY settingsChanged)
    Q_PROPERTY(bool hyperbolic READ hyperbolic NOTIFY settingsChanged)
    Q_PROPERTY(bool hasMemory READ hasMemory NOTIFY settingsChanged)

    Q_PROPERTY(int base READ base WRITE setBase NOTIFY settingsChanged)
    Q_PROPERTY(int wordSize READ wordSize WRITE setWordSize NOTIFY settingsChanged)
    Q_PROPERTY(QString wordName READ wordName NOTIFY settingsChanged)
    Q_PROPERTY(QString hexValue READ hexValue NOTIFY stateChanged)
    Q_PROPERTY(QString decValue READ decValue NOTIFY stateChanged)
    Q_PROPERTY(QString octValue READ octValue NOTIFY stateChanged)
    Q_PROPERTY(QString binValue READ binValue NOTIFY stateChanged)
    // Sixty-four characters, most significant bit first.
    Q_PROPERTY(QString bits READ bits NOTIFY stateChanged)

    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(qreal textScale READ textScale WRITE setTextScale NOTIFY textScaleChanged)

public:
    enum Mode { Basic = 0, Scientific = 1, Programmer = 2 };
    Q_ENUM(Mode)

    explicit Backend(QObject *parent = nullptr);

    int mode() const { return m_mode; }
    void setMode(int mode);

    QString expression() const { return m_expressionLine; }
    int pendingParentheses() const { return m_pending; }
    QString display() const { return m_display; }
    QString displayState() const { return m_displayState; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    QString angleUnit() const;
    bool second() const { return m_second; }
    bool hyperbolic() const { return m_hyperbolic; }
    bool hasMemory() const { return m_hasMemory; }

    int base() const { return m_base; }
    void setBase(int base);
    int wordSize() const { return m_wordSize; }
    void setWordSize(int wordSize);
    QString wordName() const;
    QString hexValue() const { return Engine::formatInteger(m_currentInteger, 16, m_wordSize, true); }
    QString decValue() const { return Engine::formatInteger(m_currentInteger, 10, m_wordSize, true); }
    QString octValue() const { return Engine::formatInteger(m_currentInteger, 8, m_wordSize, true); }
    QString binValue() const { return Engine::formatInteger(m_currentInteger, 2, m_wordSize, true); }
    QString bits() const;

    QVariantList history() const { return m_history; }

    qreal textScale() const { return m_textScale; }
    void setTextScale(qreal textScale);

    // The raw expression, for tests.
    QString rawExpression() const { return m_expression; }
    // What Ctrl+C puts on the clipboard.
    QString copyText() const;
    void pasteText(const QString &text);

    Q_INVOKABLE void pressKey(const QString &key);
    Q_INVOKABLE void typeText(const QString &text);
    Q_INVOKABLE void toggleBit(int index);
    Q_INVOKABLE void cycleWordSize();
    Q_INVOKABLE void cycleAngleUnit();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void copyResult() const;
    Q_INVOKABLE void paste();
    Q_INVOKABLE void recallHistory(int index);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE bool digitEnabled(const QString &digit) const;
    Q_INVOKABLE QVariantMap windowSize(int mode) const;
    Q_INVOKABLE void saveWindowSize(int mode, int width, int height, bool maximized);

signals:
    void modeChanged();
    void stateChanged();
    void settingsChanged();
    void historyChanged();
    void textScaleChanged();
    // A calculation just finished; the display uses it to settle the result.
    void evaluated();

private:
    struct Snapshot {
        QString expression;
        bool justEvaluated = false;
        QString evaluated;
        double result = 0;
        quint64 integerResult = 0;
        QString error;
        int base = 10;
        int wordSize = 64;

        bool operator==(const Snapshot &other) const;
    };

    bool programmer() const { return m_mode == Programmer; }
    Engine::Syntax syntax() const;
    QList<Engine::Token> tokens() const;

    bool applyKey(const QString &key);
    bool applyScientificKey(const QString &key);
    bool applyProgrammerKey(const QString &key);
    void applyCharacter(QChar character);

    void clearAll();
    void beginEntry();
    void appendDigit(QChar digit);
    void appendDecimal();
    void appendExponent();
    void appendBinary(const QString &op);
    void appendPostfix(const QString &op);
    void appendConstant(const QString &name);
    void appendLetter(QChar letter);
    void applyFunction(const QString &name);
    void applyPowerPrefix(const QString &prefix);
    void closeParenthesis();
    void toggleUnary(const QString &sign);
    void insertValue(const QString &literal);
    void backspace();
    void equals();
    void memory(const QString &key);

    bool literalFits(const QString &literal, const QList<Engine::Token> &tokens) const;
    QString resultLiteral() const;
    QString editableResult() const;
    QString pretty(const QString &expression) const;
    QString formatValue(double value, quint64 integer) const;
    Engine::Result evaluateLeniently(QString expression, bool preview, QString *used) const;
    Engine::IntegerResult evaluateIntegerLeniently(QString expression, QString *used) const;
    QString sanitizeScientific(const QString &text) const;
    QString sanitizeProgrammer(const QString &text) const;
    quint64 toInteger(double value) const;

    Snapshot snapshot() const;
    void restore(const Snapshot &snapshot);
    void record(const Snapshot &before);
    void refresh();
    void addHistory();
    void saveHistory();

    int m_mode = Basic;
    QString m_expression;
    bool m_justEvaluated = false;
    QString m_evaluated;
    double m_result = 0;
    quint64 m_integerResult = 0;
    double m_ans = 0;
    quint64 m_integerAns = 0;
    QString m_error;

    Engine::AngleUnit m_angle = Engine::AngleUnit::Degrees;
    bool m_second = false;
    bool m_hyperbolic = false;
    double m_memory = 0;
    bool m_hasMemory = false;

    int m_base = 10;
    int m_wordSize = 64;

    QVariantList m_history;
    QList<Snapshot> m_undo;
    QList<Snapshot> m_redo;
    qreal m_textScale = 1.0;

    // Derived by refresh() for the display.
    QString m_expressionLine;
    int m_pending = 0;
    QString m_display = QStringLiteral("0");
    QString m_displayState = QStringLiteral("entry");
    QString m_lastPreview = QStringLiteral("0");
    double m_currentValue = 0;
    quint64 m_currentInteger = 0;
    bool m_hasCurrent = false;
};
