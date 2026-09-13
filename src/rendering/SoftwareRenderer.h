#pragma once

#include "IRenderer.h"
#include <QPixmap>

namespace pdn {

class SoftwareRenderer : public IRenderer {
public:
    SoftwareRenderer();
    ~SoftwareRenderer() override = default;

    bool initialize() override;
    void cleanup() override;
    void resize(int viewportWidth, int viewportHeight) override;

    void render(QPainter& painter, const Document& doc, const RenderOptions& opts) override;

    bool isGpuAccelerated() const override { return false; }
    QString rendererName() const override { return "Software CPU Renderer"; }

private:
    void drawCheckerboard(QPainter& painter, const QRectF& docRect, double zoom);
    void drawPixelGrid(QPainter& painter, const QRectF& docRect, double zoom);
    void drawSelection(QPainter& painter, const Selection& selection, double zoom, const QPointF& offset, int marchingAntsOffset);

    QPixmap m_checkerPixmap;
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;
};

} // namespace pdn
