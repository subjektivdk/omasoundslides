#include "audiopreview.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

// Low sample rate is plenty for peaks: 4000 Hz = 40 samples per peak.
constexpr int WaveformRate = 4000;

QString cacheKey(const QStringList &paths)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    for (const QString &path : paths) {
        const QFileInfo info(path);
        hash.addData(path.toUtf8());
        hash.addData(QByteArray::number(info.size()));
        hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
    }
    return QString::fromLatin1(hash.result().toHex().left(16));
}

bool runFfmpeg(const QStringList &args, QByteArray *stdoutData, QString *error)
{
    QProcess ffmpeg;
    ffmpeg.start(QStringLiteral("ffmpeg"), args);
    if (!ffmpeg.waitForStarted()) {
        *error = QStringLiteral("Could not start ffmpeg");
        return false;
    }
    QByteArray out;
    while (!ffmpeg.waitForFinished(100)) {
        if (stdoutData)
            out += ffmpeg.readAllStandardOutput();
        if (ffmpeg.state() == QProcess::NotRunning)
            break;
    }
    if (stdoutData)
        *stdoutData = out + ffmpeg.readAllStandardOutput();
    if (ffmpeg.exitStatus() != QProcess::NormalExit || ffmpeg.exitCode() != 0) {
        *error = QString::fromUtf8(ffmpeg.readAllStandardError()).trimmed().section(QLatin1Char('\n'), -1);
        return false;
    }
    return true;
}

}

AudioPreview::AudioPreview(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<Result>::finished, this, &AudioPreview::finished);
}

QUrl AudioPreview::source() const
{
    return m_result.file.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_result.file);
}

void AudioPreview::rebuild(const QStringList &paths)
{
    if (paths == m_paths)
        return;
    m_paths = paths;

    if (m_building) {
        // One build at a time; the newest list wins when this one ends.
        m_rerun = true;
        return;
    }
    if (paths.isEmpty()) {
        m_result = {};
        Q_EMIT changed();
        return;
    }

    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/audio");
    QDir().mkpath(cacheDir);
    m_building = true;
    Q_EMIT buildingChanged();
    m_watcher.setFuture(QtConcurrent::run([paths, cacheDir] { return build(paths, cacheDir); }));
}

void AudioPreview::finished()
{
    m_building = false;
    Q_EMIT buildingChanged();

    if (m_rerun) {
        m_rerun = false;
        const QStringList latest = m_paths;
        m_paths.clear();
        rebuild(latest);
        return;
    }

    const Result result = m_watcher.result();
    if (!result.error.isEmpty()) {
        m_result = {};
        Q_EMIT changed();
        Q_EMIT failed(QStringLiteral("Audio preview: %1").arg(result.error));
        return;
    }
    m_result = result;
    Q_EMIT changed();
}

AudioPreview::Result AudioPreview::build(const QStringList &paths, const QString &cacheDir)
{
    Result result;
    if (paths.isEmpty())
        return result;

    const QString file = QDir(cacheDir).filePath(cacheKey(paths) + QStringLiteral(".flac"));
    if (!QFileInfo::exists(file)) {
        // Same joining as the export: resample to one format, then concat.
        QStringList args{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-y"),
                         QStringLiteral("-v"), QStringLiteral("error")};
        QString graph;
        for (int i = 0; i < paths.size(); ++i) {
            args << QStringLiteral("-i") << paths.at(i);
            graph += QStringLiteral("[%1:a]aresample=44100,aformat=sample_fmts=s16:channel_layouts=stereo[a%1];").arg(i);
        }
        for (int i = 0; i < paths.size(); ++i)
            graph += QStringLiteral("[a%1]").arg(i);
        graph += QStringLiteral("concat=n=%1:v=0:a=1[out]").arg(paths.size());

        const QString tmp = file + QStringLiteral(".part.flac");
        args << QStringLiteral("-filter_complex") << graph << QStringLiteral("-map")
             << QStringLiteral("[out]") << QStringLiteral("-c:a") << QStringLiteral("flac") << tmp;
        if (!runFfmpeg(args, nullptr, &result.error)) {
            QFile::remove(tmp);
            return result;
        }
        QFile::rename(tmp, file);
    }

    QByteArray samples;
    if (!runFfmpeg({QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-v"),
                    QStringLiteral("error"), QStringLiteral("-i"), file, QStringLiteral("-ac"),
                    QStringLiteral("1"), QStringLiteral("-ar"), QString::number(WaveformRate),
                    QStringLiteral("-f"), QStringLiteral("s16le"), QStringLiteral("-")},
                   &samples, &result.error))
        return result;

    const qsizetype count = samples.size() / 2;
    const int perPeak = WaveformRate / PeaksPerSecond;
    result.peaks.reserve(count / perPeak + 1);
    for (qsizetype start = 0; start < count; start += perPeak) {
        int peak = 0;
        const qsizetype end = std::min(count, start + perPeak);
        for (qsizetype k = start; k < end; ++k) {
            qint16 v;
            std::memcpy(&v, samples.constData() + k * 2, 2);
            peak = std::max(peak, std::abs(int(v)));
        }
        result.peaks.append(peak / 32768.0f);
    }
    // Scale so nearly the loudest moments fill the track. Using a high
    // percentile rather than the maximum keeps a single click or clap from
    // flattening everything else; quiet interviews stay readable.
    if (!result.peaks.isEmpty()) {
        QVector<float> sorted = result.peaks;
        const qsizetype at = qsizetype(sorted.size() * 0.995);
        std::nth_element(sorted.begin(), sorted.begin() + at, sorted.end());
        const float reference = sorted.at(std::min(at, sorted.size() - 1));
        if (reference > 0)
            for (float &p : result.peaks)
                p = std::min(1.0f, p / reference);
    }

    result.file = file;
    result.duration = double(count) / WaveformRate;
    return result;
}
