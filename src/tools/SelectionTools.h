#pragma once

#include "ITool.h"
#include <QPolygonF>

namespace pdn {

class RectangleSelectTool : public ITool {
public:
    ToolType type() const override { return ToolType::RectangleSelect; }
    QString name() const override { return "Rectangle Select"; }
    QString toolTip() const override { return "Rectangle Select (S)"; }
    QString shortcut() const override { return "S"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

private:
    QPointF m_startPos;
    QPointF m_currentPos;
    bool m_selecting = false;
};

class EllipseSelectTool : public ITool {
public:
    ToolType type() const override { return ToolType::EllipseSelect; }
    QString name() const override { return "Ellipse Select"; }
    QString toolTip() const override { return "Ellipse Select (S)"; }
    QString shortcut() const override { return "S"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

private:
    QPointF m_startPos;
    QPointF m_currentPos;
    bool m_selecting = false;
};

class LassoSelectTool : public ITool {
public:
    ToolType type() const override { return ToolType::LassoSelect; }
    QString name() const override { return "Lasso Select"; }
    QString toolTip() const override { return "Lasso Select (S)"; }
    QString shortcut() const override { return "S"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

private:
    QPolygonF m_polygon;
    bool m_selecting = false;
};

class MagicWandTool : public ITool {
public:
    ToolType type() const override { return ToolType::MagicWand; }
    QString name() const override { return "Magic Wand"; }
    QString toolTip() const override { return "Magic Wand (S)"; }
    QString shortcut() const override { return "S"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) override {}
    void mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) override {}

private:
    void floodSelect(Document* doc, int startX, int startY, int tolerance, SelectionCombineMode mode);
};

} // namespace pdn
