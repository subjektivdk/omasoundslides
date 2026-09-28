#include "controller.h"

#include "core/audacitylabels.h"
#include "core/exportfile.h"
#include "core/ffmpegcommand.h"

#include <QCollator>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

namespace {

const QStringList ImageExtensions = {
    QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("webp"),
    QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("bmp"),
};
const QStringList AudioExtensions = {
    QStringLiteral("mp3"), QStringLiteral("m4a"), QStringLiteral("wav"), QStringLiteral("aiff"),
    QStringLiteral("aif"), QStringLiteral("ogg"), QStringLiteral("aac"), QStringLiteral("flac"),
    QStringLiteral("opus"),
};

// Camera files come as IMG_9.jpg, IMG_10.jpg: sort them the way people count.
QStringList naturallySorted(QStringList paths)
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(paths.begin(), paths.end(), [&](const QString &a, const QString &b) {
        return collator.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0;
    });
    return paths;
}

QStringList localPaths(const QList<QUrl> &urls)
{
    QStringList paths;
    for (const QUrl &url : urls)
        if (url.isLocalFile())
            paths.append(url.toLocalFile());
    return paths;
}

QString withSuffix(QString path, const QString &suffix)
{
    if (!path.endsWith(QLatin1Char('.') + suffix, Qt::CaseInsensitive))
        path += QLatin1Char('.') + suffix;
    return path;
}

}

Controller::Controller(QObject *parent)
    : QObject(parent)
{
    connect(&m_picker, &PortalFilePicker::selected, this, &Controller::handlePicked);
    connect(&m_picker, &PortalFilePicker::failed, this, &Controller::notice);
    connect(&m_prepareWatcher, &QFutureWatcher<PreparedJob>::finished, this, &Controller::prepared);
    connect(&m_renderer, &Renderer::progress, this, [this](double fraction) {
        m_progress = fraction;
        Q_EMIT exportProgressChanged();
    });
    connect(&m_renderer, &Renderer::finished, this, &Controller::rendered);

    connect(&m_model, &ProjectModel::audioChanged, this, [this] { m_audio.rebuild(m_model.audioPaths()); });
    connect(&m_audio, &AudioPreview::failed, this, &Controller::notice);
    // The window title follows the project name.
    connect(&m_model, &ProjectModel::nameChanged, this, &Controller::projectPathChanged);
}

QString Controller::projectName() const
{
    if (!m_model.name().isEmpty())
        return m_model.name();
    return m_projectPath.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(m_projectPath).completeBaseName();
}

QString Controller::fileBaseName() const
{
    if (m_model.name().isEmpty() && m_projectPath.isEmpty())
        return QStringLiteral("slideshow");
    QString name = projectName();
    name.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("-"));
    return name;
}

QString Controller::exportPhase() const
{
    switch (m_phase) {
    case Phase::Preparing: return QStringLiteral("Checking files…");
    case Phase::Rendering: return QStringLiteral("Exporting…");
    case Phase::Idle: break;
    }
    return {};
}

bool Controller::openProject(const QString &path)
{
    QString error;
    const auto project = Project::load(path, &error);
    if (!project) {
        Q_EMIT notice(error);
        return false;
    }
    m_model.setProject(*project);
    setProjectPath(QFileInfo(path).absoluteFilePath());
    Q_EMIT projectOpened();
    Q_EMIT notice(QStringLiteral("Opened %1").arg(QFileInfo(path).fileName()));
    return true;
}

void Controller::newProject()
{
    m_model.setProject(Project{});
    setProjectPath({});
    Q_EMIT projectOpened();
}

void Controller::save()
{
    if (m_projectPath.isEmpty())
        saveAs();
    else
        saveTo(m_projectPath);
}

void Controller::saveAs()
{
    const QString suggested = m_projectPath.isEmpty()
        ? QDir(mediaFolder()).filePath(fileBaseName() + QStringLiteral(".json"))
        : m_projectPath;
    m_picker.saveProject(suggested);
}

void Controller::openProjectDialog()
{
    m_picker.openProject(mediaFolder());
}

void Controller::addImagesDialog(int insertAt)
{
    m_pendingInsertAt = insertAt;
    m_picker.addImages(mediaFolder());
}

void Controller::addAudioDialog()
{
    m_picker.addAudio(mediaFolder());
}

void Controller::exportDialog()
{
    if (exporting())
        return;
    if (m_model.rowCount() == 0) {
        Q_EMIT notice(QStringLiteral("Add images before exporting"));
        return;
    }
    if (!m_model.problems().isEmpty()) {
        Q_EMIT notice(m_model.problems().first());
        return;
    }
    const QString folder = m_projectPath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
        : QFileInfo(m_projectPath).absolutePath();
    m_picker.exportVideo(QDir(QDir(folder).exists() ? folder : QDir::homePath())
                             .filePath(fileBaseName() + QStringLiteral(".mp4")),
                         m_model.exportQuality());
}

bool Controller::openInEditor(const QString &path)
{
    QStringList launchers;
    // An explicit choice wins; handy outside Omarchy, and for the tests.
    const QString chosen = qEnvironmentVariable("OMASOUNDSLIDES_EDITOR");
    if (!chosen.isEmpty())
        launchers << chosen;
    // Omarchy's own: opens the editor picked in Omarchy's defaults and shows
    // a toast; the plain launcher is the fallback on older Omarchy versions.
    launchers << QStringLiteral("omarchy-launch-config-editor") << QStringLiteral("omarchy-launch-editor");

    for (const QString &launcher : launchers) {
        const QString exe = QStandardPaths::findExecutable(launcher);
        if (!exe.isEmpty() && QProcess::startDetached(exe, {path}))
            return true;
    }
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        return true;
    Q_EMIT notice(QStringLiteral("Could not open %1 in an editor").arg(path));
    return false;
}

void Controller::importMarkersDialog()
{
    m_picker.importMarkers(mediaFolder());
}

void Controller::exportMarkersDialog()
{
    if (m_model.project().markers.isEmpty()) {
        Q_EMIT notice(QStringLiteral("There are no markers to export"));
        return;
    }
    m_picker.exportMarkers(QDir(mediaFolder()).filePath(fileBaseName() + QStringLiteral(" markers.txt")));
}

bool Controller::importMarkers(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT notice(QStringLiteral("Cannot open %1: %2").arg(QFileInfo(path).fileName(), file.errorString()));
        return false;
    }
    QStringList warnings;
    const QList<AudacityLabels::Label> labels = AudacityLabels::parse(QString::fromUtf8(file.readAll()), &warnings);
    if (labels.isEmpty()) {
        Q_EMIT notice(QStringLiteral("No Audacity labels found in %1").arg(QFileInfo(path).fileName()));
        return false;
    }
    // A label marks where something happens: a region label by its start.
    QList<double> times;
    for (const AudacityLabels::Label &label : labels)
        times.append(label.start);
    const int count = m_model.replaceMarkers(times, QStringLiteral("Import markers"));
    QString message = QStringLiteral("Imported %1 %2 from %3")
                          .arg(count)
                          .arg(count == 1 ? QStringLiteral("marker") : QStringLiteral("markers"))
                          .arg(QFileInfo(path).fileName());
    if (!warnings.isEmpty())
        message += QStringLiteral(" · skipped %1 unreadable %2")
                       .arg(warnings.size())
                       .arg(warnings.size() == 1 ? QStringLiteral("line") : QStringLiteral("lines"));
    Q_EMIT notice(message);
    return true;
}

bool Controller::exportMarkers(const QString &path)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        Q_EMIT notice(QStringLiteral("Cannot write %1: %2").arg(QFileInfo(path).fileName(), file.errorString()));
        return false;
    }
    file.write(AudacityLabels::write(m_model.project().markers).toUtf8());
    if (!file.commit()) {
        Q_EMIT notice(QStringLiteral("Cannot save %1: %2").arg(QFileInfo(path).fileName(), file.errorString()));
        return false;
    }
    Q_EMIT notice(QStringLiteral("Exported %1 markers to %2")
                      .arg(m_model.project().markers.size())
                      .arg(QFileInfo(path).fileName()));
    return true;
}

void Controller::cancelExport()
{
    if (m_phase == Phase::Rendering) {
        m_renderer.cancel(); // finishes through rendered()
    } else if (m_phase == Phase::Preparing) {
        setPhase(Phase::Idle); // the result is ignored when it arrives
        QFile::remove(m_exportTmpPath);
        Q_EMIT notice(QStringLiteral("Export cancelled"));
    }
}

void Controller::addDroppedUrls(const QList<QUrl> &urls, int insertAt)
{
    QStringList images;
    QStringList audio;
    QString project;
    QString labels;
    QStringList skipped;
    for (const QString &path : localPaths(urls)) {
        const QString ext = QFileInfo(path).suffix().toLower();
        if (ImageExtensions.contains(ext))
            images << path;
        else if (AudioExtensions.contains(ext))
            audio << path;
        else if (ext == QLatin1String("json"))
            project = path;
        else if (ext == QLatin1String("txt"))
            labels = path;
        else
            skipped << QFileInfo(path).fileName();
    }

    if (!project.isEmpty() && images.isEmpty() && audio.isEmpty()) {
        openProject(project);
        return;
    }
    m_model.addImages(naturallySorted(images), insertAt);
    m_model.addAudio(naturallySorted(audio));
    if (!labels.isEmpty())
        importMarkers(labels);
    if (!skipped.isEmpty())
        Q_EMIT notice(QStringLiteral("Skipped: %1").arg(skipped.join(QStringLiteral(", "))));
}

void Controller::handlePicked(PortalFilePicker::Purpose purpose, const QList<QUrl> &urls,
                              const QVariantMap &choices)
{
    const QStringList paths = localPaths(urls);
    if (paths.isEmpty())
        return;

    switch (purpose) {
    case PortalFilePicker::Purpose::OpenProject:
        openProject(paths.first());
        break;
    case PortalFilePicker::Purpose::SaveProject:
        saveTo(withSuffix(paths.first(), QStringLiteral("json")));
        break;
    case PortalFilePicker::Purpose::AddImages:
        m_model.addImages(naturallySorted(paths), m_pendingInsertAt);
        break;
    case PortalFilePicker::Purpose::AddAudio:
        m_model.addAudio(naturallySorted(paths));
        break;
    case PortalFilePicker::Purpose::ExportVideo:
        // The quality picked in the dialog becomes the project's choice.
        if (choices.contains(QStringLiteral("quality")))
            m_model.setExportQuality(choices.value(QStringLiteral("quality")).toString());
        exportTo(withSuffix(paths.first(), QStringLiteral("mp4")));
        break;
    case PortalFilePicker::Purpose::ImportMarkers:
        importMarkers(paths.first());
        break;
    case PortalFilePicker::Purpose::ExportMarkers:
        exportMarkers(withSuffix(paths.first(), QStringLiteral("txt")));
        break;
    case PortalFilePicker::Purpose::None:
        break;
    }
}

bool Controller::saveTo(const QString &path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    QString error;
    const Project onDisk = m_model.project().withPathsRelativeTo(QFileInfo(absolute).absolutePath());
    if (!onDisk.save(absolute, &error)) {
        Q_EMIT notice(error);
        return false;
    }
    m_model.setModified(false);
    setProjectPath(absolute);
    Q_EMIT notice(QStringLiteral("Saved %1").arg(QFileInfo(absolute).fileName()));
    Q_EMIT saved();
    return true;
}

void Controller::exportTo(const QString &path)
{
    if (exporting())
        return;
    const QFileInfo target(path);
    m_exportPath = target.absoluteFilePath();
    // Render next to the target and rename at the end, so a failed or
    // cancelled export never leaves a half-written file under the real name.
    QString error;
    m_exportTmpPath = ExportFile::createTemporary(m_exportPath, &error);
    if (m_exportTmpPath.isEmpty()) {
        Q_EMIT notice(error);
        return;
    }
    m_progress = 0;
    Q_EMIT exportProgressChanged();
    setPhase(Phase::Preparing);

    // Probing every file with ffprobe takes a moment; keep the window responsive.
    const Project project = m_model.project();
    const QString tmp = m_exportTmpPath;
    m_prepareWatcher.setFuture(QtConcurrent::run([project, tmp] { return prepareJob(project, tmp, false); }));
}

void Controller::prepared()
{
    if (m_phase != Phase::Preparing)
        return; // cancelled meanwhile
    const PreparedJob job = m_prepareWatcher.result();
    if (!job.ok()) {
        setPhase(Phase::Idle);
        QFile::remove(m_exportTmpPath);
        Q_EMIT notice(job.errors.first());
        return;
    }
    setPhase(Phase::Rendering);
    m_renderer.start(FfmpegCommand::arguments(job.job), job.job.plan.total);
}

void Controller::rendered(bool ok, const QString &error)
{
    setPhase(Phase::Idle);
    if (!ok) {
        QFile::remove(m_exportTmpPath);
        Q_EMIT notice(error.section(QLatin1Char('\n'), 0, 1).simplified());
        return;
    }
    QString moveError;
    if (!ExportFile::finish(m_exportTmpPath, m_exportPath, &moveError)) {
        Q_EMIT notice(moveError);
        return;
    }
    Q_EMIT notice(QStringLiteral("Exported %1").arg(m_exportPath));
    Q_EMIT exportFinished(m_exportPath);
}

void Controller::setPhase(Phase phase)
{
    if (m_phase == phase)
        return;
    m_phase = phase;
    Q_EMIT exportingChanged();
}

void Controller::setProjectPath(const QString &path)
{
    if (m_projectPath == path)
        return;
    m_projectPath = path;
    Q_EMIT projectPathChanged();
}

QString Controller::mediaFolder() const
{
    if (!m_projectPath.isEmpty())
        return QFileInfo(m_projectPath).absolutePath();
    if (m_model.rowCount() > 0)
        return QFileInfo(m_model.data(m_model.index(0), ProjectModel::PathRole).toString()).absolutePath();
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return QDir(pictures).exists() ? pictures : QDir::homePath();
}
