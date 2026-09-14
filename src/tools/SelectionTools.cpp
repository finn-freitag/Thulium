#include "SelectionTools.h"
#include "../core/History.h"
#include <QPen>
#include <QQueue>
#include <cmath>
#include <vector>
#include <algorithm>

namespace pdn {

// --- RectangleSelectTool ---
void RectangleSelectTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (doc && doc->hasFloatingSelection()) {
        doc->bakeFloatingSelection();
    }
    m_startPos = docPos;
    m_currentPos = docPos;
    m_selecting = true;
}

void RectangleSelectTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_selecting) return;
    m_currentPos = docPos;
    emit doc->documentChanged();
}

void RectangleSelectTool::mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_selecting) return;
    m_selecting = false;
    m_currentPos = docPos;

    bool shiftHeld = event && (event->modifiers() & Qt::ShiftModifier);
    SelectionCombineMode effectiveMode = shiftHeld ? SelectionCombineMode::Union : ctx.selectionCombineMode;

    QRectF dragRect(m_startPos, m_currentPos);
    dragRect = dragRect.normalized();
    QRegion oldRegion = doc->selection().region();

    if (dragRect.width() > 2 || dragRect.height() > 2) {
        int x0 = static_cast<int>(std::floor(m_startPos.x()));
        int y0 = static_cast<int>(std::floor(m_startPos.y()));
        int x1 = static_cast<int>(std::floor(m_currentPos.x()));
        int y1 = static_cast<int>(std::floor(m_currentPos.y()));
        int left = std::min(x0, x1);
        int top = std::min(y0, y1);
        int right = std::max(x0, x1) + 1;
        int bottom = std::max(y0, y1) + 1;
        QRect pixelRect(left, top, right - left, bottom - top);

        doc->selection().addRect(pixelRect, effectiveMode);
        QRegion newRegion = doc->selection().region();
        if (oldRegion != newRegion) {
            doc->undoStack()->push(new SelectionUndoCommand(doc, oldRegion, newRegion, "Rectangle Select"));
        }
        emit doc->selectionChanged();
    } else {
        if (effectiveMode == SelectionCombineMode::Replace) {
            if (!doc->selection().isEmpty()) {
                doc->clearSelection();
                doc->undoStack()->push(new SelectionUndoCommand(doc, oldRegion, QRegion(), "Deselect"));
            }
        }
    }
    emit doc->documentChanged();
}

void RectangleSelectTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_selecting) return;
    int x0 = static_cast<int>(std::floor(m_startPos.x()));
    int y0 = static_cast<int>(std::floor(m_startPos.y()));
    int x1 = static_cast<int>(std::floor(m_currentPos.x()));
    int y1 = static_cast<int>(std::floor(m_currentPos.y()));
    int left = std::min(x0, x1);
    int top = std::min(y0, y1);
    int right = std::max(x0, x1) + 1;
    int bottom = std::max(y0, y1) + 1;

    QRectF vpRect(opts.panOffset.x() + left * opts.zoom,
                  opts.panOffset.y() + top * opts.zoom,
                  (right - left) * opts.zoom,
                  (bottom - top) * opts.zoom);

    painter.save();
    painter.setPen(QPen(QColor(0, 120, 215), 1.0, Qt::DashLine));
    painter.setBrush(QColor(0, 120, 215, 40));
    painter.drawRect(vpRect);
    painter.restore();
}

// --- EllipseSelectTool ---
void EllipseSelectTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (doc && doc->hasFloatingSelection()) {
        doc->bakeFloatingSelection();
    }
    m_startPos = docPos;
    m_currentPos = docPos;
    m_selecting = true;
}

void EllipseSelectTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_selecting) return;
    m_currentPos = docPos;
    emit doc->documentChanged();
}

void EllipseSelectTool::mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_selecting) return;
    m_selecting = false;
    m_currentPos = docPos;

    bool shiftHeld = event && (event->modifiers() & Qt::ShiftModifier);
    SelectionCombineMode effectiveMode = shiftHeld ? SelectionCombineMode::Union : ctx.selectionCombineMode;

    QRectF dragRect(m_startPos, m_currentPos);
    dragRect = dragRect.normalized();
    QRegion oldRegion = doc->selection().region();

    if (dragRect.width() > 2 || dragRect.height() > 2) {
        int x0 = static_cast<int>(std::floor(m_startPos.x()));
        int y0 = static_cast<int>(std::floor(m_startPos.y()));
        int x1 = static_cast<int>(std::floor(m_currentPos.x()));
        int y1 = static_cast<int>(std::floor(m_currentPos.y()));
        int left = std::min(x0, x1);
        int top = std::min(y0, y1);
        int right = std::max(x0, x1) + 1;
        int bottom = std::max(y0, y1) + 1;
        QRect pixelRect(left, top, right - left, bottom - top);

        doc->selection().addEllipse(pixelRect, effectiveMode);
        QRegion newRegion = doc->selection().region();
        if (oldRegion != newRegion) {
            doc->undoStack()->push(new SelectionUndoCommand(doc, oldRegion, newRegion, "Ellipse Select"));
        }
        emit doc->selectionChanged();
    } else {
        if (effectiveMode == SelectionCombineMode::Replace) {
            if (!doc->selection().isEmpty()) {
                doc->clearSelection();
                doc->undoStack()->push(new SelectionUndoCommand(doc, oldRegion, QRegion(), "Deselect"));
            }
        }
    }
    emit doc->documentChanged();
}

void EllipseSelectTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_selecting) return;
    int x0 = static_cast<int>(std::floor(m_startPos.x()));
    int y0 = static_cast<int>(std::floor(m_startPos.y()));
    int x1 = static_cast<int>(std::floor(m_currentPos.x()));
    int y1 = static_cast<int>(std::floor(m_currentPos.y()));
    int left = std::min(x0, x1);
    int top = std::min(y0, y1);
    int right = std::max(x0, x1) + 1;
    int bottom = std::max(y0, y1) + 1;

    QRectF vpRect(opts.panOffset.x() + left * opts.zoom,
                  opts.panOffset.y() + top * opts.zoom,
                  (right - left) * opts.zoom,
                  (bottom - top) * opts.zoom);

    painter.save();
    painter.setPen(QPen(QColor(0, 120, 215), 1.0, Qt::DashLine));
    painter.setBrush(QColor(0, 120, 215, 40));
    painter.drawEllipse(vpRect);
    painter.restore();
}

// --- LassoSelectTool ---
void LassoSelectTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (doc && doc->hasFloatingSelection()) {
        doc->bakeFloatingSelection();
    }
    m_polygon.clear();
    m_polygon << docPos;
    m_selecting = true;
}

void LassoSelectTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_selecting) return;
    m_polygon << docPos;
    emit doc->documentChanged();
}

void LassoSelectTool::mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_selecting) return;
    m_selecting = false;
    m_polygon << docPos;

    bool shiftHeld = event && (event->modifiers() & Qt::ShiftModifier);
    SelectionCombineMode effectiveMode = shiftHeld ? SelectionCombineMode::Union : ctx.selectionCombineMode;

    QRectF bounds = m_polygon.boundingRect();
    QRegion oldRegion = doc->selection().region();

    if (m_polygon.size() >= 3 && (bounds.width() > 2 || bounds.height() > 2)) {
        doc->selection().addPolygon(m_polygon, effectiveMode);
        QRegion newRegion = doc->selection().region();
        if (oldRegion != newRegion) {
            doc->undoStack()->push(new SelectionUndoCommand(doc, oldRegion, newRegion, "Lasso Select"));
        }
        emit doc->selectionChanged();
    } else {
        if (effectiveMode == SelectionCombineMode::Replace) {
            if (!doc->selection().isEmpty()) {
                doc->clearSelection();
                doc->undoStack()->push(new SelectionUndoCommand(doc, oldRegion, QRegion(), "Deselect"));
            }
        }
    }
    m_polygon.clear();
    emit doc->documentChanged();
}

void LassoSelectTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_selecting || m_polygon.isEmpty()) return;

    QPolygonF vpPoly;
    for (const auto& pt : m_polygon) {
        vpPoly << QPointF(opts.panOffset.x() + pt.x() * opts.zoom,
                          opts.panOffset.y() + pt.y() * opts.zoom);
    }

    painter.save();
    painter.setPen(QPen(QColor(0, 120, 215), 1.0, Qt::DashLine));
    painter.setBrush(QColor(0, 120, 215, 40));
    painter.drawPolygon(vpPoly);
    painter.restore();
}

// --- MagicWandTool ---
void MagicWandTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (doc && doc->hasFloatingSelection()) {
        doc->bakeFloatingSelection();
    }
    bool shiftHeld = event && (event->modifiers() & Qt::ShiftModifier);
    SelectionCombineMode effectiveMode = shiftHeld ? SelectionCombineMode::Union : ctx.selectionCombineMode;

    int x = static_cast<int>(docPos.x());
    int y = static_cast<int>(docPos.y());
    if (x < 0 || x >= doc->width() || y < 0 || y >= doc->height()) {
        if (effectiveMode == SelectionCombineMode::Replace) {
            if (!doc->selection().isEmpty()) {
                QRegion oldRegion = doc->selection().region();
                doc->clearSelection();
                doc->undoStack()->push(new SelectionUndoCommand(doc, oldRegion, QRegion(), "Deselect"));
            }
        }
        return;
    }

    floodSelect(doc, x, y, ctx.tolerance, effectiveMode);
}

void MagicWandTool::floodSelect(Document* doc, int startX, int startY, int tolerance, SelectionCombineMode mode) {
    auto layer = doc->activeLayer();
    if (!layer) return;

    int w = doc->width();
    int h = doc->height();

    const uint32_t* bits = layer->bits();
    uint32_t targetColor = bits[startY * w + startX];
    int tr = (targetColor >> 16) & 0xFF;
    int tg = (targetColor >> 8) & 0xFF;
    int tb = targetColor & 0xFF;
    int ta = (targetColor >> 24) & 0xFF;

    double maxDiff = (tolerance / 100.0) * std::sqrt(255.0 * 255.0 * 4.0);

    auto matches = [&](int x, int y) -> bool {
        uint32_t px = bits[y * w + x];
        if (px == targetColor) return true;
        if (tolerance == 0) return false;
        int pr = (px >> 16) & 0xFF;
        int pg = (px >> 8) & 0xFF;
        int pb = px & 0xFF;
        int pa = (px >> 24) & 0xFF;

        double d = std::sqrt(std::pow(pr - tr, 2) + std::pow(pg - tg, 2) +
                             std::pow(pb - tb, 2) + std::pow(pa - ta, 2));
        return d <= maxDiff;
    };

    std::vector<bool> visited(w * h, false);
    std::vector<QPoint> queue;
    queue.reserve(1024);
    queue.push_back(QPoint(startX, startY));
    visited[startY * w + startX] = true;

    size_t head = 0;
    const int dx[] = { 0, 0, -1, 1 };
    const int dy[] = { -1, 1, 0, 0 };

    while (head < queue.size()) {
        QPoint pt = queue[head++];
        int cx = pt.x();
        int cy = pt.y();

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int idx = ny * w + nx;
                if (!visited[idx] && matches(nx, ny)) {
                    visited[idx] = true;
                    queue.push_back(QPoint(nx, ny));
                }
            }
        }
    }

    // Build strictly sorted non-overlapping horizontal spans row-by-row
    QVector<QRect> rects;
    for (int y = 0; y < h; ++y) {
        int start = -1;
        int rowOffset = y * w;
        for (int x = 0; x < w; ++x) {
            if (visited[rowOffset + x]) {
                if (start == -1) start = x;
            } else {
                if (start != -1) {
                    rects.append(QRect(start, y, x - start, 1));
                    start = -1;
                }
            }
        }
        if (start != -1) {
            rects.append(QRect(start, y, w - start, 1));
        }
    }

    QRegion matchingRegion;
    if (!rects.isEmpty()) {
        matchingRegion.setRects(rects.data(), rects.size());
    }

    QRegion oldRegion = doc->selection().region();
    doc->selection().addRegion(matchingRegion, mode);
    QRegion newRegion = doc->selection().region();
    if (oldRegion != newRegion) {
        doc->undoStack()->push(new SelectionUndoCommand(doc, oldRegion, newRegion, "Magic Wand"));
    }
    emit doc->selectionChanged();
    emit doc->documentChanged();
}

} // namespace pdn
