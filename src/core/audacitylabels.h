// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Audacity's label file (File → Export Other → Export Labels, and Import
// Labels): one label per line, tab-separated
//
//   12.400000	12.400000	Interview starts     (a point label)
//   31.000000	35.500000	Silence              (a region label)
//
// Times are seconds with a '.' decimal point; the title may be missing.
// Lines starting with '\' carry spectral-selection frequencies for the label
// above and are skipped. Several label tracks are simply written one after
// the other. The format is the same in Audacity 3 and 4.
namespace AudacityLabels {

struct Label {
    double start = 0;
    double end = 0;
    QString title;
};

// Lines that can't be read are skipped and described in *warnings.
QList<Label> parse(const QString &text, QStringList *warnings = nullptr);

// Point labels titled "Marker 1", "Marker 2", … in time order.
QString write(QList<double> markers);

}
