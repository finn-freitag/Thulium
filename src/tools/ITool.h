#pragma once

#include <QString>
#include <QColor>
#include <QFont>
#include <QCursor>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <functional>
#include "../core/Document.h"
#include "../rendering/IRenderer.h"

namespace pdn {

enum class ToolType {
    RectangleSelect,
    MoveSelectedPixels,
    LassoSelect,
    MoveSelection,
    EllipseSelect,
    Zoom,
    MagicWand,
    Pan,
    PaintBucket,
    Paintbrush,
    Eraser,
    Pencil,
    ColorPicker,
    CloneStamp,
    Recolor,
    Gradient,
    Text,
    LineCurve,
    Shapes
};

enum class ShapeType {
    Rectangle,
    RoundedRectangle,
    Ellipse,
    Diamond,
    Triangle,
    Star,
    Polygon
};

enum class FillMode {
    OutlineOnly,
    FillOnly,
    OutlineAndFill
};

enum class GradientMode {
    Linear,
    Radial,
    Diamond,
    Conical
};

struct ToolContext {
    QColor primaryColor = Qt::black;
    QColor secondaryColor = Qt::white;
    int brushWidth = 2;
    int tolerance = 50; // 0..100%
    bool antiAliasing = true;
    SelectionCombineMode selectionCombineMode = SelectionCombineMode::Replace;
    ShapeType shapeType = ShapeType::Rectangle;
    FillMode fillMode = FillMode::OutlineOnly;
    GradientMode gradientMode = GradientMode::Linear;

    // Text tool properties
    QFont font = QFont("Arial", 12);
    int fontSize = 12;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    Qt::Alignment textAlign = Qt::AlignLeft;

    // Clone stamp
    QPointF cloneSource = QPointF(-1, -1);
    bool cloneSourceSet = false;

    std::function<void()> onContextChanged;
    void notifyChanged() {
        if (onContextChanged) {
            onContextChanged();
        }
    }
};

class ITool {
public:
    virtual ~ITool() = default;

    virtual ToolType type() const = 0;
    virtual QString name() const = 0;
    virtual QString toolTip() const = 0;
    virtual QString shortcut() const = 0;
    virtual QCursor cursor() const = 0;

    virtual void activate(Document* /*doc*/, ToolContext& /*ctx*/) {}
    virtual void deactivate(Document* /*doc*/, ToolContext& /*ctx*/) {}

    virtual void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) = 0;
    virtual void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) = 0;
    virtual void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) = 0;
    virtual void keyPress(QKeyEvent* /*event*/, Document* /*doc*/, ToolContext& /*ctx*/) {}

    virtual void drawOverlay(QPainter& /*painter*/, const RenderOptions& /*opts*/) {}
};

} // namespace pdn
