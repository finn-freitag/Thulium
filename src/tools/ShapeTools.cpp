#include "ShapeTools.h"
#include <QPainter>
#include <QLineF>
#include <algorithm>
#include <cmath>

namespace pdn {

namespace {

void clipToSelection(QPainter& p, const Document* doc) {
    if (!doc->selection().isEmpty()) {
        p.setClipRegion(doc->selection().region());
    }
}

QPolygonF regularPolygon(const QRectF& rect, int sides, qreal startAngle) {
    QPolygonF poly;
    const QPointF c = rect.center();
    for (int i = 0; i < sides; ++i) {
        const qreal angle = startAngle + i * 2.0 * M_PI / sides;
        poly << QPointF(c.x() + rect.width() / 2.0 * std::cos(angle), c.y() + rect.height() / 2.0 * std::sin(angle));
    }
    return poly;
}

} // namespace

// --- ShapesTool ---
QPainterPath ShapesTool::shapePath(ShapeType type, const QRectF& rect) {
    QPainterPath path;
    const qreal l = rect.left(), t = rect.top(), w = rect.width(), h = rect.height();
    const QPointF c = rect.center();
    switch (type) {
        case ShapeType::Rectangle:
            path.addRect(rect);
            break;
        case ShapeType::RoundedRectangle: {
            const qreal radius = std::min({10.0, w / 2.0, h / 2.0});
            path.addRoundedRect(rect, radius, radius);
            break;
        }
        case ShapeType::Ellipse:
            path.addEllipse(rect);
            break;
        case ShapeType::Diamond:
            path.addPolygon(QPolygonF({QPointF(c.x(), t), QPointF(rect.right(), c.y()),
                                       QPointF(c.x(), rect.bottom()), QPointF(l, c.y())}));
            break;
        case ShapeType::Triangle:
            path.addPolygon(QPolygonF({QPointF(c.x(), t), rect.bottomRight(), rect.bottomLeft()}));
            break;
        case ShapeType::Star: {
            QPolygonF poly;
            for (int i = 0; i < 10; ++i) {
                const qreal angle = -M_PI / 2.0 + i * (M_PI / 5.0);
                const qreal r = (i % 2 == 0) ? 1.0 : 0.45;
                poly << QPointF(c.x() + w / 2.0 * r * std::cos(angle), c.y() + h / 2.0 * r * std::sin(angle));
            }
            path.addPolygon(poly);
            break;
        }
        case ShapeType::Pentagon:
            path.addPolygon(regularPolygon(rect, 5, -M_PI / 2.0));
            break;
        case ShapeType::Hexagon:
            path.addPolygon(regularPolygon(rect, 6, 0.0));
            break;
        case ShapeType::Arrow:
            path.addPolygon(QPolygonF({QPointF(l, t + h * 0.3), QPointF(l + w * 0.6, t + h * 0.3),
                                       QPointF(l + w * 0.6, t), QPointF(l + w, c.y()),
                                       QPointF(l + w * 0.6, t + h), QPointF(l + w * 0.6, t + h * 0.7),
                                       QPointF(l, t + h * 0.7)}));
            break;
        case ShapeType::Heart:
            path.moveTo(c.x(), t + h * 0.28);
            path.cubicTo(c.x() + w * 0.12, t - h * 0.05, l + w * 1.05, t + h * 0.02, l + w, t + h * 0.38);
            path.cubicTo(l + w, t + h * 0.62, c.x() + w * 0.18, t + h * 0.82, c.x(), t + h);
            path.cubicTo(c.x() - w * 0.18, t + h * 0.82, l, t + h * 0.62, l, t + h * 0.38);
            path.cubicTo(l - w * 0.05, t + h * 0.02, c.x() - w * 0.12, t - h * 0.05, c.x(), t + h * 0.28);
            break;
    }
    path.closeSubpath();
    return path;
}

QRectF ShapesTool::shapeRect(bool constrainSquare) const {
    QPointF end = m_currentPos;
    if (constrainSquare) {
        // Hold Shift for equal width and height
        const QPointF d = end - m_startPos;
        const qreal side = std::max(std::abs(d.x()), std::abs(d.y()));
        end = m_startPos + QPointF(d.x() < 0 ? -side : side, d.y() < 0 ? -side : side);
    }
    return QRectF(m_startPos, end).normalized();
}

void ShapesTool::deactivate(Document* /*doc*/, ToolContext& /*ctx*/) {
    m_drawing = false;
}

void ShapesTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    // The right button draws with the colour roles swapped
    const bool swapped = event && event->button() == Qt::RightButton;
    m_outlineColor = swapped ? ctx.secondaryColor : ctx.primaryColor;
    m_fillColor = swapped ? ctx.primaryColor : ctx.secondaryColor;
    m_shapeType = ctx.shapeType;
    m_fillMode = ctx.fillMode;
    m_penWidth = ctx.brushWidth;
    m_antiAliasing = ctx.antiAliasing;
    m_startPos = docPos;
    m_currentPos = docPos;
    m_constrain = false;
    m_drawing = true;
}

void ShapesTool::mouseMove(QMouseEvent* event, Document* /*doc*/, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_drawing) return;
    m_currentPos = docPos;
    m_constrain = event && (event->modifiers() & Qt::ShiftModifier);
}

void ShapesTool::renderShape(QPainter& p, const QRectF& rect, qreal penWidth) const {
    p.setRenderHint(QPainter::Antialiasing, m_antiAliasing);
    const QPen pen(m_outlineColor, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    switch (m_fillMode) {
        case FillMode::OutlineOnly:
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            break;
        case FillMode::FillOnly:
            p.setPen(Qt::NoPen);
            p.setBrush(m_outlineColor);
            break;
        case FillMode::OutlineAndFill:
            p.setPen(pen);
            p.setBrush(m_fillColor);
            break;
    }
    p.drawPath(shapePath(m_shapeType, rect));
}

void ShapesTool::mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_drawing) return;
    m_drawing = false;
    m_currentPos = docPos;
    m_constrain = event && (event->modifiers() & Qt::ShiftModifier);

    const QRectF rect = shapeRect(m_constrain);
    if (rect.width() < 1.0 && rect.height() < 1.0) return; // a click draws nothing

    LayerEdit edit;
    if (!edit.begin(doc)) return;
    const int pad = m_penWidth / 2 + 2;
    const QRect touched = rect.toAlignedRect().adjusted(-pad, -pad, pad, pad);
    edit.prepare(touched);
    {
        QPainter p(&edit.layer()->image());
        clipToSelection(p, doc);
        renderShape(p, rect, m_penWidth);
    }
    edit.markDirty(touched);
    edit.commit("Draw Shape");
}

void ShapesTool::keyPress(QKeyEvent* event, Document* /*doc*/, ToolContext& /*ctx*/) {
    if (event->key() == Qt::Key_Escape && m_drawing) {
        m_drawing = false;
        event->accept();
    }
}

void ShapesTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_drawing) return;

    const QRectF docRect = shapeRect(m_constrain);
    const QRectF vpRect(opts.panOffset + docRect.topLeft() * opts.zoom, docRect.size() * opts.zoom);

    painter.save();
    renderShape(painter, vpRect, std::max(1.0, m_penWidth * opts.zoom));
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

    if (m_edit.isActive()) {
        const int pad = ctx.brushWidth / 2 + 2;
        QPolygonF hull;
        hull << m_p1 << m_c1 << m_c2 << m_p2; // a Bezier curve stays inside its control polygon
        const QRect touched = hull.boundingRect().toAlignedRect().adjusted(-pad, -pad, pad, pad);
        m_edit.prepare(touched);
        {
            QPainter p(&m_edit.layer()->image());
            clipToSelection(p, doc);
            renderLine(p, ctx);
        }
        m_edit.markDirty(touched);
        m_edit.commit("Line / Curve");
    }

    m_state = Idle;
    m_draggedHandle = -1;
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
            return;
        }

        // Clicked far away: commit current curve and start new line
        commit(doc, ctx);
    }

    if (!m_edit.begin(doc)) return;
    m_p1 = docPos;
    m_p2 = docPos;
    m_c1 = docPos;
    m_c2 = docPos;
    m_state = DrawingBaseLine;
}

void LineCurveTool::setBaseLineEnd(const QPointF& docPos, bool constrainAngle) {
    m_p2 = docPos;
    if (constrainAngle) {
        // Hold Shift to snap the line to multiples of 15 degrees
        QLineF line(m_p1, m_p2);
        line.setAngle(std::round(line.angle() / 15.0) * 15.0);
        m_p2 = line.p2();
    }
    m_c1 = m_p1 + (m_p2 - m_p1) / 3.0;
    m_c2 = m_p1 + (m_p2 - m_p1) * 2.0 / 3.0;
}

void LineCurveTool::mouseMove(QMouseEvent* event, Document* /*doc*/, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (m_state == DrawingBaseLine) {
        setBaseLineEnd(docPos, event && (event->modifiers() & Qt::ShiftModifier));
    } else if (m_state == DraggingHandle) {
        switch (m_draggedHandle) {
            case 0: m_p1 = docPos; break;
            case 1: m_c1 = docPos; break;
            case 2: m_c2 = docPos; break;
            case 3: m_p2 = docPos; break;
        }
    }
}

void LineCurveTool::mouseRelease(QMouseEvent* event, Document* /*doc*/, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (m_state == DrawingBaseLine) {
        setBaseLineEnd(docPos, event && (event->modifiers() & Qt::ShiftModifier));
        if (std::hypot(m_p2.x() - m_p1.x(), m_p2.y() - m_p1.y()) > 2.0) {
            m_state = AdjustingCurve;
        } else {
            m_state = Idle;
            m_edit.cancel();
        }
    } else if (m_state == DraggingHandle) {
        m_state = AdjustingCurve;
        m_draggedHandle = -1;
    }
}

void LineCurveTool::keyPress(QKeyEvent* event, Document* doc, ToolContext& ctx) {
    if (m_state == Idle) return;

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        commit(doc, ctx);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        m_state = Idle;
        m_draggedHandle = -1;
        m_edit.cancel();
        event->accept();
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
