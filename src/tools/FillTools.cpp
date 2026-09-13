#include "FillTools.h"
#include "../core/History.h"
#include <QPainter>
#include <QQueue>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <cmath>
#include <vector>

namespace pdn {

// --- PaintBucketTool ---
void PaintBucketTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    int x = static_cast<int>(docPos.x());
    int y = static_cast<int>(docPos.y());
    if (x < 0 || x >= doc->width() || y < 0 || y >= doc->height()) return;

    QColor fillColor = (event->button() == Qt::RightButton) ? ctx.secondaryColor : ctx.primaryColor;
    floodFill(doc, x, y, fillColor, ctx.tolerance);
}

void PaintBucketTool::floodFill(Document* doc, int startX, int startY, const QColor& fillColor, int tolerance) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    int w = doc->width();
    int h = doc->height();

    QImage oldSnapshot = layer->image().copy();

    uint32_t* bits = layer->bits();
    uint32_t targetColor = bits[startY * w + startX];

    uint32_t fillVal = (static_cast<uint32_t>(fillColor.alpha()) << 24) |
                       (static_cast<uint32_t>(fillColor.red()) << 16) |
                       (static_cast<uint32_t>(fillColor.green()) << 8) |
                       static_cast<uint32_t>(fillColor.blue());

    if (targetColor == fillVal) return;

    int tr = (targetColor >> 16) & 0xFF;
    int tg = (targetColor >> 8) & 0xFF;
    int tb = targetColor & 0xFF;
    int ta = (targetColor >> 24) & 0xFF;

    double maxDiff = (tolerance / 100.0) * std::sqrt(255.0 * 255.0 * 4.0);

    auto matches = [&](int x, int y) -> bool {
        if (!doc->selection().isEmpty() && !doc->selection().containsPixel(x, y)) {
            return false;
        }
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
    QQueue<QPoint> queue;
    queue.enqueue(QPoint(startX, startY));
    visited[startY * w + startX] = true;

    while (!queue.isEmpty()) {
        QPoint pt = queue.dequeue();
        int cx = pt.x();
        int cy = pt.y();

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

        for (int x = left; x <= right; ++x) {
            bits[cy * w + x] = fillVal;
        }

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

    doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), oldSnapshot, "Paint Bucket"));
    emit doc->documentChanged();
}

// --- GradientTool ---
void GradientTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_undoSnapshot = layer->image().copy();
    m_startPos = docPos;
    m_endPos = docPos;
    m_dragging = true;
}

void GradientTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_dragging) return;
    m_endPos = docPos;
    emit doc->documentChanged();
}

void GradientTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_dragging) return;
    m_dragging = false;
    m_endPos = docPos;

    applyGradient(doc, ctx);
    doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Gradient"));
    emit doc->documentChanged();
}

void GradientTool::applyGradient(Document* doc, const ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer) return;

    QPainter p(&layer->image());
    if (!doc->selection().isEmpty()) {
        p.setClipPath(doc->selection().path());
    }

    QBrush gradientBrush;
    if (ctx.gradientMode == GradientMode::Linear) {
        QLinearGradient grad(m_startPos, m_endPos);
        grad.setColorAt(0.0, ctx.primaryColor);
        grad.setColorAt(1.0, ctx.secondaryColor);
        gradientBrush = QBrush(grad);
    } else if (ctx.gradientMode == GradientMode::Radial) {
        qreal radius = QLineF(m_startPos, m_endPos).length();
        if (radius < 1.0) radius = 1.0;
        QRadialGradient grad(m_startPos, radius);
        grad.setColorAt(0.0, ctx.primaryColor);
        grad.setColorAt(1.0, ctx.secondaryColor);
        gradientBrush = QBrush(grad);
    } else if (ctx.gradientMode == GradientMode::Conical) {
        qreal angle = QLineF(m_startPos, m_endPos).angle();
        QConicalGradient grad(m_startPos, angle);
        grad.setColorAt(0.0, ctx.primaryColor);
        grad.setColorAt(0.5, ctx.secondaryColor);
        grad.setColorAt(1.0, ctx.primaryColor);
        gradientBrush = QBrush(grad);
    } else { // Diamond / default
        QLinearGradient grad(m_startPos, m_endPos);
        grad.setColorAt(0.0, ctx.primaryColor);
        grad.setColorAt(1.0, ctx.secondaryColor);
        gradientBrush = QBrush(grad);
    }

    p.fillRect(QRect(0, 0, doc->width(), doc->height()), gradientBrush);
    p.end();
}

void GradientTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_dragging) return;

    QPointF vpStart(opts.panOffset.x() + m_startPos.x() * opts.zoom,
                    opts.panOffset.y() + m_startPos.y() * opts.zoom);
    QPointF vpEnd(opts.panOffset.x() + m_endPos.x() * opts.zoom,
                  opts.panOffset.y() + m_endPos.y() * opts.zoom);

    painter.save();
    painter.setPen(QPen(Qt::white, 2.0));
    painter.drawLine(vpStart, vpEnd);
    painter.setPen(QPen(Qt::black, 1.0, Qt::DashLine));
    painter.drawLine(vpStart, vpEnd);

    painter.setBrush(Qt::white);
    painter.drawEllipse(vpStart, 4.0, 4.0);
    painter.drawEllipse(vpEnd, 4.0, 4.0);
    painter.restore();
}

} // namespace pdn
