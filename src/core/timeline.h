#pragma once

#include "project.h"

#include <QList>
#include <QStringList>

// Timing model: every image has a duration (how long its clip lasts) and an
// optional transition INTO it. A transition overlaps the end of the previous
// image with the start of this one, so it "eats" its own length:
//
//   start[0] = 0
//   start[i] = start[i-1] + duration[i-1] - transitionDuration[i]
//   total    = start[last] + duration[last]  =  Σ duration − Σ transitionDuration
//
// Straight cuts (transition "none") have transitionDuration 0.
struct TimelinePlan {
    QList<double> starts;
    double total = 0;
    // Runs of consecutive images joined by xfade. Runs are joined to each
    // other with straight cuts (the concat filter). Each run is [first, last].
    QList<QPair<int, int>> runs;
};

namespace Timeline {

TimelinePlan plan(const QList<ResolvedSlide> &slides);

// Human-readable problems that would make ffmpeg fail or produce odd output.
QStringList validate(const QList<ResolvedSlide> &slides);

// Soundslides' "auto-spaced" mode: one equal duration for every image so the
// video lasts exactly audioSeconds, taking the transitions into account.
double autoDuration(const QList<ResolvedSlide> &slides, double audioSeconds);

}
