#include "waveformitem.h"

#include <QPainter>

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
