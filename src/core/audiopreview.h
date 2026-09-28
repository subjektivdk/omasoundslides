// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVector>

// The project's audio prepared for the editor: every file joined into one
// FLAC that the media player can play and seek in, plus a waveform (peak
// level per 1/PeaksPerSecond s, 0..1). Built in the background with ffmpeg
// and cached by content, so reopening a project is instant.
class AudioPreview : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool building READ building NOTIFY buildingChanged)
    Q_PROPERTY(double duration READ duration NOTIFY changed)

public:
    // Fine enough that the waveform stays smooth at the timeline's deepest zoom.
    static constexpr int PeaksPerSecond = 400;

    struct Result {
        QString file;
        QVector<float> peaks;
        double duration = 0;
        QString error;
    };

    explicit AudioPreview(QObject *parent = nullptr);

    QUrl source() const;
    bool ready() const { return !m_result.file.isEmpty(); }
    bool building() const { return m_building; }
    double duration() const { return m_result.duration; }
    const QVector<float> &peaks() const { return m_result.peaks; }

    // Starts over for a new list of audio files (absolute paths).
    void rebuild(const QStringList &paths);

    // The blocking work, public for tests. cacheDir must exist.
    static Result build(const QStringList &paths, const QString &cacheDir);

    // Keeps the cache from growing without end: removes files not used for
    // maxAgeDays, then the least recently used until the rest fit in
    // maxBytes. Returns how many files were removed.
    static int pruneCache(const QString &cacheDir, int maxAgeDays = 30, qint64 maxBytes = qint64(2) << 30);

    static QString cacheDir();

Q_SIGNALS:
    void changed();
    void buildingChanged();
    void failed(const QString &message);

private:
    void finished();

    QFutureWatcher<Result> m_watcher;
    QStringList m_paths;
    QStringList m_pendingPaths;
    bool m_building = false;
    bool m_rerun = false;
    Result m_result;
};
