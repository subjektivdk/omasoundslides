#include "project.h"
#include "transitions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

bool readPositiveInt(const QJsonObject &obj, const char *key, int *out, QString *error)
{
    if (!obj.contains(QLatin1String(key)))
        return true;
    const QJsonValue v = obj.value(QLatin1String(key));
    if (!v.isDouble() || v.toDouble() <= 0 || v.toDouble() != int(v.toDouble())) {
        *error = QStringLiteral("output.%1 must be a positive whole number").arg(QLatin1String(key));
        return false;
    }
    *out = v.toInt();
    return true;
}

std::optional<double> readSeconds(const QJsonObject &obj, const char *key, const QString &where,
                                  bool *ok, QString *error)
{
    if (!obj.contains(QLatin1String(key)))
        return std::nullopt;
    const QJsonValue v = obj.value(QLatin1String(key));
    if (!v.isDouble() || v.toDouble() < 0) {
        *error = QStringLiteral("%1.%2 must be a number ≥ 0 (seconds)").arg(where, QLatin1String(key));
        *ok = false;
        return std::nullopt;
    }
    return v.toDouble();
}

std::optional<QString> readTransition(const QJsonObject &obj, const QString &where, bool *ok,
                                      QString *error)
{
    if (!obj.contains(QLatin1String("transition")))
        return std::nullopt;
    const QString raw = obj.value(QLatin1String("transition")).toString();
    const QString name = Transitions::canonical(raw);
    if (name.isEmpty()) {
        *error = QStringLiteral("%1.transition: unknown transition \"%2\"").arg(where, raw);
        *ok = false;
        return std::nullopt;
    }
    return name;
}

}

QString Project::resolvePath(const QString &path) const
{
    if (QDir::isAbsolutePath(path) || baseDir.isEmpty())
        return QDir::cleanPath(path);
    return QDir::cleanPath(QDir(baseDir).filePath(path));
}

Project Project::withAbsolutePaths() const
{
    Project p = *this;
    for (Slide &s : p.slides)
        s.path = resolvePath(s.path);
    for (QString &a : p.audio)
        a = resolvePath(a);
    p.baseDir.clear();
    return p;
}

Project Project::withPathsRelativeTo(const QString &dir) const
{
    const Project absolute = withAbsolutePaths();
    const QDir base(dir);
    Project p = absolute;
    for (Slide &s : p.slides)
        s.path = base.relativeFilePath(s.path);
    for (QString &a : p.audio)
        a = base.relativeFilePath(a);
    p.baseDir = QDir(dir).absolutePath();
    return p;
}

std::optional<Project> Project::fromJson(const QJsonObject &json, QString *error)
{
    Project p;
    bool ok = true;

    p.name = json.value(QLatin1String("name")).toString().trimmed();

    const QJsonObject out = json.value(QLatin1String("output")).toObject();
    if (!readPositiveInt(out, "width", &p.output.width, error)
        || !readPositiveInt(out, "height", &p.output.height, error)
        || !readPositiveInt(out, "fps", &p.output.fps, error))
        return std::nullopt;
    if (p.output.width % 2 || p.output.height % 2) {
        *error = QStringLiteral("output.width and output.height must be even numbers (an H.264 requirement)");
        return std::nullopt;
    }

    const QJsonObject def = json.value(QLatin1String("defaults")).toObject();
    const QString defWhere = QStringLiteral("defaults");
    if (auto d = readSeconds(def, "duration", defWhere, &ok, error))
        p.defaults.duration = *d;
    if (auto t = readTransition(def, defWhere, &ok, error))
        p.defaults.transition = *t;
    if (auto td = readSeconds(def, "transition_duration", defWhere, &ok, error))
        p.defaults.transitionDuration = *td;
    if (!ok)
        return std::nullopt;

    const QJsonArray images = json.value(QLatin1String("images")).toArray();
    for (qsizetype i = 0; i < images.size(); ++i) {
        const QString where = QStringLiteral("images[%1]").arg(i);
        Slide s;
        if (images.at(i).isString()) {
            s.path = images.at(i).toString();
        } else {
            const QJsonObject obj = images.at(i).toObject();
            s.path = obj.value(QLatin1String("path")).toString();
            s.duration = readSeconds(obj, "duration", where, &ok, error);
            s.transition = readTransition(obj, where, &ok, error);
            s.transitionDuration = readSeconds(obj, "transition_duration", where, &ok, error);
            if (!ok)
                return std::nullopt;
        }
        if (s.path.isEmpty()) {
            *error = QStringLiteral("%1 is missing \"path\"").arg(where);
            return std::nullopt;
        }
        p.slides.append(s);
    }

    const QJsonObject fade = json.value(QLatin1String("audio_fade")).toObject();
    const QString fadeWhere = QStringLiteral("audio_fade");
    if (auto in = readSeconds(fade, "in", fadeWhere, &ok, error))
        p.audioFadeIn = *in;
    if (auto out = readSeconds(fade, "out", fadeWhere, &ok, error))
        p.audioFadeOut = *out;
    if (!ok)
        return std::nullopt;

    const QJsonValue audio = json.value(QLatin1String("audio"));
    const QJsonArray audioList = audio.isString() ? QJsonArray{audio} : audio.toArray();
    for (const QJsonValue &a : audioList) {
        if (!a.isString() || a.toString().isEmpty()) {
            *error = QStringLiteral("audio must be a list of file paths");
            return std::nullopt;
        }
        p.audio.append(a.toString());
    }

    return p;
}

QJsonObject Project::toJson() const
{
    QJsonArray list;
    for (const Slide &s : slides) {
        QJsonObject obj{{QStringLiteral("path"), s.path}};
        if (s.duration)
            obj.insert(QStringLiteral("duration"), *s.duration);
        if (s.transition)
            obj.insert(QStringLiteral("transition"), *s.transition);
        if (s.transitionDuration)
            obj.insert(QStringLiteral("transition_duration"), *s.transitionDuration);
        list.append(obj);
    }

    QJsonObject json{
        {QStringLiteral("output"), QJsonObject{
             {QStringLiteral("width"), output.width},
             {QStringLiteral("height"), output.height},
             {QStringLiteral("fps"), output.fps},
         }},
        {QStringLiteral("defaults"), QJsonObject{
             {QStringLiteral("duration"), defaults.duration},
             {QStringLiteral("transition"), defaults.transition},
             {QStringLiteral("transition_duration"), defaults.transitionDuration},
         }},
        {QStringLiteral("images"), list},
        {QStringLiteral("audio"), QJsonArray::fromStringList(audio)},
    };
    if (!name.isEmpty())
        json.insert(QStringLiteral("name"), name);
    if (audioFadeIn > 0 || audioFadeOut > 0)
        json.insert(QStringLiteral("audio_fade"),
                    QJsonObject{{QStringLiteral("in"), audioFadeIn}, {QStringLiteral("out"), audioFadeOut}});
    return json;
}

std::optional<Project> Project::load(const QString &filePath, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        *error = QStringLiteral("Invalid JSON in %1 (character %2): %3")
                     .arg(filePath)
                     .arg(parseError.offset)
                     .arg(parseError.errorString());
        return std::nullopt;
    }
    if (!doc.isObject()) {
        *error = QStringLiteral("%1 must contain a JSON object").arg(filePath);
        return std::nullopt;
    }

    auto project = fromJson(doc.object(), error);
    if (project)
        project->baseDir = QFileInfo(filePath).absolutePath();
    return project;
}

bool Project::save(const QString &filePath, QString *error) const
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QStringLiteral("Cannot write %1: %2").arg(filePath, file.errorString());
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        *error = QStringLiteral("Cannot save %1: %2").arg(filePath, file.errorString());
        return false;
    }
    return true;
}

QList<ResolvedSlide> resolveSlides(const Project &project)
{
    QList<ResolvedSlide> result;
    result.reserve(project.slides.size());

    for (qsizetype i = 0; i < project.slides.size(); ++i) {
        const Slide &s = project.slides.at(i);
        ResolvedSlide r;
        r.path = project.resolvePath(s.path);
        r.duration = s.duration.value_or(project.defaults.duration);
        r.transition = s.transition.value_or(project.defaults.transition);
        r.transitionDuration = s.transitionDuration.value_or(project.defaults.transitionDuration);
        if (i == 0 || Transitions::isCut(r.transition) || r.transitionDuration <= 0) {
            r.transition = QStringLiteral("none");
            r.transitionDuration = 0;
        }
        result.append(r);
    }
    return result;
}
