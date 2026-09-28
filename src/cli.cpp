// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#include "cli.h"
#include "cliedit.h"
#include "core/exportfile.h"
#include "core/ffmpegcommand.h"
#include "core/prepare.h"
#include "core/project.h"
#include "core/renderer.h"
#include "core/transitions.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdio>
#include <unistd.h>

namespace {

QTextStream &out()
{
    static QTextStream s(stdout);
    return s;
}

QTextStream &err()
{
    static QTextStream s(stderr);
    return s;
}

const char *Usage =
    "Usage:\n"
    "  omasoundslides [project.json]      (open the window)\n"
    "  omasoundslides render <project.json> <out.mp4> [--auto] [--quality standard|high]\n"
    "                        [--overwrite] [--dry-run]\n"
    "  omasoundslides info <project.json> [--auto] [--json]\n"
    "  omasoundslides transitions\n";

void printMessages(const PreparedJob &p)
{
    for (const QString &w : p.warnings)
        err() << "Warning: " << w << Qt::endl;
    for (const QString &e : p.errors)
        err() << "Error: " << e << Qt::endl;
}

void printTimeline(const PreparedJob &p)
{
    const auto &slides = p.job.slides;
    out() << QStringLiteral("  #    start  duration  transition in         file") << Qt::endl;
    for (int i = 0; i < slides.size(); ++i) {
        const ResolvedSlide &s = slides.at(i);
        const QString transition = s.transitionDuration > 0
            ? QStringLiteral("%1 (%2 s)").arg(s.transition).arg(s.transitionDuration, 0, 'f', 2)
            : QStringLiteral("cut");
        out() << QStringLiteral("%1  %2  %3  %4  %5")
                     .arg(i + 1, 3)
                     .arg(p.job.plan.starts.value(i), 7, 'f', 2)
                     .arg(s.duration, 8, 'f', 2)
                     .arg(transition, -20)
                     .arg(QFileInfo(s.path).fileName())
              << Qt::endl;
    }
    out() << Qt::endl
          << QStringLiteral("Images: %1   Video length: %2 s   Audio length: %3 s")
                 .arg(slides.size())
                 .arg(p.job.plan.total, 0, 'f', 2)
                 .arg(p.audioSeconds, 0, 'f', 2)
          << Qt::endl;
}

// `quality` overrides the project's own choice when set (--quality).
std::optional<PreparedJob> loadAndPrepare(const QString &projectPath, const QString &outputPath,
                                          bool autoSpaced, std::optional<ExportQuality> quality = {})
{
    QString error;
    auto project = Project::load(projectPath, &error);
    if (!project) {
        err() << "Error: " << error << Qt::endl;
        return std::nullopt;
    }
    if (quality)
        project->output.quality = *quality;
    return prepareJob(*project, outputPath, autoSpaced);
}

int runInfo(const QString &projectPath, bool autoSpaced, bool json)
{
    const auto p = loadAndPrepare(projectPath, QString(), autoSpaced);
    if (!p)
        return 1;
    if (json) {
        // Everything on stdout, problems included, so a script needs one read.
        QString error;
        const auto project = Project::load(projectPath, &error);
        out() << QJsonDocument(CliEdit::infoJson(projectPath, *project, *p)).toJson(QJsonDocument::Indented);
        out().flush();
        return p->ok() ? 0 : 1;
    }
    printTimeline(*p);
    printMessages(*p);
    return p->ok() ? 0 : 1;
}

int runRender(QCoreApplication &app, const QString &projectPath, const QString &outputPath,
              bool autoSpaced, bool overwrite, bool dryRun, std::optional<ExportQuality> quality)
{
    if (!dryRun && QFileInfo::exists(outputPath) && !overwrite) {
        err() << "Error: " << outputPath << " already exists. Use --overwrite to replace it."
              << Qt::endl;
        return 1;
    }

    auto p = loadAndPrepare(projectPath, outputPath, autoSpaced, quality);
    if (!p)
        return 1;
    printMessages(*p);
    if (!p->ok())
        return 1;

    if (dryRun) {
        out() << FfmpegCommand::shellCommand(FfmpegCommand::arguments(p->job)) << Qt::endl;
        return 0;
    }

    // ffmpeg writes a temporary file; only a finished video gets the name.
    QString error;
    const QString temporary = ExportFile::createTemporary(outputPath, &error);
    if (temporary.isEmpty()) {
        err() << "Error: " << error << Qt::endl;
        return 1;
    }
    p->job.outputPath = temporary;
    const QStringList args = FfmpegCommand::arguments(p->job);

    const double total = p->job.plan.total;
    err() << QStringLiteral("Rendering %1 images, %2 s → %3")
                 .arg(p->job.slides.size())
                 .arg(total, 0, 'f', 2)
                 .arg(outputPath)
          << Qt::endl;

    const bool interactive = isatty(fileno(stderr));
    int lastShown = -1;
    int exitCode = 1;

    Renderer renderer;
    QObject::connect(&renderer, &Renderer::progress, [&](double fraction) {
        const int percent = int(fraction * 100);
        if (interactive && percent != lastShown) {
            err() << "\r  " << percent << "%" << Qt::flush;
        } else if (!interactive && percent / 10 != lastShown / 10) {
            err() << "  " << percent << "%" << Qt::endl;
        }
        lastShown = percent;
    });
    QObject::connect(&renderer, &Renderer::finished, [&](bool ok, const QString &error) {
        if (interactive)
            err() << "\r" << Qt::flush;
        QString moveError;
        if (ok && ExportFile::finish(temporary, QFileInfo(outputPath).absoluteFilePath(), &moveError)) {
            err() << "Done: " << outputPath << Qt::endl;
            exitCode = 0;
        } else {
            QFile::remove(temporary);
            err() << "Error: " << (ok ? moveError : error) << Qt::endl;
        }
        app.quit();
    });

    renderer.start(args, total);
    app.exec();
    return exitCode;
}

}

bool isCliCommand(const char *arg)
{
    const QByteArray a(arg);
    return a == "render" || a == "info" || a == "transitions" || a == "-h" || a == "--help"
        || a == "-v" || a == "--version" || CliEdit::isCommand(QString::fromLocal8Bit(arg));
}

int runCli(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("omasoundslides"));
    QCoreApplication::setApplicationVersion(QStringLiteral(OMASOUNDSLIDES_VERSION));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("command"),
                                 QStringLiteral("render, info, transitions, or an edit command (see below)"));
    parser.setApplicationDescription(QStringLiteral("Pictures + sound → video, like Soundslides.\n\n") + QLatin1String(Usage)
                                     + CliEdit::usage());
    const QCommandLineOption autoOption(
        QStringLiteral("auto"), QStringLiteral("Spread the audio length evenly across all images."));
    const QCommandLineOption overwriteOption(
        QStringLiteral("overwrite"), QStringLiteral("Replace the output file if it exists."));
    const QCommandLineOption dryRunOption(
        QStringLiteral("dry-run"), QStringLiteral("Print the ffmpeg command without running it."));
    const QCommandLineOption qualityOption(
        QStringLiteral("quality"),
        QStringLiteral("H.264 quality: standard (smaller) or high. Default: the project's choice."),
        QStringLiteral("standard|high"));
    const QCommandLineOption jsonOption(
        QStringLiteral("json"), QStringLiteral("info: print the project and its timeline as JSON."));
    const QCommandLineOption atOption(
        QStringLiteral("at"), QStringLiteral("add-images: insert after image N (0 = first)."), QStringLiteral("N"));
    const QCommandLineOption nameOption(
        QStringLiteral("name"), QStringLiteral("new: the project's name."), QStringLiteral("NAME"));
    parser.addOptions({autoOption, overwriteOption, dryRunOption, qualityOption, jsonOption, atOption, nameOption});
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    const QString command = args.value(0);

    if (command == QLatin1String("transitions")) {
        for (const Transitions::Preset &p : Transitions::presets())
            out() << QStringLiteral("%1  transition=%2  length=%3 s")
                         .arg(p.label, -22)
                         .arg(p.transition, -9)
                         .arg(p.duration, 0, 'f', 1)
                  << Qt::endl;
        return 0;
    }

    if (CliEdit::isCommand(command)) {
        CliEdit::Options options;
        options.name = parser.value(nameOption);
        options.overwrite = parser.isSet(overwriteOption);
        if (parser.isSet(atOption)) {
            bool ok = false;
            options.at = parser.value(atOption).toInt(&ok);
            if (!ok || options.at < 0) {
                err() << "Error: --at must be an image number (0 = first)" << Qt::endl;
                return 2;
            }
        }
        return CliEdit::run(command, args.mid(1), options);
    }

    if (command != QLatin1String("render") && command != QLatin1String("info")) {
        err() << Usage << CliEdit::usage();
        return 2;
    }

    for (const char *tool : {"ffmpeg", "ffprobe"}) {
        if (QStandardPaths::findExecutable(QLatin1String(tool)).isEmpty()) {
            err() << "Error: " << tool << " was not found. Install it with: sudo pacman -S ffmpeg"
                  << Qt::endl;
            return 1;
        }
    }

    if (command == QLatin1String("info")) {
        if (args.size() != 2) {
            err() << Usage;
            return 2;
        }
        return runInfo(args.at(1), parser.isSet(autoOption), parser.isSet(jsonOption));
    }

    if (args.size() != 3) {
        err() << Usage;
        return 2;
    }
    std::optional<ExportQuality> quality;
    if (parser.isSet(qualityOption)) {
        quality = exportQualityFromName(parser.value(qualityOption));
        if (!quality) {
            err() << "Error: --quality must be standard or high" << Qt::endl;
            return 2;
        }
    }
    return runRender(app, args.at(1), args.at(2), parser.isSet(autoOption),
                     parser.isSet(overwriteOption), parser.isSet(dryRunOption), quality);
}
