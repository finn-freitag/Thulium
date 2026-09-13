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
    bool zoomIn = (event->button() == Qt::LeftButton);
    if (onZoom) {
        onZoom(zoomIn, docPos);
    }
}

} // namespace pdn
