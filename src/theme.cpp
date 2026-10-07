// Theme watching follows Omacut and Omacalc (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <cmath>

namespace {
QHash<QString, QString> defaultColors(bool dark) {
    if (dark) {
        return {{QStringLiteral("background"), QStringLiteral("#121212")},
                {QStringLiteral("foreground"), QStringLiteral("#e8e8e8")},
                {QStringLiteral("accent"), QStringLiteral("#FFD60A")},
                {QStringLiteral("red"), QStringLiteral("#ff6b6b")}};
    }
    return {{QStringLiteral("background"), QStringLiteral("#fafafa")},
            {QStringLiteral("foreground"), QStringLiteral("#1d1d1f")},
            {QStringLiteral("accent"), QStringLiteral("#FFD60A")},
            {QStringLiteral("red"), QStringLiteral("#d03b3b")}};
}

double luminance(const QColor &color) {
    auto linear = [](double v) {
        return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4);
    };
    return .2126 * linear(color.redF()) + .7152 * linear(color.greenF())
        + .0722 * linear(color.blueF());
}

double contrast(const QColor &a, const QColor &b) {
    const double x = luminance(a) + .05;
    const double y = luminance(b) + .05;
    return x > y ? x / y : y / x;
}
}

Theme::Theme(const QString &directory, QObject *parent)
    : QObject(parent),
      m_directory(directory.isEmpty()
                      ? QDir::homePath() + QStringLiteral("/.local/state/omarchy/current")
                      : directory),
      m_colors(defaultColors(true)) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        reload();
        watch();
    });
    reload();
    watch();
}

// Text on the accent uses whichever end of the theme's own palette reads
// better on it, so a pale accent gets the theme's dark ink rather than a
// stark black.
QString Theme::accentForeground() const {
    const QColor accentColor(accent());
    const QColor backgroundColor(background());
    const QColor foregroundColor(foreground());
    return contrast(accentColor, backgroundColor) >= contrast(accentColor, foregroundColor)
        ? background()
        : foreground();
}

void Theme::setSystemDark(bool dark) {
    if (m_systemDark == dark)
        return;
    m_systemDark = dark;
    reload();
}

QHash<QString, QString> Theme::readColors(const QString &path) {
    QHash<QString, QString> colors;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return colors;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals < 0)
            continue;

        const QString key = line.left(equals).trimmed();
        QString value = line.mid(equals + 1).trimmed();
        if (value.size() >= 2
                && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                    || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
            value = value.mid(1, value.size() - 2);
        colors.insert(key, value);
    }
    return colors;
}

void Theme::reload() {
    const QHash<QString, QString> file = readColors(m_directory + QStringLiteral("/theme/colors.toml"));

    // The theme's own mode wins; failing that, the brightness of its
    // background; failing that, the desktop preference.
    bool dark = m_systemDark;
    const QString mode = file.value(QStringLiteral("mode"));
    const QColor fileBackground(file.value(QStringLiteral("background")));
    if (mode == QLatin1String("dark"))
        dark = true;
    else if (mode == QLatin1String("light"))
        dark = false;
    else if (fileBackground.isValid())
        dark = luminance(fileBackground) < .5;

    QHash<QString, QString> colors = defaultColors(dark);
    for (auto it = file.cbegin(); it != file.cend(); ++it) {
        if (QColor(it.value()).isValid())
            colors.insert(it.key(), it.value());
    }

    if (colors == m_colors && dark == m_dark)
        return;
    m_colors = colors;
    m_dark = dark;
    emit changed();
}

void Theme::watch() {
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != QLatin1String("/"))
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + QStringLiteral("/theme"),
                           m_directory + QStringLiteral("/theme/colors.toml")};
    candidates.removeDuplicates();
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);
    }
}
