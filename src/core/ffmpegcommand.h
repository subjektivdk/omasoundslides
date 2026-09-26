#pragma once

#include "project.h"
#include "timeline.h"

#include <QString>
#include <QStringList>

struct RenderJob {
    QList<ResolvedSlide> slides;
    TimelinePlan plan;
    OutputSettings output;
    QStringList audioPaths; // absolute, played back to back
    double audioSeconds = 0; // their combined length
    double audioFadeIn = 0;
    double audioFadeOut = 0;
    QString outputPath;
};

namespace FfmpegCommand {

// The -filter_complex graph: normalise every image to the output size,
// join runs with xfade, join runs to each other with concat, glue the audio.
QString filterGraph(const RenderJob &job);

// Full argument list for ffmpeg (without the program name). Progress is
// written to stdout as key=value lines (-progress pipe:1).
QStringList arguments(const RenderJob &job);

// For --dry-run: the command as something you can paste into a shell.
QString shellCommand(const QStringList &arguments);

}
