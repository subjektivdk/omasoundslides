// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#include "keybindings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace {
const QString DurationStepKey = QStringLiteral("duration_step");
const QString ScrollStepKey = QStringLiteral("duration_scroll_step");
constexpr double DefaultDurationStep = 0.5;
constexpr double DefaultScrollStep = 0.1;
}

const QList<KeyBindings::Action> &KeyBindings::defaults()
{
    static const QList<Action> actions = {
        {QStringLiteral("play_pause"), {QStringLiteral("Space")}, QStringLiteral("Play / pause")},
        {QStringLiteral("seek_back"), {QStringLiteral("Left")}, QStringLiteral("Back 1 s")},
        {QStringLiteral("seek_forward"), {QStringLiteral("Right")}, QStringLiteral("Forward 1 s")},
        {QStringLiteral("seek_back_long"), {QStringLiteral("Shift+Left")}, QStringLiteral("Back 5 s")},
        {QStringLiteral("seek_forward_long"), {QStringLiteral("Shift+Right")}, QStringLiteral("Forward 5 s")},
        {QStringLiteral("seek_back_short"), {QStringLiteral("Alt+Left")}, QStringLiteral("Back 0.2 s")},
        {QStringLiteral("seek_forward_short"), {QStringLiteral("Alt+Right")}, QStringLiteral("Forward 0.2 s")},
        {QStringLiteral("next_image"), {QStringLiteral("Up")}, QStringLiteral("Next image")},
        {QStringLiteral("previous_image"), {QStringLiteral("Down")}, QStringLiteral("Previous image")},
        {QStringLiteral("go_to_start"), {QStringLiteral("Home")}, QStringLiteral("Go to start")},
        {QStringLiteral("go_to_end"), {QStringLiteral("End")}, QStringLiteral("Go to end")},
        {QStringLiteral("add_marker"), {QStringLiteral("M")}, QStringLiteral("Add a marker at the playhead")},
        {QStringLiteral("remove_marker"), {QStringLiteral("Shift+M")}, QStringLiteral("Remove the marker nearest the playhead")},
        {QStringLiteral("previous_marker"), {QStringLiteral(",")}, QStringLiteral("Previous marker")},
        {QStringLiteral("next_marker"), {QStringLiteral(".")}, QStringLiteral("Next marker")},
        {QStringLiteral("fit_to_markers"), {QStringLiteral("Ctrl+M")}, QStringLiteral("Fit images to the markers")},
        {QStringLiteral("duration_longer"), {QStringLiteral("+"), QStringLiteral("=")}, QStringLiteral("Make the image longer")},
        {QStringLiteral("duration_shorter"), {QStringLiteral("-")}, QStringLiteral("Make the image shorter")},
        {QStringLiteral("move_image_left"), {QStringLiteral("Ctrl+Left")}, QStringLiteral("Move the image left")},
        {QStringLiteral("move_image_right"), {QStringLiteral("Ctrl+Right")}, QStringLiteral("Move the image right")},
        {QStringLiteral("remove_image"), {QStringLiteral("Delete"), QStringLiteral("Backspace")}, QStringLiteral("Remove the image")},
        {QStringLiteral("undo"), {QStringLiteral("Ctrl+Z")}, QStringLiteral("Undo")},
        {QStringLiteral("redo"), {QStringLiteral("Ctrl+Shift+Z"), QStringLiteral("Ctrl+Y")}, QStringLiteral("Redo")},
        {QStringLiteral("zoom_in"), {QStringLiteral("Ctrl++"), QStringLiteral("Ctrl+=")}, QStringLiteral("Zoom in on the timeline")},
        {QStringLiteral("zoom_out"), {QStringLiteral("Ctrl+-")}, QStringLiteral("Zoom out")},
        {QStringLiteral("zoom_fit"), {QStringLiteral("Z")}, QStringLiteral("Show the whole timeline")},
        {QStringLiteral("add_images"), {QStringLiteral("Ctrl+I")}, QStringLiteral("Add images")},
        {QStringLiteral("add_audio"), {QStringLiteral("Ctrl+L")}, QStringLiteral("Add audio")},
        {QStringLiteral("new_project"), {QStringLiteral("Ctrl+N")}, QStringLiteral("New project")},
        {QStringLiteral("open_project"), {QStringLiteral("Ctrl+O")}, QStringLiteral("Open project")},
        {QStringLiteral("save"), {QStringLiteral("Ctrl+S")}, QStringLiteral("Save project")},
        {QStringLiteral("save_as"), {QStringLiteral("Ctrl+Shift+S")}, QStringLiteral("Save as")},
        {QStringLiteral("export"), {QStringLiteral("Ctrl+E")}, QStringLiteral("Export video")},
        {QStringLiteral("edit_keybindings"), {QStringLiteral("Ctrl+,")}, QStringLiteral("Edit the keyboard shortcuts")},
        {QStringLiteral("help"), {QStringLiteral("?")}, QStringLiteral("Show the shortcuts")},
        {QStringLiteral("quit"), {QStringLiteral("Q"), QStringLiteral("Ctrl+Q")}, QStringLiteral("Quit")},
    };
    return actions;
}

KeyBindings::KeyBindings(const QString &path, QObject *parent)
    : QObject(parent)
    , m_path(path)
    , m_actions(defaults())
{
    if (!m_path.isEmpty() && !QFileInfo::exists(m_path)) {
        QDir().mkpath(QFileInfo(m_path).absolutePath());
        QSaveFile file(m_path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(defaultFileText().toUtf8());
            file.commit();
        }
    }
    // Editors often replace the file instead of writing it, which drops the
    // watch; watching the folder too catches that, and every reload re-arms.
    const auto changed = [this] {
        watch();
        reload();
    };
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, changed);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, changed);
    watch();
    reload();
}

QVariantMap KeyBindings::keys() const
{
    QVariantMap map;
    for (const Action &a : m_actions)
        map.insert(a.id, a.keys);
    return map;
}

QVariantList KeyBindings::actions() const
{
    QVariantList list;
    for (const Action &a : m_actions)
        list.append(QVariantMap{
            {QStringLiteral("id"), a.id},
            {QStringLiteral("keys"), a.keys.join(QStringLiteral("  "))},
            {QStringLiteral("description"), a.description},
        });
    return list;
}

KeyBindings::Parsed KeyBindings::parse(const QString &text)
{
    Parsed parsed;
    QSet<QString> known;
    for (const Action &a : defaults())
        known.insert(a.id);

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int n = 0; n < lines.size(); ++n) {
        QString line = lines.at(n);
        // "#" starts a comment, except as the first character of a key.
        const qsizetype hash = line.indexOf(QRegularExpression(QStringLiteral("(^|\\s)#")));
        if (hash >= 0)
            line.truncate(hash);
        line = line.trimmed();
        if (line.isEmpty())
            continue;

        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0) {
            parsed.warnings << QStringLiteral("line %1: missing \"=\"").arg(n + 1);
            continue;
        }
        const QString id = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1).trimmed();

        if (id == DurationStepKey || id == ScrollStepKey) {
            bool ok = false;
            const double v = QString(value).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
            if (ok && v > 0)
                parsed.settings.insert(id, v);
            else
                parsed.warnings << QStringLiteral("line %1: %2 must be a number greater than 0").arg(n + 1).arg(id);
            continue;
        }
        if (!known.contains(id)) {
            parsed.warnings << QStringLiteral("line %1: unknown action \"%2\"").arg(n + 1).arg(id);
            continue;
        }
        // An empty value switches the action off.
        parsed.keys.insert(id, value.split(QLatin1Char(' '), Qt::SkipEmptyParts));
    }
    return parsed;
}

QString KeyBindings::defaultFileText()
{
    QString text = QStringLiteral(
        "# omasoundslides – keyboard shortcuts\n"
        "#\n"
        "# One action per line:   action = key [key …]\n"
        "# Separate several keys for the same action with spaces.\n"
        "# Keys are written the Qt way, e.g. Space, Left, Shift+Left, Ctrl+S, Delete, F5.\n"
        "# An empty value switches the action off. The file is reloaded when you save it.\n"
        "\n");

    int width = 0;
    for (const Action &a : defaults())
        width = std::max(width, int(a.id.size() + 3 + a.keys.join(QLatin1Char(' ')).size()));
    for (const Action &a : defaults()) {
        const QString assignment = a.id + QStringLiteral(" = ") + a.keys.join(QLatin1Char(' '));
        text += assignment.leftJustified(width + 2) + QStringLiteral("# ") + a.description + QLatin1Char('\n');
    }

    text += QStringLiteral(
                "\n"
                "# Seconds per press of duration_longer / duration_shorter\n"
                "%1 = %2\n"
                "# Seconds per mouse wheel notch over a number field (hold Shift for 5 notches)\n"
                "%3 = %4\n")
                .arg(DurationStepKey)
                .arg(DefaultDurationStep)
                .arg(ScrollStepKey)
                .arg(DefaultScrollStep);
    return text;
}

void KeyBindings::reload()
{
    QString text;
    QFile file(m_path);
    if (!m_path.isEmpty() && file.open(QIODevice::ReadOnly | QIODevice::Text))
        text = QString::fromUtf8(file.readAll());

    const Parsed parsed = parse(text);
    m_actions = defaults();
    for (Action &a : m_actions)
        if (parsed.keys.contains(a.id))
            a.keys = parsed.keys.value(a.id);
    m_durationStep = parsed.settings.value(DurationStepKey, DefaultDurationStep);
    m_scrollStep = parsed.settings.value(ScrollStepKey, DefaultScrollStep);
    Q_EMIT changed();

    for (const QString &w : parsed.warnings)
        Q_EMIT warning(QStringLiteral("%1: %2").arg(QFileInfo(m_path).fileName(), w));
}

void KeyBindings::watch()
{
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);
    if (m_path.isEmpty())
        return;
    if (QFileInfo::exists(m_path))
        m_watcher.addPath(m_path);
    const QString dir = QFileInfo(m_path).absolutePath();
    if (QDir(dir).exists())
        m_watcher.addPath(dir);
}
