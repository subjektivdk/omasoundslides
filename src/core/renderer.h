// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

// Runs ffmpeg as a separate process (so a crash there never takes down the
// app) and turns its -progress output into a 0..1 fraction.
class Renderer : public QObject
{
    Q_OBJECT

public:
    explicit Renderer(QObject *parent = nullptr);

    void start(const QStringList &ffmpegArguments, double totalSeconds);
    void cancel();
    bool isRunning() const;

signals:
    void progress(double fraction);
    void finished(bool ok, const QString &error);

private:
    void readProgress();
    void handleFinished(int exitCode, QProcess::ExitStatus status);

    QProcess m_process;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrTail;
    double m_totalSeconds = 0;
    bool m_cancelled = false;
};
