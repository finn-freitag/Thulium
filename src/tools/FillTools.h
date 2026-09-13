#pragma once

#include "ITool.h"
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

private:
    void floodFill(Document* doc, int startX, int startY, const QColor& fillColor, int tolerance);
};

class GradientTool : public ITool {
public:
    ToolType type() const override { return ToolType::Gradient; }
    QString name() const override { return "Gradient"; }
    QString toolTip() const override { return "Gradient (G)"; }
    QString shortcut() const override { return "G"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

private:
    void applyGradient(Document* doc, const ToolContext& ctx);

    QPointF m_startPos;
    QPointF m_endPos;
    bool m_dragging = false;
    QImage m_undoSnapshot;
};

} // namespace pdn
