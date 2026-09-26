#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// The user's keyboard shortcuts, from ~/.config/omasoundslides/keybindings.conf:
//
//   play_pause = Space
//   seek_back  = Left H        (several keys: separate with spaces)
//
// Missing actions keep their default. The file is written with every
// default the first time, and reloaded whenever it is saved.
class KeyBindings : public QObject
{
    Q_OBJECT
    // action id → list of key sequences, for QML Shortcut.sequences
    Q_PROPERTY(QVariantMap keys READ keys NOTIFY changed)
    // [{ id, keys, description }] in file order, for the help overlay
    Q_PROPERTY(QVariantList actions READ actions NOTIFY changed)
    Q_PROPERTY(double durationStep READ durationStep NOTIFY changed)
    Q_PROPERTY(double scrollStep READ scrollStep NOTIFY changed)
    Q_PROPERTY(QString path READ path CONSTANT)

public:
    struct Action {
        QString id;
        QStringList keys;
        QString description;
    };

    struct Parsed {
        QHash<QString, QStringList> keys;
        QHash<QString, double> settings;
        QStringList warnings;
    };

    explicit KeyBindings(const QString &path, QObject *parent = nullptr);

    QVariantMap keys() const;
    QVariantList actions() const;
    double durationStep() const { return m_durationStep; }
    double scrollStep() const { return m_scrollStep; }
    QString path() const { return m_path; }

    static const QList<Action> &defaults();
    static Parsed parse(const QString &text);
    static QString defaultFileText();

    Q_INVOKABLE void reload();

Q_SIGNALS:
    void changed();
    void warning(const QString &message);

private:
    void watch();

    QString m_path;
    QList<Action> m_actions;
    double m_durationStep = 0.5;
    double m_scrollStep = 0.1;
    QFileSystemWatcher m_watcher;
};
