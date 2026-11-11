#include "SoftwareRenderer.h"
#include <QPen>
#include <QThread>
#include <algorithm>
#include <cmath>

namespace pdn {

namespace {

const QColor kWorkspaceColor(160, 160, 160);
const QColor kCanvasBorderColor(80, 80, 80);
const QColor kSelectionTint(0, 120, 215, 20);

constexpr int kOutlinePhases = 8;         // dash period in device pixels (4 black + 4 white)
constexpr uint8_t kOutlineTintIndex = 9;  // colour-table slot of the interior tint
constexpr double kPixelGridMinZoom = 4.0;

// Viewport rectangle covered by a document rectangle.
QRectF docToViewport(const QRectF& docRect, const RenderOptions& opts) {
    return QRectF(opts.panOffset.x() + docRect.x() * opts.zoom,
                  opts.panOffset.y() + docRect.y() * opts.zoom,
                  docRect.width() * opts.zoom, docRect.height() * opts.zoom);
}

} // namespace

SoftwareRenderer::SoftwareRenderer() {
    // 16x16 alternating checkerboard pattern
    m_checkerPixmap = QPixmap(16, 16);
    QPainter p(&m_checkerPixmap);
    p.fillRect(0, 0, 8, 8, QColor(255, 255, 255));
    p.fillRect(8, 0, 8, 8, QColor(220, 220, 220));
    p.fillRect(0, 8, 8, 8, QColor(220, 220, 220));
    p.fillRect(8, 8, 8, 8, QColor(255, 255, 255));
}

bool SoftwareRenderer::initialize() {
    return true;
}

void SoftwareRenderer::cleanup() {
    m_cache.clear();
    m_frame = QImage();
    m_outline = QImage();
    m_frameKey = FrameKey();
    m_outlineKey = OutlineKey();
}

QString SoftwareRenderer::rendererName() const {
    return QString("CPU (%1 threads, cached)").arg(std::max(1, QThread::idealThreadCount()));
}

void SoftwareRenderer::resize(int viewportWidth, int viewportHeight) {
    m_viewportWidth = viewportWidth;
    m_viewportHeight = viewportHeight;
}

void SoftwareRenderer::releaseDocument(const Document* doc) {
    if (m_cache.document() == doc) {
        cleanup();
    }
}

QRect SoftwareRenderer::invalidate(const Document& doc, const QRect& docRect, const RenderOptions& opts) {
    m_cache.bind(&doc);
    m_cache.invalidate(docRect);

    // Smooth scaling and the mip pyramid both bleed a little into neighbouring pixels, so the
    // repaint area is the document rectangle grown to the mip grid plus a one pixel margin.
    const int step = DisplayCache::levelStep(m_cache.levelForScale(opts.zoom * opts.devicePixelRatio));
    const QRect grown(QPoint((docRect.left() / step) * step - step, (docRect.top() / step) * step - step),
                      QPoint((docRect.right() / step + 2) * step, (docRect.bottom() / step + 2) * step));
    const QRect viewportRect = docToViewport(grown, opts).toAlignedRect().adjusted(-1, -1, 1, 1)
                                   .intersected(QRect(0, 0, m_viewportWidth, m_viewportHeight));
    if (!viewportRect.isEmpty()) {
        m_frameDirty += viewportRect;
    }
    return viewportRect;
}

void SoftwareRenderer::drawPixelGrid(QPainter& p, const QRectF& docRect, const QRectF& area, double zoom) {
    if (zoom < kPixelGridMinZoom) return;
    const QRectF visible = docRect.intersected(area);
    if (visible.isEmpty()) return;

    p.save();
    p.setPen(QPen(QColor(180, 180, 180, 100), 1.0));
    const int firstX = static_cast<int>(std::floor((visible.left() - docRect.left()) / zoom));
    const int lastX = static_cast<int>(std::ceil((visible.right() - docRect.left()) / zoom));
    for (int x = firstX; x <= lastX; ++x) {
        const qreal px = docRect.left() + x * zoom;
        p.drawLine(QPointF(px, visible.top()), QPointF(px, visible.bottom()));
    }
    const int firstY = static_cast<int>(std::floor((visible.top() - docRect.top()) / zoom));
    const int lastY = static_cast<int>(std::ceil((visible.bottom() - docRect.top()) / zoom));
    for (int y = firstY; y <= lastY; ++y) {
        const qreal py = docRect.top() + y * zoom;
        p.drawLine(QPointF(visible.left(), py), QPointF(visible.right(), py));
    }
    p.restore();
}

// Paints the static part of the viewport (everything except overlays) inside area.
void SoftwareRenderer::paintFrame(QPainter& p, const Document& doc, const RenderOptions& opts, const QRect& area) {
    p.save();
    p.setClipRect(area);

    // 1. Workspace background and drop shadow
    p.fillRect(area, kWorkspaceColor);
    const QRectF docRect = docToViewport(QRectF(0, 0, doc.width(), doc.height()), opts);
    p.fillRect(docRect.translated(3, 3), QColor(0, 0, 0, 80));

    const QRectF visible = docRect.intersected(QRectF(area));
    if (!visible.isEmpty()) {
        // 2. Checkerboard behind transparent areas, anchored to the document
        p.drawTiledPixmap(visible, m_checkerPixmap, visible.topLeft() - docRect.topLeft());

        // 3. The document, taken from the mip level that matches the on-screen scale
        const double scale = opts.zoom * opts.devicePixelRatio;
        const int levelIndex = m_cache.levelForScale(scale);
        const int step = DisplayCache::levelStep(levelIndex);

        // Visible source rectangle in level coordinates, padded so filtering has neighbours
        const int lx0 = static_cast<int>(std::floor((visible.left() - docRect.left()) / opts.zoom / step)) - 1;
        const int ly0 = static_cast<int>(std::floor((visible.top() - docRect.top()) / opts.zoom / step)) - 1;
        const int lx1 = static_cast<int>(std::ceil((visible.right() - docRect.left()) / opts.zoom / step)) + 1;
        const int ly1 = static_cast<int>(std::ceil((visible.bottom() - docRect.top()) / opts.zoom / step)) + 1;
        const QImage& levelImage = m_cache.level(levelIndex, QRect(QPoint(lx0, ly0), QPoint(lx1, ly1)));
        const QRect source = QRect(QPoint(lx0, ly0), QPoint(lx1, ly1)).intersected(levelImage.rect());

        if (!source.isEmpty()) {
            const double levelScale = scale * step; // device pixels per level pixel
            const bool smooth = levelScale < 2.0 && std::abs(levelScale - 1.0) > 1e-6;
            p.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
            const QRectF target = docToViewport(QRectF(source.x() * step, source.y() * step,
                                                       source.width() * step, source.height() * step), opts);
            p.setClipRect(visible, Qt::IntersectClip); // the last mip column may overhang the canvas
            p.drawImage(target, levelImage, QRectF(source));
            p.setClipRect(area);
        }

        if (opts.showPixelGrid) {
            drawPixelGrid(p, docRect, QRectF(area), opts.zoom);
        }
    }

    // 4. Canvas border
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(kCanvasBorderColor, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(docRect);
    p.restore();
}

void SoftwareRenderer::render(QPainter& painter, const Document& doc, const RenderOptions& opts) {
    m_cache.bind(&doc);

    const QRect viewport(0, 0, m_viewportWidth, m_viewportHeight);
    const QRect exposed = opts.exposedRect.isValid() ? opts.exposedRect.intersected(viewport) : viewport;
    if (exposed.isEmpty()) return;

    const qreal dpr = opts.devicePixelRatio;
    const QSize deviceSize(static_cast<int>(std::ceil(m_viewportWidth * dpr)),
                           static_cast<int>(std::ceil(m_viewportHeight * dpr)));
    const FrameKey key{&doc, doc.size(), opts.zoom, opts.panOffset, deviceSize, opts.showPixelGrid};
    if (!(key == m_frameKey) || m_frame.size() != deviceSize) {
        if (m_frame.size() != deviceSize) {
            m_frame = QImage(deviceSize, QImage::Format_RGB32);
        }
        m_frame.setDevicePixelRatio(dpr);
        m_frameKey = key;
        m_frameDirty = viewport;
    }

    // Bring the exposed part of the frame cache up to date...
    const QRegion stale = m_frameDirty.intersected(exposed);
    if (!stale.isEmpty()) {
        QPainter fp(&m_frame);
        for (const QRect& r : stale) {
            paintFrame(fp, doc, opts, r);
        }
        fp.end();
        m_frameDirty -= stale;
    }

    // ...and blit it. Overlay-only repaints end up doing nothing but this.
    const QRectF deviceSource(exposed.x() * dpr, exposed.y() * dpr, exposed.width() * dpr, exposed.height() * dpr);
    painter.drawImage(QRectF(exposed), m_frame, deviceSource);

    drawSelection(painter, doc.selection(), opts);
}

// Rasterises the selection outline into an 8-bit image whose pixel values encode the dash
// phase. The outline consists purely of horizontal and vertical segments, so this is a few
// clipped memory fills and does not get slower when the selection becomes intricate.
void SoftwareRenderer::rebuildOutline(const Selection& selection, const RenderOptions& opts) {
    const qreal dpr = opts.devicePixelRatio;
    const int w = m_outlineKey.deviceSize.width();
    const int h = m_outlineKey.deviceSize.height();
    if (m_outline.size() != m_outlineKey.deviceSize) {
        m_outline = QImage(m_outlineKey.deviceSize, QImage::Format_Indexed8);
        m_outline.setColorCount(kOutlineTintIndex + 1);
    }
    m_outline.setDevicePixelRatio(dpr);
    m_outline.fill(0);
    m_outlineBounds = QRect();
    if (w <= 0 || h <= 0) return;

    const double scale = opts.zoom * dpr;
    const double originX = opts.panOffset.x() * dpr;
    const double originY = opts.panOffset.y() * dpr;
    auto toDeviceX = [&](int docX) { return static_cast<int>(std::lround(originX + docX * scale)); };
    auto toDeviceY = [&](int docY) { return static_cast<int>(std::lround(originY + docY * scale)); };
    const int thickness = std::max(1, static_cast<int>(std::lround(dpr)));

    uchar* bits = m_outline.bits();
    const qsizetype bpl = m_outline.bytesPerLine();
    int minX = w, minY = h, maxX = -1, maxY = -1;
    auto touch = [&](int x0, int y0, int x1, int y1) {
        minX = std::min(minX, x0); minY = std::min(minY, y0);
        maxX = std::max(maxX, x1); maxY = std::max(maxY, y1);
    };

    // Interior tint
    for (const QRect& r : selection.region()) {
        const int x0 = std::max(0, toDeviceX(r.left()));
        const int x1 = std::min(w, toDeviceX(r.right() + 1));
        const int y0 = std::max(0, toDeviceY(r.top()));
        const int y1 = std::min(h, toDeviceY(r.bottom() + 1));
        if (x0 >= x1 || y0 >= y1) continue;
        for (int y = y0; y < y1; ++y) {
            std::fill(bits + y * bpl + x0, bits + y * bpl + x1, kOutlineTintIndex);
        }
        touch(x0, y0, x1 - 1, y1 - 1);
    }

    // Outline: value 1..8 = dash phase, advancing along x + y so that horizontal and vertical
    // runs (and the pixel staircases of curved selections) form one continuous dash pattern.
    auto plot = [&](int x, int y) {
        bits[y * bpl + x] = static_cast<uchar>(1 + (((x + y) / thickness) & (kOutlinePhases - 1)));
    };
    for (const QLine& seg : selection.outlineSegments()) {
        if (seg.y1() == seg.y2()) {
            const int y = std::min(toDeviceY(seg.y1()), h - thickness);
            const int x0 = std::max(0, toDeviceX(std::min(seg.x1(), seg.x2())));
            const int x1 = std::min(w - 1, toDeviceX(std::max(seg.x1(), seg.x2())) + thickness - 1);
            if (y + thickness <= 0 || y >= h || x0 > x1) continue;
            for (int t = std::max(0, y); t < y + thickness; ++t) {
                for (int x = x0; x <= x1; ++x) plot(x, t);
            }
            touch(x0, std::max(0, y), x1, y + thickness - 1);
        } else {
            const int x = std::min(toDeviceX(seg.x1()), w - thickness);
            const int y0 = std::max(0, toDeviceY(std::min(seg.y1(), seg.y2())));
            const int y1 = std::min(h - 1, toDeviceY(std::max(seg.y1(), seg.y2())) + thickness - 1);
            if (x + thickness <= 0 || x >= w || y0 > y1) continue;
            for (int y = y0; y <= y1; ++y) {
                for (int t = std::max(0, x); t < x + thickness; ++t) plot(t, y);
            }
            touch(std::max(0, x), y0, x + thickness - 1, y1);
        }
    }
    if (maxX >= minX && maxY >= minY) {
        m_outlineBounds = QRect(QPoint(minX, minY), QPoint(maxX, maxY));
    }
}

void SoftwareRenderer::drawSelection(QPainter& painter, const Selection& selection, const RenderOptions& opts) {
    if (selection.isEmpty()) return;

    const qreal dpr = opts.devicePixelRatio;
    const QSize deviceSize(static_cast<int>(std::ceil(m_viewportWidth * dpr)),
                           static_cast<int>(std::ceil(m_viewportHeight * dpr)));
    const OutlineKey key{&selection, selection.revision(), opts.zoom, opts.panOffset, deviceSize};
    if (!(key == m_outlineKey)) {
        m_outlineKey = key;
        rebuildOutline(selection, opts);
    }
    if (m_outlineBounds.isEmpty()) return;

    // Marching ants: advance the dash pattern by rotating the colour table.
    m_outline.setColor(0, qRgba(0, 0, 0, 0));
    for (int phase = 0; phase < kOutlinePhases; ++phase) {
        const bool black = ((phase + opts.marchingAntsOffset) & (kOutlinePhases - 1)) < kOutlinePhases / 2;
        m_outline.setColor(1 + phase, black ? qRgb(0, 0, 0) : qRgb(255, 255, 255));
    }
    m_outline.setColor(kOutlineTintIndex, kSelectionTint.rgba());

    QRectF source(m_outlineBounds);
    if (opts.exposedRect.isValid()) {
        const QRect e = opts.exposedRect;
        source = source.intersected(QRectF(e.x() * dpr, e.y() * dpr, e.width() * dpr, e.height() * dpr));
        if (source.isEmpty()) return;
    }
    const QRectF target(source.x() / dpr, source.y() / dpr, source.width() / dpr, source.height() / dpr);
    painter.drawImage(target, m_outline, source);
}

} // namespace pdn
