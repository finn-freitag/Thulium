#pragma once

#include "ITool.h"
#include <QPointF>
#include <QImage>

namespace pdn {

class ShapesTool : public ITool {
public:
    ToolType type() const override { return ToolType::Shapes; }
    QString name() const override { return "Shapes"; }
    QString toolTip() const override { return "Shapes (O)"; }
    QString shortcut() const override { return "O"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

private:
    void renderShape(QPainter& p, const QRectF& rect, const ToolContext& ctx);

    QPointF m_startPos;
    QPointF m_currentPos;
    bool m_drawing = false;
    QImage m_undoSnapshot;
    ToolContext m_currentCtx;
};

class LineCurveTool : public ITool {
public:
    ToolType type() const override { return ToolType::LineCurve; }
    QString name() const override { return "Line / Curve"; }
    QString toolTip() const override { return "Line / Curve (O)"; }
    QString shortcut() const override { return "O"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

private:
    void renderLine(QPainter& p, const ToolContext& ctx);

    QPointF m_p1;
    QPointF m_p2;
    QPointF m_c1;
    QPointF m_c2;
    int m_curveStage = 0; // 0 = not started, 1 = dragging line, 2 = adjusting curve
    QImage m_undoSnapshot;
    ToolContext m_currentCtx;
};

} // namespace pdn
