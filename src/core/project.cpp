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
        *error = QStringLiteral("output.%1 skal være et positivt heltal").arg(QLatin1String(key));
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
        *error = QStringLiteral("%1.%2 skal være et tal ≥ 0 (sekunder)").arg(where, QLatin1String(key));
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
        *error = QStringLiteral("%1.transition: ukendt overgang \"%2\"").arg(where, raw);
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

std::optional<Project> Project::fromJson(const QJsonObject &json, QString *error)
{
    Project p;
    bool ok = true;

    const QJsonObject out = json.value(QLatin1String("output")).toObject();
    if (!readPositiveInt(out, "width", &p.output.width, error)
        || !readPositiveInt(out, "height", &p.output.height, error)
        || !readPositiveInt(out, "fps", &p.output.fps, error))
        return std::nullopt;
    if (p.output.width % 2 || p.output.height % 2) {
        *error = QStringLiteral("output.width og output.height skal være lige tal (krav fra H.264)");
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
            *error = QStringLiteral("%1 mangler \"path\"").arg(where);
            return std::nullopt;
        }
        p.slides.append(s);
    }

    const QJsonValue audio = json.value(QLatin1String("audio"));
    const QJsonArray audioList = audio.isString() ? QJsonArray{audio} : audio.toArray();
    for (const QJsonValue &a : audioList) {
        if (!a.isString() || a.toString().isEmpty()) {
            *error = QStringLiteral("audio skal være en liste af filstier");
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

    return QJsonObject{
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
}

std::optional<Project> Project::load(const QString &filePath, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Kan ikke åbne %1: %2").arg(filePath, file.errorString());
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        *error = QStringLiteral("Ugyldig JSON i %1 (tegn %2): %3")
                     .arg(filePath)
                     .arg(parseError.offset)
                     .arg(parseError.errorString());
        return std::nullopt;
    }
    if (!doc.isObject()) {
        *error = QStringLiteral("%1 skal indeholde et JSON-objekt").arg(filePath);
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
        *error = QStringLiteral("Kan ikke skrive %1: %2").arg(filePath, file.errorString());
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        *error = QStringLiteral("Kan ikke gemme %1: %2").arg(filePath, file.errorString());
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
