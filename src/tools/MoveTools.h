#pragma once

#include "ITool.h"
#include <QImage>
#include <QTransform>
#include <QPainterPath>
#include <array>

namespace pdn {

enum class TransformMode {
    Resize,
    Rotate
};

enum class TransformHandle {
    None,
    NW,
    N,
    NE,
    E,
    SE,
    S,
    SW,
    W
};

enum class DragAction {
    None,
    Translating,
    Resizing,
    Rotating
};

class MoveToolBase : public ITool {
public:
    QCursor cursor() const override;

    void activate(Document* doc, ToolContext& ctx) override;
    void deactivate(Document* doc, ToolContext& ctx) override;

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void keyPress(QKeyEvent* event, Document* doc, ToolContext& ctx) override;
    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

    virtual void nudge(Document* doc, qreal dx, qreal dy);

    TransformMode mode() const { return m_mode; }
    void setMode(TransformMode m) { m_mode = m; }
    const QTransform& transform() const { return m_transform; }
    std::array<QPointF, 4> currentQuad() const;
    virtual void initSessionFromDoc(Document* doc);

protected:
    virtual void onSessionStarted(Document* doc) {}
    virtual void onTransformUpdated(Document* doc) {}
    virtual void onSessionEnded(Document* doc) {}
    virtual bool isPixelTool() const { return false; }

    void updateDocTransform(Document* doc);
    QPointF currentCenter() const;

    QPointF docToVp(const QPointF& docPt) const;
    TransformHandle hitTestHandle(const QPointF& docPos, const std::array<QPointF, 4>& quad) const;
    bool hitTestInside(const QPointF& docPos, Document* doc, const std::array<QPointF, 4>& quad) const;
    void applyResize(const QPointF& docPos, bool shiftHeld);

    TransformMode m_mode = TransformMode::Resize;
    TransformHandle m_hoverHandle = TransformHandle::None;
    bool m_hoverInside = false;
    DragAction m_dragAction = DragAction::None;
    TransformHandle m_activeHandle = TransformHandle::None;

    // Transformation state
    bool m_hasSession = false;
    QRectF m_localRect;
    QPainterPath m_localPath;
    QTransform m_transform;

    // Drag start state
    QPointF m_pressDocPos;
    QPoint m_pressVpPos;
    bool m_isDrag = false;
    QTransform m_initialTransform;
    std::array<QPointF, 4> m_initialQuad;
    QPainterPath m_initialSelectionPath;
    bool m_needsLiftOnDrag = false;
    bool m_wasLiftedInThisDrag = false;
    QImage m_preLiftImage;
    QImage m_postLiftImage;
    QImage m_liftedFloatingImage;
    int m_liftedLayerIndex = 0;

    qreal m_startAngleRad = 0.0;
    qreal m_prevAngleRad = 0.0;
    qreal m_accumulatedAngleDeg = 0.0;
    qreal m_baseQuadAngleDeg = 0.0;
    QPointF m_rotateCenter;

    RenderOptions m_lastRenderOpts;
    Document* m_currentDoc = nullptr;
    static QCursor s_rotateCursor;
    static bool s_rotateCursorInitialized;
    static void initRotateCursor();
};

class MoveSelectionTool : public MoveToolBase {
public:
    ToolType type() const override { return ToolType::MoveSelection; }
    QString name() const override { return "Move Selection"; }
    QString toolTip() const override { return "Move Selection (M)"; }
    QString shortcut() const override { return "M"; }

    void nudge(Document* doc, qreal dx, qreal dy) override;
    void initSessionFromDoc(Document* doc) override;

protected:
    void onSessionStarted(Document* doc) override;
    void onTransformUpdated(Document* doc) override;
    void onSessionEnded(Document* doc) override;
    bool isPixelTool() const override { return false; }

private:
    QRegion m_startRegion;
};

class MoveSelectedPixelsTool : public MoveToolBase {
public:
    ToolType type() const override { return ToolType::MoveSelectedPixels; }
    QString name() const override { return "Move Selected Pixels"; }
    QString toolTip() const override { return "Move Selected Pixels (M)"; }
    QString shortcut() const override { return "M"; }

    void commit(Document* doc);
    void nudge(Document* doc, qreal dx, qreal dy) override;
    void initSessionFromDoc(Document* doc) override;

protected:
    void onSessionStarted(Document* doc) override;
    void onTransformUpdated(Document* doc) override;
    void onSessionEnded(Document* doc) override;
    bool isPixelTool() const override { return true; }
};

} // namespace pdn
