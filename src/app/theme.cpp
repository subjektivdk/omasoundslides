// Theme following adapted from Omacut (https://github.com/omacom-io/omacut, MIT).

#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {

QString omarchyCurrentDir()
{
    return QDir::homePath() + QStringLiteral("/.local/state/omarchy/current");
}

QString omarchyColorsPath()
{
    return omarchyCurrentDir() + QStringLiteral("/theme/colors.toml");
}

// A neutral dark palette for systems without an Omarchy theme.
const QHash<QString, QString> Fallback = {
    {QStringLiteral("mode"), QStringLiteral("dark")},
    {QStringLiteral("accent"), QStringLiteral("#FFD60A")},
    {QStringLiteral("background"), QStringLiteral("#0e0e10")},
    {QStringLiteral("foreground"), QStringLiteral("#d6d6da")},
    {QStringLiteral("bright_foreground"), QStringLiteral("#ffffff")},
    {QStringLiteral("dark_foreground"), QStringLiteral("#8a8a90")},
    {QStringLiteral("red"), QStringLiteral("#ff8a80")},
    {QStringLiteral("yellow"), QStringLiteral("#e8b04a")},
};

}

Theme::Theme(QObject *parent)
    : Theme(omarchyColorsPath(), parent)
{
}

Theme::Theme(const QString &colorsPath, QObject *parent)
    : QObject(parent)
    , m_path(colorsPath)
{
    // The theme lives behind a symlink that gets swapped on theme change, so
    // every reload also re-arms the watched paths.
    const auto reload = [this] {
        watch();
        load();
    };
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, reload);
    watch();
    load();
}

QColor Theme::accentForeground() const
{
    return m_accent.lightnessF() < 0.55 ? QColor(Qt::white) : QColor(Qt::black);
}

QHash<QString, QString> Theme::readColorsFile(const QString &path)
{
    QHash<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return values;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals < 0)
            continue;
        QString value = line.mid(equals + 1).trimmed();
        if (value.size() >= 2
            && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
            value = value.mid(1, value.size() - 2);
        values.insert(line.left(equals).trimmed(), value);
    }
    return values;
}

QColor Theme::mix(const QColor &a, const QColor &b, double t)
{
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t),
                            float(a.greenF() + (b.greenF() - a.greenF()) * t),
                            float(a.blueF() + (b.blueF() - a.blueF()) * t));
}

void Theme::load()
{
    const QHash<QString, QString> file = readColorsFile(m_path);
    // A theme's own values where valid, the fallback palette otherwise.
    auto color = [&](const QString &key, const QString &alternative = {}) {
        for (const QString &k : {key, alternative}) {
            if (k.isEmpty())
                continue;
            const QColor c = QColor::fromString(file.value(k));
            if (c.isValid())
                return c;
        }
        return QColor::fromString(Fallback.value(key));
    };

    const QColor background = color(QStringLiteral("background"));
    const QColor foreground = color(QStringLiteral("foreground"));
    const QString mode = file.value(QStringLiteral("mode"));
    const bool dark = mode.isEmpty() ? background.lightnessF() < 0.5 : mode != QLatin1String("light");

    const QColor accent = color(QStringLiteral("accent"));
    const QColor bright = color(QStringLiteral("bright_foreground"), QStringLiteral("foreground"));
    QColor muted = QColor::fromString(file.value(QStringLiteral("dark_foreground")));
    if (!muted.isValid())
        muted = file.isEmpty() ? color(QStringLiteral("dark_foreground")) : mix(foreground, background, 0.4);
    const QColor red = color(QStringLiteral("red"));
    const QColor yellow = color(QStringLiteral("yellow"));

    if (dark == m_dark && accent == m_accent && background == m_background && foreground == m_foreground
        && bright == m_brightForeground && muted == m_darkForeground && red == m_red && yellow == m_yellow)
        return;
    m_dark = dark;
    m_accent = accent;
    m_background = background;
    m_foreground = foreground;
    m_brightForeground = bright;
    m_darkForeground = muted;
    m_red = red;
    m_yellow = yellow;
    Q_EMIT changed();
}

void Theme::watch()
{
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);

    // For Omarchy's own file, watch the "current" link and the theme folder
    // too: switching themes replaces them rather than editing the file.
    const QFileInfo colors(m_path);
    const QString themeDir = colors.absolutePath();
    const QString currentDir = QFileInfo(themeDir).absolutePath();
    for (const QString &dir : {currentDir, themeDir})
        if (QDir(dir).exists())
            m_watcher.addPath(dir);
    if (colors.exists())
        m_watcher.addPath(m_path);
}
