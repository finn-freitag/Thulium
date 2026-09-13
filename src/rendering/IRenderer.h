#pragma once

#include <QPainter>
#include <QPointF>
#include "../core/Document.h"

namespace pdn {

struct RenderOptions {
    double zoom = 1.0;
    QPointF panOffset = QPointF(0, 0);
    bool showPixelGrid = false;
    bool showRulers = true;
    int marchingAntsOffset = 0;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    virtual void resize(int viewportWidth, int viewportHeight) = 0;

    // Renders document onto destination painter / surface
    virtual void render(QPainter& painter, const Document& doc, const RenderOptions& opts) = 0;

    virtual bool isGpuAccelerated() const = 0;
    virtual QString rendererName() const = 0;
};

} // namespace pdn
