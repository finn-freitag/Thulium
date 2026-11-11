#pragma once

#include "ITool.h"
#include "LayerEdit.h"
#include <QPointF>
#include <QImage>

namespace pdn {

class PaintBucketTool : public ITool {
public:
    ToolType type() const override { return ToolType::PaintBucket; }
    QString name() const override { return "Paint Bucket"; }
    QString toolTip() const override { return "Paint Bucket (F)"; }
    QString shortcut() const override { return "F"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) override {}
    void mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) override {}
};

// Drag to draw a gradient from the primary to the secondary colour (right button: reversed).
// The gradient is rendered live while dragging.
class GradientTool : public ITool {
public:
    ToolType type() const override { return ToolType::Gradient; }
    QString name() const override { return "Gradient"; }
    QString toolTip() const override { return "Gradient (G)"; }
    QString shortcut() const override { return "G"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void deactivate(Document* doc, ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void keyPress(QKeyEvent* event, Document* doc, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

private:
    void renderGradient(Document* doc, const ToolContext& ctx);

    LayerEdit m_edit;
    QPointF m_startPos;
    QPointF m_endPos;
    QColor m_startColor;
    QColor m_endColor;
};

} // namespace pdn
