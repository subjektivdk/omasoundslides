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
    enum class Purpose { None, OpenProject, SaveProject, AddImages, AddAudio, ExportVideo };

    explicit PortalFilePicker(QObject *parent = nullptr);

    void openProject(const QString &folder);
    void saveProject(const QString &suggestedPath);
    void addImages(const QString &folder);
    void addAudio(const QString &folder);
    void exportVideo(const QString &suggestedPath);

Q_SIGNALS:
    void selected(PortalFilePicker::Purpose purpose, const QList<QUrl> &urls);
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
