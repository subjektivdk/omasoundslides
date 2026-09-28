// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

// omasoundslides — pictures + sound → video. Qt Quick (QML) UI, ffmpeg renders.

#include "app/controller.h"
#include "app/theme.h"
#include "app/waveformitem.h"
#include "core/keybindings.h"
#include "cli.h"
#include "version.h"

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

int main(int argc, char *argv[])
{
    if (argc > 1 && isCliCommand(argv[1]))
        return runCli(argc, argv);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omasoundslides"));
    app.setApplicationVersion(QStringLiteral(OMASOUNDSLIDES_VERSION));
    // Wayland app_id; ties the window to omasoundslides.desktop and its icon.
    app.setDesktopFileName(QStringLiteral("omasoundslides"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("omasoundslides")));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    qmlRegisterType<WaveformItem>("Omasoundslides", 1, 0, "Waveform");
    qmlRegisterUncreatableType<AudioPreview>("Omasoundslides", 1, 0, "AudioPreview",
                                             QStringLiteral("Provided by the app"));

    Theme theme;
    Controller controller;
    KeyBindings keys(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                     + QStringLiteral("/keybindings.conf"));
    QObject::connect(&keys, &KeyBindings::warning, &controller, &Controller::notice);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("project"), controller.project());
    engine.rootContext()->setContextProperty(QStringLiteral("audioPreview"), controller.audio());
    engine.rootContext()->setContextProperty(QStringLiteral("keys"), &keys);
    QFile logo(QStringLiteral(":/qml/logo.txt"));
    engine.rootContext()->setContextProperty(
        QStringLiteral("logoText"), logo.open(QIODevice::ReadOnly) ? QString::fromUtf8(logo.readAll()) : QString());
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (argc > 1)
        controller.openProject(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath());

    // Development aid: OMASOUNDSLIDES_SCREENSHOT=out.png saves the window once
    // it has settled and quits. Works with QT_QPA_PLATFORM=offscreen.
    const QString screenshot = qEnvironmentVariable("OMASOUNDSLIDES_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        const int delay = qEnvironmentVariableIntValue("OMASOUNDSLIDES_SCREENSHOT_DELAY");
        QTimer::singleShot(delay > 0 ? delay : 1500, &app, [&] {
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
                window->grabWindow().save(screenshot);
            QCoreApplication::exit(0);
        });
    }

    return app.exec();
}
