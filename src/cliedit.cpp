#include "cliedit.h"

#include "core/audacitylabels.h"
#include "core/probe.h"
#include "core/projectmodel.h"
#include "core/transitions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>
#include <functional>

namespace CliEdit {

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

// Thrown for a bad value; turned into "Error: …" and exit code 1.
struct Failure {
    QString message;
};

// "12.5", "12,5" or "1:23.5" (minutes:seconds) → seconds.
double parseSeconds(const QString &text, const QString &what)
{
    QString t = text.trimmed();
    t.replace(QLatin1Char(','), QLatin1Char('.'));
    double minutes = 0;
    const qsizetype colon = t.indexOf(QLatin1Char(':'));
    bool ok = true;
    if (colon >= 0) {
        minutes = t.left(colon).toDouble(&ok);
        t = t.mid(colon + 1);
    }
    bool ok2 = false;
    const double seconds = t.toDouble(&ok2);
    if (!ok || !ok2 || seconds < 0 || minutes < 0)
        throw Failure{QStringLiteral("%1: \"%2\" is not a time in seconds").arg(what, text)};
    return minutes * 60 + seconds;
}

// A 1-based image number → 0-based index.
int parseImage(const QString &text, int count)
{
    bool ok = false;
    const int n = text.toInt(&ok);
    if (!ok || n < 1 || n > count)
        throw Failure{count == 0 ? QStringLiteral("the project has no images")
                                 : QStringLiteral("image \"%1\" does not exist (1–%2)").arg(text).arg(count)};
    return n - 1;
}

const Transitions::Preset &parsePreset(const QString &id)
{
    QStringList ids;
    for (const Transitions::Preset &p : Transitions::presets()) {
        if (p.id == id)
            return p;
        ids << p.id;
    }
    throw Failure{QStringLiteral("unknown transition \"%1\" (use %2)").arg(id, ids.join(QStringLiteral(", ")))};
}

QPair<QString, QString> splitAssignment(const QString &arg)
{
    const qsizetype equals = arg.indexOf(QLatin1Char('='));
    if (equals <= 0)
        throw Failure{QStringLiteral("\"%1\" is not key=value").arg(arg)};
    return {arg.left(equals).trimmed(), arg.mid(equals + 1).trimmed()};
}

QString describe(const ProjectModel &model)
{
    return QStringLiteral("%1 images, %2 s").arg(model.rowCount()).arg(model.videoDuration(), 0, 'f', 2);
}

// Loads a project into a model, runs `edit`, and saves it the way the
// window's Save does. Nothing is written when `edit` fails.
int editProject(const QString &path, const std::function<QString(ProjectModel &)> &edit)
{
    QString error;
    const auto project = Project::load(path, &error);
    if (!project) {
        err() << "Error: " << error << Qt::endl;
        return 1;
    }
    ProjectModel model;
    model.setProject(*project);

    QString summary;
    try {
        summary = edit(model);
    } catch (const Failure &failure) {
        err() << "Error: " << failure.message << Qt::endl;
        return 1;
    }

    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (!model.project().withPathsRelativeTo(QFileInfo(absolute).absolutePath()).save(absolute, &error)) {
        err() << "Error: " << error << Qt::endl;
        return 1;
    }
    out() << summary << Qt::endl;
    for (const QString &problem : model.problems())
        err() << "Warning: " << problem << Qt::endl;
    return 0;
}

void requireMedia(const QStringList &paths, bool audio)
{
    for (const QString &path : paths) {
        const MediaInfo info = Probe::inspect(path);
        if (!info.ok)
            throw Failure{info.error};
        if (audio ? !info.hasAudio : !info.hasVideo)
            throw Failure{QStringLiteral("%1 is not %2").arg(path, audio ? QStringLiteral("audio")
                                                                         : QStringLiteral("an image"))};
    }
}

int usageError()
{
    err() << "Usage:\n" << usage();
    return 2;
}

}

bool isCommand(const QString &command)
{
    static const QStringList commands = {
        QStringLiteral("new"), QStringLiteral("add-images"), QStringLiteral("add-audio"),
        QStringLiteral("remove-image"), QStringLiteral("move-image"), QStringLiteral("set"),
        QStringLiteral("set-image"), QStringLiteral("markers"), QStringLiteral("fit"),
    };
    return commands.contains(command);
}

QString usage()
{
    return QStringLiteral(
        "  omasoundslides new <project.json> [--name NAME] [--overwrite]\n"
        "  omasoundslides add-images <project.json> <image>... [--at N]\n"
        "  omasoundslides add-audio <project.json> <audio>...\n"
        "  omasoundslides remove-image <project.json> <n>\n"
        "  omasoundslides move-image <project.json> <from> <to>\n"
        "  omasoundslides set <project.json> key=value...\n"
        "      name, duration, transition, fps, resolution=WxH, quality, fade-in, fade-out\n"
        "  omasoundslides set-image <project.json> <n> key=value...\n"
        "      duration=S|default, transition=<id>|default\n"
        "  omasoundslides markers <project.json> list|clear\n"
        "  omasoundslides markers <project.json> add|remove <seconds>...\n"
        "  omasoundslides markers <project.json> import|export <labels.txt>\n"
        "  omasoundslides fit <project.json> audio|markers\n"
        "  Images are numbered from 1; times are seconds or m:ss; transitions are\n"
        "  the ids from `omasoundslides transitions`.\n");
}

int run(const QString &command, const QStringList &args, const Options &options)
{
    if (args.isEmpty())
        return usageError();
    const QString path = args.first();
    const QStringList rest = args.mid(1);

    if (command == QLatin1String("new")) {
        if (!rest.isEmpty())
            return usageError();
        if (QFileInfo::exists(path) && !options.overwrite) {
            err() << "Error: " << path << " already exists. Use --overwrite to replace it." << Qt::endl;
            return 1;
        }
        Project project;
        project.name = options.name.trimmed();
        QString error;
        if (!project.save(path, &error)) {
            err() << "Error: " << error << Qt::endl;
            return 1;
        }
        out() << "Created " << path << Qt::endl;
        return 0;
    }

    if (command == QLatin1String("add-images") || command == QLatin1String("add-audio")) {
        if (rest.isEmpty())
            return usageError();
        const bool audio = command == QLatin1String("add-audio");
        return editProject(path, [&](ProjectModel &model) {
            requireMedia(rest, audio);
            if (audio) {
                model.addAudio(rest);
                return QStringLiteral("Added %1 audio file(s) · audio %2 s")
                    .arg(rest.size())
                    .arg(model.audioDuration(), 0, 'f', 2);
            }
            int at = -1;
            if (options.at >= 0) {
                if (options.at > model.rowCount())
                    throw Failure{QStringLiteral("--at %1: the project has %2 images").arg(options.at).arg(model.rowCount())};
                at = options.at; // after image N = before index N
            }
            model.addImages(rest, at);
            return QStringLiteral("Added %1 image(s) · %2").arg(rest.size()).arg(describe(model));
        });
    }

    if (command == QLatin1String("remove-image")) {
        if (rest.size() != 1)
            return usageError();
        return editProject(path, [&](ProjectModel &model) {
            model.removeImage(parseImage(rest.first(), model.rowCount()));
            return QStringLiteral("Removed image %1 · %2").arg(rest.first(), describe(model));
        });
    }

    if (command == QLatin1String("move-image")) {
        if (rest.size() != 2)
            return usageError();
        return editProject(path, [&](ProjectModel &model) {
            const int from = parseImage(rest.at(0), model.rowCount());
            const int to = parseImage(rest.at(1), model.rowCount());
            model.moveImage(from, to);
            return QStringLiteral("Moved image %1 to %2").arg(from + 1).arg(to + 1);
        });
    }

    if (command == QLatin1String("set")) {
        if (rest.isEmpty())
            return usageError();
        return editProject(path, [&](ProjectModel &model) {
            for (const QString &arg : rest) {
                const auto [key, value] = splitAssignment(arg);
                if (key == QLatin1String("name")) {
                    model.setName(value);
                } else if (key == QLatin1String("duration")) {
                    const double d = parseSeconds(value, key);
                    if (d <= 0)
                        throw Failure{QStringLiteral("duration must be more than 0")};
                    model.setDefaultDuration(d);
                } else if (key == QLatin1String("transition")) {
                    const Transitions::Preset &p = parsePreset(value);
                    model.setDefaultTransitionPreset(p.transition, p.duration);
                } else if (key == QLatin1String("fps")) {
                    bool ok = false;
                    const int fps = value.toInt(&ok);
                    if (!ok || fps <= 0 || fps > 120)
                        throw Failure{QStringLiteral("fps must be a whole number from 1 to 120")};
                    model.setFps(fps);
                } else if (key == QLatin1String("resolution")) {
                    const QStringList wh = value.toLower().split(QLatin1Char('x'));
                    bool okW = false;
                    bool okH = false;
                    const int w = wh.value(0).toInt(&okW);
                    const int h = wh.value(1).toInt(&okH);
                    if (wh.size() != 2 || !okW || !okH || w <= 0 || h <= 0 || w % 2 || h % 2)
                        throw Failure{QStringLiteral("resolution must be WIDTHxHEIGHT with even numbers, e.g. 1920x1080")};
                    model.setResolution(w, h);
                } else if (key == QLatin1String("quality")) {
                    if (!exportQualityFromName(value))
                        throw Failure{QStringLiteral("quality must be standard or high")};
                    model.setExportQuality(value);
                } else if (key == QLatin1String("fade-in")) {
                    model.setAudioFadeIn(parseSeconds(value, key));
                } else if (key == QLatin1String("fade-out")) {
                    model.setAudioFadeOut(parseSeconds(value, key));
                } else {
                    throw Failure{QStringLiteral("unknown setting \"%1\"").arg(key)};
                }
            }
            return QStringLiteral("Updated · %1").arg(describe(model));
        });
    }

    if (command == QLatin1String("set-image")) {
        if (rest.size() < 2)
            return usageError();
        return editProject(path, [&](ProjectModel &model) {
            const int index = parseImage(rest.first(), model.rowCount());
            for (const QString &arg : rest.mid(1)) {
                const auto [key, value] = splitAssignment(arg);
                const bool reset = value == QLatin1String("default");
                if (key == QLatin1String("duration")) {
                    if (reset) {
                        model.resetDuration(index);
                        continue;
                    }
                    const double d = parseSeconds(value, key);
                    if (d <= 0)
                        throw Failure{QStringLiteral("duration must be more than 0")};
                    model.setDuration(index, d);
                } else if (key == QLatin1String("transition")) {
                    if (reset) {
                        model.resetTransitionPreset(index);
                        continue;
                    }
                    if (index == 0)
                        throw Failure{QStringLiteral("the first image has no transition in")};
                    const Transitions::Preset &p = parsePreset(value);
                    model.setTransitionPreset(index, p.transition, p.duration);
                } else {
                    throw Failure{QStringLiteral("unknown image setting \"%1\" (duration, transition)").arg(key)};
                }
            }
            return QStringLiteral("Updated image %1 · %2").arg(index + 1).arg(describe(model));
        });
    }

    if (command == QLatin1String("markers")) {
        const QString action = rest.value(0);
        const QStringList values = rest.mid(1);
        if (action == QLatin1String("list")) {
            QString error;
            const auto project = Project::load(path, &error);
            if (!project) {
                err() << "Error: " << error << Qt::endl;
                return 1;
            }
            QList<double> markers = project->markers;
            std::sort(markers.begin(), markers.end());
            for (double m : markers)
                out() << QString::number(m, 'f', 3) << Qt::endl;
            return 0;
        }
        if (action == QLatin1String("export")) {
            if (values.size() != 1)
                return usageError();
            QString error;
            const auto project = Project::load(path, &error);
            if (!project) {
                err() << "Error: " << error << Qt::endl;
                return 1;
            }
            QSaveFile file(values.first());
            if (!file.open(QIODevice::WriteOnly)) {
                err() << "Error: cannot write " << values.first() << Qt::endl;
                return 1;
            }
            file.write(AudacityLabels::write(project->markers).toUtf8());
            if (!file.commit()) {
                err() << "Error: cannot save " << values.first() << Qt::endl;
                return 1;
            }
            out() << "Exported " << project->markers.size() << " markers to " << values.first() << Qt::endl;
            return 0;
        }
        if (action == QLatin1String("clear") && values.isEmpty()) {
            return editProject(path, [&](ProjectModel &model) {
                model.clearMarkers();
                return QStringLiteral("Cleared the markers");
            });
        }
        if ((action == QLatin1String("add") || action == QLatin1String("remove")) && !values.isEmpty()) {
            return editProject(path, [&](ProjectModel &model) {
                int changed = 0;
                for (const QString &value : values) {
                    const double t = parseSeconds(value, QStringLiteral("marker"));
                    if (action == QLatin1String("add"))
                        changed += model.addMarker(t) >= 0;
                    else if (model.removeMarkerNear(t, 0.5))
                        ++changed;
                    else
                        throw Failure{QStringLiteral("no marker within 0.5 s of %1").arg(value)};
                }
                return QStringLiteral("%1 %2 marker(s) · %3 in total")
                    .arg(action == QLatin1String("add") ? QStringLiteral("Added") : QStringLiteral("Removed"))
                    .arg(changed)
                    .arg(model.markers().size());
            });
        }
        if (action == QLatin1String("import") && values.size() == 1) {
            return editProject(path, [&](ProjectModel &model) {
                QFile file(values.first());
                if (!file.open(QIODevice::ReadOnly))
                    throw Failure{QStringLiteral("cannot open %1").arg(values.first())};
                QStringList warnings;
                const auto labels = AudacityLabels::parse(QString::fromUtf8(file.readAll()), &warnings);
                if (labels.isEmpty())
                    throw Failure{QStringLiteral("no Audacity labels in %1").arg(values.first())};
                QList<double> times;
                for (const auto &label : labels)
                    times.append(label.start);
                const int count = model.replaceMarkers(times, QStringLiteral("Import markers"));
                for (const QString &w : warnings)
                    err() << "Warning: " << w << Qt::endl;
                return QStringLiteral("Imported %1 markers").arg(count);
            });
        }
        return usageError();
    }

    if (command == QLatin1String("fit")) {
        if (rest.size() != 1)
            return usageError();
        const QString what = rest.first();
        if (what != QLatin1String("audio") && what != QLatin1String("markers"))
            return usageError();
        return editProject(path, [&](ProjectModel &model) {
            if (what == QLatin1String("audio")) {
                if (!model.fitToAudio())
                    throw Failure{QStringLiteral("fitting needs images and audio")};
                return QStringLiteral("Each image now lasts %1 s · %2")
                    .arg(model.defaultDuration(), 0, 'f', 3)
                    .arg(describe(model));
            }
            const int used = model.fitToMarkers();
            if (used == 0)
                throw Failure{QStringLiteral("fitting needs at least two images and one marker")};
            return QStringLiteral("Fitted %1 image changes to markers · %2").arg(used).arg(describe(model));
        });
    }

    return usageError();
}

QJsonObject infoJson(const QString &projectPath, const Project &project, const PreparedJob &prepared)
{
    QJsonArray images;
    for (int i = 0; i < prepared.job.slides.size(); ++i) {
        const ResolvedSlide &r = prepared.job.slides.at(i);
        const Slide &s = project.slides.value(i);
        images.append(QJsonObject{
            {QStringLiteral("number"), i + 1},
            {QStringLiteral("path"), r.path},
            {QStringLiteral("start"), prepared.job.plan.starts.value(i)},
            {QStringLiteral("duration"), r.duration},
            {QStringLiteral("duration_set"), s.duration.has_value()},
            {QStringLiteral("transition"), r.transition},
            {QStringLiteral("transition_duration"), r.transitionDuration},
            {QStringLiteral("transition_set"), s.transition.has_value() || s.transitionDuration.has_value()},
        });
    }

    QJsonArray audio;
    for (const QString &path : prepared.job.audioPaths)
        audio.append(QJsonObject{
            {QStringLiteral("path"), path},
            {QStringLiteral("duration"), Probe::inspect(path).duration},
        });

    QList<double> sorted = project.markers;
    std::sort(sorted.begin(), sorted.end());
    QJsonArray markers;
    for (double m : sorted)
        markers.append(m);

    return QJsonObject{
        {QStringLiteral("project"), QFileInfo(projectPath).absoluteFilePath()},
        {QStringLiteral("name"), project.name},
        {QStringLiteral("output"), project.toJson().value(QStringLiteral("output"))},
        {QStringLiteral("defaults"), project.toJson().value(QStringLiteral("defaults"))},
        {QStringLiteral("video_duration"), prepared.job.plan.total},
        {QStringLiteral("audio_duration"), prepared.audioSeconds},
        {QStringLiteral("audio_fade"), QJsonObject{{QStringLiteral("in"), project.audioFadeIn},
                                                   {QStringLiteral("out"), project.audioFadeOut}}},
        {QStringLiteral("images"), images},
        {QStringLiteral("audio"), audio},
        {QStringLiteral("markers"), markers},
        {QStringLiteral("errors"), QJsonArray::fromStringList(prepared.errors)},
        {QStringLiteral("warnings"), QJsonArray::fromStringList(prepared.warnings)},
    };
}

}
