#pragma once

#include "ITool.h"
#include <QPolygonF>

namespace pdn {

// Shared behaviour of the drag-to-select tools.
//
// How a new shape combines with the existing selection follows the tool option, overridden
// by modifiers while dragging: Shift or Ctrl = add, right mouse button = subtract,
// Ctrl + right button = invert (xor). A click without dragging deselects (in Replace mode).
class DragSelectTool : public ITool {
public:
    QCursor cursor() const override { return Qt::CrossCursor; }

    void deactivate(Document* doc, ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

protected:
    // Region covered by the shape dragged from start to current (document coordinates).
    virtual QRegion shapeRegion(const QPointF& start, const QPointF& current) const = 0;
    // Called for every drag position; lets tools collect the path (lasso).
    virtual void dragMoved(const QPointF& /*docPos*/) {}
    // Tools that are too expensive to re-rasterise on every mouse move preview with an overlay
    // instead and only apply the selection on release.
    virtual bool livePreview() const { return true; }

    bool isSelecting() const { return m_selecting; }
    QPointF startPos() const { return m_startPos; }
    QPointF currentPos() const { return m_currentPos; }

private:
    SelectionCombineMode effectiveMode(const QMouseEvent* event, const ToolContext& ctx) const;
    void applyShape(Document* doc, SelectionCombineMode mode);
    void finish(Document* doc, SelectionCombineMode mode, bool wasDrag);

    QPointF m_startPos;
    QPointF m_currentPos;
    QPoint m_pressScreenPos;
    QRegion m_baseRegion; // selection before this drag started
    Qt::MouseButton m_button = Qt::LeftButton;
    bool m_selecting = false;
    bool m_dragged = false;
};

class RectangleSelectTool : public DragSelectTool {
public:
    ToolType type() const override { return ToolType::RectangleSelect; }
    QString name() const override { return "Rectangle Select"; }
    QString toolTip() const override { return "Rectangle Select (S)"; }
    QString shortcut() const override { return "S"; }

protected:
    QRegion shapeRegion(const QPointF& start, const QPointF& current) const override;
};

class EllipseSelectTool : public DragSelectTool {
public:
    ToolType type() const override { return ToolType::EllipseSelect; }
    QString name() const override { return "Ellipse Select"; }
    QString toolTip() const override { return "Ellipse Select (S)"; }
    QString shortcut() const override { return "S"; }

protected:
    QRegion shapeRegion(const QPointF& start, const QPointF& current) const override;
};

class LassoSelectTool : public DragSelectTool {
public:
    ToolType type() const override { return ToolType::LassoSelect; }
    QString name() const override { return "Lasso Select"; }
    QString toolTip() const override { return "Lasso Select (S)"; }
    QString shortcut() const override { return "S"; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

protected:
    QRegion shapeRegion(const QPointF& start, const QPointF& current) const override;
    void dragMoved(const QPointF& docPos) override;
    bool livePreview() const override { return false; }

private:
    QPolygonF m_polygon;
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
};

// Selection combine mode for a mouse event, shared by all selection tools.
SelectionCombineMode selectionModeForEvent(const QMouseEvent* event, Qt::MouseButton button, const ToolContext& ctx);

// Replaces the document's selection and records the change in the undo history.
void commitSelection(Document* doc, const QRegion& oldRegion, const QRegion& newRegion, const QString& actionName);

} // namespace pdn
