#pragma once

#include "ITool.h"
#include "LayerEdit.h"
#include <QPointF>
#include <QImage>

namespace pdn {

// Base for tools that paint a round stroke along the mouse path (paintbrush, eraser, clone stamp).
//
// The stroke is accumulated as an 8-bit coverage image. Every update recomputes the touched
// pixels from the layer's original content plus the stroke's coverage, so overlapping segments
// of one stroke never build up opacity, and only the touched rectangle is recomposited.
class StrokeTool : public ITool {
public:
    QCursor cursor() const override { return Qt::CrossCursor; }

    void deactivate(Document* doc, ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

protected:
    // Called on mouse press before the stroke starts; return false to not start one.
    virtual bool beginStroke(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) = 0;
    // Computes count result pixels of row y starting at column x from the original pixels and
    // the stroke coverage (0..255).
    virtual void composeRow(uint32_t* dst, const uint32_t* original, const uint8_t* coverage,
                            int x, int y, int count) const = 0;
    virtual QString actionName() const = 0;

    const LayerEdit& edit() const { return m_edit; }
    // Extra area whose original pixels the stroke reads when painting dirty (clone source).
    virtual QRect sourceRectFor(const QRect& /*dirty*/) const { return QRect(); }

private:
    void stroke(Document* doc, const QPointF& from, const QPointF& to, const ToolContext& ctx);
    void stampSoftSegment(const QPointF& from, const QPointF& to, const ToolContext& ctx);
    void finishStroke();

    LayerEdit m_edit;
    QImage m_coverage; // Format_Alpha8, layer-sized; all zero between strokes
    QPointF m_lastPos;
};

class PaintbrushTool : public StrokeTool {
public:
    ToolType type() const override { return ToolType::Paintbrush; }
    QString name() const override { return "Paintbrush"; }
    QString toolTip() const override { return "Paintbrush (B)"; }
    QString shortcut() const override { return "B"; }

protected:
    bool beginStroke(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void composeRow(uint32_t* dst, const uint32_t* original, const uint8_t* coverage, int x, int y, int count) const override;
    QString actionName() const override { return "Paintbrush"; }

private:
    QColor m_activeColor;
    ColorCompositionMode m_compositionMode = ColorCompositionMode::DrawOver;
    BlendMode m_blendMode = BlendMode::Normal;
};

class EraserTool : public StrokeTool {
public:
    ToolType type() const override { return ToolType::Eraser; }
    QString name() const override { return "Eraser"; }
    QString toolTip() const override { return "Eraser (E)"; }
    QString shortcut() const override { return "E"; }

protected:
    bool beginStroke(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void composeRow(uint32_t* dst, const uint32_t* original, const uint8_t* coverage, int x, int y, int count) const override;
    QString actionName() const override { return "Eraser"; }

private:
    uint32_t m_strength = 255; // alpha of the active colour: how much one stroke erases
};

class CloneStampTool : public StrokeTool {
public:
    ToolType type() const override { return ToolType::CloneStamp; }
    QString name() const override { return "Clone Stamp"; }
    QString toolTip() const override { return "Clone Stamp (L) - Ctrl+Click to set source"; }
    QString shortcut() const override { return "L"; }

    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

protected:
    bool beginStroke(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void composeRow(uint32_t* dst, const uint32_t* original, const uint8_t* coverage, int x, int y, int count) const override;
    QString actionName() const override { return "Clone Stamp"; }
    QRect sourceRectFor(const QRect& dirty) const override { return dirty.translated(m_sourceOffset); }

private:
    QPoint m_sourceOffset; // source pixel = destination pixel + offset
    QPointF m_anchor;
    bool m_hasAnchor = false;
};

class PencilTool : public ITool {
public:
    ToolType type() const override { return ToolType::Pencil; }
    QString name() const override { return "Pencil"; }
    QString toolTip() const override { return "Pencil (P)"; }
    QString shortcut() const override { return "P"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void deactivate(Document* doc, ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

private:
    void drawSegment(Document* doc, const QPoint& from, const QPoint& to);

    LayerEdit m_edit;
    QPoint m_lastPixel;
    QColor m_activeColor;
};

class ColorPickerTool : public ITool {
public:
    ToolType type() const override { return ToolType::ColorPicker; }
    QString name() const override { return "Color Picker"; }
    QString toolTip() const override { return "Color Picker (K)"; }
    QString shortcut() const override { return "K"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

private:
    void sample(Document* doc, const QPointF& docPos, ToolContext& ctx, bool isRightButton);
};

class RecolorTool : public ITool {
public:
    ToolType type() const override { return ToolType::Recolor; }
    QString name() const override { return "Recolor Tool"; }
    QString toolTip() const override { return "Recolor (R)"; }
    QString shortcut() const override { return "R"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void deactivate(Document* doc, ToolContext& ctx) override;
    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

private:
    void applyRecolor(Document* doc, const QPointF& docPos, const ToolContext& ctx);

    LayerEdit m_edit;
    QPointF m_lastPos;
    QColor m_targetColor;
    QColor m_replacementColor;
};

} // namespace pdn
