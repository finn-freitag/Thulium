#pragma once

#include <QPointF>
#include <QPainter>
#include <cmath>
#include "../rendering/IRenderer.h"

namespace pdn {

// Interface for receiving canvas coordinate events (e.g. for center point picking)
class ICanvasPointReceiver {
public:
    virtual ~ICanvasPointReceiver() = default;
    virtual void onCanvasPointPicked(const QPointF& docPos) = 0;
};

// Interface for rendering interactive overlays on top of the document canvas
class ICanvasOverlayProvider {
public:
    virtual ~ICanvasOverlayProvider() = default;
    virtual void drawCanvasOverlay(QPainter& painter, const RenderOptions& opts) = 0;
};

// Bridge interface connecting Effects/Plugins with the MainWindow Canvas
class ICanvasInteractionBridge {
public:
    virtual ~ICanvasInteractionBridge() = default;

    virtual void setPointReceiver(ICanvasPointReceiver* receiver) = 0;
    virtual ICanvasPointReceiver* pointReceiver() const = 0;

    virtual void setOverlayProvider(ICanvasOverlayProvider* provider) = 0;
    virtual ICanvasOverlayProvider* overlayProvider() const = 0;

    virtual void requestCanvasRepaint() = 0;
};

// Helper function to draw an interactive crosshair and optional radius guide on the canvas
inline void drawCrosshairOverlay(QPainter& painter, const QPointF& docCenter, double zoom,
                                 const QPointF& panOffset, double radiusInDoc = 0.0) {
    QPointF viewCenter(panOffset.x() + docCenter.x() * zoom,
                       panOffset.y() + docCenter.y() * zoom);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    double r = 10.0;

    // Dark shadow outline for contrast on light canvas
    painter.setPen(QPen(QColor(0, 0, 0, 200), 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(viewCenter, r, r);
    painter.drawLine(QPointF(viewCenter.x() - 16, viewCenter.y()), QPointF(viewCenter.x() - 4, viewCenter.y()));
    painter.drawLine(QPointF(viewCenter.x() + 4, viewCenter.y()), QPointF(viewCenter.x() + 16, viewCenter.y()));
    painter.drawLine(QPointF(viewCenter.x(), viewCenter.y() - 16), QPointF(viewCenter.x(), viewCenter.y() - 4));
    painter.drawLine(QPointF(viewCenter.x(), viewCenter.y() + 4), QPointF(viewCenter.x(), viewCenter.y() + 16));

    // Crisp white foreground lines
    painter.setPen(QPen(QColor(255, 255, 255, 240), 1.5));
    painter.drawEllipse(viewCenter, r, r);
    painter.drawLine(QPointF(viewCenter.x() - 16, viewCenter.y()), QPointF(viewCenter.x() - 4, viewCenter.y()));
    painter.drawLine(QPointF(viewCenter.x() + 4, viewCenter.y()), QPointF(viewCenter.x() + 16, viewCenter.y()));
    painter.drawLine(QPointF(viewCenter.x(), viewCenter.y() - 16), QPointF(viewCenter.x(), viewCenter.y() - 4));
    painter.drawLine(QPointF(viewCenter.x(), viewCenter.y() + 4), QPointF(viewCenter.x(), viewCenter.y() + 16));

    // Center point
    painter.setBrush(Qt::white);
    painter.drawEllipse(viewCenter, 2.0, 2.0);

    // Optional radius boundary circle (e.g. for Twist or Vignette)
    if (radiusInDoc > 0.0) {
        double viewR = radiusInDoc * zoom;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(0, 0, 0, 160), 2.0, Qt::DashLine));
        painter.drawEllipse(viewCenter, viewR, viewR);
        painter.setPen(QPen(QColor(255, 255, 255, 220), 1.0, Qt::DashLine));
        painter.drawEllipse(viewCenter, viewR, viewR);
    }

    painter.restore();
}

} // namespace pdn
