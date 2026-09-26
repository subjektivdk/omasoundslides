#include "ffmpegcommand.h"

#include <QRegularExpression>

namespace {

QString seconds(double s)
{
    return QString::number(s, 'f', 3);
}

QString normalizeFilter(const OutputSettings &o)
{
    const QString w = QString::number(o.width);
    const QString h = QString::number(o.height);
    return QStringLiteral("scale=%1:%2:force_original_aspect_ratio=decrease,"
                          "pad=%1:%2:(ow-iw)/2:(oh-ih)/2:color=black,"
                          "setsar=1,fps=%3,format=yuv420p")
        .arg(w, h, QString::number(o.fps));
}

}

namespace FfmpegCommand {

QString filterGraph(const RenderJob &job)
{
    QStringList chains;
    const auto &slides = job.slides;
    const auto &runs = job.plan.runs;
    const bool singleRun = runs.size() == 1;
    const QString vout = QStringLiteral("vout");

    // A lone single-image run writes its normalised output straight to [vout].
    auto slideLabel = [&](int i) {
        if (singleRun && runs.first().first == runs.first().second)
            return vout;
        return QStringLiteral("v%1").arg(i);
    };

    const QString normalize = normalizeFilter(job.output);
    for (int i = 0; i < slides.size(); ++i)
        chains << QStringLiteral("[%1:v]%2[%3]").arg(QString::number(i), normalize, slideLabel(i));

    QStringList runLabels;
    for (int r = 0; r < runs.size(); ++r) {
        const auto [first, last] = runs.at(r);
        QString current = slideLabel(first);
        const double runStart = job.plan.starts.at(first);
        for (int i = first + 1; i <= last; ++i) {
            const ResolvedSlide &s = slides.at(i);
            const QString next = (singleRun && i == last) ? vout : QStringLiteral("x%1").arg(i);
            chains << QStringLiteral("[%1][%2]xfade=transition=%3:duration=%4:offset=%5[%6]")
                          .arg(current, slideLabel(i), s.transition, seconds(s.transitionDuration),
                               seconds(job.plan.starts.at(i) - runStart), next);
            current = next;
        }
        runLabels << current;
    }

    if (!singleRun) {
        QString inputs;
        for (const QString &label : runLabels)
            inputs += QStringLiteral("[%1]").arg(label);
        chains << QStringLiteral("%1concat=n=%2:v=1:a=0[%3]")
                      .arg(inputs, QString::number(runLabels.size()), vout);
    }

    if (!job.audioPaths.isEmpty()) {
        const int firstAudio = int(slides.size());
        QString inputs;
        for (int k = 0; k < job.audioPaths.size(); ++k) {
            chains << QStringLiteral("[%1:a]aresample=48000,aformat=sample_fmts=fltp:"
                                     "channel_layouts=stereo[a%2]")
                          .arg(firstAudio + k)
                          .arg(k);
            inputs += QStringLiteral("[a%1]").arg(k);
        }
        // apad fills with silence if the audio is shorter than the pictures;
        // the output -t cuts it if it is longer.
        if (job.audioPaths.size() == 1)
            chains << QStringLiteral("[a0]apad[aout]");
        else
            chains << QStringLiteral("%1concat=n=%2:v=0:a=1,apad[aout]")
                          .arg(inputs, QString::number(job.audioPaths.size()));
    }

    return chains.join(QStringLiteral(";\n"));
}

QStringList arguments(const RenderJob &job)
{
    QStringList args = {
        QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-y"),
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-progress"), QStringLiteral("pipe:1"), QStringLiteral("-nostats"),
    };

    const QString fps = QString::number(job.output.fps);
    for (const ResolvedSlide &s : job.slides)
        args << QStringLiteral("-loop") << QStringLiteral("1")
             << QStringLiteral("-framerate") << fps
             << QStringLiteral("-t") << seconds(s.duration)
             << QStringLiteral("-i") << s.path;
    for (const QString &audio : job.audioPaths)
        args << QStringLiteral("-i") << audio;

    args << QStringLiteral("-filter_complex") << filterGraph(job)
         << QStringLiteral("-map") << QStringLiteral("[vout]");
    if (!job.audioPaths.isEmpty())
        args << QStringLiteral("-map") << QStringLiteral("[aout]");

    args << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("medium")
         << QStringLiteral("-crf") << QStringLiteral("20")
         << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
         << QStringLiteral("-r") << fps;
    if (!job.audioPaths.isEmpty())
        args << QStringLiteral("-c:a") << QStringLiteral("aac")
             << QStringLiteral("-b:a") << QStringLiteral("192k");

    args << QStringLiteral("-t") << seconds(job.plan.total)
         << QStringLiteral("-movflags") << QStringLiteral("+faststart")
         << job.outputPath;
    return args;
}

QString shellCommand(const QStringList &arguments)
{
    QStringList parts{QStringLiteral("ffmpeg")};
    for (QString arg : arguments) {
        const bool plain = !arg.isEmpty()
            && arg.contains(QRegularExpression(QStringLiteral("^[A-Za-z0-9_./:+=,-]+$")));
        if (!plain)
            arg = QLatin1Char('\'') + arg.replace(QLatin1Char('\''), QLatin1String("'\\''"))
                + QLatin1Char('\'');
        parts << arg;
    }
    return parts.join(QLatin1Char(' '));
}

}
