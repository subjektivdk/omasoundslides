// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

struct MediaInfo {
    bool ok = false;
    QString error;
    double duration = 0; // seconds; 0 for still images
    int width = 0;
    int height = 0;
    bool hasVideo = false;
    bool hasAudio = false;
};

namespace Probe {

// What a file is expected to be. Images are read with ffmpeg's image2
// demuxer with sequence patterns switched off, so a name like img%03d.png
// is that one file and not img001.png, img002.png, …
enum class Kind { Any, Image };

// Runs ffprobe on a file and blocks until it finishes. The path is made
// absolute first, so a name starting with '-' can't pass for an option.
MediaInfo inspect(const QString &path, Kind kind = Kind::Any);

// ffmpeg input options that read exactly `path` as a still image.
QStringList imageInputOptions();

}
