#include "prepare.h"
#include "probe.h"
#include "timeline.h"

#include <QFileInfo>

namespace {
// Soundslides recommends staying below 120 images; beyond that one big
// ffmpeg filter graph starts using a lot of memory.
constexpr int RecommendedMaxImages = 120;
}

PreparedJob prepareJob(const Project &project, const QString &outputPath, bool autoSpaced)
{
    PreparedJob p;
    p.job.output = project.output;
    p.job.outputPath = QFileInfo(outputPath).absoluteFilePath();
    p.job.slides = resolveSlides(project);

    for (int i = 0; i < p.job.slides.size(); ++i) {
        const MediaInfo info = Probe::inspect(p.job.slides.at(i).path, Probe::Kind::Image);
        if (!info.ok)
            p.errors << QStringLiteral("Image %1: %2").arg(i + 1).arg(info.error);
        else if (!info.hasVideo)
            p.errors << QStringLiteral("Image %1: %2 is not an image")
                            .arg(i + 1)
                            .arg(p.job.slides.at(i).path);
    }

    for (const QString &relative : project.audio) {
        const QString path = project.resolvePath(relative);
        const MediaInfo info = Probe::inspect(path);
        if (!info.ok) {
            p.errors << QStringLiteral("Audio: %1").arg(info.error);
            continue;
        }
        if (!info.hasAudio) {
            p.errors << QStringLiteral("Audio: %1 has no audio stream").arg(path);
            continue;
        }
        p.job.audioPaths << path;
        p.audioSeconds += info.duration;
    }

    if (autoSpaced) {
        if (project.audio.isEmpty())
            p.errors << QStringLiteral("--auto needs at least one audio file");
        else if (p.audioSeconds > 0) {
            const double d = Timeline::autoDuration(p.job.slides, p.audioSeconds);
            for (ResolvedSlide &s : p.job.slides)
                s.duration = d;
        }
    }

    p.job.audioSeconds = p.audioSeconds;
    p.job.audioFadeIn = project.audioFadeIn;
    p.job.audioFadeOut = project.audioFadeOut;

    p.errors << Timeline::validate(p.job.slides);
    p.job.plan = Timeline::plan(p.job.slides);

    if (p.job.slides.size() > RecommendedMaxImages)
        p.warnings << QStringLiteral("%1 images is a lot. Above %2, ffmpeg may use a lot of memory")
                          .arg(p.job.slides.size())
                          .arg(RecommendedMaxImages);

    if (!p.job.audioPaths.isEmpty() && p.errors.isEmpty()) {
        const double diff = p.job.plan.total - p.audioSeconds;
        if (diff > 0.05)
            p.warnings << QStringLiteral("The images last %1 s longer than the audio. "
                                         "The end will be silent")
                              .arg(diff, 0, 'f', 2);
        else if (diff < -0.05)
            p.warnings << QStringLiteral("The audio lasts %1 s longer than the images. "
                                         "It will be cut off")
                              .arg(-diff, 0, 'f', 2);
    }
    if (p.job.audioPaths.isEmpty() && project.audio.isEmpty())
        p.warnings << QStringLiteral("No audio in the project. The video will be silent");

    return p;
}
