#include "ViewTools.h"

namespace pdn {

// --- PanTool ---
void PanTool::mousePress(QMouseEvent* event, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    m_lastScreenPos = event->pos();
    m_panning = true;
}

void PanTool::mouseMove(QMouseEvent* event, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    if (!m_panning) return;
    QPoint delta = event->pos() - m_lastScreenPos;
    m_lastScreenPos = event->pos();
    if (onPan) {
        onPan(QPointF(delta.x(), delta.y()));
    }
}

void PanTool::mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    m_panning = false;
}

// --- ZoomTool ---
void ZoomTool::mousePress(QMouseEvent* event, Document* /*doc*/, const QPointF& docPos, ToolContext& /*ctx*/) {
    m_button = event->button();
    m_pressScreenPos = event->pos();
    m_startPos = docPos;
    m_currentPos = docPos;
    m_pressed = true;
    m_dragged = false;
}

void ZoomTool::mouseMove(QMouseEvent* event, Document* /*doc*/, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_pressed) return;
    m_currentPos = docPos;
    if ((event->pos() - m_pressScreenPos).manhattanLength() > 4) {
        m_dragged = true;
    }
}

void ZoomTool::mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_pressed) return;
    m_pressed = false;
    m_currentPos = docPos;
    if (m_dragged && m_button == Qt::LeftButton) {
        if (onZoomToRect) onZoomToRect(QRectF(m_startPos, m_currentPos).normalized());
    } else if (onZoom) {
        onZoom(m_button == Qt::LeftButton, m_startPos);
    }
    m_dragged = false;
}

void ZoomTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_pressed || !m_dragged || m_button != Qt::LeftButton) return;
    const QRectF vpRect = QRectF(opts.panOffset + m_startPos * opts.zoom,
                                 opts.panOffset + m_currentPos * opts.zoom).normalized();
    painter.save();
    painter.setPen(QPen(QColor(0, 120, 215), 1.0));
    painter.setBrush(QColor(0, 120, 215, 40));
    painter.drawRect(vpRect);
    painter.restore();
}

} // namespace pdn
