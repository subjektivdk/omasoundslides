// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#include "waveformitem.h"

#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

WaveformItem::WaveformItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(false);
}

void WaveformItem::setPreview(AudioPreview *preview)
{
    if (m_preview == preview)
        return;
    disconnect(m_previewConnection);
    m_preview = preview;
    if (m_preview)
        m_previewConnection = connect(m_preview, &AudioPreview::changed, this, [this] { update(); });
    Q_EMIT previewChanged();
    update();
}

void WaveformItem::setViewStart(double seconds)
{
    if (qFuzzyCompare(m_viewStart, seconds))
        return;
    m_viewStart = seconds;
    Q_EMIT viewChanged();
    update();
}

void WaveformItem::setPixelsPerSecond(double pps)
{
    if (pps <= 0 || qFuzzyCompare(m_pixelsPerSecond, pps))
        return;
    m_pixelsPerSecond = pps;
    Q_EMIT viewChanged();
    update();
}

void WaveformItem::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    Q_EMIT colorChanged();
    update();
}

void WaveformItem::paint(QPainter *painter)
{
    if (!m_preview || m_preview->peaks().isEmpty())
        return;

    const QVector<float> &peaks = m_preview->peaks();
    const double perSecond = AudioPreview::PeaksPerSecond;
    const int w = int(width());
    const double mid = height() / 2.0;
    const double half = height() / 2.0 - 1;

    // A faint centre line, so silence still shows where the audio is.
    QColor line = m_color;
    line.setAlphaF(0.35);
    const double audioEndX = (peaks.size() / perSecond - m_viewStart) * m_pixelsPerSecond;
    painter->setPen(QPen(line, 1));
    painter->drawLine(QPointF(std::max(0.0, -m_viewStart * m_pixelsPerSecond), mid),
                      QPointF(std::min<double>(w, audioEndX), mid));

    // Zoomed in (fewer than one peak per pixel): a filled outline through the
    // peaks, like Kdenlive, instead of a staircase of repeated lines.
    if (m_pixelsPerSecond >= perSecond) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        const qsizetype first = std::max<qsizetype>(0, qsizetype(std::floor(m_viewStart * perSecond)) - 1);
        const qsizetype last = std::min<qsizetype>(peaks.size() - 1,
                                                   qsizetype(std::ceil((m_viewStart + w / m_pixelsPerSecond) * perSecond)) + 1);
        if (first > last)
            return;
        QPolygonF outline;
        for (qsizetype i = first; i <= last; ++i)
            outline << QPointF((i / perSecond - m_viewStart) * m_pixelsPerSecond, mid - std::max(0.5, peaks.at(i) * half));
        for (qsizetype i = last; i >= first; --i)
            outline << QPointF((i / perSecond - m_viewStart) * m_pixelsPerSecond, mid + std::max(0.5, peaks.at(i) * half));
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_color);
        painter->drawPolygon(outline);
        return;
    }

    painter->setPen(QPen(m_color, 1));
    for (int x = 0; x < w; ++x) {
        const double t0 = m_viewStart + x / m_pixelsPerSecond;
        const double t1 = m_viewStart + (x + 1) / m_pixelsPerSecond;
        const qsizetype a = qsizetype(std::floor(t0 * perSecond));
        const qsizetype b = std::max(a + 1, qsizetype(std::ceil(t1 * perSecond)));
        if (a >= peaks.size())
            break;
        if (b <= 0)
            continue;
        float peak = 0;
        for (qsizetype i = std::max<qsizetype>(0, a); i < std::min(b, peaks.size()); ++i)
            peak = std::max(peak, peaks.at(i));
        const double h = std::max(0.5, peak * half);
        painter->drawLine(QPointF(x + 0.5, mid - h), QPointF(x + 0.5, mid + h));
    }
}
