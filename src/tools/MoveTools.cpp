#include "MoveTools.h"
#include "../core/History.h"
#include <QPainter>

namespace pdn {

// --- MoveSelectionTool ---
void MoveSelectionTool::mousePress(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& docPos, ToolContext& /*ctx*/) {
    m_lastPos = docPos;
    m_moving = true;
}

void MoveSelectionTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_moving) return;
    QPointF delta = docPos - m_lastPos;
    doc->selection().translate(delta.x(), delta.y());
    m_lastPos = docPos;
    emit doc->selectionChanged();
    emit doc->documentChanged();
}

void MoveSelectionTool::mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    m_moving = false;
}

// --- MoveSelectedPixelsTool ---
void MoveSelectedPixelsTool::activate(Document* /*doc*/, ToolContext& /*ctx*/) {
}

void MoveSelectedPixelsTool::deactivate(Document* doc, ToolContext& /*ctx*/) {
    commit(doc);
}

void MoveSelectedPixelsTool::commit(Document* doc) {
    if (!doc) return;
    if (doc->hasFloatingSelection()) {
        doc->bakeFloatingSelection();
    }
}

void MoveSelectedPixelsTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!doc) return;
    if (!doc->hasFloatingSelection()) {
        if (!doc->selection().isEmpty()) {
            doc->liftSelectionToFloating();
        } else {
            return;
        }
    }
    m_lastPos = docPos;
    m_moving = true;
}

void MoveSelectedPixelsTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_moving || !doc || !doc->hasFloatingSelection()) return;
    QPointF delta = docPos - m_lastPos;
    doc->moveFloatingSelection(delta);
    m_lastPos = docPos;
}

void MoveSelectedPixelsTool::mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    m_moving = false;
}

void MoveSelectedPixelsTool::nudge(Document* doc, qreal dx, qreal dy) {
    if (!doc) return;
    if (!doc->hasFloatingSelection()) {
        if (!doc->selection().isEmpty()) {
            doc->liftSelectionToFloating();
        } else {
            return;
        }
    }
    doc->moveFloatingSelection(QPointF(dx, dy));
}

void MoveSelectedPixelsTool::keyPress(QKeyEvent* event, Document* doc, ToolContext& /*ctx*/) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (doc && doc->hasFloatingSelection()) {
            commit(doc);
            event->accept();
        }
    }
}

void MoveSelectedPixelsTool::drawOverlay(QPainter& /*painter*/, const RenderOptions& /*opts*/) {
    // Document::compositeInto renders the floating selection directly into the document
    // underneath the marching ants selection border.
}

} // namespace pdn
