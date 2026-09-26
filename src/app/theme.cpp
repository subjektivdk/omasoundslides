// Theme following adapted from Omacut (https://github.com/omacom-io/omacut, MIT).

#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {
const QString DefaultAccent = QStringLiteral("#FFD60A");

QString omarchyCurrentDir()
{
    return QDir::homePath() + QStringLiteral("/.local/state/omarchy/current");
}

QString omarchyColorsPath()
{
    return omarchyCurrentDir() + QStringLiteral("/theme/colors.toml");
}
}

Theme::Theme(QObject *parent)
    : QObject(parent)
    , m_accent(DefaultAccent)
{
    // The theme lives behind a symlink that gets swapped on theme change, so
    // every reload also re-arms the watched paths.
    const auto changed = [this] {
        watch();
        load();
    };
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, changed);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, changed);
    watch();
    load();
}

QString Theme::accentForeground() const
{
    return foregroundFor(m_accent);
}

QString Theme::accentFromColorsFile(const QString &path, const QString &fallback)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return fallback;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals < 0 || line.left(equals).trimmed() != QLatin1String("accent"))
            continue;

        QString value = line.mid(equals + 1).trimmed();
        if (value.size() >= 2
            && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
            value = value.mid(1, value.size() - 2);
        return QColor::fromString(value).isValid() ? value : fallback;
    }
    return fallback;
}

QString Theme::foregroundFor(const QString &color)
{
    const QColor parsed = QColor::fromString(color);
    if (!parsed.isValid())
        return QStringLiteral("black");
    const double luminance = 0.299 * parsed.redF() + 0.587 * parsed.greenF() + 0.114 * parsed.blueF();
    return luminance < 0.5 ? QStringLiteral("white") : QStringLiteral("black");
}

void Theme::load()
{
    const QString accent = accentFromColorsFile(omarchyColorsPath(), DefaultAccent);
    if (accent == m_accent)
        return;
    m_accent = accent;
    Q_EMIT accentChanged();
}

void Theme::watch()
{
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);

    const QString currentDir = omarchyCurrentDir();
    const QString themeDir = currentDir + QStringLiteral("/theme");
    if (QDir(currentDir).exists())
        m_watcher.addPath(currentDir);
    if (QDir(themeDir).exists())
        m_watcher.addPath(themeDir);
    if (QFileInfo::exists(omarchyColorsPath()))
        m_watcher.addPath(omarchyColorsPath());
}
