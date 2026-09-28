// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#pragma once

#include "core/prepare.h"
#include "core/project.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

// Command-line editing of a project file, for scripts and AI agents: every
// change the window can make, as a command. Edits go through ProjectModel,
// so the rules, rounding and validation are exactly the window's, and the
// file is written the way Save writes it (paths relative to the project).
namespace CliEdit {

struct Options {
    int at = -1;       // --at N: insert after image N (1-based); -1 appends
    QString name;      // --name for `new`
    bool overwrite = false;
};

// new, add-images, add-audio, remove-image, move-image, set, set-image,
// markers, fit
bool isCommand(const QString &command);

// args: everything after the command name. Returns the exit code:
// 0 done, 1 failed (bad file, bad value), 2 wrong usage.
int run(const QString &command, const QStringList &args, const Options &options);

// Usage lines for these commands, for the CLI's help text.
QString usage();

// The whole project and its timeline, for `info --json`.
QJsonObject infoJson(const QString &projectPath, const Project &project, const PreparedJob &prepared);

}
