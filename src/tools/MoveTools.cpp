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

void MoveSelectedPixelsTool::liftPixels(Document* doc) {
    if (m_hasFloating) return;
    auto layer = doc->activeLayer();
    if (!layer) return;

    m_undoSnapshot = layer->image().copy();

    QRectF bounds = doc->selection().boundingRect();
    if (bounds.isEmpty()) {
        bounds = QRectF(0, 0, doc->width(), doc->height());
    }
    m_originalSelectionBounds = bounds;
    QRect srcRect = bounds.toAlignedRect().intersected(QRect(0, 0, doc->width(), doc->height()));

    m_floatingImage = QImage(srcRect.size(), QImage::Format_ARGB32);
    m_floatingImage.fill(Qt::transparent);

    // Copy selected pixels to floating image
    QPainter pFloat(&m_floatingImage);
    pFloat.drawImage(-srcRect.x(), -srcRect.y(), layer->image());
    pFloat.end();

    // Mask floating image with selection
    if (!doc->selection().isEmpty()) {
        QPainter pMask(&m_floatingImage);
        pMask.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        pMask.translate(-srcRect.x(), -srcRect.y());
        pMask.fillPath(doc->selection().path(), Qt::black);
        pMask.end();
    }

    // Clear the selected area on the layer
    QPainter pLayer(&layer->image());
    pLayer.setCompositionMode(QPainter::CompositionMode_Clear);
    if (!doc->selection().isEmpty()) {
        pLayer.fillPath(doc->selection().path(), Qt::transparent);
    } else {
        pLayer.fillRect(srcRect, Qt::transparent);
    }
    pLayer.end();

    m_floatingOffset = srcRect.topLeft();
    m_hasFloating = true;
}

void MoveSelectedPixelsTool::commit(Document* doc) {
    if (!m_hasFloating || !doc) return;
    auto layer = doc->activeLayer();
    if (layer) {
        QPainter p(&layer->image());
        p.drawImage(m_floatingOffset, m_floatingImage);
        p.end();

        doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Move Pixels"));
    }

    m_hasFloating = false;
    m_floatingImage = QImage();
    emit doc->documentChanged();
}

void MoveSelectedPixelsTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_hasFloating) {
        liftPixels(doc);
    }
    m_lastPos = docPos;
    m_moving = true;
}

void MoveSelectedPixelsTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_moving) return;
    QPointF delta = docPos - m_lastPos;
    m_floatingOffset += delta;
    doc->selection().translate(delta.x(), delta.y());
    m_lastPos = docPos;

    emit doc->selectionChanged();
    emit doc->documentChanged();
}

void MoveSelectedPixelsTool::mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    m_moving = false;
}

void MoveSelectedPixelsTool::nudge(Document* doc, qreal dx, qreal dy) {
    if (!m_hasFloating) {
        liftPixels(doc);
    }
    m_floatingOffset += QPointF(dx, dy);
    doc->selection().translate(dx, dy);
    emit doc->selectionChanged();
    emit doc->documentChanged();
}

void MoveSelectedPixelsTool::keyPress(QKeyEvent* event, Document* doc, ToolContext& /*ctx*/) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_hasFloating) {
            commit(doc);
            event->accept();
        }
    }
}

void MoveSelectedPixelsTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_hasFloating || m_floatingImage.isNull()) return;

    QRectF targetRect(opts.panOffset.x() + m_floatingOffset.x() * opts.zoom,
                      opts.panOffset.y() + m_floatingOffset.y() * opts.zoom,
                      m_floatingImage.width() * opts.zoom,
                      m_floatingImage.height() * opts.zoom);

    painter.save();
    if (opts.zoom >= 4.0) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    } else {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    }
    painter.drawImage(targetRect, m_floatingImage);
    painter.restore();
}

} // namespace pdn
