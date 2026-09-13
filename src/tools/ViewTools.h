#pragma once

#include "ITool.h"
#include <QPointF>

namespace pdn {

class PanTool : public ITool {
public:
    ToolType type() const override { return ToolType::Pan; }
    QString name() const override { return "Pan / Hand"; }
    QString toolTip() const override { return "Pan (H)"; }
    QString shortcut() const override { return "H"; }
    QCursor cursor() const override { return Qt::OpenHandCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;

    // Viewport pan callback
    std::function<void(const QPointF& delta)> onPan;

private:
    QPoint m_lastScreenPos;
    bool m_panning = false;
};

class ZoomTool : public ITool {
public:
    ToolType type() const override { return ToolType::Zoom; }
    QString name() const override { return "Zoom"; }
    QString toolTip() const override { return "Zoom (Z)"; }
    QString shortcut() const override { return "Z"; }
    QCursor cursor() const override { return Qt::CrossCursor; }

    void mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) override;
    void mouseMove(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) override {}
    void mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) override {}

    // Viewport zoom callback: true = in, false = out
    std::function<void(bool zoomIn, const QPointF& center)> onZoom;
};

} // namespace pdn
