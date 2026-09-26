#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

// Follows the Omarchy theme's accent color live, like Omacut does. Falls back
// to a fixed accent when there is no Omarchy theme (other distros).
class Theme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY accentChanged)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY accentChanged)

public:
    explicit Theme(QObject *parent = nullptr);

    QString accent() const { return m_accent; }
    QString accentForeground() const;

    static QString accentFromColorsFile(const QString &path, const QString &fallback);
    // "black" or "white", whichever stays legible on the given color.
    static QString foregroundFor(const QString &color);

Q_SIGNALS:
    void accentChanged();

private:
    void load();
    void watch();

    QString m_accent;
    QFileSystemWatcher m_watcher;
};
