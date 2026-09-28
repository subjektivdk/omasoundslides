// Drives the real window offscreen: keys and mouse wheel as a user sends them.

#include "app/controller.h"
#include "app/theme.h"
#include "app/waveformitem.h"
#include "core/keybindings.h"

#include <QElapsedTimer>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

namespace {

bool runFfmpeg(const QStringList &args)
{
    QProcess p;
    p.start(QStringLiteral("ffmpeg"),
            QStringList{QStringLiteral("-hide_banner"), QStringLiteral("-v"), QStringLiteral("error"),
                        QStringLiteral("-y")}
                + args);
    return p.waitForFinished(30000) && p.exitCode() == 0;
}

void wheel(QQuickWindow *window, QPointF scenePos, int steps, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QWheelEvent event(scenePos, window->mapToGlobal(scenePos), QPoint(), QPoint(0, 120 * steps),
                      Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(window, &event);
}

// Items made by a Repeater hang in the visual tree only, not the QObject
// tree that findChild() searches.
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (QQuickItem *child : root->childItems())
        if (QQuickItem *found = findItem(child, name))
            return found;
    return nullptr;
}

QPointF centerOf(QQuickItem *item)
{
    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
}

}

class TestGui : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        auto file = [&](const QString &name) { return m_dir.filePath(name); };
        for (const char *color : {"red", "green", "blue"})
            QVERIFY(runFfmpeg({"-f", "lavfi", "-i", QStringLiteral("color=c=%1:s=320x200").arg(color),
                               "-frames:v", "1", file(QStringLiteral("%1.png").arg(color))}));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=duration=20", file("lyd.wav")}));
        QFile project(file("show.json"));
        QVERIFY(project.open(QIODevice::WriteOnly));
        project.write(R"({"images": ["red.png", "green.png", "blue.png"], "audio": ["lyd.wav"]})");
        project.close();

        qmlRegisterType<WaveformItem>("Omasoundslides", 1, 0, "Waveform");
        qmlRegisterUncreatableType<AudioPreview>("Omasoundslides", 1, 0, "AudioPreview", QString());
        QQuickStyle::setStyle(QStringLiteral("Material"));

        m_keys = new KeyBindings(file("keybindings.conf"), this);
        m_engine.rootContext()->setContextProperty(QStringLiteral("theme"), &m_theme);
        m_engine.rootContext()->setContextProperty(QStringLiteral("app"), &m_controller);
        m_engine.rootContext()->setContextProperty(QStringLiteral("project"), m_controller.project());
        m_engine.rootContext()->setContextProperty(QStringLiteral("audioPreview"), m_controller.audio());
        m_engine.rootContext()->setContextProperty(QStringLiteral("keys"), m_keys);
        QFile logo(QStringLiteral(":/qml/logo.txt"));
        QVERIFY(logo.open(QIODevice::ReadOnly));
        m_engine.rootContext()->setContextProperty(QStringLiteral("logoText"), QString::fromUtf8(logo.readAll()));
        m_engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        QVERIFY(!m_engine.rootObjects().isEmpty());
        m_window = qobject_cast<QQuickWindow *>(m_engine.rootObjects().first());
        QVERIFY(m_window);
        QVERIFY(m_controller.openProject(file("show.json")));
        m_window->show();
        m_window->requestActivate();
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
        QTRY_VERIFY(m_controller.audio()->ready());
    }

    void spaceStartsAndStopsPlayback()
    {
        QVERIFY(!m_window->property("playing").toBool());
        QTest::keyClick(m_window, Qt::Key_Space);
        QTRY_VERIFY(m_window->property("playing").toBool());
        QTest::keyClick(m_window, Qt::Key_Space);
        QTRY_VERIFY(!m_window->property("playing").toBool());
    }

    void spaceWorksAfterUsingAComboBox()
    {
        auto *combo = m_window->findChild<QQuickItem *>(QStringLiteral("imageTransitionBox"));
        QVERIFY(combo);
        combo->forceActiveFocus();
        QTest::keyClick(m_window, Qt::Key_Space);
        QTRY_VERIFY(m_window->property("playing").toBool());
        QTest::keyClick(m_window, Qt::Key_Space);
        QTRY_VERIFY(!m_window->property("playing").toBool());
    }

    void spaceWorksWhileANumberFieldHasFocus()
    {
        auto *field = m_window->findChild<QQuickItem *>(QStringLiteral("imageDurationField"));
        QVERIFY(field);
        field->forceActiveFocus();
        QVERIFY(field->hasActiveFocus());
        QTest::keyClick(m_window, Qt::Key_Space);
        QTRY_VERIFY(m_window->property("playing").toBool());
        QTest::keyClick(m_window, Qt::Key_Space);
        QTRY_VERIFY(!m_window->property("playing").toBool());
    }

    void clickingThePreviewReleasesTheNameField()
    {
        auto *name = m_window->findChild<QQuickItem *>(QStringLiteral("projectNameField"));
        QVERIFY(name);
        name->forceActiveFocus();
        QVERIFY(name->hasActiveFocus());
        // In the name field Space types a space instead of playing.
        QTest::keyClick(m_window, Qt::Key_Space);
        QVERIFY(!m_window->property("playing").toBool());

        auto *preview = m_window->findChild<QQuickItem *>(QStringLiteral("previewFrame"));
        QTest::mouseClick(m_window, Qt::LeftButton, {}, centerOf(preview).toPoint());
        QTRY_VERIFY(!name->hasActiveFocus());
        QTest::keyClick(m_window, Qt::Key_Space);
        QTRY_VERIFY(m_window->property("playing").toBool());
        QTest::keyClick(m_window, Qt::Key_Space);
        QTRY_VERIFY(!m_window->property("playing").toBool());
    }

    void smoothScrollingAddsUp()
    {
        // A smooth-scrolling wheel or touchpad on a number field: eight
        // events of 15 = one notch.
        ProjectModel *model = m_controller.project();
        auto *field = m_window->findChild<QQuickItem *>(QStringLiteral("imageDurationField"));
        const int selected = m_window->property("selected").toInt();
        const double before = model->durationOf(selected);
        for (int i = 0; i < 8; ++i) {
            QWheelEvent event(centerOf(field), m_window->mapToGlobal(centerOf(field)), QPoint(0, 2),
                              QPoint(0, 15), Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
            QCoreApplication::sendEvent(m_window, &event);
        }
        QTRY_COMPARE(model->durationOf(selected), before + 0.1);
    }

    void transitionDropdownShowsSoundslidesChoices()
    {
        ProjectModel *model = m_controller.project();
        QMetaObject::invokeMethod(m_window, "select", Q_ARG(QVariant, 2));
        auto *box = m_window->findChild<QQuickItem *>(QStringLiteral("imageTransitionBox"));
        QVERIFY(box);
        QCOMPARE(box->property("count").toInt(), 7);

        model->setTransitionPreset(2, QStringLiteral("fadeblack"), 2);
        QTRY_COMPARE(box->property("displayText").toString(), QStringLiteral("Fade out/in – Slow"));
        model->setTransitionPreset(2, QStringLiteral("fade"), 1.5); // not a preset length
        QTRY_COMPARE(box->property("displayText").toString(), QStringLiteral("Crossfade – 1.50 s"));

        // Choosing from the list sets transition and length together.
        QMetaObject::invokeMethod(box, "activated", Q_ARG(int, 1));
        QTRY_COMPARE(model->data(model->index(2), ProjectModel::TransitionDurationRole).toDouble(), 0.5);
        QCOMPARE(box->property("displayText").toString(), QStringLiteral("Crossfade – Fast"));
        model->resetTransitionPreset(2);
    }

    void ctrlZUndoesAWheelBurst()
    {
        ProjectModel *model = m_controller.project();
        auto *field = m_window->findChild<QQuickItem *>(QStringLiteral("imageDurationField"));
        const int selected = m_window->property("selected").toInt();
        const double before = model->durationOf(selected);
        for (int i = 0; i < 3; ++i)
            wheel(m_window, centerOf(field), 1);
        QTRY_COMPARE(model->durationOf(selected), before + 0.3);
        QTest::keyClick(m_window, Qt::Key_Z, Qt::ControlModifier);
        QTRY_COMPARE(model->durationOf(selected), before);
        QTest::keyClick(m_window, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_COMPARE(model->durationOf(selected), before + 0.3);
        QTest::keyClick(m_window, Qt::Key_Z, Qt::ControlModifier);
        QTRY_COMPARE(model->durationOf(selected), before);
    }

    void mAddsAMarkerAndEdgesSnapToIt()
    {
        ProjectModel *model = m_controller.project();
        auto *timeline = m_window->findChild<QQuickItem *>(QStringLiteral("timeline"));
        // A marker 1.3 s into image 1's time.
        const double marker = model->startOf(1) + model->durationOf(1) - 0.7;
        QMetaObject::invokeMethod(m_window, "seek", Q_ARG(QVariant, marker));
        QTest::keyClick(m_window, Qt::Key_M);
        QTRY_COMPARE(model->markers().size(), 1);
        QCOMPARE(model->markers().first().toDouble(), marker);
        // Move the playhead away so it isn't a snap point itself.
        QMetaObject::invokeMethod(m_window, "seek", Q_ARG(QVariant, 0.0));

        QVariant result;
        QVERIFY(QMetaObject::invokeMethod(timeline, "blockAt", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 1)));
        auto *block = result.value<QQuickItem *>();
        const double pps = timeline->property("pps").toDouble();
        const QPointF edge = block->mapToScene(QPointF(block->width(), block->height() / 2));
        const QPointF nearMarker(block->mapToScene(QPointF((marker - model->startOf(1)) * pps + 4, 0)).x(), edge.y());

        const double start = model->startOf(1);
        QTest::mousePress(m_window, Qt::LeftButton, {}, edge.toPoint());
        QTest::mouseMove(m_window, ((edge + nearMarker) / 2).toPoint());
        QTest::mouseMove(m_window, nearMarker.toPoint());
        QTest::mouseRelease(m_window, Qt::LeftButton, {}, nearMarker.toPoint());
        QTRY_VERIFY(qAbs(model->durationOf(1) - std::round((marker - start) * 100) / 100) < 0.006);

        model->undo();
        model->clearMarkers();
    }

    void fadeHandleSetsTheFadeIn()
    {
        ProjectModel *model = m_controller.project();
        auto *handle = m_window->findChild<QQuickItem *>(QStringLiteral("fadeInHandle"));
        QVERIFY(handle);
        QVERIFY(handle->isVisible());
        const QPointF from = centerOf(handle);
        const QPointF to = from + QPointF(60, 0);
        QTest::mousePress(m_window, Qt::LeftButton, {}, from.toPoint());
        QTest::mouseMove(m_window, (from + QPointF(30, 0)).toPoint());
        QTest::mouseMove(m_window, to.toPoint());
        QTest::mouseRelease(m_window, Qt::LeftButton, {}, to.toPoint());
        QTRY_VERIFY(model->audioFadeIn() > 0.3);
        model->undo();
        QCOMPARE(model->audioFadeIn(), 0.0);
    }

    void arrowsMoveThePlayhead()
    {
        const double before = m_window->property("position").toDouble();
        QTest::keyClick(m_window, Qt::Key_Right);
        QTRY_COMPARE(m_window->property("position").toDouble(), before + 1);
    }

    void wheelOverTheTracksZooms()
    {
        ProjectModel *model = m_controller.project();
        auto *timeline = m_window->findChild<QQuickItem *>(QStringLiteral("timeline"));
        QVERIFY(timeline);
        QVariant result;
        QVERIFY(QMetaObject::invokeMethod(timeline, "blockAt", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 1)));
        auto *block = result.value<QQuickItem *>();
        const double duration = model->durationOf(1);
        const double fitted = timeline->property("pps").toDouble();

        // Over an image: scrolling down zooms in, and the duration stays put.
        wheel(m_window, centerOf(block), -2);
        QTRY_VERIFY(timeline->property("pps").toDouble() > fitted * 1.5);
        QCOMPARE(model->durationOf(1), duration);

        // Shift pans instead.
        const double viewStart = timeline->property("viewStart").toDouble();
        wheel(m_window, centerOf(block), -1, Qt::ShiftModifier);
        QTRY_VERIFY(timeline->property("viewStart").toDouble() > viewStart);

        // Over the audio: scrolling up zooms back out.
        auto *audio = m_window->findChild<QQuickItem *>(QStringLiteral("fadeInHandle"))->parentItem();
        wheel(m_window, centerOf(audio), 4);
        QTRY_COMPARE(timeline->property("pps").toDouble(), fitted);
        QMetaObject::invokeMethod(timeline, "fit");
    }

    void touchpadScrollingZoomsAndNudges()
    {
        // Wayland reports touchpad scrolling from its own TouchPad device, in
        // many small steps. Handlers that only accept Mouse ignore it.
        QPointingDevice touchpad(QStringLiteral("test touchpad"), 4242, QInputDevice::DeviceType::TouchPad,
                                 QPointingDevice::PointerType::Finger,
                                 QInputDevice::Capability::Position | QInputDevice::Capability::Scroll
                                     | QInputDevice::Capability::PixelScroll,
                                 1, 0);
        auto swipe = [&](QPointF at, int dy, int dx = 0) {
            const Qt::ScrollPhase phases[] = {Qt::ScrollBegin, Qt::ScrollUpdate, Qt::ScrollUpdate,
                                              Qt::ScrollUpdate, Qt::ScrollUpdate, Qt::ScrollEnd};
            for (Qt::ScrollPhase phase : phases) {
                const bool moving = phase == Qt::ScrollUpdate;
                QWheelEvent event(at, m_window->mapToGlobal(at), moving ? QPoint(dx, dy) / 4 : QPoint(),
                                  moving ? QPoint(dx, dy) : QPoint(), Qt::NoButton, Qt::NoModifier, phase,
                                  false, Qt::MouseEventNotSynthesized, &touchpad);
                QCoreApplication::sendEvent(m_window, &event);
            }
        };

        auto *timeline = m_window->findChild<QQuickItem *>(QStringLiteral("timeline"));
        QMetaObject::invokeMethod(timeline, "fit");
        const double fitted = timeline->property("pps").toDouble();
        auto *audio = m_window->findChild<QQuickItem *>(QStringLiteral("fadeInHandle"))->parentItem();
        swipe(centerOf(audio), -60); // 4 × 60 = two notches, downwards: zoom in
        QTRY_VERIFY(timeline->property("pps").toDouble() > fitted * 1.4);

        const double viewStart = timeline->property("viewStart").toDouble();
        swipe(centerOf(audio), 0, -60); // sideways: pan
        QTRY_VERIFY(timeline->property("viewStart").toDouble() > viewStart);
        QMetaObject::invokeMethod(timeline, "fit");

        ProjectModel *model = m_controller.project();
        auto *field = m_window->findChild<QQuickItem *>(QStringLiteral("imageDurationField"));
        const int selected = m_window->property("selected").toInt();
        const double before = model->durationOf(selected);
        swipe(centerOf(field), 30); // 4 × 30 = one notch
        QTRY_COMPARE(model->durationOf(selected), before + 0.1);
        model->undo();
    }

    void audioChipsCanBeDraggedIntoOrder()
    {
        ProjectModel *model = m_controller.project();
        const QString second = m_dir.filePath(QStringLiteral("anden.wav"));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=frequency=880:duration=3", second}));
        model->addAudio({second});
        const QString first = model->audioPaths().first();

        QQuickItem *chip0 = nullptr;
        QQuickItem *chip1 = nullptr;
        QTRY_VERIFY((chip0 = findItem(m_window->contentItem(), QStringLiteral("audioChip0")))
                    && (chip1 = findItem(m_window->contentItem(), QStringLiteral("audioChip1"))));
        // Wait for the row to lay the new chip out next to the first.
        QTRY_VERIFY(chip1->x() >= chip0->x() + chip0->width());
        // Drag the first chip to the right, past the middle of the second.
        const QPointF from = centerOf(chip0);
        const QPointF to(chip1->mapToScene(QPointF(chip1->width() * 0.75, 0)).x(), from.y());
        QTest::mousePress(m_window, Qt::LeftButton, {}, from.toPoint());
        for (int step = 1; step <= 10; ++step)
            QTest::mouseMove(m_window, (from + (to - from) * step / 10.0).toPoint());
        QTest::mouseRelease(m_window, Qt::LeftButton, {}, to.toPoint());
        QTRY_COMPARE(model->audioPaths(), (QStringList{second, first}));

        model->undo();
        QCOMPARE(model->audioPaths(), (QStringList{first, second}));
        model->undo(); // and the added file
        QCOMPARE(model->audioPaths(), QStringList{first});
    }

    void editShortcutsOpensTheFileInTheEditor()
    {
        // Stand-in editor: records what it was asked to open.
        const QString log = m_dir.filePath(QStringLiteral("editor.log"));
        const QString editor = m_dir.filePath(QStringLiteral("editor.sh"));
        QFile script(editor);
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write(QStringLiteral("#!/bin/sh\necho \"$1\" > '%1'\n").arg(log).toUtf8());
        script.close();
        script.setPermissions(script.permissions() | QFileDevice::ExeOwner);
        qputenv("OMASOUNDSLIDES_EDITOR", editor.toUtf8());

        QTest::keyClick(m_window, Qt::Key_Comma, Qt::ControlModifier);
        QTRY_VERIFY(QFileInfo::exists(log));
        QFile result(log);
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(result.readAll()).trimmed(), m_keys->path());
        qunsetenv("OMASOUNDSLIDES_EDITOR");
    }

    void theProjectTabHasALabelsHeading()
    {
        auto *heading = m_window->findChild<QQuickItem *>(QStringLiteral("labelsHeading"));
        QVERIFY(heading);
        QCOMPARE(heading->property("text").toString(), QStringLiteral("Labels"));
    }

    void namesFromOutsideAreNeverMarkup()
    {
        ProjectModel *model = m_controller.project();
        const QString evil = QStringLiteral("<img src=\"https://example.invalid/x.png\"><b>show</b>");
        model->setName(evil);
        auto *name = m_window->findChild<QQuickItem *>(QStringLiteral("projectNameLabel"));
        QVERIFY(name);
        QCOMPARE(name->property("textFormat").toInt(), 0); // Text.PlainText
        QVERIFY(name->property("text").toString().startsWith(evil));

        // Messages quoting a name are escaped before the status line styles them.
        Q_EMIT m_controller.notice(QStringLiteral("Opened ") + evil);
        auto *status = m_window->findChild<QQuickItem *>(QStringLiteral("statusLabel"));
        QVERIFY(status);
        QTRY_VERIFY(status->property("text").toString().contains(QStringLiteral("&lt;img src=")));
        QVERIFY(!status->property("text").toString().contains(QStringLiteral("<img")));
        model->undo();
    }

    void upAndDownStepThroughImages()
    {
        QMetaObject::invokeMethod(m_window, "select", Q_ARG(QVariant, 1));
        QTest::keyClick(m_window, Qt::Key_Up);
        QTRY_COMPARE(m_window->property("selected").toInt(), 2);
        QTest::keyClick(m_window, Qt::Key_Down);
        QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_COMPARE(m_window->property("selected").toInt(), 0);
    }

    void wheelOverTheDurationFieldChangesIt()
    {
        ProjectModel *model = m_controller.project();
        auto *field = m_window->findChild<QQuickItem *>(QStringLiteral("imageDurationField"));
        QVERIFY(field);
        const int selected = m_window->property("selected").toInt();
        const double before = model->durationOf(selected);
        wheel(m_window, centerOf(field), 3);
        QTRY_COMPARE(model->durationOf(selected), before + 0.3);
    }

    void previewShowsTheCrossfade()
    {
        ProjectModel *model = m_controller.project();
        // Halfway through the 1 s crossfade from red (image 1) into green (image 2).
        const double t = model->startOf(1) + 0.5;
        QMetaObject::invokeMethod(m_window, "seek", Q_ARG(QVariant, t));
        QTRY_COMPARE(m_window->property("position").toDouble(), t);
        QTest::qWait(300);

        auto *preview = m_window->findChild<QQuickItem *>(QStringLiteral("previewFrame"));
        QVERIFY(preview);
        const QImage shot = m_window->grabWindow();
        const QPointF c = centerOf(preview) * m_window->devicePixelRatio();
        const QColor color = shot.pixelColor(c.toPoint());
        qInfo() << "mid-transition pixel" << color.name();
        QVERIFY2(color.red() > 60 && color.green() > 60, qPrintable(color.name()));
    }

    void previewShowsTheCrossfadeWhilePlaying()
    {
        ProjectModel *model = m_controller.project();
        const double transitionStart = model->startOf(2); // green → blue
        QMetaObject::invokeMethod(m_window, "seek", Q_ARG(QVariant, transitionStart - 0.3));
        QMetaObject::invokeMethod(m_window, "play");
        auto *preview = m_window->findChild<QQuickItem *>(QStringLiteral("previewFrame"));
        QStringList seen;
        bool mixed = false;
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 1800) {
            QTest::qWait(50);
            const QImage shot = m_window->grabWindow();
            const QColor color = shot.pixelColor((centerOf(preview) * m_window->devicePixelRatio()).toPoint());
            const double pos = m_window->property("position").toDouble();
            seen << QStringLiteral("%1:%2").arg(pos - transitionStart, 0, 'f', 2).arg(color.name());
            if (color.green() > 20 && color.blue() > 20)
                mixed = true;
        }
        QMetaObject::invokeMethod(m_window, "pause");
        qInfo() << seen.join(QLatin1Char(' '));
        QVERIFY2(mixed, "never saw green and blue mixed while playing");
    }

private:
    QTemporaryDir m_dir;
    Theme m_theme;
    Controller m_controller;
    KeyBindings *m_keys = nullptr;
    QQmlApplicationEngine m_engine;
    QQuickWindow *m_window = nullptr;
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestGui test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_gui.moc"
