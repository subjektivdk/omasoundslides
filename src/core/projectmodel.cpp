#include "projectmodel.h"
#include "probe.h"
#include "transitions.h"

#include <QFileInfo>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace {
// Seconds are edited with two decimals in the GUI; store them the same way so
// 0.1 + 0.2 style noise never shows up in the project file.
double roundSeconds(double s)
{
    return std::round(std::max(0.0, s) * 100) / 100;
}
}

ProjectModel::ProjectModel(QObject *parent)
    : QAbstractListModel(parent)
{
    recompute();
}

int ProjectModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_project.slides.size());
}

QVariant ProjectModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !validIndex(index.row()))
        return {};
    const int row = index.row();
    const Slide &s = m_project.slides.at(row);
    const ResolvedSlide &r = m_resolved.at(row);

    switch (role) {
    case PathRole: return s.path;
    case UrlRole: return QUrl::fromLocalFile(s.path);
    case FileNameRole: return QFileInfo(s.path).fileName();
    case DurationRole: return r.duration;
    case DurationSetRole: return s.duration.has_value();
    // The effective transition as the user sees it: "none" on the first image.
    case TransitionRole: return r.transition;
    case TransitionSetRole: return s.transition.has_value();
    case TransitionDurationRole:
        return s.transitionDuration.value_or(m_project.defaults.transitionDuration);
    case TransitionDurationSetRole: return s.transitionDuration.has_value();
    case StartRole: return m_plan.starts.value(row);
    case IsFirstRole: return row == 0;
    }
    return {};
}

QHash<int, QByteArray> ProjectModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {UrlRole, "url"},
        {FileNameRole, "fileName"},
        {DurationRole, "duration"},
        {DurationSetRole, "durationSet"},
        {TransitionRole, "transition"},
        {TransitionSetRole, "transitionSet"},
        {TransitionDurationRole, "transitionDuration"},
        {TransitionDurationSetRole, "transitionDurationSet"},
        {StartRole, "start"},
        {IsFirstRole, "isFirst"},
    };
}

void ProjectModel::setProject(const Project &project)
{
    beginResetModel();
    m_project = project.withAbsolutePaths();
    m_audioDurations.clear();
    for (const QString &path : m_project.audio)
        m_audioDurations.append(Probe::inspect(path).duration);
    recompute();
    endResetModel();
    Q_EMIT timingChanged();

    Q_EMIT countChanged();
    Q_EMIT nameChanged();
    Q_EMIT defaultsChanged();
    Q_EMIT outputChanged();
    Q_EMIT audioChanged();
    Q_EMIT audioFadeChanged();
    setModified(false);
}

void ProjectModel::setName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed == m_project.name)
        return;
    m_project.name = trimmed;
    Q_EMIT nameChanged();
    setModified(true);
}

void ProjectModel::setDefaultDuration(double seconds)
{
    seconds = roundSeconds(seconds);
    if (seconds <= 0 || seconds == m_project.defaults.duration)
        return;
    m_project.defaults.duration = seconds;
    Q_EMIT defaultsChanged();
    edited();
}

void ProjectModel::setDefaultTransition(const QString &name)
{
    const QString canonical = Transitions::canonical(name);
    if (canonical.isEmpty() || canonical == m_project.defaults.transition)
        return;
    m_project.defaults.transition = canonical;
    Q_EMIT defaultsChanged();
    edited();
}

void ProjectModel::setDefaultTransitionDuration(double seconds)
{
    seconds = roundSeconds(seconds);
    if (seconds == m_project.defaults.transitionDuration)
        return;
    m_project.defaults.transitionDuration = seconds;
    Q_EMIT defaultsChanged();
    edited();
}

void ProjectModel::setResolution(int width, int height)
{
    if (width <= 0 || height <= 0 || width % 2 || height % 2)
        return;
    if (width == m_project.output.width && height == m_project.output.height)
        return;
    m_project.output.width = width;
    m_project.output.height = height;
    Q_EMIT outputChanged();
    setModified(true);
}

void ProjectModel::setFps(int fps)
{
    if (fps <= 0 || fps == m_project.output.fps)
        return;
    m_project.output.fps = fps;
    Q_EMIT outputChanged();
    setModified(true);
}

QVariantList ProjectModel::audio() const
{
    QVariantList list;
    for (qsizetype i = 0; i < m_project.audio.size(); ++i)
        list.append(QVariantMap{
            {QStringLiteral("path"), m_project.audio.at(i)},
            {QStringLiteral("fileName"), QFileInfo(m_project.audio.at(i)).fileName()},
            {QStringLiteral("duration"), m_audioDurations.value(i)},
        });
    return list;
}

void ProjectModel::setAudioFadeIn(double seconds)
{
    seconds = roundSeconds(seconds);
    if (seconds == m_project.audioFadeIn)
        return;
    m_project.audioFadeIn = seconds;
    Q_EMIT audioFadeChanged();
    setModified(true);
}

void ProjectModel::setAudioFadeOut(double seconds)
{
    seconds = roundSeconds(seconds);
    if (seconds == m_project.audioFadeOut)
        return;
    m_project.audioFadeOut = seconds;
    Q_EMIT audioFadeChanged();
    setModified(true);
}

double ProjectModel::audioGainAt(double seconds) const
{
    const double audio = audioDuration();
    const double end = audio > 0 && m_plan.total > 0 ? std::min(audio, m_plan.total) : audio;
    double in = m_project.audioFadeIn;
    double out = m_project.audioFadeOut;
    if (in + out > end && in + out > 0) {
        const double scale = end / (in + out);
        in *= scale;
        out *= scale;
    }
    double gain = 1;
    if (in > 0 && seconds < in)
        gain = std::min(gain, std::max(0.0, seconds / in));
    if (out > 0 && seconds > end - out)
        gain = std::min(gain, std::max(0.0, (end - seconds) / out));
    return gain;
}

double ProjectModel::audioDuration() const
{
    double total = 0;
    for (double d : m_audioDurations)
        total += d;
    return total;
}

QVariantList ProjectModel::transitionPresets() const
{
    QVariantList list;
    for (const Transitions::Preset &p : Transitions::presets())
        list.append(QVariantMap{
            {QStringLiteral("id"), p.id},
            {QStringLiteral("label"), p.label},
            {QStringLiteral("transition"), p.transition},
            {QStringLiteral("duration"), p.duration},
        });
    return list;
}

void ProjectModel::setModified(bool modified)
{
    if (m_modified == modified)
        return;
    m_modified = modified;
    Q_EMIT modifiedChanged();
}

void ProjectModel::addImages(const QStringList &paths, int index)
{
    if (paths.isEmpty())
        return;
    if (index < 0 || index > m_project.slides.size())
        index = int(m_project.slides.size());

    beginInsertRows({}, index, index + int(paths.size()) - 1);
    for (qsizetype i = 0; i < paths.size(); ++i)
        m_project.slides.insert(index + i, Slide{QFileInfo(paths.at(i)).absoluteFilePath(), {}, {}, {}});
    recompute();
    endInsertRows();
    Q_EMIT countChanged();
    notify();
}

void ProjectModel::removeImage(int index)
{
    if (!validIndex(index))
        return;
    beginRemoveRows({}, index, index);
    m_project.slides.removeAt(index);
    recompute();
    endRemoveRows();
    Q_EMIT countChanged();
    notify();
}

void ProjectModel::moveImage(int from, int to)
{
    if (!validIndex(from) || !validIndex(to) || from == to)
        return;
    // Qt's beginMoveRows wants the destination as "insert before this row".
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_project.slides.move(from, to);
    recompute();
    endMoveRows();
    notify();
}

void ProjectModel::setDuration(int index, double seconds)
{
    seconds = roundSeconds(seconds);
    if (!validIndex(index) || seconds <= 0)
        return;
    m_project.slides[index].duration = seconds;
    edited();
}

void ProjectModel::resetDuration(int index)
{
    if (!validIndex(index))
        return;
    m_project.slides[index].duration.reset();
    edited();
}

void ProjectModel::setTransition(int index, const QString &name)
{
    const QString canonical = Transitions::canonical(name);
    if (!validIndex(index) || canonical.isEmpty())
        return;
    m_project.slides[index].transition = canonical;
    edited();
}

void ProjectModel::resetTransition(int index)
{
    if (!validIndex(index))
        return;
    m_project.slides[index].transition.reset();
    edited();
}

void ProjectModel::setTransitionDuration(int index, double seconds)
{
    if (!validIndex(index))
        return;
    m_project.slides[index].transitionDuration = roundSeconds(seconds);
    edited();
}

void ProjectModel::resetTransitionDuration(int index)
{
    if (!validIndex(index))
        return;
    m_project.slides[index].transitionDuration.reset();
    edited();
}

void ProjectModel::setTransitionPreset(int index, const QString &transition, double seconds)
{
    const QString canonical = Transitions::canonical(transition);
    if (!validIndex(index) || canonical.isEmpty())
        return;
    m_project.slides[index].transition = canonical;
    m_project.slides[index].transitionDuration = roundSeconds(seconds);
    edited();
}

void ProjectModel::resetTransitionPreset(int index)
{
    if (!validIndex(index))
        return;
    m_project.slides[index].transition.reset();
    m_project.slides[index].transitionDuration.reset();
    edited();
}

void ProjectModel::setDefaultTransitionPreset(const QString &transition, double seconds)
{
    const QString canonical = Transitions::canonical(transition);
    if (canonical.isEmpty())
        return;
    m_project.defaults.transition = canonical;
    // A cut has no length. Keep the default length, since images that set
    // only their own transition type borrow it; 0 would turn them into cuts.
    if (!Transitions::isCut(canonical))
        m_project.defaults.transitionDuration = roundSeconds(seconds);
    Q_EMIT defaultsChanged();
    edited();
}

void ProjectModel::addAudio(const QStringList &paths)
{
    if (paths.isEmpty())
        return;
    for (const QString &path : paths) {
        const QString absolute = QFileInfo(path).absoluteFilePath();
        m_project.audio.append(absolute);
        m_audioDurations.append(Probe::inspect(absolute).duration);
    }
    Q_EMIT audioChanged();
    edited();
}

void ProjectModel::removeAudio(int index)
{
    if (index < 0 || index >= m_project.audio.size())
        return;
    m_project.audio.removeAt(index);
    m_audioDurations.removeAt(index);
    Q_EMIT audioChanged();
    edited();
}

bool ProjectModel::fitToAudio()
{
    const double audio = audioDuration();
    if (m_project.slides.isEmpty() || audio <= 0)
        return false;

    for (Slide &s : m_project.slides)
        s.duration.reset();
    // Round down to whole milliseconds so the pictures never outlast the
    // audio; across many images hundredths would add up to a visible gap.
    const double d = std::floor(Timeline::autoDuration(resolveSlides(m_project), audio) * 1000) / 1000;
    m_project.defaults.duration = d;
    Q_EMIT defaultsChanged();
    edited();
    return true;
}

int ProjectModel::durationOverrideCount() const
{
    int n = 0;
    for (const Slide &s : m_project.slides)
        n += s.duration.has_value();
    return n;
}

double ProjectModel::startOf(int index) const
{
    return m_plan.starts.value(index);
}

double ProjectModel::durationOf(int index) const
{
    return validIndex(index) ? m_resolved.at(index).duration : 0;
}

double ProjectModel::visibleStartOf(int index) const
{
    return validIndex(index) ? m_plan.starts.at(index) + m_resolved.at(index).transitionDuration : 0;
}

QVariantMap ProjectModel::frameAt(double seconds) const
{
    QVariantMap frame{
        {QStringLiteral("from"), -1},
        {QStringLiteral("to"), -1},
        {QStringLiteral("mix"), 0.0},
        {QStringLiteral("transition"), QStringLiteral("none")},
        {QStringLiteral("current"), -1},
    };
    const int n = int(m_resolved.size());
    if (n == 0)
        return frame;

    // The last image whose clip has started. Starts only ever increase.
    int i = 0;
    while (i + 1 < n && m_plan.starts.at(i + 1) <= seconds)
        ++i;

    // Inside the transition into image i: it overlaps the end of i - 1.
    const ResolvedSlide &s = m_resolved.at(i);
    const double into = seconds - m_plan.starts.at(i);
    if (i > 0 && s.transitionDuration > 0 && into < s.transitionDuration) {
        const double mix = std::clamp(into / s.transitionDuration, 0.0, 1.0);
        frame[QStringLiteral("from")] = i - 1;
        frame[QStringLiteral("to")] = i;
        frame[QStringLiteral("mix")] = mix;
        frame[QStringLiteral("transition")] = s.transition;
        frame[QStringLiteral("current")] = mix < 0.5 ? i - 1 : i;
        return frame;
    }
    frame[QStringLiteral("from")] = i;
    frame[QStringLiteral("current")] = i;
    return frame;
}

QUrl ProjectModel::urlOf(int index) const
{
    return validIndex(index) ? QUrl::fromLocalFile(m_project.slides.at(index).path) : QUrl();
}

void ProjectModel::recompute()
{
    m_resolved = resolveSlides(m_project);
    m_plan = Timeline::plan(m_resolved);
    m_problems = m_resolved.isEmpty() ? QStringList() : Timeline::validate(m_resolved);
}

void ProjectModel::edited()
{
    recompute();
    notify();
}

void ProjectModel::notify()
{
    if (!m_project.slides.isEmpty())
        Q_EMIT dataChanged(index(0), index(int(m_project.slides.size()) - 1));
    Q_EMIT timingChanged();
    setModified(true);
}
