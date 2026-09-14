#include "BrushTools.h"
#include "../core/History.h"
#include <QPainter>
#include <cmath>
#include <algorithm>

namespace pdn {

// --- PaintbrushTool ---
void PaintbrushTool::deactivate(Document* doc, ToolContext& ctx) {
    if (m_drawing && doc) {
        mouseRelease(nullptr, doc, m_lastPos, ctx);
    }
}

void PaintbrushTool::applyStrokeToLayer(Document* doc, const QRect& dirtyRect) {
    auto layer = doc->activeLayer();
    if (!layer || dirtyRect.isEmpty()) return;

    QRect clampedDirty = dirtyRect.intersected(layer->image().rect());
    if (clampedDirty.isEmpty()) return;

    bool hasSelection = !doc->selection().isEmpty();
    uint8_t opacity = static_cast<uint8_t>(m_activeColor.alpha());

    int yTop = clampedDirty.top();
    int yBottom = clampedDirty.bottom();
    int xLeft = clampedDirty.left();
    int xRight = clampedDirty.right();

    uint32_t A_user = opacity;
    uint32_t R_user = m_activeColor.red();
    uint32_t G_user = m_activeColor.green();
    uint32_t B_user = m_activeColor.blue();

    for (int y = yTop; y <= yBottom; ++y) {
        uint32_t* dstLine = layer->scanLine(y);
        const uint32_t* undoLine = reinterpret_cast<const uint32_t*>(m_undoSnapshot.constScanLine(y));
        const uint32_t* strokeLine = reinterpret_cast<const uint32_t*>(m_strokeImage.constScanLine(y));

        for (int x = xLeft; x <= xRight; ++x) {
            if (hasSelection && !doc->selection().containsPixel(x, y)) {
                continue;
            }

            uint32_t bg = undoLine[x];
            uint32_t stroke = strokeLine[x];

            if (m_compositionMode == ColorCompositionMode::DrawOver) {
                dstLine[x] = blendPixel(BlendMode::Normal, bg, stroke, opacity);
            } else { // ColorCompositionMode::Overwrite
                uint32_t C = (stroke >> 24) & 0xFF;
                if (C == 0) {
                    dstLine[x] = bg;
                } else {
                    uint32_t A_dst = (bg >> 24) & 0xFF;
                    uint32_t R_dst = (bg >> 16) & 0xFF;
                    uint32_t G_dst = (bg >> 8) & 0xFF;
                    uint32_t B_dst = bg & 0xFF;

                    uint32_t termDst = (255 - C) * A_dst;
                    uint32_t termUser = C * A_user;
                    uint32_t totalWeight = termDst + termUser;
                    uint32_t outA = (totalWeight + 127) / 255;

                    if (outA == 0) {
                        dstLine[x] = 0;
                    } else {
                        uint32_t outR = (termDst * R_dst + termUser * R_user + totalWeight / 2) / totalWeight;
                        uint32_t outG = (termDst * G_dst + termUser * G_user + totalWeight / 2) / totalWeight;
                        uint32_t outB = (termDst * B_dst + termUser * B_user + totalWeight / 2) / totalWeight;
                        dstLine[x] = (outA << 24) | ((outR & 0xFF) << 16) | ((outG & 0xFF) << 8) | (outB & 0xFF);
                    }
                }
            }
        }
    }
}

void PaintbrushTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_undoSnapshot = layer->image().copy();
    m_drawing = true;
    m_lastPos = docPos;
    m_activeColor = (event && event->button() == Qt::RightButton) ? ctx.secondaryColor : ctx.primaryColor;
    m_compositionMode = ctx.compositionMode;

    m_strokeImage = QImage(layer->image().size(), QImage::Format_ARGB32);
    m_strokeImage.fill(Qt::transparent);

    QColor strokeColor = m_activeColor;
    strokeColor.setAlpha(255);

    QPainter pStroke(&m_strokeImage);
    if (!doc->selection().isEmpty()) {
        pStroke.setClipPath(doc->selection().path());
    }
    pStroke.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);
    pStroke.setPen(QPen(strokeColor, ctx.brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    pStroke.drawPoint(docPos);
    pStroke.end();

    int pad = static_cast<int>(std::ceil(ctx.brushWidth / 2.0)) + 4;
    QRectF dirtyF(docPos, docPos);
    dirtyF = dirtyF.adjusted(-pad, -pad, pad, pad);
    applyStrokeToLayer(doc, dirtyF.toAlignedRect());

    emit doc->documentChanged();
}

void PaintbrushTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_drawing) return;
    auto layer = doc->activeLayer();
    if (!layer) return;

    QColor strokeColor = m_activeColor;
    strokeColor.setAlpha(255);

    QPainter pStroke(&m_strokeImage);
    if (!doc->selection().isEmpty()) {
        pStroke.setClipPath(doc->selection().path());
    }
    pStroke.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);
    pStroke.setPen(QPen(strokeColor, ctx.brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    pStroke.drawLine(m_lastPos, docPos);
    pStroke.end();

    int pad = static_cast<int>(std::ceil(ctx.brushWidth / 2.0)) + 4;
    QRectF dirtyF(m_lastPos, docPos);
    dirtyF = dirtyF.normalized().adjusted(-pad, -pad, pad, pad);
    applyStrokeToLayer(doc, dirtyF.toAlignedRect());

    m_lastPos = docPos;
    emit doc->documentChanged();
}

void PaintbrushTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    if (!m_drawing) return;
    m_drawing = false;
    doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Paintbrush"));
    m_strokeImage = QImage();
}

// --- PencilTool ---
void PencilTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_undoSnapshot = layer->image().copy();
    m_drawing = true;
    m_lastPos = docPos;
    m_activeColor = (event->button() == Qt::RightButton) ? ctx.secondaryColor : ctx.primaryColor;

    QPoint pt(static_cast<int>(std::floor(docPos.x())), static_cast<int>(std::floor(docPos.y())));

    QPainter p(&layer->image());
    if (!doc->selection().isEmpty()) {
        p.setClipPath(doc->selection().path());
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(m_activeColor, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.drawPoint(pt);
    p.end();

    emit doc->documentChanged();
}

void PencilTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_drawing) return;
    auto layer = doc->activeLayer();
    if (!layer) return;

    QPoint lastPt(static_cast<int>(std::floor(m_lastPos.x())), static_cast<int>(std::floor(m_lastPos.y())));
    QPoint curPt(static_cast<int>(std::floor(docPos.x())), static_cast<int>(std::floor(docPos.y())));

    if (lastPt != curPt) {
        QPainter p(&layer->image());
        if (!doc->selection().isEmpty()) {
            p.setClipPath(doc->selection().path());
        }
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(m_activeColor, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        p.drawLine(lastPt, curPt);
        p.end();

        emit doc->documentChanged();
    }

    m_lastPos = docPos;
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
