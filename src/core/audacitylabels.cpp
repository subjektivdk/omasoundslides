#include "audacitylabels.h"

#include <algorithm>

namespace AudacityLabels {

namespace {

// Audacity writes '.', but reads ',' too (files edited by hand or in a
// spreadsheet with a comma locale), so accept both.
bool readSeconds(QString text, double *out)
{
    bool ok = false;
    const double value = text.trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    if (!ok || value < 0)
        return false;
    *out = value;
    return true;
}

}

QList<Label> parse(const QString &text, QStringList *warnings)
{
    QList<Label> labels;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (qsizetype n = 0; n < lines.size(); ++n) {
        QString line = lines.at(n);
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (line.trimmed().isEmpty() || line.startsWith(QLatin1Char('\\')))
            continue;

        const QStringList fields = line.split(QLatin1Char('\t'));
        Label label;
        if (!readSeconds(fields.value(0), &label.start)) {
            if (warnings)
                *warnings << QStringLiteral("line %1: \"%2\" is not a time").arg(n + 1).arg(fields.value(0));
            continue;
        }
        // Like Audacity: if the second field isn't a number, it's the title of
        // a point label.
        int titleField = 2;
        if (!readSeconds(fields.value(1), &label.end)) {
            label.end = label.start;
            titleField = 1;
        }
        label.title = fields.value(titleField);
        labels.append(label);
    }
    return labels;
}

QString write(QList<double> markers)
{
    std::sort(markers.begin(), markers.end());
    QString text;
    for (qsizetype i = 0; i < markers.size(); ++i) {
        const QString t = QString::number(markers.at(i), 'f', 6);
        text += QStringLiteral("%1\t%1\tMarker %2\n").arg(t).arg(i + 1);
    }
    return text;
}

}
