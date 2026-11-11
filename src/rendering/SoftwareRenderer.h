#pragma once

#include "IRenderer.h"
#include "DisplayCache.h"
#include <QImage>
#include <QPixmap>
#include <QRegion>

namespace pdn {

// CPU renderer built around three caches so that a repaint only does the work that changed:
//   1. DisplayCache   - premultiplied composite of the document as a lazily updated mip pyramid
//   2. frame cache    - the finished viewport image (background, checkerboard, document, grid);
//                       overlay-only repaints (marching ants, tool previews) are a single blit
//   3. outline cache  - the selection outline rasterised once per view/selection change;
//                       animating the marching ants only swaps its 10-entry colour table
class SoftwareRenderer : public IRenderer {
public:
    SoftwareRenderer();
    ~SoftwareRenderer() override = default;

    bool initialize() override;
    void cleanup() override;
    void resize(int viewportWidth, int viewportHeight) override;

    void render(QPainter& painter, const Document& doc, const RenderOptions& opts) override;
    QRect invalidate(const Document& doc, const QRect& docRect, const RenderOptions& opts) override;
    void releaseDocument(const Document* doc) override;

    QString rendererName() const override;

private:
    struct FrameKey {
        const Document* doc = nullptr;
        QSize docSize;
        double zoom = 0.0;
        QPointF pan;
        QSize deviceSize;
        bool pixelGrid = false;
        bool operator==(const FrameKey& o) const {
            return doc == o.doc && docSize == o.docSize && zoom == o.zoom && pan == o.pan &&
                   deviceSize == o.deviceSize && pixelGrid == o.pixelGrid;
        }
    };

    struct OutlineKey {
        const Selection* selection = nullptr;
        quint64 revision = 0;
        double zoom = 0.0;
        QPointF pan;
        QSize deviceSize;
        bool operator==(const OutlineKey& o) const {
            return selection == o.selection && revision == o.revision && zoom == o.zoom &&
                   pan == o.pan && deviceSize == o.deviceSize;
        }
    };

    void paintFrame(QPainter& p, const Document& doc, const RenderOptions& opts, const QRect& area);
    void drawPixelGrid(QPainter& p, const QRectF& docRect, const QRectF& area, double zoom);
    void drawSelection(QPainter& painter, const Selection& selection, const RenderOptions& opts);
    void rebuildOutline(const Selection& selection, const RenderOptions& opts);

    QPixmap m_checkerPixmap;
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;

    DisplayCache m_cache;

    QImage m_frame;          // device pixels
    FrameKey m_frameKey;
    QRegion m_frameDirty;    // viewport (logical) coordinates

    QImage m_outline;        // Format_Indexed8, device pixels
    OutlineKey m_outlineKey;
    QRect m_outlineBounds;   // device pixels; empty if nothing visible
};

} // namespace pdn
