// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#pragma once

#include "ffmpegcommand.h"
#include "project.h"

#include <QStringList>

struct PreparedJob {
    RenderJob job;
    double audioSeconds = 0;
    QStringList errors;   // rendering is not possible
    QStringList warnings; // rendering works, but the user should know

    bool ok() const { return errors.isEmpty(); }
};

// Everything between "a project file" and "an ffmpeg command": resolve
// defaults, probe every file, apply auto-spacing, validate the timing.
// Shared by the CLI and (later) the GUI.
PreparedJob prepareJob(const Project &project, const QString &outputPath, bool autoSpaced);
