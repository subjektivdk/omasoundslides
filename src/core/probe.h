#pragma once

#include <QString>

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

// Runs ffprobe on a file and blocks until it finishes.
MediaInfo inspect(const QString &path);

}
