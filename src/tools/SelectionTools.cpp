#include "SelectionTools.h"
#include <QPen>
#include <QQueue>
#include <cmath>
#include <vector>

namespace pdn {

// --- RectangleSelectTool ---
void RectangleSelectTool::mousePress(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& docPos, ToolContext& /*ctx*/) {
    m_startPos = docPos;
    m_currentPos = docPos;
    m_selecting = true;
}

void RectangleSelectTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_selecting) return;
    m_currentPos = docPos;
    emit doc->documentChanged();
}

void RectangleSelectTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_selecting) return;
    m_selecting = false;
    m_currentPos = docPos;

    QRectF rect(m_startPos, m_currentPos);
    rect = rect.normalized();
    if (rect.width() > 0 && rect.height() > 0) {
        doc->selection().addRect(rect, ctx.selectionCombineMode);
        emit doc->selectionChanged();
    }
    emit doc->documentChanged();
}

void RectangleSelectTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_selecting) return;
    QRectF docRect(m_startPos, m_currentPos);
    docRect = docRect.normalized();

    QRectF vpRect(opts.panOffset.x() + docRect.x() * opts.zoom,
                  opts.panOffset.y() + docRect.y() * opts.zoom,
                  docRect.width() * opts.zoom,
                  docRect.height() * opts.zoom);

    painter.save();
    painter.setPen(QPen(QColor(0, 120, 215), 1.0, Qt::DashLine));
    painter.setBrush(QColor(0, 120, 215, 40));
    painter.drawRect(vpRect);
    painter.restore();
}

// --- EllipseSelectTool ---
void EllipseSelectTool::mousePress(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& docPos, ToolContext& /*ctx*/) {
    m_startPos = docPos;
    m_currentPos = docPos;
    m_selecting = true;
}

void EllipseSelectTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_selecting) return;
    m_currentPos = docPos;
    emit doc->documentChanged();
}

void EllipseSelectTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_selecting) return;
    m_selecting = false;
    m_currentPos = docPos;

    QRectF rect(m_startPos, m_currentPos);
    rect = rect.normalized();
    if (rect.width() > 0 && rect.height() > 0) {
        doc->selection().addEllipse(rect, ctx.selectionCombineMode);
        emit doc->selectionChanged();
    }
    emit doc->documentChanged();
}

void EllipseSelectTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_selecting) return;
    QRectF docRect(m_startPos, m_currentPos);
    docRect = docRect.normalized();

    QRectF vpRect(opts.panOffset.x() + docRect.x() * opts.zoom,
                  opts.panOffset.y() + docRect.y() * opts.zoom,
                  docRect.width() * opts.zoom,
                  docRect.height() * opts.zoom);

    painter.save();
    painter.setPen(QPen(QColor(0, 120, 215), 1.0, Qt::DashLine));
    painter.setBrush(QColor(0, 120, 215, 40));
    painter.drawEllipse(vpRect);
    painter.restore();
}

// --- LassoSelectTool ---
void LassoSelectTool::mousePress(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& docPos, ToolContext& /*ctx*/) {
    m_polygon.clear();
    m_polygon << docPos;
    m_selecting = true;
}

void LassoSelectTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_selecting) return;
    m_polygon << docPos;
    emit doc->documentChanged();
}

void LassoSelectTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_selecting) return;
    m_selecting = false;
    m_polygon << docPos;

    if (m_polygon.size() >= 3) {
        doc->selection().addPolygon(m_polygon, ctx.selectionCombineMode);
        emit doc->selectionChanged();
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
void MagicWandTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    int x = static_cast<int>(docPos.x());
    int y = static_cast<int>(docPos.y());
    if (x < 0 || x >= doc->width() || y < 0 || y >= doc->height()) return;

    floodSelect(doc, x, y, ctx.tolerance, ctx.selectionCombineMode);
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
        int pr = (px >> 16) & 0xFF;
        int pg = (px >> 8) & 0xFF;
        int pb = px & 0xFF;
        int pa = (px >> 24) & 0xFF;

        double d = std::sqrt(std::pow(pr - tr, 2) + std::pow(pg - tg, 2) +
                             std::pow(pb - tb, 2) + std::pow(pa - ta, 2));
        return d <= maxDiff;
    };

    std::vector<bool> visited(w * h, false);
    QPainterPath wandPath;

    // Scanline flood fill or BFS
    QQueue<QPoint> queue;
    queue.enqueue(QPoint(startX, startY));
    visited[startY * w + startX] = true;

    QRegion matchingRegion;
    QVector<QRect> rects;

    while (!queue.isEmpty()) {
        QPoint pt = queue.dequeue();
        int cx = pt.x();
        int cy = pt.y();

        // Expand horizontally
        int left = cx;
        while (left > 0 && !visited[cy * w + (left - 1)] && matches(left - 1, cy)) {
            --left;
            visited[cy * w + left] = true;
        }

        int right = cx;
        while (right < w - 1 && !visited[cy * w + (right + 1)] && matches(right + 1, cy)) {
            ++right;
            visited[cy * w + right] = true;
        }

        rects.append(QRect(left, cy, right - left + 1, 1));

        // Check above and below
        for (int nx = left; nx <= right; ++nx) {
            if (cy > 0 && !visited[(cy - 1) * w + nx] && matches(nx, cy - 1)) {
                visited[(cy - 1) * w + nx] = true;
                queue.enqueue(QPoint(nx, cy - 1));
            }
            if (cy < h - 1 && !visited[(cy + 1) * w + nx] && matches(nx, cy + 1)) {
                visited[(cy + 1) * w + nx] = true;
                queue.enqueue(QPoint(nx, cy + 1));
            }
        }
    }

    matchingRegion.setRects(rects.data(), rects.size());
    wandPath.addRegion(matchingRegion);

    doc->selection().addPath(wandPath, mode);
    emit doc->selectionChanged();
    emit doc->documentChanged();
}

} // namespace pdn
