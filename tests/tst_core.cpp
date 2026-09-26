#include "core/ffmpegcommand.h"
#include "core/prepare.h"
#include "core/probe.h"
#include "core/project.h"
#include "core/renderer.h"
#include "core/timeline.h"
#include "core/transitions.h"

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
        QVERIFY(problems.first().startsWith(QStringLiteral("Billede 2")));
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

        QVERIFY(!Project::fromJson(
            QJsonDocument::fromJson(R"({"output": {"width": 1001}})").object(), &error));
        QVERIFY(!Project::fromJson(
            QJsonDocument::fromJson(R"({"images": [{"duration": 3}]})").object(), &error));
    }

    void jsonRoundTrip()
    {
        const Project p = projectFromJson(R"({
            "output": {"width": 1280, "height": 720, "fps": 25},
            "images": [{"path": "a.jpg", "duration": 2.5}, {"path": "b.jpg", "transition": "wipeleft"}],
            "audio": ["x.mp3"]
        })");
        QString error;
        const auto again = Project::fromJson(p.toJson(), &error);
        QVERIFY(again);
        QCOMPARE(again->output.width, 1280);
        QCOMPARE(again->output.fps, 25);
        QCOMPARE(again->slides.size(), 2);
        QCOMPARE(*again->slides[0].duration, 2.5);
        QVERIFY(!again->slides[1].duration);
        QCOMPARE(*again->slides[1].transition, QStringLiteral("wipeleft"));
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

    void singleImageNeedsNoTransitions()
    {
        RenderJob job;
        job.slides = {slide(4)};
        job.plan = Timeline::plan(job.slides);
        QVERIFY(FfmpegCommand::filterGraph(job).endsWith(QStringLiteral("[vout]")));
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
