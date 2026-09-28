// SPDX-FileCopyrightText: 2026 David Heinemeier Hansson
// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

// Based on Omacut's portal file chooser (https://github.com/omacom-io/omacut).

#pragma once

#include <QList>
#include <QObject>
#include <QUrl>
#include <QVariantMap>

// Native file dialogs through xdg-desktop-portal, so the picker is the one
// the Omarchy desktop provides. One request at a time; the result comes back
// with the purpose it was asked for.
class PortalFilePicker : public QObject
{
    Q_OBJECT

public:
    enum class Purpose { None, OpenProject, SaveProject, AddImages, AddAudio, ExportVideo, ImportMarkers, ExportMarkers };

    explicit PortalFilePicker(QObject *parent = nullptr);

    void openProject(const QString &folder);
    void saveProject(const QString &suggestedPath);
    void addImages(const QString &folder);
    void addAudio(const QString &folder);
    // The save dialog carries a "Quality" drop-down (standard / high),
    // starting at currentQuality; the choice comes back in `choices`.
    void exportVideo(const QString &suggestedPath, const QString &currentQuality);
    void importMarkers(const QString &folder);
    void exportMarkers(const QString &suggestedPath);

Q_SIGNALS:
    // choices: the dialog's drop-downs, id → chosen option id.
    void selected(PortalFilePicker::Purpose purpose, const QList<QUrl> &urls, const QVariantMap &choices);
    void failed(const QString &message);

private Q_SLOTS:
    void handleResponse(uint response, const QVariantMap &results);

private:
    bool request(const QString &method, const QString &title, QVariantMap options, Purpose purpose);
    bool connectToRequestPath(const QString &path);
    void clearPending();

    QString m_pendingPath;
    Purpose m_pending = Purpose::None;
};
