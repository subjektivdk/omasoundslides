// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#include "probe.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

namespace Probe {

QStringList imageInputOptions()
{
    return {QStringLiteral("-f"), QStringLiteral("image2"), QStringLiteral("-pattern_type"), QStringLiteral("none")};
}

MediaInfo inspect(const QString &path, Kind kind)
{
    MediaInfo info;
    const QFileInfo file(path);
    if (!file.isFile()) {
        info.error = QStringLiteral("File not found: %1").arg(path);
        return info;
    }

    QStringList args{QStringLiteral("-v"), QStringLiteral("error"),
                     QStringLiteral("-show_entries"),
                     QStringLiteral("format=duration:stream=codec_type,codec_name,width,height"),
                     QStringLiteral("-of"), QStringLiteral("json")};
    if (kind == Kind::Image)
        args << imageInputOptions();
    args << file.absoluteFilePath();

    QProcess ffprobe;
    ffprobe.start(QStringLiteral("ffprobe"), args);
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
        // A picture has a codec ffmpeg knows and a size. (Read as image2, any
        // file shows a "video" stream; for audio it is codec "unknown", 0×0.)
        const int width = stream.value(QLatin1String("width")).toInt();
        const int height = stream.value(QLatin1String("height")).toInt();
        const QString codec = stream.value(QLatin1String("codec_name")).toString();
        if (type == QLatin1String("video") && !info.hasVideo && width > 0 && height > 0
            && !codec.isEmpty() && codec != QLatin1String("unknown")) {
            info.hasVideo = true;
            info.width = width;
            info.height = height;
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
