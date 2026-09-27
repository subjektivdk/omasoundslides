#include "app/controller.h"
#include "core/audacitylabels.h"
#include "core/audiopreview.h"
#include "core/ffmpegcommand.h"
#include "core/keybindings.h"
#include "core/prepare.h"
#include "core/probe.h"
#include "core/project.h"
#include "core/projectmodel.h"
#include "core/renderer.h"
#include "core/timeline.h"
#include "core/transitions.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QJsonDocument>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

namespace {

ResolvedSlide slide(double duration, const QString &transition = QStringLiteral("none"),
                    double transitionDuration = 0)
{
    return {QStringLiteral("/img.jpg"), duration, transition, transitionDuration};
}

Project projectFromJson(const char *json)
{
    QString error;
    const auto p = Project::fromJson(QJsonDocument::fromJson(json).object(), &error);
    if (!p)
        qFatal("invalid test project: %s", qPrintable(error));
    return *p;
}

bool haveFfmpeg()
{
    return !QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty()
        && !QStandardPaths::findExecutable(QStringLiteral("ffprobe")).isEmpty();
}

bool runFfmpeg(const QStringList &args)
{
    QProcess p;
    p.start(QStringLiteral("ffmpeg"),
            QStringList{QStringLiteral("-hide_banner"), QStringLiteral("-v"), QStringLiteral("error"),
                        QStringLiteral("-y")}
                + args);
    return p.waitForFinished(30000) && p.exitCode() == 0;
}

}

class TestCore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void threeImagesWithCrossfadesLastThirteenSeconds()
    {
        const QList<ResolvedSlide> slides = {slide(5), slide(5, "fade", 1), slide(5, "fade", 1)};
        const TimelinePlan plan = Timeline::plan(slides);
        QCOMPARE(plan.starts, (QList<double>{0, 4, 8}));
        QCOMPARE(plan.total, 13.0);
        QCOMPARE(plan.runs.size(), 1);
    }

    void straightCutsSplitIntoRuns()
    {
        const QList<ResolvedSlide> slides = {slide(6), slide(5, "fadeblack", 0.5), slide(5)};
        const TimelinePlan plan = Timeline::plan(slides);
        QCOMPARE(plan.starts, (QList<double>{0, 5.5, 10.5}));
        QCOMPARE(plan.total, 15.5);
        QCOMPARE(plan.runs, (QList<QPair<int, int>>{{0, 1}, {2, 2}}));
    }

    void onlyCutsMeansDurationsJustAddUp()
    {
        const TimelinePlan plan = Timeline::plan({slide(2), slide(3), slide(4)});
        QCOMPARE(plan.total, 9.0);
        QCOMPARE(plan.runs.size(), 3);
    }

    void autoDurationFillsTheAudioExactly()
    {
        QList<ResolvedSlide> slides = {slide(1), slide(1, "fade", 1), slide(1, "fade", 1)};
        const double d = Timeline::autoDuration(slides, 13);
        QCOMPARE(d, 5.0);
        for (ResolvedSlide &s : slides)
            s.duration = d;
        QCOMPARE(Timeline::plan(slides).total, 13.0);
    }

    void transitionsLongerThanTheImageAreRejected()
    {
        const QList<ResolvedSlide> slides = {slide(3), slide(2, "fade", 1), slide(3, "fade", 1.5)};
        const QStringList problems = Timeline::validate(slides);
        QCOMPARE(problems.size(), 1);
        QVERIFY(problems.first().startsWith(QStringLiteral("Image 2")));
        QVERIFY(Timeline::validate({slide(3), slide(2, "fade", 1), slide(3, "fade", 1)}).isEmpty());
    }

    void defaultsAndOverridesAreResolved()
    {
        const Project p = projectFromJson(R"({
            "defaults": {"duration": 5, "transition": "crossfade", "transition_duration": 1},
            "images": [
                "01.jpg",
                {"path": "02.jpg", "duration": 6, "transition": "fadeout", "transition_duration": 0.5},
                {"path": "03.jpg", "transition": "cut"}
            ]
        })");
        const QList<ResolvedSlide> r = resolveSlides(p);
        QCOMPARE(r.size(), 3);
        QCOMPARE(r[0].transition, QStringLiteral("none")); // first image never has one
        QCOMPARE(r[0].duration, 5.0);
        QCOMPARE(r[1].transition, QStringLiteral("fadeblack"));
        QCOMPARE(r[1].transitionDuration, 0.5);
        QCOMPARE(r[1].duration, 6.0);
        QCOMPARE(r[2].transition, QStringLiteral("none"));
        QCOMPARE(r[2].transitionDuration, 0.0);
    }

    void invalidProjectsGiveReadableErrors()
    {
        QString error;
        QVERIFY(!Project::fromJson(QJsonDocument::fromJson(
                                       R"({"images": [{"path": "a.jpg", "transition": "sparkles"}]})")
                                       .object(),
                                   &error));
        QVERIFY(error.contains(QStringLiteral("sparkles")));

        // Only Soundslides' transitions: other ffmpeg ones are refused too.
        QVERIFY(!Project::fromJson(QJsonDocument::fromJson(
                                       R"({"images": [{"path": "a.jpg", "transition": "wipeleft"}]})")
                                       .object(),
                                   &error));

        QVERIFY(!Project::fromJson(
            QJsonDocument::fromJson(R"({"output": {"width": 1001}})").object(), &error));
        QVERIFY(!Project::fromJson(
            QJsonDocument::fromJson(R"({"images": [{"duration": 3}]})").object(), &error));
    }

    void jsonRoundTrip()
    {
        const Project p = projectFromJson(R"({
            "name": "Trav på Jydsk Væddeløbsbane",
            "audio_fade": {"in": 1.5, "out": 4},
            "output": {"width": 1280, "height": 720, "fps": 25},
            "images": [{"path": "a.jpg", "duration": 2.5}, {"path": "b.jpg", "transition": "fadeblack"}],
            "audio": ["x.mp3"]
        })");
        QString error;
        const auto again = Project::fromJson(p.toJson(), &error);
        QVERIFY(again);
        QCOMPARE(again->name, QStringLiteral("Trav på Jydsk Væddeløbsbane"));
        QCOMPARE(again->audioFadeIn, 1.5);
        QCOMPARE(again->audioFadeOut, 4.0);
        QCOMPARE(again->output.width, 1280);
        QCOMPARE(again->output.fps, 25);
        QCOMPARE(again->slides.size(), 2);
        QCOMPARE(*again->slides[0].duration, 2.5);
        QVERIFY(!again->slides[1].duration);
        QCOMPARE(*again->slides[1].transition, QStringLiteral("fadeblack"));
        QCOMPARE(again->audio, QStringList{QStringLiteral("x.mp3")});
    }

    void relativePathsResolveAgainstTheProjectFolder()
    {
        Project p;
        p.baseDir = QStringLiteral("/home/me/show");
        QCOMPARE(p.resolvePath(QStringLiteral("img/01.jpg")), QStringLiteral("/home/me/show/img/01.jpg"));
        QCOMPARE(p.resolvePath(QStringLiteral("/tmp/a.jpg")), QStringLiteral("/tmp/a.jpg"));
    }

    void filterGraphMatchesTheHandWrittenExample()
    {
        RenderJob job;
        job.slides = {slide(5), slide(5, "fade", 1), slide(5, "fadeblack", 1)};
        job.plan = Timeline::plan(job.slides);
        job.audioPaths = {QStringLiteral("/lyd.mp3")};
        const QString graph = FfmpegCommand::filterGraph(job);

        QVERIFY(graph.contains(QStringLiteral("[v0][v1]xfade=transition=fade:duration=1.000:offset=4.000[x1]")));
        QVERIFY(graph.contains(QStringLiteral("[x1][v2]xfade=transition=fadeblack:duration=1.000:offset=8.000[vout]")));
        QVERIFY(graph.contains(QStringLiteral("[3:a]aresample=48000")));
        QVERIFY(!graph.contains(QStringLiteral("concat")));
    }

    void cutsAreJoinedWithConcat()
    {
        RenderJob job;
        job.slides = {slide(4), slide(4, "fade", 1), slide(4), slide(4, "fade", 1)};
        job.plan = Timeline::plan(job.slides);
        const QString graph = FfmpegCommand::filterGraph(job);

        // Offsets are relative to the start of each run, not the whole video.
        QVERIFY(graph.contains(QStringLiteral("[v0][v1]xfade=transition=fade:duration=1.000:offset=3.000[x1]")));
        QVERIFY(graph.contains(QStringLiteral("[v2][v3]xfade=transition=fade:duration=1.000:offset=3.000[x3]")));
        QVERIFY(graph.contains(QStringLiteral("[x1][x3]concat=n=2:v=1:a=0[vout]")));
    }

    void audioFadesAreAppliedWhereTheAudioEnds()
    {
        RenderJob job;
        job.slides = {slide(5), slide(5, "fade", 1)};
        job.plan = Timeline::plan(job.slides); // 9 s of pictures
        job.audioPaths = {QStringLiteral("/lyd.mp3")};
        job.audioSeconds = 20; // longer than the video: fade out at the video's end
        job.audioFadeIn = 2;
        job.audioFadeOut = 3;
        QVERIFY(FfmpegCommand::filterGraph(job).contains(
            QStringLiteral("[a0]afade=t=in:st=0:d=2.000,afade=t=out:st=6.000:d=3.000,apad[aout]")));

        job.audioSeconds = 7; // shorter than the video: fade out at the audio's end
        QVERIFY(FfmpegCommand::filterGraph(job).contains(QStringLiteral("afade=t=out:st=4.000:d=3.000")));

        job.audioFadeIn = 0;
        job.audioFadeOut = 0;
        QVERIFY(!FfmpegCommand::filterGraph(job).contains(QStringLiteral("afade")));
    }

    void exportQualityPicksTheH264Settings()
    {
        RenderJob job;
        job.slides = {slide(3)};
        job.plan = Timeline::plan(job.slides);
        job.outputPath = QStringLiteral("/tmp/out.mp4");
        const QString standard = FfmpegCommand::arguments(job).join(QLatin1Char(' '));
        QVERIFY(standard.contains(QStringLiteral("-c:v libx264 -preset medium -crf 20")));
        job.output.quality = ExportQuality::High;
        const QString high = FfmpegCommand::arguments(job).join(QLatin1Char(' '));
        QVERIFY(high.contains(QStringLiteral("-c:v libx264 -preset slow -crf 18")));
        QVERIFY(high.contains(QStringLiteral("-movflags +faststart /tmp/out.mp4")));

        // Stored in the project; anything but standard/high is refused.
        Project p;
        p.output.quality = ExportQuality::High;
        QString error;
        QCOMPARE(Project::fromJson(p.toJson(), &error)->output.quality, ExportQuality::High);
        QVERIFY(!Project::fromJson(QJsonDocument::fromJson(R"({"output": {"quality": "ultra"}})").object(), &error));
        QCOMPARE(projectFromJson("{}").output.quality, ExportQuality::Standard);

        ProjectModel model;
        model.setExportQuality(QStringLiteral("high"));
        QCOMPARE(model.exportQuality(), QStringLiteral("high"));
        model.setExportQuality(QStringLiteral("nonsense"));
        QCOMPARE(model.exportQuality(), QStringLiteral("high"));
        model.undo();
        QCOMPARE(model.exportQuality(), QStringLiteral("standard"));
    }

    void singleImageNeedsNoTransitions()
    {
        RenderJob job;
        job.slides = {slide(4)};
        job.plan = Timeline::plan(job.slides);
        QVERIFY(FfmpegCommand::filterGraph(job).endsWith(QStringLiteral("[vout]")));
    }

    void pathsAreStoredRelativeToTheProjectFile()
    {
        Project p;
        p.slides = {Slide{QStringLiteral("/home/me/show/img/01.jpg"), {}, {}, {}}};
        p.audio = {QStringLiteral("/home/me/lyd/interview.mp3")};
        const Project onDisk = p.withPathsRelativeTo(QStringLiteral("/home/me/show"));
        QCOMPARE(onDisk.slides[0].path, QStringLiteral("img/01.jpg"));
        QCOMPARE(onDisk.audio[0], QStringLiteral("../lyd/interview.mp3"));

        const Project back = onDisk.withAbsolutePaths();
        QCOMPARE(back.slides[0].path, QStringLiteral("/home/me/show/img/01.jpg"));
        QCOMPARE(back.audio[0], QStringLiteral("/home/me/lyd/interview.mp3"));
    }

    void modelKeepsTheTimelineInStep()
    {
        ProjectModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QVERIFY(!model.isModified());
        model.addImages({QStringLiteral("/a.jpg"), QStringLiteral("/b.jpg"), QStringLiteral("/c.jpg")});
        QVERIFY(model.isModified());
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.videoDuration(), 13.0); // defaults: 5 s, 1 s fade

        model.addImages({QStringLiteral("/x.jpg")}, 1);
        auto name = [&](int row) { return model.data(model.index(row), ProjectModel::FileNameRole).toString(); };
        QCOMPARE(name(1), QStringLiteral("x.jpg"));

        model.moveImage(1, 3);
        QCOMPARE(name(3), QStringLiteral("x.jpg"));
        QCOMPARE(name(1), QStringLiteral("b.jpg"));
        model.moveImage(3, 0);
        QCOMPARE(name(0), QStringLiteral("x.jpg"));

        model.removeImage(0);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.startOf(2), 8.0);
    }

    void modelOverridesAndDefaults()
    {
        ProjectModel model;
        model.addImages({QStringLiteral("/a.jpg"), QStringLiteral("/b.jpg"), QStringLiteral("/c.jpg")});

        model.setDuration(0, 7.004);
        QCOMPARE(model.durationOf(0), 7.0); // rounded to hundredths
        QVERIFY(model.data(model.index(0), ProjectModel::DurationSetRole).toBool());
        QCOMPARE(model.startOf(1), 6.0);

        model.setDefaultDuration(4);
        QCOMPARE(model.durationOf(0), 7.0); // still overridden
        QCOMPARE(model.durationOf(1), 4.0);
        model.resetDuration(0);
        QCOMPARE(model.durationOf(0), 4.0);

        model.setTransition(2, QStringLiteral("cut"));
        QCOMPARE(model.data(model.index(2), ProjectModel::TransitionRole).toString(), QStringLiteral("none"));
        QCOMPARE(model.videoDuration(), 4 + 4 + 4 - 1.0);

        model.setTransitionDuration(1, 5);
        QVERIFY(!model.problems().isEmpty()); // 5 s fade into a 4 s image
        model.resetTransitionDuration(1);
        QVERIFY(model.problems().isEmpty());

        // The first image never shows a transition, whatever the default is.
        QCOMPARE(model.data(model.index(0), ProjectModel::TransitionRole).toString(), QStringLiteral("none"));
    }

    void soundslidesTransitionPresets()
    {
        const auto &presets = Transitions::presets();
        QCOMPARE(presets.size(), 7);
        QStringList labels;
        for (const auto &p : presets)
            labels << QStringLiteral("%1=%2/%3").arg(p.label, p.transition).arg(p.duration);
        QCOMPARE(labels, (QStringList{
                             "Straight cut=none/0",
                             "Crossfade – Fast=fade/0.5", "Crossfade – Medium=fade/1", "Crossfade – Slow=fade/2",
                             "Fade out/in – Fast=fadeblack/0.5", "Fade out/in – Medium=fadeblack/1",
                             "Fade out/in – Slow=fadeblack/2"}));

        ProjectModel model;
        model.addImages({QStringLiteral("/a.jpg"), QStringLiteral("/b.jpg")});
        model.setTransitionPreset(1, QStringLiteral("fadeblack"), 2);
        QCOMPARE(model.data(model.index(1), ProjectModel::TransitionRole).toString(), QStringLiteral("fadeblack"));
        QCOMPARE(model.data(model.index(1), ProjectModel::TransitionDurationRole).toDouble(), 2.0);
        QCOMPARE(model.videoDuration(), 5 + 5 - 2.0);
        model.resetTransitionPreset(1);
        QVERIFY(!model.data(model.index(1), ProjectModel::TransitionSetRole).toBool());
        QVERIFY(!model.data(model.index(1), ProjectModel::TransitionDurationSetRole).toBool());

        model.setDefaultTransitionPreset(QStringLiteral("fade"), 0.5);
        QCOMPARE(model.defaultTransition(), QStringLiteral("fade"));
        QCOMPARE(model.defaultTransitionDuration(), 0.5);
        // A cut as the default must not take the length from images that
        // set only their own transition type.
        model.setTransitionPreset(1, QStringLiteral("fade"), 1);
        model.resetTransitionDuration(1); // "transition": "fade", length from the project
        model.setDefaultTransitionPreset(QStringLiteral("none"), 0);
        QCOMPARE(model.defaultTransitionDuration(), 0.5);
        QCOMPARE(model.data(model.index(1), ProjectModel::TransitionRole).toString(), QStringLiteral("fade"));
        QCOMPARE(model.videoDuration(), 5 + 5 - 0.5);

        model.setTransitionPreset(1, QStringLiteral("wipeleft"), 1); // not ours: ignored
        QCOMPARE(model.data(model.index(1), ProjectModel::TransitionRole).toString(), QStringLiteral("fade"));
    }

    void undoAndRedo()
    {
        ProjectModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QVERIFY(!model.canUndo());
        model.addImages({QStringLiteral("/a.jpg"), QStringLiteral("/b.jpg"), QStringLiteral("/c.jpg")});
        model.setModified(false); // "saved"
        QVERIFY(!model.isModified());

        // A burst of changes to one image (a mouse wheel) is one undo step.
        for (int i = 1; i <= 5; ++i)
            model.setDuration(1, 5 + i * 0.1);
        QCOMPARE(model.durationOf(1), 5.5);
        QVERIFY(model.isModified());
        model.undo();
        QCOMPARE(model.durationOf(1), 5.0);
        QVERIFY(!model.isModified()); // back where it was saved
        model.redo();
        QCOMPARE(model.durationOf(1), 5.5);

        model.moveImage(0, 2);
        QCOMPARE(model.data(model.index(2), ProjectModel::FileNameRole).toString(), QStringLiteral("a.jpg"));
        model.removeImage(0);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.undoText(), QStringLiteral("Remove image"));
        model.undo();
        QCOMPARE(model.rowCount(), 3);
        model.undo();
        QCOMPARE(model.data(model.index(0), ProjectModel::FileNameRole).toString(), QStringLiteral("a.jpg"));

        model.setName(QStringLiteral("Show"));
        model.setAudioFadeIn(2);
        model.undo();
        model.undo();
        QCOMPARE(model.name(), QString());
        QCOMPARE(model.audioFadeIn(), 0.0);

        // Opening a project starts a new history.
        model.setProject(Project{});
        QVERIFY(!model.canUndo());
        QVERIFY(!model.isModified());
    }

    void markers()
    {
        ProjectModel model;
        QCOMPARE(model.addMarker(4), 0);
        QCOMPARE(model.addMarker(10), 1);
        QCOMPARE(model.addMarker(1.5), 2);
        QCOMPARE(model.addMarker(10.02), -1); // same cue
        QCOMPARE(model.markerAfter(4), 10.0);
        QCOMPARE(model.markerBefore(4), 1.5);
        QCOMPARE(model.markerAfter(10), -1.0);

        QVERIFY(!model.removeMarkerNear(7, 0.5));
        QVERIFY(model.removeMarkerNear(9.8, 0.5));
        QCOMPARE(model.markers().size(), 2);
        model.undo();
        QCOMPARE(model.markers().size(), 3);

        model.moveMarker(2, 2);
        model.moveMarker(2, 2.5); // one drag, one undo step
        model.undo();
        QCOMPARE(model.markers().at(2).toDouble(), 1.5);

        // Saved sorted; read back.
        QString error;
        const auto again = Project::fromJson(model.project().toJson(), &error);
        QVERIFY(again);
        QCOMPARE(again->markers, (QList<double>{1.5, 4, 10}));
    }

    void audacityLabelsAreRead()
    {
        // As Audacity 3/4 writes them: tab-separated, six decimals, a
        // spectral line starting with '\', and a second track appended.
        const QString text = QStringLiteral(
            "2.150000\t2.150000\tpoint label\r\n"
            "3.400000\t6.100000\tregion label with spaces\r\n"
            "\\\t1484.669312\t2969.338523\r\n"
            "8.000000\t8.000000\t\r\n"
            "not a time\t1\tbroken\r\n"
            "\r\n"
            "9,5\t9,5\tcomma decimals\n"
            "12.25\tone-sided\n");
        QStringList warnings;
        const auto labels = AudacityLabels::parse(text, &warnings);
        QCOMPARE(labels.size(), 5);
        QCOMPARE(labels[0].start, 2.15);
        QCOMPARE(labels[0].title, QStringLiteral("point label"));
        QCOMPARE(labels[1].start, 3.4);
        QCOMPARE(labels[1].end, 6.1);
        QCOMPARE(labels[1].title, QStringLiteral("region label with spaces"));
        QCOMPARE(labels[2].title, QString());
        QCOMPARE(labels[3].start, 9.5);
        QCOMPARE(labels[4].start, 12.25);
        QCOMPARE(labels[4].end, 12.25);
        QCOMPARE(labels[4].title, QStringLiteral("one-sided"));
        QCOMPARE(warnings.size(), 1);
        QVERIFY(warnings.first().startsWith(QStringLiteral("line 5")));
    }

    void audacityLabelsAreWritten()
    {
        const QString text = AudacityLabels::write({31, 12.4});
        QCOMPARE(text, QStringLiteral("12.400000\t12.400000\tMarker 1\n31.000000\t31.000000\tMarker 2\n"));
        const auto back = AudacityLabels::parse(text);
        QCOMPARE(back.size(), 2);
        QCOMPARE(back[1].start, 31.0);
    }

    void importedMarkersReplaceTheOldOnesInOneStep()
    {
        ProjectModel model;
        model.addMarker(1);
        QCOMPARE(model.replaceMarkers({30, 12.4, 12.41, 5}, QStringLiteral("Import markers")), 3);
        QCOMPARE(model.markers(), (QVariantList{5.0, 12.4, 30.0}));
        QCOMPARE(model.undoText(), QStringLiteral("Import markers"));
        model.undo();
        QCOMPARE(model.markers(), (QVariantList{1.0}));
    }

    void fitToMarkersPutsEachChangeOnItsMarker()
    {
        ProjectModel model;
        model.addImages({QStringLiteral("/a.jpg"), QStringLiteral("/b.jpg"), QStringLiteral("/c.jpg")});
        QCOMPARE(model.fitToMarkers(), 0); // no markers
        model.addMarker(10);
        model.addMarker(4); // order of setting doesn't matter
        QCOMPARE(model.fitToMarkers(), 2);

        // Crossfades of 1 s are centred on the markers.
        QCOMPARE(model.startOf(1), 3.5);
        QCOMPARE(model.startOf(2), 9.5);
        QCOMPARE(model.frameAt(4).value("mix").toDouble(), 0.5);
        QCOMPARE(model.frameAt(10).value("mix").toDouble(), 0.5);
        QCOMPARE(model.durationOf(2), 5.0); // no audio: the last keeps its duration

        // A cut changes exactly on the marker.
        model.setTransitionPreset(1, QStringLiteral("none"), 0);
        model.fitToMarkers();
        QCOMPARE(model.startOf(1), 4.0);

        model.undo();
        model.undo();
        QCOMPARE(model.startOf(1), 3.5);
        model.undo();
        QCOMPARE(model.startOf(1), 4.0); // before the first fit: 5 − 1
    }

    void modelFitsImagesToTheAudio()
    {
        if (!haveFfmpeg())
            QSKIP("ffmpeg/ffprobe er ikke installeret");
        QTemporaryDir dir;
        const QString audio = dir.filePath(QStringLiteral("a.wav"));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=duration=20", audio}));

        ProjectModel model;
        QVERIFY(!model.fitToAudio()); // nothing to fit yet
        model.addImages({QStringLiteral("/a.jpg"), QStringLiteral("/b.jpg"), QStringLiteral("/c.jpg"),
                         QStringLiteral("/d.jpg")});
        model.addAudio({audio});
        QVERIFY(qAbs(model.audioDuration() - 20) < 0.05);

        model.setDuration(2, 9);
        QCOMPARE(model.durationOverrideCount(), 1);
        QVERIFY(model.fitToAudio());
        QCOMPARE(model.durationOverrideCount(), 0);
        QCOMPARE(model.defaultDuration(), 5.75); // (20 + 3 × 1) / 4

        // Fades as the playback volume sees them: 2 s in, 4 s out, audio 20 s.
        model.setAudioFadeIn(2);
        model.setAudioFadeOut(4);
        QCOMPARE(model.audioGainAt(0), 0.0);
        QCOMPARE(model.audioGainAt(1), 0.5);
        QCOMPARE(model.audioGainAt(10), 1.0);
        QVERIFY(qAbs(model.audioGainAt(model.audioDuration() - 1) - 0.25) < 0.02);

        // Audio files can be reordered, and undone.
        const QString second = dir.filePath(QStringLiteral("b.wav"));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=duration=5", second}));
        model.addAudio({second});
        const double total = model.audioDuration();
        model.moveAudio(1, 0);
        QCOMPARE(model.audioPaths(), (QStringList{second, audio}));
        QCOMPARE(model.audioDuration(), total);
        QCOMPARE(model.audio().first().toMap().value("fileName").toString(), QStringLiteral("b.wav"));
        model.undo();
        QCOMPARE(model.audioPaths(), (QStringList{audio, second}));
        model.undo(); // the second file again
        QCOMPARE(model.audioPaths(), QStringList{audio});

        // With a marker for every change, the last image lasts until the audio ends.
        model.addMarker(3);
        model.addMarker(8);
        model.addMarker(14);
        QCOMPARE(model.fitToMarkers(), 3);
        QVERIFY(qAbs(model.videoDuration() - model.audioDuration()) < 0.002);
        QVERIFY(model.videoDuration() <= model.audioDuration() + 0.001);
        QVERIFY(model.audioDuration() - model.videoDuration() < 0.05);
    }

    void modelLoadsAndSavesWithRelativePaths()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir(QStringLiteral("img"));
        ProjectModel model;
        model.addImages({dir.filePath(QStringLiteral("img/01.jpg"))});
        model.setDuration(0, 3);

        const QString file = dir.filePath(QStringLiteral("show.json"));
        QString error;
        QVERIFY(model.project().withPathsRelativeTo(dir.path()).save(file, &error));

        const auto loaded = Project::load(file, &error);
        QVERIFY2(loaded, qPrintable(error));
        QCOMPARE(loaded->slides[0].path, QStringLiteral("img/01.jpg"));

        ProjectModel reopened;
        reopened.setProject(*loaded);
        QVERIFY(!reopened.isModified());
        QCOMPARE(reopened.data(reopened.index(0), ProjectModel::PathRole).toString(),
                 dir.filePath(QStringLiteral("img/01.jpg")));
        QCOMPARE(reopened.durationOf(0), 3.0);
    }

    void controllerSavesOpensAndExports()
    {
        if (!haveFfmpeg())
            QSKIP("ffmpeg/ffprobe er ikke installeret");
        QTemporaryDir dir;
        auto file = [&](const QString &name) { return dir.filePath(name); };
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "color=c=red:s=300x200", "-frames:v", "1", file("1.jpg")}));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "color=c=blue:s=300x200", "-frames:v", "1", file("2.jpg")}));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=duration=3", file("lyd.wav")}));

        Controller controller;
        QSignalSpy notices(&controller, &Controller::notice);
        // Dropped files are sorted into images and audio by extension.
        controller.addDroppedUrls({QUrl::fromLocalFile(file("2.jpg")), QUrl::fromLocalFile(file("lyd.wav")),
                                   QUrl::fromLocalFile(file("1.jpg")), QUrl::fromLocalFile(file("x.txt"))});
        ProjectModel *model = controller.project();
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->data(model->index(0), ProjectModel::FileNameRole).toString(), QStringLiteral("1.jpg"));
        QCOMPARE(model->audio().size(), 1);
        QVERIFY(notices.last().at(0).toString().contains(QStringLiteral("x.txt")));

        model->setResolution(320, 180);
        model->setFps(10);
        QVERIFY(model->fitToAudio());
        QVERIFY(controller.saveTo(file("show.json")));
        QVERIFY(!model->isModified());
        QCOMPARE(controller.projectName(), QStringLiteral("show"));

        Controller reopened;
        QVERIFY(reopened.openProject(file("show.json")));
        QCOMPARE(reopened.project()->rowCount(), 2);
        QCOMPARE(reopened.project()->outputWidth(), 320);

        // Markers out to an Audacity label file and back; a dropped .txt imports too.
        reopened.project()->addMarker(1.25);
        reopened.project()->addMarker(0.5);
        QVERIFY(reopened.exportMarkers(file("labels.txt")));
        QFile labels(file("labels.txt"));
        QVERIFY(labels.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(labels.readAll()),
                 QStringLiteral("0.500000\t0.500000\tMarker 1\n1.250000\t1.250000\tMarker 2\n"));
        reopened.project()->clearMarkers();
        QVERIFY(reopened.importMarkers(file("labels.txt")));
        QCOMPARE(reopened.project()->markers(), (QVariantList{0.5, 1.25}));
        reopened.project()->clearMarkers();
        reopened.addDroppedUrls({QUrl::fromLocalFile(file("labels.txt"))});
        QCOMPARE(reopened.project()->markers().size(), 2);
        QVERIFY(!reopened.importMarkers(file("show.json"))); // not labels

        reopened.project()->setExportQuality(QStringLiteral("high"));
        QSignalSpy exported(&reopened, &Controller::exportFinished);
        reopened.exportTo(file("ud.mp4"));
        QVERIFY(reopened.exporting());
        QVERIFY(exported.wait(60000));
        QVERIFY(!reopened.exporting());
        QVERIFY(QFileInfo::exists(file("ud.mp4")));
        QVERIFY(!QFileInfo::exists(file(".ud.part.mp4")));
        const MediaInfo info = Probe::inspect(file("ud.mp4"));
        QVERIFY(qAbs(info.duration - 3) < 0.15);
    }

    void frameAtFollowsTheTransitions()
    {
        ProjectModel model;
        model.addImages({QStringLiteral("/a.jpg"), QStringLiteral("/b.jpg"), QStringLiteral("/c.jpg")});
        // starts 0, 4, 8; each 5 s with a 1 s fade into b and c
        auto frame = [&](double t) { return model.frameAt(t); };
        QCOMPARE(frame(2).value("from").toInt(), 0);
        QCOMPARE(frame(2).value("to").toInt(), -1);
        QCOMPARE(frame(4.25).value("to").toInt(), 1);
        QCOMPARE(frame(4.25).value("mix").toDouble(), 0.25);
        QCOMPARE(frame(4.25).value("current").toInt(), 0);
        QCOMPARE(frame(4.75).value("current").toInt(), 1);
        QCOMPARE(frame(6).value("from").toInt(), 1);
        QCOMPARE(frame(100).value("from").toInt(), 2);
        QCOMPARE(model.visibleStartOf(1), 5.0);
        QCOMPARE(model.frameAt(model.visibleStartOf(2)).value("current").toInt(), 2);

        model.setTransition(1, QStringLiteral("cut"));
        QCOMPARE(frame(4.25).value("to").toInt(), -1);
    }

    void keyBindingsParse()
    {
        const auto parsed = KeyBindings::parse(QStringLiteral(
            "# a comment\n"
            "play_pause = Space K   # trailing comment\n"
            "seek_back =\n"
            "duration_scroll_step = 0,25\n"
            "frobnicate = F1\n"
            "nonsense\n"));
        QCOMPARE(parsed.keys.value(QStringLiteral("play_pause")), (QStringList{"Space", "K"}));
        QVERIFY(parsed.keys.contains(QStringLiteral("seek_back")));
        QVERIFY(parsed.keys.value(QStringLiteral("seek_back")).isEmpty()); // switched off
        QCOMPARE(parsed.settings.value(QStringLiteral("duration_scroll_step")), 0.25);
        QCOMPARE(parsed.warnings.size(), 2);

        // The file we write for new users must parse back to the defaults.
        const auto defaults = KeyBindings::parse(KeyBindings::defaultFileText());
        QVERIFY2(defaults.warnings.isEmpty(), qPrintable(defaults.warnings.join('\n')));
        for (const auto &action : KeyBindings::defaults())
            QCOMPARE(defaults.keys.value(action.id), action.keys);
    }

    void keyBindingsFileIsCreatedAndReloaded()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sub/keybindings.conf"));
        KeyBindings keys(path);
        QVERIFY(QFileInfo::exists(path));
        QCOMPARE(keys.keys().value(QStringLiteral("play_pause")).toStringList(), QStringList{"Space"});
        QCOMPARE(keys.scrollStep(), 0.1);

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("play_pause = P\nduration_step = 1\n");
        file.close();
        keys.reload();
        QCOMPARE(keys.keys().value(QStringLiteral("play_pause")).toStringList(), QStringList{"P"});
        QCOMPARE(keys.keys().value(QStringLiteral("seek_back")).toStringList(), QStringList{"Left"});
        QCOMPARE(keys.durationStep(), 1.0);
    }

    void audioPreviewJoinsFilesAndMeasuresPeaks()
    {
        if (!haveFfmpeg())
            QSKIP("ffmpeg/ffprobe er ikke installeret");
        QTemporaryDir dir;
        const QString a = dir.filePath(QStringLiteral("a.wav"));
        const QString b = dir.filePath(QStringLiteral("b.mp3"));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=frequency=440:duration=2", a}));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "anullsrc=r=22050:cl=mono", "-t", "1.5", b}));

        const auto result = AudioPreview::build({a, b}, dir.path());
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(QFileInfo::exists(result.file));
        QVERIFY(qAbs(result.duration - 3.5) < 0.1);
        QVERIFY(qAbs(result.peaks.size() - 3.5 * AudioPreview::PeaksPerSecond) < 40);
        // Loud sine first, silence after.
        QVERIFY(result.peaks.at(AudioPreview::PeaksPerSecond / 2) > 0.5);
        QVERIFY(result.peaks.at(3 * AudioPreview::PeaksPerSecond) < 0.01);
        for (float p : result.peaks)
            QVERIFY(p >= 0 && p <= 1);

        // Cached: the second build reuses the joined file.
        const QDateTime before = QFileInfo(result.file).lastModified();
        const auto again = AudioPreview::build({a, b}, dir.path());
        QCOMPARE(again.file, result.file);
        QCOMPARE(QFileInfo(again.file).lastModified(), before);
    }

    void rendersARealVideo()
    {
        if (!haveFfmpeg())
            QSKIP("ffmpeg/ffprobe er ikke installeret");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto file = [&](const QString &name) { return dir.filePath(name); };

        // Landscape, portrait and square, so the letterboxing gets exercised.
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "color=c=red:s=400x300", "-frames:v", "1", file("1.png")}));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "color=c=green:s=300x500", "-frames:v", "1", file("2.jpg")}));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "color=c=blue:s=256x256", "-frames:v", "1", file("3.png")}));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=frequency=440:duration=3", file("a.wav")}));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=frequency=660:duration=2.5", "-ac", "2", file("b.mp3")}));

        QFile projectFile(file("project.json"));
        QVERIFY(projectFile.open(QIODevice::WriteOnly));
        projectFile.write(R"({
            "output": {"width": 320, "height": 180, "fps": 10},
            "defaults": {"duration": 2, "transition": "fade", "transition_duration": 0.5},
            "images": ["1.png", {"path": "2.jpg", "transition": "cut"}, {"path": "3.png", "duration": 3}],
            "audio": ["a.wav", "b.mp3"]
        })");
        projectFile.close();

        QString error;
        const auto project = Project::load(file("project.json"), &error);
        QVERIFY2(project, qPrintable(error));

        const PreparedJob prepared = prepareJob(*project, file("out.mp4"), false);
        QVERIFY2(prepared.ok(), qPrintable(prepared.errors.join('\n')));
        QCOMPARE(prepared.job.plan.total, 6.5); // 2 + 2 + 3 − 0.5
        QVERIFY(qAbs(prepared.audioSeconds - 5.5) < 0.1);

        Renderer renderer;
        QSignalSpy finished(&renderer, &Renderer::finished);
        QSignalSpy progress(&renderer, &Renderer::progress);
        renderer.start(FfmpegCommand::arguments(prepared.job), prepared.job.plan.total);
        QVERIFY(finished.wait(60000));
        QVERIFY2(finished.first().at(0).toBool(), qPrintable(finished.first().at(1).toString()));
        QVERIFY(!progress.isEmpty());
        QCOMPARE(progress.last().at(0).toDouble(), 1.0);

        const MediaInfo out = Probe::inspect(file("out.mp4"));
        QVERIFY2(out.ok, qPrintable(out.error));
        QCOMPARE(out.width, 320);
        QCOMPARE(out.height, 180);
        QVERIFY(out.hasAudio);
        QVERIFY2(qAbs(out.duration - 6.5) < 0.15, qPrintable(QString::number(out.duration)));
    }
};

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
