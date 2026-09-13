#pragma once

#include "ITool.h"
#include <QPointF>
#include <QString>
#include <QImage>

namespace pdn {

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
    QPointF m_textPos;
    QString m_text;
    bool m_active = false;
    bool m_cursorVisible = true;
    QImage m_undoSnapshot;
    ToolContext m_currentCtx;
};

} // namespace pdn
