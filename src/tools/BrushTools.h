#pragma once

#include "ITool.h"
#include <QPointF>
#include <QImage>

namespace pdn {

class PaintbrushTool : public ITool {
public:
    ToolType type() const override { return ToolType::Paintbrush; }
    QString name() const override { return "Paintbrush"; }
    QString toolTip() const override { return "Paintbrush (B)"; }
    QString shortcut() const override { return "B"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

private:
    QPointF m_lastPos;
    QImage m_undoSnapshot;
    bool m_drawing = false;
    QColor m_activeColor;
};

class PencilTool : public ITool {
public:
    ToolType type() const override { return ToolType::Pencil; }
    QString name() const override { return "Pencil"; }
    QString toolTip() const override { return "Pencil (P)"; }
    QString shortcut() const override { return "P"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

private:
    QPointF m_lastPos;
    QImage m_undoSnapshot;
    bool m_drawing = false;
    QColor m_activeColor;
};

class EraserTool : public ITool {
public:
    ToolType type() const override { return ToolType::Eraser; }
    QString name() const override { return "Eraser"; }
    QString toolTip() const override { return "Eraser (E)"; }
    QString shortcut() const override { return "E"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

private:
    QPointF m_lastPos;
    QImage m_undoSnapshot;
    bool m_drawing = false;
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

class CloneStampTool : public ITool {
public:
    ToolType type() const override { return ToolType::CloneStamp; }
    QString name() const override { return "Clone Stamp"; }
    QString toolTip() const override { return "Clone Stamp (L) - Ctrl+Click to set source"; }
    QString shortcut() const override { return "L"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

private:
    QPointF m_lastPos;
    QPointF m_sourceOffset;
    QImage m_undoSnapshot;
    bool m_stamping = false;
};

class RecolorTool : public ITool {
public:
    ToolType type() const override { return ToolType::Recolor; }
    QString name() const override { return "Recolor Tool"; }
    QString toolTip() const override { return "Recolor (R)"; }
    QString shortcut() const override { return "R"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

private:
    void applyRecolor(Document* doc, const QPointF& docPos, ToolContext& ctx);
    QImage m_undoSnapshot;
    bool m_recoloring = false;
};

} // namespace pdn
