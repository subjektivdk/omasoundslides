// Portal file chooser adapted from Omacut (https://github.com/omacom-io/omacut, MIT).

#include "portalfilepicker.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>

namespace {

// The portal's filter format: [(name, [(type, pattern)])], type 0 = glob, 1 = MIME.
struct FilterRule {
    uint type;
    QString pattern;
};
using FilterRules = QList<FilterRule>;

struct FileFilter {
    QString name;
    FilterRules rules;
};
using FileFilters = QList<FileFilter>;

// The "choices" option, a drop-down in the dialog:
// [(id, label, [(option id, option label)], initial option id)].
struct ChoiceOption {
    QString id;
    QString label;
};
using ChoiceOptions = QList<ChoiceOption>;

struct Choice {
    QString id;
    QString label;
    ChoiceOptions options;
    QString initial;
};
using Choices = QList<Choice>;

QDBusArgument &operator<<(QDBusArgument &argument, const FilterRule &rule)
{
    argument.beginStructure();
    argument << rule.type << rule.pattern;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, FilterRule &rule)
{
    argument.beginStructure();
    argument >> rule.type >> rule.pattern;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const FileFilter &filter)
{
    argument.beginStructure();
    argument << filter.name << filter.rules;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, FileFilter &filter)
{
    argument.beginStructure();
    argument >> filter.name >> filter.rules;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ChoiceOption &option)
{
    argument.beginStructure();
    argument << option.id << option.label;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ChoiceOption &option)
{
    argument.beginStructure();
    argument >> option.id >> option.label;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Choice &choice)
{
    argument.beginStructure();
    argument << choice.id << choice.label << choice.options << choice.initial;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Choice &choice)
{
    argument.beginStructure();
    argument >> choice.id >> choice.label >> choice.options >> choice.initial;
    argument.endStructure();
    return argument;
}

}

Q_DECLARE_METATYPE(FilterRule)
Q_DECLARE_METATYPE(FileFilter)
Q_DECLARE_METATYPE(ChoiceOption)
Q_DECLARE_METATYPE(Choice)

namespace {

void registerTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<FilterRule>();
        qDBusRegisterMetaType<FilterRules>();
        qDBusRegisterMetaType<FileFilter>();
        qDBusRegisterMetaType<FileFilters>();
        qDBusRegisterMetaType<ChoiceOption>();
        qDBusRegisterMetaType<ChoiceOptions>();
        qDBusRegisterMetaType<Choice>();
        qDBusRegisterMetaType<Choices>();
        return true;
    }();
    Q_UNUSED(registered);
}

FileFilter globFilter(const QString &name, const QString &mime, const QStringList &extensions)
{
    FileFilter filter{name, {}};
    if (!mime.isEmpty())
        filter.rules.append({1, mime});
    for (const QString &ext : extensions) {
        filter.rules.append({0, QStringLiteral("*.") + ext});
        filter.rules.append({0, QStringLiteral("*.") + ext.toUpper()});
    }
    return filter;
}

const FileFilter AllFiles{QStringLiteral("All files"), {{0, QStringLiteral("*")}}};

QString token()
{
    return QStringLiteral("omasoundslides_%1").arg(QRandomGenerator::global()->generate());
}

QByteArray portalPath(const QString &path)
{
    QByteArray bytes = path.toUtf8();
    bytes.append('\0');
    return bytes;
}

QVariantMap openOptions(const QString &accept, const QString &folder, bool multiple,
                        const FileFilter &filter)
{
    return {
        {QStringLiteral("accept_label"), accept},
        {QStringLiteral("modal"), true},
        {QStringLiteral("multiple"), multiple},
        {QStringLiteral("current_folder"), portalPath(folder)},
        {QStringLiteral("filters"), QVariant::fromValue(FileFilters{filter, AllFiles})},
        {QStringLiteral("current_filter"), QVariant::fromValue(filter)},
    };
}

QVariantMap saveOptions(const QString &accept, const QString &suggestedPath, const FileFilter &filter)
{
    const QFileInfo target(suggestedPath);
    return {
        {QStringLiteral("accept_label"), accept},
        {QStringLiteral("modal"), true},
        {QStringLiteral("current_folder"), portalPath(target.absolutePath())},
        {QStringLiteral("current_name"), target.fileName()},
        {QStringLiteral("filters"), QVariant::fromValue(FileFilters{filter})},
        {QStringLiteral("current_filter"), QVariant::fromValue(filter)},
    };
}

const FileFilter ProjectFilter = globFilter(QStringLiteral("Project"), {}, {QStringLiteral("json")});
const FileFilter ImageFilter = globFilter(
    QStringLiteral("Images"), QStringLiteral("image/*"),
    {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("webp"),
     QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("bmp")});
const FileFilter AudioFilter = globFilter(
    QStringLiteral("Audio"), QStringLiteral("audio/*"),
    {QStringLiteral("mp3"), QStringLiteral("m4a"), QStringLiteral("wav"), QStringLiteral("aiff"),
     QStringLiteral("aif"), QStringLiteral("ogg"), QStringLiteral("aac"), QStringLiteral("flac"),
     QStringLiteral("opus")});
const FileFilter Mp4Filter = globFilter(QStringLiteral("MP4 video"), {}, {QStringLiteral("mp4")});
const FileFilter LabelFilter = globFilter(QStringLiteral("Audacity labels"), {}, {QStringLiteral("txt")});

}

PortalFilePicker::PortalFilePicker(QObject *parent)
    : QObject(parent)
{
    registerTypes();
}

void PortalFilePicker::openProject(const QString &folder)
{
    request(QStringLiteral("OpenFile"), QStringLiteral("Open Project"),
            openOptions(QStringLiteral("Open"), folder, false, ProjectFilter), Purpose::OpenProject);
}

void PortalFilePicker::saveProject(const QString &suggestedPath)
{
    request(QStringLiteral("SaveFile"), QStringLiteral("Save Project"),
            saveOptions(QStringLiteral("Save"), suggestedPath, ProjectFilter), Purpose::SaveProject);
}

void PortalFilePicker::addImages(const QString &folder)
{
    request(QStringLiteral("OpenFile"), QStringLiteral("Add Images"),
            openOptions(QStringLiteral("Add"), folder, true, ImageFilter), Purpose::AddImages);
}

void PortalFilePicker::addAudio(const QString &folder)
{
    request(QStringLiteral("OpenFile"), QStringLiteral("Add Audio"),
            openOptions(QStringLiteral("Add"), folder, true, AudioFilter), Purpose::AddAudio);
}

void PortalFilePicker::exportVideo(const QString &suggestedPath, const QString &currentQuality)
{
    QVariantMap options = saveOptions(QStringLiteral("Export"), suggestedPath, Mp4Filter);
    const ChoiceOptions qualities = {
        {QStringLiteral("standard"), QStringLiteral("Standard")},
        {QStringLiteral("high"), QStringLiteral("High")},
    };
    options.insert(QStringLiteral("choices"),
                   QVariant::fromValue(Choices{{QStringLiteral("quality"), QStringLiteral("Quality"), qualities,
                                                currentQuality}}));
    request(QStringLiteral("SaveFile"), QStringLiteral("Export Video"), options, Purpose::ExportVideo);
}

void PortalFilePicker::importMarkers(const QString &folder)
{
    request(QStringLiteral("OpenFile"), QStringLiteral("Import Markers from Audacity Labels"),
            openOptions(QStringLiteral("Import"), folder, false, LabelFilter), Purpose::ImportMarkers);
}

void PortalFilePicker::exportMarkers(const QString &suggestedPath)
{
    request(QStringLiteral("SaveFile"), QStringLiteral("Export Markers as Audacity Labels"),
            saveOptions(QStringLiteral("Export"), suggestedPath, LabelFilter), Purpose::ExportMarkers);
}

bool PortalFilePicker::connectToRequestPath(const QString &path)
{
    m_pendingPath = path;
    return QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
        QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"), this,
        SLOT(handleResponse(uint,QVariantMap)));
}

bool PortalFilePicker::request(const QString &method, const QString &title, QVariantMap options,
                               Purpose purpose)
{
    if (m_pending != Purpose::None)
        return false;

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"),
                          QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.FileChooser"), bus);
    if (!portal.isValid()) {
        Q_EMIT failed(QStringLiteral("The file chooser (xdg-desktop-portal) is not available."));
        return false;
    }

    // Subscribe to the Response signal at the path the portal will derive from
    // our handle_token *before* calling, so a fast response can't slip past.
    const QString handle = token();
    options.insert(QStringLiteral("handle_token"), handle);
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString predicted =
        QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, handle);

    m_pending = purpose;
    if (!connectToRequestPath(predicted)) {
        clearPending();
        Q_EMIT failed(QStringLiteral("Could not listen for the file chooser's response."));
        return false;
    }

    auto *watcher = new QDBusPendingCallWatcher(portal.asyncCall(method, QString(), title, options), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (m_pending == Purpose::None)
            return; // already answered while the reply was in flight

        if (reply.isError()) {
            clearPending();
            Q_EMIT failed(QStringLiteral("The file chooser failed: %1").arg(reply.error().message()));
            return;
        }
        // Old portal versions may use a different request path than predicted.
        const QString actual = reply.value().path();
        if (actual != m_pendingPath) {
            QDBusConnection::sessionBus().disconnect(
                QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"), this,
                SLOT(handleResponse(uint,QVariantMap)));
            if (!connectToRequestPath(actual)) {
                clearPending();
                Q_EMIT failed(QStringLiteral("Could not listen for the file chooser's response."));
            }
        }
    });
    return true;
}

void PortalFilePicker::handleResponse(uint response, const QVariantMap &results)
{
    const Purpose purpose = m_pending;
    clearPending();
    if (response != 0)
        return; // cancelled

    QList<QUrl> urls;
    for (const QString &uri : results.value(QStringLiteral("uris")).toStringList())
        urls.append(QUrl(uri));

    // The drop-downs come back as [(id, chosen option id)].
    QVariantMap choices;
    const QVariant choicesValue = results.value(QStringLiteral("choices"));
    if (choicesValue.canConvert<QDBusArgument>()) {
        const QDBusArgument arg = choicesValue.value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            QString id;
            QString value;
            arg.beginStructure();
            arg >> id >> value;
            arg.endStructure();
            choices.insert(id, value);
        }
        arg.endArray();
    }
    if (!urls.isEmpty())
        Q_EMIT selected(purpose, urls, choices);
}

void PortalFilePicker::clearPending()
{
    if (!m_pendingPath.isEmpty())
        QDBusConnection::sessionBus().disconnect(
            QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
            QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"), this,
            SLOT(handleResponse(uint,QVariantMap)));
    m_pendingPath.clear();
    m_pending = Purpose::None;
}
