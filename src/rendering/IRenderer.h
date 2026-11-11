#pragma once

#include <QPainter>
#include <QPointF>
#include <QRect>
#include "../core/Document.h"

namespace pdn {

struct RenderOptions {
    double zoom = 1.0;
    QPointF panOffset = QPointF(0, 0);
    bool showPixelGrid = false;
    bool showRulers = true;
    int marchingAntsOffset = 0;

    // Part of the viewport that needs repainting (viewport coordinates). Null = everything.
    QRect exposedRect;
    // Device pixels per logical pixel of the paint device (HiDPI screens).
    qreal devicePixelRatio = 1.0;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    virtual void resize(int viewportWidth, int viewportHeight) = 0;

    // Renders document onto destination painter / surface
    virtual void render(QPainter& painter, const Document& doc, const RenderOptions& opts) = 0;

    // The document's composited pixels changed inside docRect. Returns the viewport rectangle
    // that has to be repainted as a result (null if nothing visible is affected).
    virtual QRect invalidate(const Document& doc, const QRect& docRect, const RenderOptions& opts) = 0;

    // Drops everything cached for doc (it is being closed or is no longer displayed).
    virtual void releaseDocument(const Document* doc) = 0;

    virtual QString rendererName() const = 0;
};

} // namespace pdn
