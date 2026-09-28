// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

// Writing an export safely: ffmpeg writes into a new, uniquely named file
// next to the target (so the final move stays on one filesystem), and only a
// finished export takes the target's name. A failed or cancelled export
// never leaves a half-written file under the real name, and the temporary
// name can't be guessed in advance and pointed somewhere else.
namespace ExportFile {

// Creates the empty temporary file (".omasoundslides-XXXXXX.mp4" in the
// target's folder) and returns its path, or an empty string and *error.
QString createTemporary(const QString &target, QString *error);

// Moves the finished temporary file onto target, replacing any existing
// file in one atomic step.
bool finish(const QString &temporary, const QString &target, QString *error);

}
