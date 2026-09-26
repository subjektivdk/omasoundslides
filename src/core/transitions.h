#pragma once

#include <QString>
#include <QStringList>

namespace Transitions {

// Every xfade transition name ffmpeg accepts, plus "none" for a straight cut.
const QStringList &all();

// Maps Soundslides-style aliases ("crossfade", "cut", "fadeout") to the
// canonical name. Returns an empty string when the name is unknown.
QString canonical(const QString &name);

bool isCut(const QString &canonicalName);

}
