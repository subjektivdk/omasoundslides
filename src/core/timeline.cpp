// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#include "timeline.h"
#include "transitions.h"

namespace Timeline {

TimelinePlan plan(const QList<ResolvedSlide> &slides)
{
    TimelinePlan p;
    if (slides.isEmpty())
        return p;

    p.starts.append(0);
    int runStart = 0;
    for (int i = 1; i < slides.size(); ++i) {
        const ResolvedSlide &s = slides.at(i);
        p.starts.append(p.starts.last() + slides.at(i - 1).duration - s.transitionDuration);
        if (Transitions::isCut(s.transition)) {
            p.runs.append({runStart, i - 1});
            runStart = i;
        }
    }
    p.runs.append({runStart, int(slides.size() - 1)});
    p.total = p.starts.last() + slides.last().duration;
    return p;
}

QStringList validate(const QList<ResolvedSlide> &slides)
{
    QStringList problems;
    if (slides.isEmpty())
        problems << QStringLiteral("The project has no images");

    for (int i = 0; i < slides.size(); ++i) {
        const ResolvedSlide &s = slides.at(i);
        const double in = s.transitionDuration;
        const double out = i + 1 < slides.size() ? slides.at(i + 1).transitionDuration : 0;
        const int n = i + 1;

        if (s.duration <= 0)
            problems << QStringLiteral("Image %1: the duration must be greater than 0").arg(n);
        else if (in + out > s.duration + 1e-9)
            problems << QStringLiteral("Image %1: the transitions in (%2 s) and out (%3 s) are longer "
                                       "together than the image lasts (%4 s)")
                            .arg(n)
                            .arg(in)
                            .arg(out)
                            .arg(s.duration);
        if (in > 60)
            problems << QStringLiteral("Image %1: a transition can last 60 s at most").arg(n);
    }
    return problems;
}

double autoDuration(const QList<ResolvedSlide> &slides, double audioSeconds)
{
    if (slides.isEmpty())
        return 0;
    double transitions = 0;
    for (const ResolvedSlide &s : slides)
        transitions += s.transitionDuration;
    return (audioSeconds + transitions) / slides.size();
}

}
