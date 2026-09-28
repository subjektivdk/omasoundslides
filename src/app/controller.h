// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#pragma once

#include "core/audiopreview.h"
#include "core/prepare.h"
#include "core/projectmodel.h"
#include "core/renderer.h"
#include "portalfilepicker.h"

#include <QFutureWatcher>
#include <QObject>
#include <QUrl>

// Everything the window does that touches files: opening and saving the
// project, adding media through the portal or drag and drop, and exporting.
class Controller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ProjectModel *project READ project CONSTANT)
    Q_PROPERTY(AudioPreview *audio READ audio CONSTANT)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectPathChanged)
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectPathChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportProgressChanged)
    Q_PROPERTY(QString exportPhase READ exportPhase NOTIFY exportingChanged)

public:
    explicit Controller(QObject *parent = nullptr);

    ProjectModel *project() { return &m_model; }
    AudioPreview *audio() { return &m_audio; }
    QString projectPath() const { return m_projectPath; }
    QString projectName() const;
    bool exporting() const { return m_phase != Phase::Idle; }
    double exportProgress() const { return m_progress; }
    QString exportPhase() const;

    Q_INVOKABLE bool openProject(const QString &path);
    Q_INVOKABLE void newProject();
    // Saves to the current file, or asks where when there is none yet.
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAs();
    bool saveTo(const QString &path);

    Q_INVOKABLE void openProjectDialog();
    Q_INVOKABLE void addImagesDialog(int insertAt = -1);
    Q_INVOKABLE void addAudioDialog();
    Q_INVOKABLE void exportDialog();
    Q_INVOKABLE void importMarkersDialog();
    Q_INVOKABLE void exportMarkersDialog();
    // Opens a text file (the keybindings) in the user's editor: the one
    // chosen in Omarchy, else $OMASOUNDSLIDES_EDITOR, else the desktop's
    // handler for the file.
    Q_INVOKABLE bool openInEditor(const QString &path);
    // Audacity's label file: import replaces the markers (one undo step).
    bool importMarkers(const QString &path);
    bool exportMarkers(const QString &path);
    // Renders the project to path (the dialog ends up here too).
    Q_INVOKABLE void exportTo(const QString &path);
    Q_INVOKABLE void cancelExport();

    // Files dropped on the window: images go into the strip at insertAt,
    // audio is appended, a .txt is read as Audacity labels, a .json project
    // is opened.
    Q_INVOKABLE void addDroppedUrls(const QList<QUrl> &urls, int insertAt = -1);

Q_SIGNALS:
    void projectPathChanged();
    void exportingChanged();
    void exportProgressChanged();
    // One-line messages for the status bar.
    void notice(const QString &text);
    void exportFinished(const QString &path);
    // A project was opened or a new one started: the window resets its view.
    void projectOpened();
    void saved();

private:
    enum class Phase { Idle, Preparing, Rendering };

    void handlePicked(PortalFilePicker::Purpose purpose, const QList<QUrl> &urls, const QVariantMap &choices);
    void prepared();
    void rendered(bool ok, const QString &error);
    void setPhase(Phase phase);
    void setProjectPath(const QString &path);
    QString mediaFolder() const;
    // The project name made safe to use as a file name.
    QString fileBaseName() const;

    ProjectModel m_model;
    AudioPreview m_audio;
    PortalFilePicker m_picker;
    Renderer m_renderer;
    QFutureWatcher<PreparedJob> m_prepareWatcher;
    QString m_projectPath;
    QString m_exportPath;
    QString m_exportTmpPath;
    int m_pendingInsertAt = -1;
    double m_progress = 0;
    Phase m_phase = Phase::Idle;
};
