#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QString>

// The window's colors, taken live from the current Omarchy theme
// (~/.local/state/omarchy/current/theme/colors.toml): dark when the theme is
// dark, light when it is light, in the theme's own hues. The in-between
// shades (panels, fields, borders, muted text) are blended from the theme's
// background and foreground, so they work in either direction. Without an
// Omarchy theme it falls back to a neutral dark palette.
class Theme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    // Text on the accent: black or white, whichever reads.
    Q_PROPERTY(QColor accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QColor window READ window NOTIFY changed)      // behind everything
    Q_PROPERTY(QColor panel READ panel NOTIFY changed)        // inspector, timeline, dialogs
    Q_PROPERTY(QColor raised READ raised NOTIFY changed)      // chips, tracks, cards
    Q_PROPERTY(QColor control READ control NOTIFY changed)    // buttons on panels
    Q_PROPERTY(QColor border READ border NOTIFY changed)      // ticks, separators
    Q_PROPERTY(QColor text READ text NOTIFY changed)
    Q_PROPERTY(QColor textStrong READ textStrong NOTIFY changed) // headings
    Q_PROPERTY(QColor textMuted READ textMuted NOTIFY changed)   // hints, labels
    Q_PROPERTY(QColor textFaint READ textFaint NOTIFY changed)   // disabled, placeholders
    Q_PROPERTY(QColor danger READ danger NOTIFY changed)         // problems
    Q_PROPERTY(QColor marker READ marker NOTIFY changed)         // timeline markers
    Q_PROPERTY(QColor playhead READ playhead NOTIFY changed)
    Q_PROPERTY(QColor waveform READ waveform NOTIFY changed)
    // Kept for the wordmark: the theme's plain foreground.
    Q_PROPERTY(QColor foreground READ text NOTIFY changed)

public:
    explicit Theme(QObject *parent = nullptr);
    // For tests: read a given colors.toml instead of Omarchy's current one.
    explicit Theme(const QString &colorsPath, QObject *parent = nullptr);

    bool dark() const { return m_dark; }
    QColor accent() const { return m_accent; }
    QColor accentForeground() const;
    QColor window() const { return m_background; }
    QColor panel() const { return mix(m_background, m_foreground, m_dark ? 0.05 : 0.04); }
    QColor raised() const { return mix(m_background, m_foreground, m_dark ? 0.10 : 0.08); }
    QColor control() const { return mix(m_background, m_foreground, m_dark ? 0.17 : 0.13); }
    QColor border() const { return mix(m_background, m_foreground, 0.28); }
    QColor text() const { return m_foreground; }
    QColor textStrong() const { return m_brightForeground; }
    QColor textMuted() const { return m_darkForeground; }
    QColor textFaint() const { return mix(m_darkForeground, m_background, 0.4); }
    QColor danger() const { return m_red; }
    QColor marker() const { return m_yellow; }
    QColor playhead() const { return m_brightForeground; }
    QColor waveform() const { return m_dark ? m_accent.lighter(150) : m_accent.darker(110); }

    // All `key = "#rrggbb"` pairs of an Omarchy colors.toml, plus `mode`.
    static QHash<QString, QString> readColorsFile(const QString &path);
    // "a" at 0, "b" at 1.
    static QColor mix(const QColor &a, const QColor &b, double t);

Q_SIGNALS:
    void changed();

private:
    void load();
    void watch();

    QString m_path;
    bool m_dark = true;
    QColor m_accent;
    QColor m_background;
    QColor m_foreground;
    QColor m_brightForeground;
    QColor m_darkForeground;
    QColor m_red;
    QColor m_yellow;
    QFileSystemWatcher m_watcher;
};
