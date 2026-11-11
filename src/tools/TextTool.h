#pragma once

#include "ITool.h"
#include <QPointF>
#include <QString>
#include <QStringList>

namespace pdn {

// Click to place a caret, then type. Enter starts a new line; the text is committed to the
// layer with Ctrl+Enter, by clicking elsewhere or by switching tools. Escape discards it.
class TextTool : public ITool {
public:
    ToolType type() const override { return ToolType::Text; }
    QString name() const override { return "Text"; }
    QString toolTip() const override { return "Text (T)"; }
    QString shortcut() const override { return "T"; }
    QCursor cursor() const override { return Qt::IBeamCursor; }

    bool isEditing() const { return m_active; }

    void activate(Document* doc, ToolContext& ctx) override;
    void deactivate(Document* doc, ToolContext& ctx) override;

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) override {}
    void mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) override {}
    void keyPress(QKeyEvent* event, Document* doc, ToolContext& ctx) override;

    void drawOverlay(QPainter& painter, const RenderOptions& opts) override;

    void commit(Document* doc, const ToolContext& ctx);

private:
    // Draws the text block with its anchor at origin; scale converts document to painter units.
    void renderText(QPainter& p, const QPointF& origin, qreal scale, const ToolContext& ctx, bool drawCaret) const;
    QRectF textBounds(const ToolContext& ctx) const;
    QStringList lines() const { return m_text.split('\n'); }
    void cancel();

    QPointF m_textPos;
    QString m_text;
    int m_caret = 0; // index into m_text
    bool m_active = false;
    ToolContext m_currentCtx;
};

} // namespace pdn
