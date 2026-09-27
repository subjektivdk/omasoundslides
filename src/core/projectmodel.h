#pragma once

#include "project.h"
#include "timeline.h"

#include <QAbstractListModel>
#include <QStringList>
#include <QUndoStack>
#include <QUrl>
#include <QVariantList>

// The project as the GUI edits it: one row per image, plus the project-wide
// settings, the audio list and the markers as properties. Paths are always
// absolute here. Every edit recomputes the timeline, so start times and the
// total length shown in the GUI are exactly what the renderer will produce,
// and every edit can be undone.
class ProjectModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(double defaultDuration READ defaultDuration WRITE setDefaultDuration NOTIFY defaultsChanged)
    Q_PROPERTY(QString defaultTransition READ defaultTransition WRITE setDefaultTransition NOTIFY defaultsChanged)
    Q_PROPERTY(double defaultTransitionDuration READ defaultTransitionDuration WRITE setDefaultTransitionDuration NOTIFY defaultsChanged)
    Q_PROPERTY(int outputWidth READ outputWidth NOTIFY outputChanged)
    Q_PROPERTY(int outputHeight READ outputHeight NOTIFY outputChanged)
    Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY outputChanged)
    Q_PROPERTY(QVariantList audio READ audio NOTIFY audioChanged)
    Q_PROPERTY(double audioFadeIn READ audioFadeIn WRITE setAudioFadeIn NOTIFY audioFadeChanged)
    Q_PROPERTY(double audioFadeOut READ audioFadeOut WRITE setAudioFadeOut NOTIFY audioFadeChanged)
    Q_PROPERTY(double audioDuration READ audioDuration NOTIFY audioChanged)
    Q_PROPERTY(double audioEnd READ audioEnd NOTIFY timingChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY markersChanged)
    Q_PROPERTY(double videoDuration READ videoDuration NOTIFY timingChanged)
    Q_PROPERTY(QStringList problems READ problems NOTIFY timingChanged)
    Q_PROPERTY(bool modified READ isModified NOTIFY modifiedChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoStateChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoStateChanged)
    // [{ id, label, transition, duration }]: Soundslides' seven choices
    Q_PROPERTY(QVariantList transitionPresets READ transitionPresets CONSTANT)

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        UrlRole,
        FileNameRole,
        DurationRole,
        DurationSetRole,
        TransitionRole,
        TransitionSetRole,
        TransitionDurationRole,
        TransitionDurationSetRole,
        StartRole,
        IsFirstRole,
    };

    // Everything an edit can change, as undo and redo restore it.
    struct Snapshot {
        Project project;
        QList<double> audioDurations;
    };

    explicit ProjectModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replaces everything and forgets the undo history; audio lengths are
    // probed with ffprobe.
    void setProject(const Project &project);
    Project project() const { return m_project; }

    QString name() const { return m_project.name; }
    void setName(const QString &name);

    QStringList audioPaths() const { return m_project.audio; }

    double defaultDuration() const { return m_project.defaults.duration; }
    QString defaultTransition() const { return m_project.defaults.transition; }
    double defaultTransitionDuration() const { return m_project.defaults.transitionDuration; }
    void setDefaultDuration(double seconds);
    void setDefaultTransition(const QString &name);
    void setDefaultTransitionDuration(double seconds);

    int outputWidth() const { return m_project.output.width; }
    int outputHeight() const { return m_project.output.height; }
    int fps() const { return m_project.output.fps; }
    Q_INVOKABLE void setResolution(int width, int height);
    void setFps(int fps);

    QVariantList audio() const;
    double audioFadeIn() const { return m_project.audioFadeIn; }
    double audioFadeOut() const { return m_project.audioFadeOut; }
    void setAudioFadeIn(double seconds);
    void setAudioFadeOut(double seconds);
    // Where the audio stops being heard in the video: its own end, or the
    // end of the pictures when that comes first. The fade-out ends here.
    double audioEnd() const;
    // Playback volume at `seconds` (0..1) with the fades applied, the same
    // way the export applies them.
    Q_INVOKABLE double audioGainAt(double seconds) const;
    double audioDuration() const;
    double videoDuration() const { return m_plan.total; }
    QStringList problems() const { return m_problems; }
    QVariantList transitionPresets() const;

    // Modified = the undo history is not where it was last saved.
    bool isModified() const { return !m_undo.isClean(); }
    // false marks the current state as saved; true is ignored.
    void setModified(bool modified);

    bool canUndo() const { return m_undo.canUndo(); }
    bool canRedo() const { return m_undo.canRedo(); }
    QString undoText() const { return m_undo.undoText(); }
    QString redoText() const { return m_undo.redoText(); }
    Q_INVOKABLE void undo() { m_undo.undo(); }
    Q_INVOKABLE void redo() { m_undo.redo(); }

    // index < 0 appends.
    Q_INVOKABLE void addImages(const QStringList &paths, int index = -1);
    Q_INVOKABLE void removeImage(int index);
    Q_INVOKABLE void moveImage(int from, int to);

    Q_INVOKABLE void setDuration(int index, double seconds);
    Q_INVOKABLE void resetDuration(int index);
    Q_INVOKABLE void setTransition(int index, const QString &name);
    Q_INVOKABLE void resetTransition(int index);
    Q_INVOKABLE void setTransitionDuration(int index, double seconds);
    Q_INVOKABLE void resetTransitionDuration(int index);
    // A preset sets the transition and its length together, as one edit.
    Q_INVOKABLE void setTransitionPreset(int index, const QString &transition, double seconds);
    Q_INVOKABLE void resetTransitionPreset(int index);
    Q_INVOKABLE void setDefaultTransitionPreset(const QString &transition, double seconds);

    Q_INVOKABLE void addAudio(const QStringList &paths);
    Q_INVOKABLE void removeAudio(int index);

    // Markers, in the order they were set (not necessarily by time).
    QVariantList markers() const;
    // Returns the new marker's index, or -1 when one is already that close.
    Q_INVOKABLE int addMarker(double seconds);
    Q_INVOKABLE void removeMarker(int index);
    // Removes the marker nearest to `seconds` if it is within `tolerance`.
    Q_INVOKABLE bool removeMarkerNear(double seconds, double tolerance);
    Q_INVOKABLE void moveMarker(int index, double seconds);
    Q_INVOKABLE void clearMarkers();
    // Replaces all markers in one undoable step (sorted, near-duplicates
    // merged). Returns how many markers there are afterwards.
    int replaceMarkers(QList<double> seconds, const QString &undoText);
    // The first marker after / last marker before `seconds`, or -1.
    Q_INVOKABLE double markerAfter(double seconds) const;
    Q_INVOKABLE double markerBefore(double seconds) const;

    // Soundslides' auto-spacing: one duration for all images so the video
    // ends with the audio. Clears per-image durations. False without audio.
    Q_INVOKABLE bool fitToAudio();
    // Soundslides' precise timing: the change to image k+1 happens at marker
    // k (the middle of its transition, or the cut). With a marker for every
    // change, the last image lasts until the audio ends. Returns how many
    // markers were used; 0 when there was nothing to fit.
    Q_INVOKABLE int fitToMarkers();
    Q_INVOKABLE int durationOverrideCount() const;

    // Resolved values, handy for the inspector.
    Q_INVOKABLE double startOf(int index) const;
    Q_INVOKABLE double durationOf(int index) const;
    // When the image is fully on screen: after its incoming transition.
    Q_INVOKABLE double visibleStartOf(int index) const;

    // What the video shows at `seconds`, for the preview during playback:
    // { from, to, mix, transition, current }. `to` is -1 outside a
    // transition; `mix` runs 0 → 1 through it; `current` is the image that
    // dominates the frame (the one the timeline should highlight).
    Q_INVOKABLE QVariantMap frameAt(double seconds) const;
    Q_INVOKABLE QUrl urlOf(int index) const;

    // Used by the undo commands.
    Snapshot snapshot() const { return {m_project, m_audioDurations}; }
    void restore(const Snapshot &snapshot);

Q_SIGNALS:
    void countChanged();
    void nameChanged();
    void defaultsChanged();
    void outputChanged();
    void audioChanged();
    void audioFadeChanged();
    void markersChanged();
    void timingChanged();
    void modifiedChanged();
    void undoStateChanged();

private:
    bool validIndex(int index) const { return index >= 0 && index < m_project.slides.size(); }
    bool validMarker(int index) const { return index >= 0 && index < m_project.markers.size(); }
    // Recomputes the resolved slides, the plan and the problems. Safe to call
    // between begin*Rows and end*Rows since it emits nothing.
    void recompute();
    // Refreshes every row (one change can move the start time of everything
    // after it) and announces the new timing.
    void notify();
    void edited(); // recompute() + notify()
    // Puts the change from `before` to now on the undo stack. Changes with
    // the same non-empty mergeKey in quick succession become one undo step.
    void record(const Snapshot &before, const QString &text, const QString &mergeKey = {});

    Project m_project;
    QList<double> m_audioDurations;
    QList<ResolvedSlide> m_resolved;
    TimelinePlan m_plan;
    QStringList m_problems;
    QUndoStack m_undo;
};
