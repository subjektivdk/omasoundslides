#include "projectmodel.h"
#include "probe.h"
#include "transitions.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QUndoCommand>
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

// Computed durations (fitting to audio or markers) keep milliseconds, so
// rounding errors don't add up over many images.
double roundMillis(double s)
{
    return std::round(std::max(0.0, s) * 1000) / 1000;
}

// Two markers closer than this are the same cue.
constexpr double MarkerMinGap = 0.05;
// Edits with the same merge key closer together than this are one undo step:
// every notch of a mouse wheel, every pixel of a drag.
constexpr qint64 MergeWindowMs = 1200;

// One undoable edit: the project before and after it. The edit has already
// been applied when the command is pushed, so its first redo() does nothing.
class EditCommand : public QUndoCommand
{
public:
    EditCommand(ProjectModel *model, ProjectModel::Snapshot before, ProjectModel::Snapshot after,
                const QString &text, const QString &mergeKey)
        : QUndoCommand(text)
        , m_model(model)
        , m_before(std::move(before))
        , m_after(std::move(after))
        , m_mergeKey(mergeKey)
    {
        m_lastEdit.start();
    }

    int id() const override
    {
        return m_mergeKey.isEmpty() ? -1 : int(qHash(m_mergeKey) & 0x7fffffff);
    }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *next = static_cast<const EditCommand *>(other);
        if (next->m_mergeKey != m_mergeKey || m_lastEdit.elapsed() > MergeWindowMs)
            return false;
        m_after = next->m_after;
        m_lastEdit.restart();
        return true;
    }

    void undo() override { m_model->restore(m_before); }

    void redo() override
    {
        if (m_firstRedo) {
            m_firstRedo = false;
            return;
        }
        m_model->restore(m_after);
    }

private:
    ProjectModel *m_model;
    ProjectModel::Snapshot m_before;
    ProjectModel::Snapshot m_after;
    QString m_mergeKey;
    QElapsedTimer m_lastEdit;
    bool m_firstRedo = true;
};

}

ProjectModel::ProjectModel(QObject *parent)
    : QAbstractListModel(parent)
{
    recompute();
    connect(&m_undo, &QUndoStack::cleanChanged, this, &ProjectModel::modifiedChanged);
    connect(&m_undo, &QUndoStack::indexChanged, this, &ProjectModel::undoStateChanged);
    connect(&m_undo, &QUndoStack::canUndoChanged, this, &ProjectModel::undoStateChanged);
    connect(&m_undo, &QUndoStack::canRedoChanged, this, &ProjectModel::undoStateChanged);
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
    Snapshot fresh{project.withAbsolutePaths(), {}};
    for (const QString &path : fresh.project.audio)
        fresh.audioDurations.append(Probe::inspect(path).duration);
    restore(fresh);
    m_undo.clear();
    Q_EMIT modifiedChanged();
    Q_EMIT undoStateChanged();
}

void ProjectModel::restore(const Snapshot &snapshot)
{
    const bool sameRows = snapshot.project.slides.size() == m_project.slides.size();
    if (!sameRows)
        beginResetModel();
    m_project = snapshot.project;
    m_audioDurations = snapshot.audioDurations;
    recompute();
    if (!sameRows) {
        endResetModel();
        Q_EMIT countChanged();
    } else if (!m_project.slides.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(int(m_project.slides.size()) - 1));
    }
    Q_EMIT nameChanged();
    Q_EMIT defaultsChanged();
    Q_EMIT outputChanged();
    Q_EMIT audioChanged();
    Q_EMIT audioFadeChanged();
    Q_EMIT markersChanged();
    Q_EMIT timingChanged();
}

void ProjectModel::record(const Snapshot &before, const QString &text, const QString &mergeKey)
{
    m_undo.push(new EditCommand(this, before, snapshot(), text, mergeKey));
}

void ProjectModel::setModified(bool modified)
{
    if (!modified)
        m_undo.setClean();
}

void ProjectModel::setName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed == m_project.name)
        return;
    const Snapshot before = snapshot();
    m_project.name = trimmed;
    Q_EMIT nameChanged();
    record(before, QStringLiteral("Rename project"));
}

void ProjectModel::setDefaultDuration(double seconds)
{
    seconds = roundSeconds(seconds);
    if (seconds <= 0 || seconds == m_project.defaults.duration)
        return;
    const Snapshot before = snapshot();
    m_project.defaults.duration = seconds;
    Q_EMIT defaultsChanged();
    edited();
    record(before, QStringLiteral("Default duration"), QStringLiteral("default-duration"));
}

void ProjectModel::setDefaultTransition(const QString &name)
{
    const QString canonical = Transitions::canonical(name);
    if (canonical.isEmpty() || canonical == m_project.defaults.transition)
        return;
    const Snapshot before = snapshot();
    m_project.defaults.transition = canonical;
    Q_EMIT defaultsChanged();
    edited();
    record(before, QStringLiteral("Default transition"));
}

void ProjectModel::setDefaultTransitionDuration(double seconds)
{
    seconds = roundSeconds(seconds);
    if (seconds == m_project.defaults.transitionDuration)
        return;
    const Snapshot before = snapshot();
    m_project.defaults.transitionDuration = seconds;
    Q_EMIT defaultsChanged();
    edited();
    record(before, QStringLiteral("Default transition length"), QStringLiteral("default-transition-length"));
}

void ProjectModel::setResolution(int width, int height)
{
    if (width <= 0 || height <= 0 || width % 2 || height % 2)
        return;
    if (width == m_project.output.width && height == m_project.output.height)
        return;
    const Snapshot before = snapshot();
    m_project.output.width = width;
    m_project.output.height = height;
    Q_EMIT outputChanged();
    record(before, QStringLiteral("Resolution"));
}

void ProjectModel::setFps(int fps)
{
    if (fps <= 0 || fps == m_project.output.fps)
        return;
    const Snapshot before = snapshot();
    m_project.output.fps = fps;
    Q_EMIT outputChanged();
    record(before, QStringLiteral("Frame rate"));
}

void ProjectModel::setExportQuality(const QString &name)
{
    const auto quality = exportQualityFromName(name);
    if (!quality || *quality == m_project.output.quality)
        return;
    const Snapshot before = snapshot();
    m_project.output.quality = *quality;
    Q_EMIT outputChanged();
    record(before, QStringLiteral("Export quality"));
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
    const Snapshot before = snapshot();
    m_project.audioFadeIn = seconds;
    Q_EMIT audioFadeChanged();
    record(before, QStringLiteral("Audio fade in"), QStringLiteral("fade-in"));
}

void ProjectModel::setAudioFadeOut(double seconds)
{
    seconds = roundSeconds(seconds);
    if (seconds == m_project.audioFadeOut)
        return;
    const Snapshot before = snapshot();
    m_project.audioFadeOut = seconds;
    Q_EMIT audioFadeChanged();
    record(before, QStringLiteral("Audio fade out"), QStringLiteral("fade-out"));
}

double ProjectModel::audioEnd() const
{
    const double audio = audioDuration();
    return audio > 0 && m_plan.total > 0 ? std::min(audio, m_plan.total) : audio;
}

double ProjectModel::audioGainAt(double seconds) const
{
    const double end = audioEnd();
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

void ProjectModel::addImages(const QStringList &paths, int index)
{
    if (paths.isEmpty())
        return;
    if (index < 0 || index > m_project.slides.size())
        index = int(m_project.slides.size());

    const Snapshot before = snapshot();
    beginInsertRows({}, index, index + int(paths.size()) - 1);
    for (qsizetype i = 0; i < paths.size(); ++i)
        m_project.slides.insert(index + i, Slide{QFileInfo(paths.at(i)).absoluteFilePath(), {}, {}, {}});
    recompute();
    endInsertRows();
    Q_EMIT countChanged();
    notify();
    record(before, paths.size() == 1 ? QStringLiteral("Add image") : QStringLiteral("Add images"));
}

void ProjectModel::removeImage(int index)
{
    if (!validIndex(index))
        return;
    const Snapshot before = snapshot();
    beginRemoveRows({}, index, index);
    m_project.slides.removeAt(index);
    recompute();
    endRemoveRows();
    Q_EMIT countChanged();
    notify();
    record(before, QStringLiteral("Remove image"));
}

void ProjectModel::moveImage(int from, int to)
{
    if (!validIndex(from) || !validIndex(to) || from == to)
        return;
    const Snapshot before = snapshot();
    // Qt's beginMoveRows wants the destination as "insert before this row".
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_project.slides.move(from, to);
    recompute();
    endMoveRows();
    notify();
    record(before, QStringLiteral("Move image"));
}

void ProjectModel::setDuration(int index, double seconds)
{
    seconds = roundSeconds(seconds);
    if (!validIndex(index) || seconds <= 0 || m_project.slides.at(index).duration == seconds)
        return;
    const Snapshot before = snapshot();
    m_project.slides[index].duration = seconds;
    edited();
    record(before, QStringLiteral("Image duration"), QStringLiteral("duration:%1").arg(index));
}

void ProjectModel::resetDuration(int index)
{
    if (!validIndex(index) || !m_project.slides.at(index).duration)
        return;
    const Snapshot before = snapshot();
    m_project.slides[index].duration.reset();
    edited();
    record(before, QStringLiteral("Reset image duration"));
}

void ProjectModel::setTransition(int index, const QString &name)
{
    const QString canonical = Transitions::canonical(name);
    if (!validIndex(index) || canonical.isEmpty())
        return;
    const Snapshot before = snapshot();
    m_project.slides[index].transition = canonical;
    edited();
    record(before, QStringLiteral("Transition"));
}

void ProjectModel::resetTransition(int index)
{
    if (!validIndex(index))
        return;
    const Snapshot before = snapshot();
    m_project.slides[index].transition.reset();
    edited();
    record(before, QStringLiteral("Reset transition"));
}

void ProjectModel::setTransitionDuration(int index, double seconds)
{
    if (!validIndex(index))
        return;
    const Snapshot before = snapshot();
    m_project.slides[index].transitionDuration = roundSeconds(seconds);
    edited();
    record(before, QStringLiteral("Transition length"), QStringLiteral("transition-length:%1").arg(index));
}

void ProjectModel::resetTransitionDuration(int index)
{
    if (!validIndex(index))
        return;
    const Snapshot before = snapshot();
    m_project.slides[index].transitionDuration.reset();
    edited();
    record(before, QStringLiteral("Reset transition length"));
}

void ProjectModel::setTransitionPreset(int index, const QString &transition, double seconds)
{
    const QString canonical = Transitions::canonical(transition);
    if (!validIndex(index) || canonical.isEmpty())
        return;
    const Snapshot before = snapshot();
    m_project.slides[index].transition = canonical;
    m_project.slides[index].transitionDuration = roundSeconds(seconds);
    edited();
    record(before, QStringLiteral("Transition"));
}

void ProjectModel::resetTransitionPreset(int index)
{
    if (!validIndex(index))
        return;
    const Snapshot before = snapshot();
    m_project.slides[index].transition.reset();
    m_project.slides[index].transitionDuration.reset();
    edited();
    record(before, QStringLiteral("Reset transition"));
}

void ProjectModel::setDefaultTransitionPreset(const QString &transition, double seconds)
{
    const QString canonical = Transitions::canonical(transition);
    if (canonical.isEmpty())
        return;
    const Snapshot before = snapshot();
    m_project.defaults.transition = canonical;
    // A cut has no length. Keep the default length, since images that set
    // only their own transition type borrow it; 0 would turn them into cuts.
    if (!Transitions::isCut(canonical))
        m_project.defaults.transitionDuration = roundSeconds(seconds);
    Q_EMIT defaultsChanged();
    edited();
    record(before, QStringLiteral("Default transition"));
}

void ProjectModel::addAudio(const QStringList &paths)
{
    if (paths.isEmpty())
        return;
    const Snapshot before = snapshot();
    for (const QString &path : paths) {
        const QString absolute = QFileInfo(path).absoluteFilePath();
        m_project.audio.append(absolute);
        m_audioDurations.append(Probe::inspect(absolute).duration);
    }
    Q_EMIT audioChanged();
    edited();
    record(before, QStringLiteral("Add audio"));
}

void ProjectModel::removeAudio(int index)
{
    if (index < 0 || index >= m_project.audio.size())
        return;
    const Snapshot before = snapshot();
    m_project.audio.removeAt(index);
    m_audioDurations.removeAt(index);
    Q_EMIT audioChanged();
    edited();
    record(before, QStringLiteral("Remove audio"));
}

void ProjectModel::moveAudio(int from, int to)
{
    const int n = int(m_project.audio.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;
    const Snapshot before = snapshot();
    m_project.audio.move(from, to);
    m_audioDurations.move(from, to);
    Q_EMIT audioChanged();
    edited();
    record(before, QStringLiteral("Reorder audio"));
}

QVariantList ProjectModel::markers() const
{
    QVariantList list;
    for (double m : m_project.markers)
        list.append(m);
    return list;
}

int ProjectModel::addMarker(double seconds)
{
    seconds = roundMillis(seconds);
    for (double m : m_project.markers)
        if (std::abs(m - seconds) < MarkerMinGap)
            return -1;
    const Snapshot before = snapshot();
    m_project.markers.append(seconds);
    Q_EMIT markersChanged();
    record(before, QStringLiteral("Add marker"));
    return int(m_project.markers.size()) - 1;
}

void ProjectModel::removeMarker(int index)
{
    if (!validMarker(index))
        return;
    const Snapshot before = snapshot();
    m_project.markers.removeAt(index);
    Q_EMIT markersChanged();
    record(before, QStringLiteral("Remove marker"));
}

bool ProjectModel::removeMarkerNear(double seconds, double tolerance)
{
    int nearest = -1;
    for (int i = 0; i < m_project.markers.size(); ++i)
        if (nearest < 0 || std::abs(m_project.markers.at(i) - seconds) < std::abs(m_project.markers.at(nearest) - seconds))
            nearest = i;
    if (nearest < 0 || std::abs(m_project.markers.at(nearest) - seconds) > tolerance)
        return false;
    removeMarker(nearest);
    return true;
}

void ProjectModel::moveMarker(int index, double seconds)
{
    seconds = roundMillis(seconds);
    if (!validMarker(index) || m_project.markers.at(index) == seconds)
        return;
    const Snapshot before = snapshot();
    m_project.markers[index] = seconds;
    Q_EMIT markersChanged();
    record(before, QStringLiteral("Move marker"), QStringLiteral("marker:%1").arg(index));
}

void ProjectModel::clearMarkers()
{
    if (m_project.markers.isEmpty())
        return;
    const Snapshot before = snapshot();
    m_project.markers.clear();
    Q_EMIT markersChanged();
    record(before, QStringLiteral("Clear markers"));
}

int ProjectModel::replaceMarkers(QList<double> seconds, const QString &undoText)
{
    std::sort(seconds.begin(), seconds.end());
    QList<double> markers;
    for (double s : seconds) {
        s = roundMillis(s);
        if (markers.isEmpty() || s - markers.last() >= MarkerMinGap)
            markers.append(s);
    }
    if (markers == m_project.markers)
        return int(markers.size());
    const Snapshot before = snapshot();
    m_project.markers = markers;
    Q_EMIT markersChanged();
    record(before, undoText);
    return int(markers.size());
}

double ProjectModel::markerAfter(double seconds) const
{
    double best = -1;
    for (double m : m_project.markers)
        if (m > seconds + 0.001 && (best < 0 || m < best))
            best = m;
    return best;
}

double ProjectModel::markerBefore(double seconds) const
{
    double best = -1;
    for (double m : m_project.markers)
        if (m < seconds - 0.001 && m > best)
            best = m;
    return best;
}

bool ProjectModel::fitToAudio()
{
    const double audio = audioDuration();
    if (m_project.slides.isEmpty() || audio <= 0)
        return false;

    const Snapshot before = snapshot();
    for (Slide &s : m_project.slides)
        s.duration.reset();
    // Round down to whole milliseconds so the pictures never outlast the
    // audio; across many images hundredths would add up to a visible gap.
    const double d = std::floor(Timeline::autoDuration(resolveSlides(m_project), audio) * 1000) / 1000;
    m_project.defaults.duration = d;
    Q_EMIT defaultsChanged();
    edited();
    record(before, QStringLiteral("Fit images to the audio"));
    return true;
}

int ProjectModel::fitToMarkers()
{
    const int n = int(m_project.slides.size());
    QList<double> cues = m_project.markers;
    std::sort(cues.begin(), cues.end());
    const int used = std::min(int(cues.size()), n - 1);
    if (used <= 0)
        return 0;

    // The transitions stay as they are; only the durations move. Image k+1's
    // transition is centred on marker k, so the change happens on the cue.
    const QList<ResolvedSlide> resolved = resolveSlides(m_project);
    QList<double> starts{0};
    for (int k = 1; k <= used; ++k)
        starts.append(cues.at(k - 1) - resolved.at(k).transitionDuration / 2);

    const Snapshot before = snapshot();
    for (int k = 0; k < used; ++k)
        m_project.slides[k].duration = roundMillis(starts.at(k + 1) - starts.at(k) + resolved.at(k + 1).transitionDuration);
    // With a marker for every change, the last image runs to the end of the audio.
    const double audio = audioDuration();
    if (used == n - 1 && audio > starts.last())
        m_project.slides[n - 1].duration = roundMillis(audio - starts.last());
    edited();
    record(before, QStringLiteral("Fit images to markers"));
    return used;
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
}
