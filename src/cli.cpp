#include "cli.h"
#include "core/ffmpegcommand.h"
#include "core/prepare.h"
#include "core/project.h"
#include "core/renderer.h"
#include "core/transitions.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
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
    "  omasoundslides render <project.json> <out.mp4> [--auto] [--overwrite] [--dry-run]\n"
    "  omasoundslides info <project.json> [--auto]\n"
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

std::optional<PreparedJob> loadAndPrepare(const QString &projectPath, const QString &outputPath,
                                          bool autoSpaced)
{
    QString error;
    const auto project = Project::load(projectPath, &error);
    if (!project) {
        err() << "Error: " << error << Qt::endl;
        return std::nullopt;
    }
    return prepareJob(*project, outputPath, autoSpaced);
}

int runInfo(const QString &projectPath, bool autoSpaced)
{
    const auto p = loadAndPrepare(projectPath, QString(), autoSpaced);
    if (!p)
        return 1;
    printTimeline(*p);
    printMessages(*p);
    return p->ok() ? 0 : 1;
}

int runRender(QCoreApplication &app, const QString &projectPath, const QString &outputPath,
              bool autoSpaced, bool overwrite, bool dryRun)
{
    if (!dryRun && QFileInfo::exists(outputPath) && !overwrite) {
        err() << "Error: " << outputPath << " already exists. Use --overwrite to replace it."
              << Qt::endl;
        return 1;
    }

    const auto p = loadAndPrepare(projectPath, outputPath, autoSpaced);
    if (!p)
        return 1;
    printMessages(*p);
    if (!p->ok())
        return 1;

    const QStringList args = FfmpegCommand::arguments(p->job);
    if (dryRun) {
        out() << FfmpegCommand::shellCommand(args) << Qt::endl;
        return 0;
    }

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
        if (ok) {
            err() << "Done: " << outputPath << Qt::endl;
            exitCode = 0;
        } else {
            err() << "Error: " << error << Qt::endl;
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
        || a == "-v" || a == "--version";
}

int runCli(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("omasoundslides"));
    QCoreApplication::setApplicationVersion(QStringLiteral(OMASOUNDSLIDES_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Pictures + sound → video, like Soundslides."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("render, info or transitions"));
    const QCommandLineOption autoOption(
        QStringLiteral("auto"), QStringLiteral("Spread the audio length evenly across all images."));
    const QCommandLineOption overwriteOption(
        QStringLiteral("overwrite"), QStringLiteral("Replace the output file if it exists."));
    const QCommandLineOption dryRunOption(
        QStringLiteral("dry-run"), QStringLiteral("Print the ffmpeg command without running it."));
    parser.addOptions({autoOption, overwriteOption, dryRunOption});
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

    if (command != QLatin1String("render") && command != QLatin1String("info")) {
        err() << Usage;
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
        return runInfo(args.at(1), parser.isSet(autoOption));
    }

    if (args.size() != 3) {
        err() << Usage;
        return 2;
    }
    return runRender(app, args.at(1), args.at(2), parser.isSet(autoOption),
                     parser.isSet(overwriteOption), parser.isSet(dryRunOption));
}
