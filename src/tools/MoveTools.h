#pragma once

#include "ITool.h"
#include <QImage>

namespace pdn {

class MoveSelectionTool : public ITool {
public:
    ToolType type() const override { return ToolType::MoveSelection; }
    QString name() const override { return "Move Selection"; }
    QString toolTip() const override { return "Move Selection (M)"; }
    QString shortcut() const override { return "M"; }
    QCursor cursor() const override { return Qt::SizeAllCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

private:
    QPointF m_lastPos;
    bool m_moving = false;
};

class MoveSelectedPixelsTool : public ITool {
public:
    ToolType type() const override { return ToolType::MoveSelectedPixels; }
    QString name() const override { return "Move Selected Pixels"; }
    QString toolTip() const override { return "Move Selected Pixels (M)"; }
    QString shortcut() const override { return "M"; }
    QCursor cursor() const override { return Qt::SizeAllCursor; }

    void activate(Document* doc, ToolContext& ctx) override;
    void deactivate(Document* doc, ToolContext& ctx) override;

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void keyPress(QKeyEvent* event, Document* doc, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

    void commit(Document* doc);
    void nudge(Document* doc, qreal dx, qreal dy);

private:
    QPointF m_lastPos;
    bool m_moving = false;
};

} // namespace pdn
