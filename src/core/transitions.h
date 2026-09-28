// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// The transitions the original Soundslides offers, and nothing more:
// a straight cut, a crossfade and a fade out/fade in through black, the
// last two at three speeds. Stored as an ffmpeg xfade name plus a length.
namespace Transitions {

struct Preset {
    QString id;         // "crossfade-medium"
    QString label;      // "Crossfade – Medium"
    QString transition; // "none", "fade" or "fadeblack"
    double duration;    // seconds; 0 for a cut
};

// Straight cut, Crossfade Fast/Medium/Slow, Fade out/in Fast/Medium/Slow.
const QList<Preset> &presets();

// The transition names: "none", "fade", "fadeblack".
const QStringList &all();

// Maps a name or a Soundslides-style alias ("crossfade", "cut", "fadeout")
// to its canonical name. Returns an empty string when it is not one of ours.
QString canonical(const QString &name);

bool isCut(const QString &canonicalName);

}
