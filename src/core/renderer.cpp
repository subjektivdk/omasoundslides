#include "renderer.h"

#include <algorithm>

namespace {
constexpr qsizetype StderrTailBytes = 4000;
}

Renderer::Renderer(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &Renderer::readProgress);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_stderrTail += m_process.readAllStandardError();
        if (m_stderrTail.size() > StderrTailBytes)
            m_stderrTail = m_stderrTail.right(StderrTailBytes);
    });
    connect(&m_process, &QProcess::finished, this, &Renderer::handleFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            emit finished(false, QStringLiteral("Could not start ffmpeg. Is it installed?"));
    });
}

void Renderer::start(const QStringList &ffmpegArguments, double totalSeconds)
{
    m_totalSeconds = totalSeconds;
    m_cancelled = false;
    m_stdoutBuffer.clear();
    m_stderrTail.clear();
    m_process.start(QStringLiteral("ffmpeg"), ffmpegArguments);
}

void Renderer::cancel()
{
    if (!isRunning())
        return;
    m_cancelled = true;
    m_process.terminate();
    if (!m_process.waitForFinished(3000))
        m_process.kill();
}

bool Renderer::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void Renderer::readProgress()
{
    m_stdoutBuffer += m_process.readAllStandardOutput();
    qsizetype newline;
    while ((newline = m_stdoutBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_stdoutBuffer.left(newline).trimmed();
        m_stdoutBuffer.remove(0, newline + 1);

        // Despite the name, out_time_ms is in microseconds too; prefer out_time_us.
        if (line.startsWith("out_time_us=") && m_totalSeconds > 0) {
            bool ok = false;
            const qint64 us = line.mid(12).toLongLong(&ok);
            if (ok && us >= 0)
                emit progress(std::clamp(us / 1e6 / m_totalSeconds, 0.0, 1.0));
        } else if (line == "progress=end") {
            emit progress(1.0);
        }
    }
}

void Renderer::handleFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_cancelled) {
        emit finished(false, QStringLiteral("Export cancelled"));
        return;
    }
    if (status != QProcess::NormalExit || exitCode != 0) {
        const QString detail = QString::fromUtf8(m_stderrTail).trimmed();
        emit finished(false, QStringLiteral("ffmpeg failed (code %1)%2")
                                 .arg(exitCode)
                                 .arg(detail.isEmpty() ? QString() : QStringLiteral(":\n") + detail));
        return;
    }
    emit finished(true, {});
}
