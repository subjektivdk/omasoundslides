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
        const MediaInfo info = Probe::inspect(p.job.slides.at(i).path);
        if (!info.ok)
            p.errors << QStringLiteral("Billede %1: %2").arg(i + 1).arg(info.error);
        else if (!info.hasVideo)
            p.errors << QStringLiteral("Billede %1: %2 er ikke et billede")
                            .arg(i + 1)
                            .arg(p.job.slides.at(i).path);
    }

    for (const QString &relative : project.audio) {
        const QString path = project.resolvePath(relative);
        const MediaInfo info = Probe::inspect(path);
        if (!info.ok) {
            p.errors << QStringLiteral("Lyd: %1").arg(info.error);
            continue;
        }
        if (!info.hasAudio) {
            p.errors << QStringLiteral("Lyd: %1 indeholder ikke noget lydspor").arg(path);
            continue;
        }
        p.job.audioPaths << path;
        p.audioSeconds += info.duration;
    }

    if (autoSpaced) {
        if (project.audio.isEmpty())
            p.errors << QStringLiteral("--auto kræver mindst én lydfil");
        else if (p.audioSeconds > 0) {
            const double d = Timeline::autoDuration(p.job.slides, p.audioSeconds);
            for (ResolvedSlide &s : p.job.slides)
                s.duration = d;
        }
    }

    p.errors << Timeline::validate(p.job.slides);
    p.job.plan = Timeline::plan(p.job.slides);

    if (p.job.slides.size() > RecommendedMaxImages)
        p.warnings << QStringLiteral("%1 billeder er mange. Over %2 kan ffmpeg bruge meget hukommelse")
                          .arg(p.job.slides.size())
                          .arg(RecommendedMaxImages);

    if (!p.job.audioPaths.isEmpty() && p.errors.isEmpty()) {
        const double diff = p.job.plan.total - p.audioSeconds;
        if (diff > 0.05)
            p.warnings << QStringLiteral("Billederne varer %1 s længere end lyden. "
                                         "Der bliver stilhed til sidst")
                              .arg(diff, 0, 'f', 2);
        else if (diff < -0.05)
            p.warnings << QStringLiteral("Lyden varer %1 s længere end billederne. "
                                         "Lyden bliver klippet af")
                              .arg(-diff, 0, 'f', 2);
    }
    if (p.job.audioPaths.isEmpty() && project.audio.isEmpty())
        p.warnings << QStringLiteral("Ingen lyd i projektet. Videoen bliver uden lyd");

    return p;
}
