#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

// One image as stored in the project file. Unset fields fall back to the
// project defaults, like Soundslides' "use project default" in the Item Inspector.
struct Slide {
    QString path;
    std::optional<double> duration;
    // Transition INTO this image from the previous one. Ignored on the first image.
    std::optional<QString> transition;
    std::optional<double> transitionDuration;
};

struct OutputSettings {
    int width = 1920;
    int height = 1080;
    int fps = 30;
};

struct Defaults {
    double duration = 5.0;
    QString transition = QStringLiteral("fade");
    double transitionDuration = 1.0;
};

struct Project {
    QString name; // optional; the file name is used when empty
    OutputSettings output;
    Defaults defaults;
    QList<Slide> slides;
    QStringList audio;
    // Fades on the joined audio, in seconds; 0 = none. The fade-out ends
    // where the audio ends in the video.
    double audioFadeIn = 0;
    double audioFadeOut = 0;
    // Cue points in seconds, set while listening; images can be fitted to them.
    QList<double> markers;
    // Directory that relative paths in the project file are resolved against.
    QString baseDir;

    QString resolvePath(const QString &path) const;

    // The GUI works with absolute paths; the file on disk stores them relative
    // to the project file so a project folder can be moved or shared.
    Project withAbsolutePaths() const;
    Project withPathsRelativeTo(const QString &dir) const;

    static std::optional<Project> fromJson(const QJsonObject &json, QString *error);
    QJsonObject toJson() const;

    static std::optional<Project> load(const QString &filePath, QString *error);
    bool save(const QString &filePath, QString *error) const;
};

// A slide with every value filled in and its path made absolute.
struct ResolvedSlide {
    QString path;
    double duration = 0;
    QString transition; // canonical xfade name, or "none" for a straight cut
    double transitionDuration = 0; // always 0 when transition is "none"
};

QList<ResolvedSlide> resolveSlides(const Project &project);
