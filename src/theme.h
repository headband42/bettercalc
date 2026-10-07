#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

// Follows the Omarchy theme live: the palette in
// ~/.local/state/omarchy/current/theme/colors.toml, re-read whenever a theme
// switch replaces it. Without Omarchy, the desktop's light or dark preference
// picks a neutral palette instead.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QString danger READ danger NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr);

    bool dark() const { return m_dark; }
    QString background() const { return m_colors.value(QStringLiteral("background")); }
    QString foreground() const { return m_colors.value(QStringLiteral("foreground")); }
    QString accent() const { return m_colors.value(QStringLiteral("accent")); }
    QString danger() const { return m_colors.value(QStringLiteral("red")); }
    QString accentForeground() const;

    // The desktop's preference, used when the theme doesn't say.
    void setSystemDark(bool dark);

    static QHash<QString, QString> readColors(const QString &path);

signals:
    void changed();

private:
    void reload();
    void watch();

    QString m_directory;
    bool m_systemDark = true;
    bool m_dark = true;
    QHash<QString, QString> m_colors;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
