// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>

#include "core/audiopreview.h"

// Draws the visible part of the audio preview's waveform. The item is as
// wide as the timeline viewport; viewStart and pixelsPerSecond say which
// seconds that is, so zooming never needs a huge texture.
class WaveformItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(AudioPreview *preview READ preview WRITE setPreview NOTIFY previewChanged)
    Q_PROPERTY(double viewStart READ viewStart WRITE setViewStart NOTIFY viewChanged)
    Q_PROPERTY(double pixelsPerSecond READ pixelsPerSecond WRITE setPixelsPerSecond NOTIFY viewChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
    explicit WaveformItem(QQuickItem *parent = nullptr);

    AudioPreview *preview() const { return m_preview; }
    void setPreview(AudioPreview *preview);
    double viewStart() const { return m_viewStart; }
    void setViewStart(double seconds);
    double pixelsPerSecond() const { return m_pixelsPerSecond; }
    void setPixelsPerSecond(double pps);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);

    void paint(QPainter *painter) override;

Q_SIGNALS:
    void previewChanged();
    void viewChanged();
    void colorChanged();

private:
    QPointer<AudioPreview> m_preview;
    QMetaObject::Connection m_previewConnection;
    double m_viewStart = 0;
    double m_pixelsPerSecond = 10;
    QColor m_color = Qt::white;
};
