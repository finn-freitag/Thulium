#include "BrushTools.h"
#include <QLineF>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace pdn {

namespace {

inline QPoint toPixel(const QPointF& p) {
    return QPoint(static_cast<int>(std::floor(p.x())), static_cast<int>(std::floor(p.y())));
}

// Selection mask row for y, or nullptr when nothing is selected (= everything is editable).
inline const uint8_t* selectionRow(const Document* doc, int y) {
    const Selection& sel = doc->selection();
    return sel.isEmpty() ? nullptr : sel.mask(doc->width(), doc->height()).constScanLine(y);
}

} // namespace

// --- StrokeTool ---

void StrokeTool::deactivate(Document* /*doc*/, ToolContext& /*ctx*/) {
    // Switching tools mid-stroke keeps what has been painted so far
    finishStroke();
    m_coverage = QImage();
}

void StrokeTool::finishStroke() {
    // Wipe just the stroke's footprint so the buffer is clean for the next stroke
    const QRect used = m_edit.dirtyRect();
    for (int y = used.top(); y <= used.bottom() && !used.isEmpty(); ++y) {
        std::fill_n(m_coverage.scanLine(y) + used.left(), used.width(), uint8_t(0));
    }
    m_edit.commit(actionName());
}

void StrokeTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (m_edit.isActive()) finishStroke(); // the release of the previous stroke never arrived
    if (!beginStroke(event, doc, docPos, ctx)) return;
    if (!m_edit.begin(doc)) return;

    if (m_coverage.size() != m_edit.layer()->size()) {
        m_coverage = QImage(m_edit.layer()->size(), QImage::Format_Alpha8);
        m_coverage.fill(0);
    }
    m_lastPos = docPos;
    stroke(doc, docPos, docPos, ctx);
}

void StrokeTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_edit.isActive()) return;
    stroke(doc, m_lastPos, docPos, ctx);
    m_lastPos = docPos;
}

void StrokeTool::mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    if (!m_edit.isActive()) return;
    finishStroke();
}

// Soft brushes: stamps round dabs along the segment. Each dab is fully opaque out to
// hardness * radius and fades smoothly to nothing at the radius. Dabs combine by maximum, so
// overlapping dabs of one stroke never add up.
void StrokeTool::stampSoftSegment(const QPointF& from, const QPointF& to, const ToolContext& ctx) {
    const double radius = std::max(0.5, ctx.brushWidth / 2.0);
    const double inner = radius * std::clamp(ctx.brushHardness, 0, 100) / 100.0;
    const double fade = std::max(radius - inner, 1e-6);
    const double length = QLineF(from, to).length();
    const double spacing = std::max(0.5, radius / 4.0);
    const int steps = std::max(1, static_cast<int>(std::ceil(length / spacing)));
    const int w = m_coverage.width(), h = m_coverage.height();

    for (int i = (length > 0 ? 1 : 0); i <= steps; ++i) {
        const QPointF c = from + (to - from) * (static_cast<double>(i) / steps);
        const int x0 = std::max(0, static_cast<int>(std::floor(c.x() - radius)));
        const int x1 = std::min(w - 1, static_cast<int>(std::ceil(c.x() + radius)));
        const int y0 = std::max(0, static_cast<int>(std::floor(c.y() - radius)));
        const int y1 = std::min(h - 1, static_cast<int>(std::ceil(c.y() + radius)));
        for (int y = y0; y <= y1; ++y) {
            uint8_t* row = m_coverage.scanLine(y);
            const double dy = y + 0.5 - c.y();
            for (int x = x0; x <= x1; ++x) {
                const double dx = x + 0.5 - c.x();
                const double dist = std::sqrt(dx * dx + dy * dy);
                if (dist >= radius) continue;
                double t = dist <= inner ? 1.0 : 1.0 - (dist - inner) / fade;
                t = t * t * (3.0 - 2.0 * t); // smoothstep: no visible ring where the fade starts
                const uint8_t value = static_cast<uint8_t>(t * 255.0 + 0.5);
                if (value > row[x]) row[x] = value;
            }
        }
    }
}

void StrokeTool::stroke(Document* doc, const QPointF& from, const QPointF& to, const ToolContext& ctx) {
    if (ctx.brushHardness < 100) {
        stampSoftSegment(from, to, ctx);
    } else {
        QPainter p(&m_coverage);
        p.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);
        p.setPen(QPen(Qt::black, ctx.brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (from == to) p.drawPoint(to);
        else p.drawLine(from, to);
    }

    const int pad = static_cast<int>(std::ceil(ctx.brushWidth / 2.0)) + 2;
    const QRect dirty = QRectF(from, to).normalized().toAlignedRect().adjusted(-pad, -pad, pad, pad)
                            .intersected(m_coverage.rect());
    if (dirty.isEmpty()) return;

    m_edit.prepare(dirty);
    m_edit.prepare(sourceRectFor(dirty));

    Layer* layer = m_edit.layer();
    for (int y = dirty.top(); y <= dirty.bottom(); ++y) {
        uint32_t* dst = layer->scanLine(y);
        const uint32_t* orig = m_edit.originalRow(y);
        const uint8_t* coverage = m_coverage.constScanLine(y);
        const uint8_t* mask = selectionRow(doc, y);
        if (!mask) {
            composeRow(dst + dirty.left(), orig + dirty.left(), coverage + dirty.left(), dirty.left(), y, dirty.width());
            continue;
        }
        // Only compose the runs of selected pixels
        int x = dirty.left();
        while (x <= dirty.right()) {
            while (x <= dirty.right() && !mask[x]) ++x;
            int runEnd = x;
            while (runEnd <= dirty.right() && mask[runEnd]) ++runEnd;
            if (runEnd > x) composeRow(dst + x, orig + x, coverage + x, x, y, runEnd - x);
            x = runEnd;
        }
    }
    m_edit.markDirty(dirty);
}

// --- PaintbrushTool ---

bool PaintbrushTool::beginStroke(QMouseEvent* event, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& ctx) {
    m_activeColor = (event && event->button() == Qt::RightButton) ? ctx.secondaryColor : ctx.primaryColor;
    m_compositionMode = ctx.compositionMode;
    m_blendMode = ctx.brushBlendMode;
    return true;
}

void PaintbrushTool::composeRow(uint32_t* dst, const uint32_t* original, const uint8_t* coverage,
                                int /*x*/, int /*y*/, int count) const {
    const uint32_t colorRgb = m_activeColor.rgb() & 0x00FFFFFFu;
    const uint32_t colorA = static_cast<uint32_t>(m_activeColor.alpha());
    const uint32_t colorR = m_activeColor.red(), colorG = m_activeColor.green(), colorB = m_activeColor.blue();

    if (m_compositionMode == ColorCompositionMode::DrawOver) {
        for (int i = 0; i < count; ++i) {
            const uint32_t c = coverage[i];
            dst[i] = c ? blendPixel(m_blendMode, original[i], (c << 24) | colorRgb, static_cast<uint8_t>(colorA))
                       : original[i];
        }
        return;
    }

    // Overwrite: the stroke replaces the pixels below, colour and alpha, weighted by coverage.
    for (int i = 0; i < count; ++i) {
        const uint32_t c = coverage[i];
        const uint32_t bg = original[i];
        if (c == 0) {
            dst[i] = bg;
            continue;
        }
        const uint32_t bgA = bg >> 24;
        const uint32_t weightBg = (255 - c) * bgA;
        const uint32_t weightColor = c * colorA;
        const uint32_t total = weightBg + weightColor;
        const uint32_t outA = (total + 127) / 255;
        if (outA == 0) {
            dst[i] = 0;
            continue;
        }
        const uint32_t outR = (weightBg * ((bg >> 16) & 0xFF) + weightColor * colorR + total / 2) / total;
        const uint32_t outG = (weightBg * ((bg >> 8) & 0xFF) + weightColor * colorG + total / 2) / total;
        const uint32_t outB = (weightBg * (bg & 0xFF) + weightColor * colorB + total / 2) / total;
        dst[i] = (outA << 24) | ((outR & 0xFF) << 16) | ((outG & 0xFF) << 8) | (outB & 0xFF);
    }
}

// --- EraserTool ---

bool EraserTool::beginStroke(QMouseEvent* event, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& ctx) {
    // Like Paint.NET, the alpha of the active colour sets how strongly the eraser erases.
    const QColor& c = (event && event->button() == Qt::RightButton) ? ctx.secondaryColor : ctx.primaryColor;
    m_strength = static_cast<uint32_t>(c.alpha());
    return true;
}

void EraserTool::composeRow(uint32_t* dst, const uint32_t* original, const uint8_t* coverage,
                            int /*x*/, int /*y*/, int count) const {
    for (int i = 0; i < count; ++i) {
        const uint32_t erase = (coverage[i] * m_strength + 127) / 255;
        const uint32_t bg = original[i];
        if (erase == 0) {
            dst[i] = bg;
            continue;
        }
        const uint32_t outA = ((bg >> 24) * (255 - erase) + 127) / 255;
        dst[i] = outA == 0 ? 0u : (outA << 24) | (bg & 0x00FFFFFFu);
    }
}

// --- CloneStampTool ---

bool CloneStampTool::beginStroke(QMouseEvent* event, Document* /*doc*/, const QPointF& docPos, ToolContext& ctx) {
    if (event && (event->modifiers() & Qt::ControlModifier)) {
        ctx.cloneSource = docPos;
        ctx.cloneSourceSet = true;
        m_anchor = docPos;
        m_hasAnchor = true;
        return false;
    }
    if (!ctx.cloneSourceSet) return false;
    m_anchor = ctx.cloneSource;
    m_hasAnchor = true;
    m_sourceOffset = toPixel(ctx.cloneSource) - toPixel(docPos);
    return true;
}

void CloneStampTool::composeRow(uint32_t* dst, const uint32_t* original, const uint8_t* coverage,
                                int x, int y, int count) const {
    // Sample from the layer as it was before the stroke, so the stamp never copies itself.
    const QSize sourceSize = edit().layer()->size();
    const int sy = y + m_sourceOffset.y();
    const uint32_t* srcRow = (sy >= 0 && sy < sourceSize.height()) ? edit().originalRow(sy) : nullptr;
    for (int i = 0; i < count; ++i) {
        const uint32_t c = coverage[i];
        const int sx = x + i + m_sourceOffset.x();
        if (c == 0 || !srcRow || sx < 0 || sx >= sourceSize.width()) {
            dst[i] = original[i];
            continue;
        }
        const uint32_t src = srcRow[sx];
        // Cross-fade between the original pixel and the cloned pixel by coverage (in alpha-weighted space)
        const uint32_t bg = original[i];
        const uint32_t wBg = (255 - c) * (bg >> 24);
        const uint32_t wSrc = c * (src >> 24);
        const uint32_t total = wBg + wSrc;
        const uint32_t outA = (total + 127) / 255;
        if (outA == 0) {
            dst[i] = 0;
            continue;
        }
        const uint32_t outR = (wBg * ((bg >> 16) & 0xFF) + wSrc * ((src >> 16) & 0xFF) + total / 2) / total;
        const uint32_t outG = (wBg * ((bg >> 8) & 0xFF) + wSrc * ((src >> 8) & 0xFF) + total / 2) / total;
        const uint32_t outB = (wBg * (bg & 0xFF) + wSrc * (src & 0xFF) + total / 2) / total;
        dst[i] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
    }
}

void CloneStampTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_hasAnchor) return;
    const QPointF vp = opts.panOffset + m_anchor * opts.zoom;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const QPen& pen : {QPen(Qt::white, 3.0), QPen(Qt::black, 1.0)}) {
        painter.setPen(pen);
        painter.drawLine(vp - QPointF(6, 0), vp + QPointF(6, 0));
        painter.drawLine(vp - QPointF(0, 6), vp + QPointF(0, 6));
    }
    painter.restore();
}

// --- PencilTool ---

void PencilTool::deactivate(Document* /*doc*/, ToolContext& /*ctx*/) {
    m_edit.commit("Pencil");
}

void PencilTool::drawSegment(Document* doc, const QPoint& from, const QPoint& to) {
    const QRect touched = QRect(from, to).normalized().adjusted(-1, -1, 1, 1);
    m_edit.prepare(touched);
    {
        QPainter p(&m_edit.layer()->image());
        if (!doc->selection().isEmpty()) {
            p.setClipRegion(doc->selection().region());
        }
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(m_activeColor, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        if (from == to) p.drawPoint(to);
        else p.drawLine(from, to);
    }
    m_edit.markDirty(touched);
}

void PencilTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    m_edit.commit("Pencil"); // no-op unless the previous stroke never got its release
    if (!m_edit.begin(doc)) return;
    m_activeColor = (event && event->button() == Qt::RightButton) ? ctx.secondaryColor : ctx.primaryColor;
    m_lastPixel = toPixel(docPos);
    drawSegment(doc, m_lastPixel, m_lastPixel);
}

void PencilTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!m_edit.isActive()) return;
    const QPoint pixel = toPixel(docPos);
    if (pixel == m_lastPixel) return;
    drawSegment(doc, m_lastPixel, pixel);
    m_lastPixel = pixel;
}

void PencilTool::mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    m_edit.commit("Pencil");
}

// --- ColorPickerTool ---

void ColorPickerTool::sample(Document* doc, const QPointF& docPos, ToolContext& ctx, bool isRightButton) {
    const QPoint px = toPixel(docPos);
    if (px.x() < 0 || px.x() >= doc->width() || px.y() < 0 || px.y() >= doc->height()) return;

    const QColor c = QColor::fromRgba(doc->compositePixel(px.x(), px.y()));
    const bool targetIsPrimary = isRightButton ? !ctx.activeColorIsPrimary : ctx.activeColorIsPrimary;
    QColor& target = targetIsPrimary ? ctx.primaryColor : ctx.secondaryColor;
    if (target == c) return;
    target = c;
    ctx.notifyChanged();
}

void ColorPickerTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    sample(doc, docPos, ctx, event->button() == Qt::RightButton);
}

void ColorPickerTool::mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (event->buttons() & (Qt::LeftButton | Qt::RightButton)) {
        sample(doc, docPos, ctx, event->buttons() & Qt::RightButton);
    }
}

void ColorPickerTool::mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
}

// --- RecolorTool ---

void RecolorTool::deactivate(Document* /*doc*/, ToolContext& /*ctx*/) {
    m_edit.commit("Recolor");
}

void RecolorTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    m_edit.commit("Recolor"); // no-op unless the previous stroke never got its release
    if (!m_edit.begin(doc)) return;
    // Left button replaces the secondary colour with the primary one; right button the reverse.
    const bool swap = event && event->button() == Qt::RightButton;
    m_targetColor = swap ? ctx.primaryColor : ctx.secondaryColor;
    m_replacementColor = swap ? ctx.secondaryColor : ctx.primaryColor;
    m_lastPos = docPos;
    applyRecolor(doc, docPos, ctx);
}

void RecolorTool::mouseMove(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_edit.isActive()) return;
    // Step along the mouse path so fast movements do not leave gaps
    const qreal distance = QLineF(m_lastPos, docPos).length();
    const qreal spacing = std::max(1.0, ctx.brushWidth / 4.0);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / spacing)));
    for (int i = 1; i <= steps; ++i) {
        applyRecolor(doc, m_lastPos + (docPos - m_lastPos) * (static_cast<qreal>(i) / steps), ctx);
    }
    m_lastPos = docPos;
}

void RecolorTool::mouseRelease(QMouseEvent* /*event*/, Document* /*doc*/, const QPointF& /*docPos*/, ToolContext& /*ctx*/) {
    m_edit.commit("Recolor");
}

void RecolorTool::applyRecolor(Document* doc, const QPointF& docPos, const ToolContext& ctx) {
    Layer* layer = m_edit.layer();
    const int radius = std::max(1, ctx.brushWidth / 2);
    const QPoint center = toPixel(docPos);
    const QRect area = QRect(center.x() - radius, center.y() - radius, 2 * radius + 1, 2 * radius + 1)
                           .intersected(layer->image().rect());
    if (area.isEmpty()) return;
    m_edit.prepare(area);

    const int tr = m_targetColor.red(), tg = m_targetColor.green(), tb = m_targetColor.blue();
    const uint32_t replacement = m_replacementColor.rgb() & 0x00FFFFFFu;
    const double tol = (ctx.tolerance / 100.0) * 255.0;
    const double tolSquared = tol * tol;

    for (int y = area.top(); y <= area.bottom(); ++y) {
        uint32_t* line = layer->scanLine(y);
        const uint8_t* mask = selectionRow(doc, y);
        const int dy = y - center.y();
        for (int x = area.left(); x <= area.right(); ++x) {
            const int dx = x - center.x();
            if (dx * dx + dy * dy > radius * radius) continue;
            if (mask && !mask[x]) continue;
            const uint32_t px = line[x];
            const int dr = static_cast<int>((px >> 16) & 0xFF) - tr;
            const int dg = static_cast<int>((px >> 8) & 0xFF) - tg;
            const int db = static_cast<int>(px & 0xFF) - tb;
            if (dr * dr + dg * dg + db * db <= tolSquared) {
                line[x] = (px & 0xFF000000u) | replacement; // keep the pixel's own alpha
            }
        }
    }
    m_edit.markDirty(area);
}

} // namespace pdn
