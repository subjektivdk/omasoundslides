#include "probe.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

namespace Probe {

MediaInfo inspect(const QString &path)
{
    MediaInfo info;
    if (!QFileInfo(path).isFile()) {
        info.error = QStringLiteral("File not found: %1").arg(path);
        return info;
    }

    QProcess ffprobe;
    ffprobe.start(QStringLiteral("ffprobe"),
                  {QStringLiteral("-v"), QStringLiteral("error"),
                   QStringLiteral("-show_entries"),
                   QStringLiteral("format=duration:stream=codec_type,width,height"),
                   QStringLiteral("-of"), QStringLiteral("json"), path});
    if (!ffprobe.waitForStarted()) {
        info.error = QStringLiteral("Could not start ffprobe. Is ffmpeg installed?");
        return info;
    }
    ffprobe.waitForFinished(30000);
    if (ffprobe.exitStatus() != QProcess::NormalExit || ffprobe.exitCode() != 0) {
        const QString detail = QString::fromUtf8(ffprobe.readAllStandardError()).trimmed();
        info.error = QStringLiteral("ffprobe cannot read %1%2")
                         .arg(path, detail.isEmpty() ? QString() : QStringLiteral(": ") + detail);
        return info;
    }

    const QJsonObject root = QJsonDocument::fromJson(ffprobe.readAllStandardOutput()).object();
    for (const QJsonValue &v : root.value(QLatin1String("streams")).toArray()) {
        const QJsonObject stream = v.toObject();
        const QString type = stream.value(QLatin1String("codec_type")).toString();
        if (type == QLatin1String("video") && !info.hasVideo) {
            info.hasVideo = true;
            info.width = stream.value(QLatin1String("width")).toInt();
            info.height = stream.value(QLatin1String("height")).toInt();
        } else if (type == QLatin1String("audio")) {
            info.hasAudio = true;
        }
    }
    // ffprobe reports duration as a string, e.g. "12.345000".
    info.duration = root.value(QLatin1String("format")).toObject()
                        .value(QLatin1String("duration")).toString().toDouble();
    info.ok = true;
    return info;
}

}
