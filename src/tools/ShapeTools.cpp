#include "ShapeTools.h"
#include "../core/History.h"
#include <QPainter>
#include <cmath>

namespace pdn {

// --- ShapesTool ---
void ShapesTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_undoSnapshot = layer->image().copy();
    m_startPos = docPos;
    m_currentPos = docPos;
    m_drawing = true;
    m_currentCtx = ctx;
}

void ShapesTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_drawing) return;
    m_currentPos = docPos;
    emit doc->documentChanged();
}

void ShapesTool::renderShape(QPainter& p, const QRectF& rect, const ToolContext& ctx) {
    p.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);

    QPen pen(ctx.primaryColor, ctx.brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QBrush brush(ctx.secondaryColor);

    if (ctx.fillMode == FillMode::OutlineOnly) {
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
    } else if (ctx.fillMode == FillMode::FillOnly) {
        p.setPen(Qt::NoPen);
        p.setBrush(QBrush(ctx.primaryColor));
    } else { // OutlineAndFill
        p.setPen(pen);
        p.setBrush(brush);
    }

    switch (ctx.shapeType) {
        case ShapeType::Rectangle:
            p.drawRect(rect);
            break;
        case ShapeType::RoundedRectangle:
            p.drawRoundedRect(rect, 10, 10);
            break;
        case ShapeType::Ellipse:
            p.drawEllipse(rect);
            break;
        case ShapeType::Diamond: {
            QPolygonF poly;
            poly << QPointF(rect.center().x(), rect.top())
                 << QPointF(rect.right(), rect.center().y())
                 << QPointF(rect.center().x(), rect.bottom())
                 << QPointF(rect.left(), rect.center().y());
            p.drawPolygon(poly);
            break;
        }
        case ShapeType::Triangle: {
            QPolygonF poly;
            poly << QPointF(rect.center().x(), rect.top())
                 << QPointF(rect.right(), rect.bottom())
                 << QPointF(rect.left(), rect.bottom());
            p.drawPolygon(poly);
            break;
        }
        case ShapeType::Star: {
            QPolygonF poly;
            qreal cx = rect.center().x();
            qreal cy = rect.center().y();
            qreal rx = rect.width() / 2.0;
            qreal ry = rect.height() / 2.0;
            for (int i = 0; i < 10; ++i) {
                qreal angle = -M_PI / 2.0 + i * (M_PI / 5.0);
                qreal r = (i % 2 == 0) ? 1.0 : 0.45;
                poly << QPointF(cx + rx * r * std::cos(angle), cy + ry * r * std::sin(angle));
            }
            p.drawPolygon(poly);
            break;
        }
        default:
            p.drawRect(rect);
            break;
    }
}

void ShapesTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_drawing) return;
    m_drawing = false;
    m_currentPos = docPos;

    auto layer = doc->activeLayer();
    if (layer) {
        QRectF rect(m_startPos, m_currentPos);
        rect = rect.normalized();

        QPainter p(&layer->image());
        if (!doc->selection().isEmpty()) {
            p.setClipPath(doc->selection().path());
        }
        renderShape(p, rect, ctx);
        p.end();

        doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Draw Shape"));
    }

    emit doc->documentChanged();
}

void ShapesTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_drawing) return;

    QRectF docRect(m_startPos, m_currentPos);
    docRect = docRect.normalized();

    QRectF vpRect(opts.panOffset.x() + docRect.x() * opts.zoom,
                  opts.panOffset.y() + docRect.y() * opts.zoom,
                  docRect.width() * opts.zoom,
                  docRect.height() * opts.zoom);

    painter.save();
    ToolContext previewCtx = m_currentCtx;
    previewCtx.brushWidth = static_cast<int>(previewCtx.brushWidth * opts.zoom);
    if (previewCtx.brushWidth < 1) previewCtx.brushWidth = 1;
    renderShape(painter, vpRect, previewCtx);
    painter.restore();
}

// --- LineCurveTool ---
void LineCurveTool::activate(Document* /*doc*/, ToolContext& ctx) {
    m_currentCtx = ctx;
}

void LineCurveTool::deactivate(Document* doc, ToolContext& ctx) {
    m_currentCtx = ctx;
    commit(doc, ctx);
}

void LineCurveTool::commit(Document* doc, const ToolContext& ctx) {
    if ((m_state != AdjustingCurve && m_state != DraggingHandle) || !doc) {
        m_state = Idle;
        m_draggedHandle = -1;
        return;
    }

    auto layer = doc->activeLayer();
    if (layer) {
        QPainter p(&layer->image());
        if (!doc->selection().isEmpty()) {
            p.setClipPath(doc->selection().path());
        }
        renderLine(p, ctx);
        p.end();

        doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Line / Curve"));
    }

    m_state = Idle;
    m_draggedHandle = -1;
    emit doc->documentChanged();
}

int LineCurveTool::hitTestHandle(const QPointF& docPos, double threshold) const {
    const QPointF handles[4] = { m_p1, m_c1, m_c2, m_p2 };
    for (int i = 0; i < 4; ++i) {
        if (std::hypot(docPos.x() - handles[i].x(), docPos.y() - handles[i].y()) <= threshold) {
            return i;
        }
    }
    return -1;
}

double LineCurveTool::distanceToCurve(const QPointF& docPos, double& outT) const {
    double minDist = 1e9;
    outT = 0.0;
    const int steps = 50;
    for (int i = 0; i <= steps; ++i) {
        double t = static_cast<double>(i) / steps;
        double u = 1.0 - t;
        double tt = t * t;
        double uu = u * u;
        double uuu = uu * u;
        double ttt = tt * t;

        QPointF pt = uuu * m_p1 + 3.0 * uu * t * m_c1 + 3.0 * u * tt * m_c2 + ttt * m_p2;
        double dist = std::hypot(docPos.x() - pt.x(), docPos.y() - pt.y());
        if (dist < minDist) {
            minDist = dist;
            outT = t;
        }
    }
    return minDist;
}

void LineCurveTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    m_currentCtx = ctx;
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    if (m_state == AdjustingCurve) {
        double threshold = std::max(10.0, ctx.brushWidth * 0.5 + 6.0);
        int handle = hitTestHandle(docPos, threshold);
        if (handle >= 0) {
            m_draggedHandle = handle;
            m_state = DraggingHandle;
            return;
        }

        double t = 0.0;
        double dist = distanceToCurve(docPos, t);
        if (dist <= threshold + 6.0) {
            if (t < 0.5) {
                m_draggedHandle = 1; // C1
                m_c1 = docPos;
            } else {
                m_draggedHandle = 2; // C2
                m_c2 = docPos;
            }
            m_state = DraggingHandle;
            emit doc->documentChanged();
            return;
        }

        // Clicked far away: commit current curve and start new line
        commit(doc, ctx);
    }

    m_undoSnapshot = layer->image().copy();
    m_p1 = docPos;
    m_p2 = docPos;
    m_c1 = docPos;
    m_c2 = docPos;
    m_state = DrawingBaseLine;
    emit doc->documentChanged();
}

void LineCurveTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (m_state == DrawingBaseLine) {
        m_p2 = docPos;
        m_c1 = m_p1 + (m_p2 - m_p1) * 0.333;
        m_c2 = m_p1 + (m_p2 - m_p1) * 0.666;
        emit doc->documentChanged();
    } else if (m_state == DraggingHandle) {
        switch (m_draggedHandle) {
            case 0: m_p1 = docPos; break;
            case 1: m_c1 = docPos; break;
            case 2: m_c2 = docPos; break;
            case 3: m_p2 = docPos; break;
        }
        emit doc->documentChanged();
    }
}

void LineCurveTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (m_state == DrawingBaseLine) {
        m_p2 = docPos;
        m_c1 = m_p1 + (m_p2 - m_p1) * 0.333;
        m_c2 = m_p1 + (m_p2 - m_p1) * 0.666;
        if (std::hypot(m_p2.x() - m_p1.x(), m_p2.y() - m_p1.y()) > 2.0) {
            m_state = AdjustingCurve;
        } else {
            m_state = Idle;
        }
        emit doc->documentChanged();
    } else if (m_state == DraggingHandle) {
        m_state = AdjustingCurve;
        m_draggedHandle = -1;
        emit doc->documentChanged();
    }
}

void LineCurveTool::keyPress(QKeyEvent* event, Document* doc, ToolContext& ctx) {
    if (m_state == Idle) return;

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        commit(doc, ctx);
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        m_state = Idle;
        m_draggedHandle = -1;
        emit doc->documentChanged();
        return;
    }
}

void LineCurveTool::renderLine(QPainter& p, const ToolContext& ctx) {
    p.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);
    p.setPen(QPen(ctx.primaryColor, ctx.brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    QPainterPath path;
    path.moveTo(m_p1);
    path.cubicTo(m_c1, m_c2, m_p2);
    p.drawPath(path);
}

void LineCurveTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (m_state == Idle) return;

    QPointF vpP1(opts.panOffset.x() + m_p1.x() * opts.zoom, opts.panOffset.y() + m_p1.y() * opts.zoom);
    QPointF vpP2(opts.panOffset.x() + m_p2.x() * opts.zoom, opts.panOffset.y() + m_p2.y() * opts.zoom);
    QPointF vpC1(opts.panOffset.x() + m_c1.x() * opts.zoom, opts.panOffset.y() + m_c1.y() * opts.zoom);
    QPointF vpC2(opts.panOffset.x() + m_c2.x() * opts.zoom, opts.panOffset.y() + m_c2.y() * opts.zoom);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, m_currentCtx.antiAliasing);
    qreal width = std::max(1.0, m_currentCtx.brushWidth * opts.zoom);
    painter.setPen(QPen(m_currentCtx.primaryColor, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    QPainterPath path;
    path.moveTo(vpP1);
    path.cubicTo(vpC1, vpC2, vpP2);
    painter.drawPath(path);

    if (m_state == AdjustingCurve || m_state == DraggingHandle) {
        painter.setPen(QPen(QColor(128, 128, 128, 180), 1.0, Qt::DashLine));
        painter.drawLine(vpP1, vpC1);
        painter.drawLine(vpP2, vpC2);

        auto drawHandle = [&](const QPointF& pt, bool isCtrl) {
            painter.setBrush(isCtrl ? Qt::white : QColor(220, 240, 255));
            painter.setPen(QPen(Qt::black, 1.0));
            painter.drawEllipse(pt, 4.0, 4.0);
        };
        drawHandle(vpP1, false);
        drawHandle(vpC1, true);
        drawHandle(vpC2, true);
        drawHandle(vpP2, false);
    }
    painter.restore();
}

} // namespace pdn
