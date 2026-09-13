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
void LineCurveTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    if (m_curveStage == 0) {
        m_undoSnapshot = layer->image().copy();
        m_p1 = docPos;
        m_p2 = docPos;
        m_c1 = docPos;
        m_c2 = docPos;
        m_curveStage = 1;
        m_currentCtx = ctx;
    } else if (m_curveStage == 2) {
        m_c1 = docPos;
        m_curveStage = 3;
    } else if (m_curveStage == 3) {
        m_c2 = docPos;
        m_curveStage = 4;
    }
}

void LineCurveTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (m_curveStage == 1) {
        m_p2 = docPos;
        m_c1 = m_p1 + (m_p2 - m_p1) * 0.33;
        m_c2 = m_p1 + (m_p2 - m_p1) * 0.66;
        emit doc->documentChanged();
    } else if (m_curveStage == 3) {
        m_c1 = docPos;
        emit doc->documentChanged();
    } else if (m_curveStage == 4) {
        m_c2 = docPos;
        emit doc->documentChanged();
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

void LineCurveTool::mouseRelease(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (m_curveStage == 1) {
        m_p2 = docPos;
        m_c1 = m_p1 + (m_p2 - m_p1) * 0.33;
        m_c2 = m_p1 + (m_p2 - m_p1) * 0.66;
        // Move to control point adjustment
        m_curveStage = 2;
        emit doc->documentChanged();
    } else if (m_curveStage >= 3) {
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
        m_curveStage = 0;
        emit doc->documentChanged();
    }
}

void LineCurveTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (m_curveStage == 0) return;

    QPointF vpP1(opts.panOffset.x() + m_p1.x() * opts.zoom, opts.panOffset.y() + m_p1.y() * opts.zoom);
    QPointF vpP2(opts.panOffset.x() + m_p2.x() * opts.zoom, opts.panOffset.y() + m_p2.y() * opts.zoom);
    QPointF vpC1(opts.panOffset.x() + m_c1.x() * opts.zoom, opts.panOffset.y() + m_c1.y() * opts.zoom);
    QPointF vpC2(opts.panOffset.x() + m_c2.x() * opts.zoom, opts.panOffset.y() + m_c2.y() * opts.zoom);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(m_currentCtx.primaryColor, m_currentCtx.brushWidth * opts.zoom, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    QPainterPath path;
    path.moveTo(vpP1);
    path.cubicTo(vpC1, vpC2, vpP2);
    painter.drawPath(path);

    if (m_curveStage >= 2) {
        painter.setPen(QPen(Qt::gray, 1.0, Qt::DashLine));
        painter.drawLine(vpP1, vpC1);
        painter.drawLine(vpP2, vpC2);

        painter.setBrush(Qt::white);
        painter.setPen(QPen(Qt::black, 1.0));
        painter.drawEllipse(vpC1, 4.0, 4.0);
        painter.drawEllipse(vpC2, 4.0, 4.0);
    }
    painter.restore();
}

} // namespace pdn
