#include "BrushTools.h"
#include "../core/History.h"
#include <QPainter>
#include <cmath>
#include <algorithm>

namespace pdn {

// --- PaintbrushTool ---
void PaintbrushTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_undoSnapshot = layer->image().copy();
    m_drawing = true;
    m_lastPos = docPos;
    m_activeColor = (event->button() == Qt::RightButton) ? ctx.secondaryColor : ctx.primaryColor;

    QPainter p(&layer->image());
    if (!doc->selection().isEmpty()) {
        p.setClipPath(doc->selection().path());
    }
    p.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);
    p.setPen(QPen(m_activeColor, ctx.brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPoint(docPos);
    p.end();

    emit doc->documentChanged();
}

void PaintbrushTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_drawing) return;
    auto layer = doc->activeLayer();
    if (!layer) return;

    QPainter p(&layer->image());
    if (!doc->selection().isEmpty()) {
        p.setClipPath(doc->selection().path());
    }
    p.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);
    p.setPen(QPen(m_activeColor, ctx.brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(m_lastPos, docPos);
    p.end();

    m_lastPos = docPos;
    emit doc->documentChanged();
}

void PaintbrushTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    if (!m_drawing) return;
    m_drawing = false;
    doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Paintbrush"));
}

// --- PencilTool ---
void PencilTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_undoSnapshot = layer->image().copy();
    m_drawing = true;
    m_lastPos = docPos;
    m_activeColor = (event->button() == Qt::RightButton) ? ctx.secondaryColor : ctx.primaryColor;

    QPainter p(&layer->image());
    if (!doc->selection().isEmpty()) {
        p.setClipPath(doc->selection().path());
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(m_activeColor, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.drawPoint(docPos.toPoint());
    p.end();

    emit doc->documentChanged();
}

void PencilTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_drawing) return;
    auto layer = doc->activeLayer();
    if (!layer) return;

    QPainter p(&layer->image());
    if (!doc->selection().isEmpty()) {
        p.setClipPath(doc->selection().path());
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(m_activeColor, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.drawLine(m_lastPos.toPoint(), docPos.toPoint());
    p.end();

    m_lastPos = docPos;
    emit doc->documentChanged();
}

void PencilTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    if (!m_drawing) return;
    m_drawing = false;
    doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Pencil"));
}

// --- EraserTool ---
void EraserTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_undoSnapshot = layer->image().copy();
    m_drawing = true;
    m_lastPos = docPos;

    QPainter p(&layer->image());
    if (!doc->selection().isEmpty()) {
        p.setClipPath(doc->selection().path());
    }
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);
    p.setPen(QPen(Qt::transparent, ctx.brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPoint(docPos);
    p.end();

    emit doc->documentChanged();
}

void EraserTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_drawing) return;
    auto layer = doc->activeLayer();
    if (!layer) return;

    QPainter p(&layer->image());
    if (!doc->selection().isEmpty()) {
        p.setClipPath(doc->selection().path());
    }
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);
    p.setPen(QPen(Qt::transparent, ctx.brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(m_lastPos, docPos);
    p.end();

    m_lastPos = docPos;
    emit doc->documentChanged();
}

void EraserTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    if (!m_drawing) return;
    m_drawing = false;
    doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Eraser"));
}

// --- ColorPickerTool ---
void ColorPickerTool::sample(Document* doc, const QPointF& docPos, ToolContext& ctx, bool isRightButton) {
    int x = static_cast<int>(docPos.x());
    int y = static_cast<int>(docPos.y());
    if (x < 0 || x >= doc->width() || y < 0 || y >= doc->height()) return;

    QImage composite = doc->composite();
    QColor c = composite.pixelColor(x, y);

    bool targetIsPrimary = isRightButton ? !ctx.activeColorIsPrimary : ctx.activeColorIsPrimary;
    if (targetIsPrimary) {
        ctx.primaryColor = c;
    } else {
        ctx.secondaryColor = c;
    }
    ctx.notifyChanged();
    emit doc->documentChanged();
}

void ColorPickerTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    sample(doc, docPos, ctx, event->button() == Qt::RightButton);
}

void ColorPickerTool::mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (event->buttons() & (Qt::LeftButton | Qt::RightButton)) {
        sample(doc, docPos, ctx, event->buttons() & Qt::RightButton);
    }
}

void ColorPickerTool::mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
}

// --- CloneStampTool ---
void CloneStampTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (event->modifiers() & Qt::ControlModifier) {
        ctx.cloneSource = docPos;
        ctx.cloneSourceSet = true;
        return;
    }

    if (!ctx.cloneSourceSet) return;

    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_undoSnapshot = layer->image().copy();
    m_stamping = true;
    m_sourceOffset = ctx.cloneSource - docPos;
    m_lastPos = docPos;

    mouseMove(event, doc, docPos, ctx);
}

void CloneStampTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_stamping) return;
    auto layer = doc->activeLayer();
    if (!layer) return;

    QPointF srcPos = docPos + m_sourceOffset;
    int radius = ctx.brushWidth / 2;
    if (radius < 1) radius = 1;

    QRect srcRect(static_cast<int>(srcPos.x() - radius), static_cast<int>(srcPos.y() - radius), radius * 2, radius * 2);
    QRect dstRect(static_cast<int>(docPos.x() - radius), static_cast<int>(docPos.y() - radius), radius * 2, radius * 2);

    QPainter p(&layer->image());
    if (!doc->selection().isEmpty()) {
        p.setClipPath(doc->selection().path());
    }
    p.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);
    p.drawImage(dstRect, m_undoSnapshot, srcRect);
    p.end();

    m_lastPos = docPos;
    emit doc->documentChanged();
}

void CloneStampTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    if (!m_stamping) return;
    m_stamping = false;
    doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Clone Stamp"));
}

// --- RecolorTool ---
void RecolorTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_undoSnapshot = layer->image().copy();
    m_recoloring = true;
    applyRecolor(doc, docPos, ctx);
}

void RecolorTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_recoloring) return;
    applyRecolor(doc, docPos, ctx);
}

void RecolorTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    if (!m_recoloring) return;
    m_recoloring = false;
    doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Recolor"));
}

void RecolorTool::applyRecolor(Document* doc, const QPointF& docPos, ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer) return;

    int radius = ctx.brushWidth / 2;
    if (radius < 1) radius = 1;
    int rSquared = radius * radius;

    int cx = static_cast<int>(docPos.x());
    int cy = static_cast<int>(docPos.y());

    int minX = std::max(0, cx - radius);
    int maxX = std::min(doc->width() - 1, cx + radius);
    int minY = std::max(0, cy - radius);
    int maxY = std::min(doc->height() - 1, cy + radius);

    QColor targetColor = ctx.secondaryColor;
    QColor replacementColor = ctx.primaryColor;
    double tol = (ctx.tolerance / 100.0) * 255.0;

    for (int y = minY; y <= maxY; ++y) {
        uint32_t* line = layer->scanLine(y);
        for (int x = minX; x <= maxX; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= rSquared) {
                if (!doc->selection().isEmpty() && !doc->selection().containsPixel(x, y)) {
                    continue;
                }
                uint32_t px = line[x];
                int pr = (px >> 16) & 0xFF;
                int pg = (px >> 8) & 0xFF;
                int pb = px & 0xFF;

                double diff = std::sqrt(std::pow(pr - targetColor.red(), 2) +
                                        std::pow(pg - targetColor.green(), 2) +
                                        std::pow(pb - targetColor.blue(), 2));
                if (diff <= tol) {
                    uint32_t alpha = (px >> 24) & 0xFF;
                    line[x] = (alpha << 24) |
                              (static_cast<uint32_t>(replacementColor.red()) << 16) |
                              (static_cast<uint32_t>(replacementColor.green()) << 8) |
                              static_cast<uint32_t>(replacementColor.blue());
                }
            }
        }
    }
    emit doc->documentChanged();
}

} // namespace pdn
