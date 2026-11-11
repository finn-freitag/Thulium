#pragma once

#include "ITool.h"
#include "LayerEdit.h"
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

    void deactivate(Document* doc, ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void keyPress(QKeyEvent* event, Document* doc, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

    // Outline of a shape inscribed in rect.
    static QPainterPath shapePath(ShapeType type, const QRectF& rect);

private:
    void renderShape(QPainter& p, const QRectF& rect, qreal penWidth) const;
    QRectF shapeRect(bool constrainSquare) const;

    QPointF m_startPos;
    QPointF m_currentPos;
    bool m_drawing = false;
    bool m_constrain = false;
    // Options captured when the drag started
    ShapeType m_shapeType = ShapeType::Rectangle;
    FillMode m_fillMode = FillMode::OutlineOnly;
    QColor m_outlineColor;
    QColor m_fillColor;
    int m_penWidth = 2;
    bool m_antiAliasing = true;
};

class LineCurveTool : public ITool {
public:
    ToolType type() const override { return ToolType::LineCurve; }
    QString name() const override { return "Line / Curve"; }
    QString toolTip() const override { return "Line / Curve (V)"; }
    QString shortcut() const override { return "V"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    bool isEditing() const { return m_state != Idle; }

    void activate(Document* doc, ToolContext& ctx) override;
    void deactivate(Document* doc, ToolContext& ctx) override;

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void keyPress(QKeyEvent* event, Document* doc, ToolContext& ctx) override;

    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

    void commit(Document* doc, const ToolContext& ctx);

private:
    void renderLine(QPainter& p, const ToolContext& ctx);
    void setBaseLineEnd(const QPointF& docPos, bool constrainAngle);
    int hitTestHandle(const QPointF& docPos, double threshold) const;
    double distanceToCurve(const QPointF& docPos, double& outT) const;

    enum State {
        Idle,
        DrawingBaseLine,
        AdjustingCurve,
        DraggingHandle
    };

    State m_state = Idle;
    int m_draggedHandle = -1; // 0=P1, 1=C1, 2=C2, 3=P2

    QPointF m_p1;
    QPointF m_p2;
    QPointF m_c1;
    QPointF m_c2;

    LayerEdit m_edit;
    ToolContext m_currentCtx;
};

} // namespace pdn
