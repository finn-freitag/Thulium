#include "FillTools.h"
#include "../core/FloodFill.h"
#include "../core/Parallel.h"
#include <QLineF>
#include <QPainter>
#include <algorithm>
#include <array>
#include <cmath>

namespace pdn {

namespace {

constexpr int kGradientLutSize = 1024;

// Straight-alpha ARGB lookup table from c0 (t = 0) to c1 (t = 1), interpolated in premultiplied
// space so that fades to transparent do not pick up the colour of the transparent end.
std::array<uint32_t, kGradientLutSize> buildGradientLut(const QColor& c0, const QColor& c1) {
    std::array<uint32_t, kGradientLutSize> lut;
    const double a0 = c0.alphaF(), a1 = c1.alphaF();
    for (int i = 0; i < kGradientLutSize; ++i) {
        const double t = static_cast<double>(i) / (kGradientLutSize - 1);
        const double a = a0 + (a1 - a0) * t;
        auto channel = [&](double v0, double v1) {
            if (a <= 0.0) return 0;
            const double premult = v0 * a0 + (v1 * a1 - v0 * a0) * t;
            return std::clamp(static_cast<int>(std::lround(premult / a * 255.0)), 0, 255);
        };
        lut[i] = (static_cast<uint32_t>(std::lround(a * 255.0)) << 24) |
                 (channel(c0.redF(), c1.redF()) << 16) |
                 (channel(c0.greenF(), c1.greenF()) << 8) |
                 channel(c0.blueF(), c1.blueF());
    }
    return lut;
}

inline uint32_t toArgb(const QColor& c) {
    return c.rgba();
}

} // namespace

// --- PaintBucketTool ---
void PaintBucketTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    const QPoint seed(static_cast<int>(std::floor(docPos.x())), static_cast<int>(std::floor(docPos.y())));
    LayerEdit edit;
    if (!edit.begin(doc) || !edit.layer()->image().rect().contains(seed)) return;

    const QColor fillColor = (event && event->button() == Qt::RightButton) ? ctx.secondaryColor : ctx.primaryColor;
    const uint32_t fill = toArgb(fillColor);
    if (fill >> 24 == 0 && ctx.compositionMode == ColorCompositionMode::DrawOver) return; // nothing to paint

    const Selection& sel = doc->selection();
    const QImage* selectionMask = sel.isEmpty() ? nullptr : &sel.mask(doc->width(), doc->height());
    const FloodMask flood = floodFill(edit.layer()->image(), seed, ctx.tolerance,
                                      ctx.floodMode == FloodMode::Contiguous, selectionMask);
    if (flood.isEmpty()) return;

    Layer* layer = edit.layer();
    const QRect b = flood.bounds;
    edit.prepare(b);
    layer->bits(); // detach before the worker threads write
    const BlendMode blendMode = ctx.brushBlendMode;
    // An opaque colour in Normal mode simply replaces the pixel
    const bool overwrite = ctx.compositionMode == ColorCompositionMode::Overwrite ||
                           ((fill >> 24) == 255 && blendMode == BlendMode::Normal);
    parallelFor(b.top(), b.bottom() + 1, 64, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            uint32_t* line = layer->scanLine(y);
            const uint8_t* filled = flood.row(y);
            for (int x = b.left(); x <= b.right(); ++x) {
                if (!filled[x]) continue;
                line[x] = overwrite ? fill : blendPixel(blendMode, line[x], fill, 255);
            }
        }
    });

    edit.markDirty(b);
    edit.commit("Paint Bucket");
}

// --- GradientTool ---
void GradientTool::deactivate(Document* /*doc*/, ToolContext& /*ctx*/) {
    m_edit.commit("Gradient");
}

void GradientTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    m_edit.commit("Gradient"); // no-op unless the previous drag never got its release
    if (!m_edit.begin(doc)) return;
    const bool reversed = event && event->button() == Qt::RightButton;
    m_startColor = reversed ? ctx.secondaryColor : ctx.primaryColor;
    m_endColor = reversed ? ctx.primaryColor : ctx.secondaryColor;
    m_startPos = docPos;
    m_endPos = docPos;
}

void GradientTool::mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_edit.isActive()) return;
    m_endPos = docPos;
    if (event && (event->modifiers() & Qt::ShiftModifier)) {
        // Constrain the direction to multiples of 15 degrees
        QLineF line(m_startPos, m_endPos);
        line.setAngle(std::round(line.angle() / 15.0) * 15.0);
        m_endPos = line.p2();
    }
    renderGradient(doc, ctx);
}

void GradientTool::mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_edit.isActive()) return;
    mouseMove(event, doc, docPos, ctx);
    m_edit.commit("Gradient");
}

void GradientTool::keyPress(QKeyEvent* event, Document* /*doc*/, ToolContext& /*ctx*/) {
    if (event->key() == Qt::Key_Escape && m_edit.isActive()) {
        m_edit.cancel();
        event->accept();
    }
}

void GradientTool::renderGradient(Document* doc, const ToolContext& ctx) {
    Layer* layer = m_edit.layer();
    const Selection& sel = doc->selection();
    const QRect area = sel.isEmpty() ? layer->image().rect()
                                     : sel.region().boundingRect().intersected(layer->image().rect());
    if (area.isEmpty()) return;
    m_edit.prepare(area);
    const QImage* mask = sel.isEmpty() ? nullptr : &sel.mask(doc->width(), doc->height());

    const auto lut = buildGradientLut(m_startColor, m_endColor);
    const double sx = m_startPos.x(), sy = m_startPos.y();
    const double dx = m_endPos.x() - sx, dy = m_endPos.y() - sy;
    const double lengthSquared = std::max(dx * dx + dy * dy, 1e-6);
    const double length = std::sqrt(lengthSquared);
    const double baseAngle = std::atan2(dy, dx);
    const GradientMode mode = ctx.gradientMode;
    const bool overwrite = ctx.compositionMode == ColorCompositionMode::Overwrite;
    const BlendMode blendMode = ctx.brushBlendMode;

    // Position along the gradient, 0 at the start colour and 1 at the end colour.
    auto gradientPosition = [&](double px, double py) -> double {
        const double rx = px - sx, ry = py - sy;
        const double along = (rx * dx + ry * dy) / lengthSquared;   // projection onto the drag line
        const double across = (rx * dy - ry * dx) / lengthSquared;  // perpendicular distance, same units
        switch (mode) {
            case GradientMode::Linear:          return along;
            case GradientMode::LinearReflected: return std::abs(along);
            case GradientMode::Radial:          return std::sqrt(rx * rx + ry * ry) / length;
            case GradientMode::Diamond:         return std::abs(along) + std::abs(across);
            case GradientMode::Conical: {
                double turn = (std::atan2(ry, rx) - baseAngle) / (2.0 * M_PI);
                turn -= std::floor(turn);
                return turn <= 0.5 ? turn * 2.0 : (1.0 - turn) * 2.0;
            }
            case GradientMode::Spiral: {
                double turn = (std::atan2(ry, rx) - baseAngle) / (2.0 * M_PI);
                turn -= std::floor(turn);
                double t = std::sqrt(rx * rx + ry * ry) / length + turn;
                t -= std::floor(t);
                return t <= 0.5 ? t * 2.0 : (1.0 - t) * 2.0;
            }
        }
        return along;
    };

    layer->bits(); // detach before the worker threads write
    parallelFor(area.top(), area.bottom() + 1, 32, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            uint32_t* dst = layer->scanLine(y);
            const uint32_t* orig = m_edit.originalRow(y);
            const uint8_t* maskRow = mask ? mask->constScanLine(y) : nullptr;
            for (int x = area.left(); x <= area.right(); ++x) {
                if (maskRow && !maskRow[x]) {
                    dst[x] = orig[x];
                    continue;
                }
                const double t = std::clamp(gradientPosition(x + 0.5, y + 0.5), 0.0, 1.0);
                const uint32_t color = lut[static_cast<int>(t * (kGradientLutSize - 1) + 0.5)];
                dst[x] = overwrite ? color : blendPixel(blendMode, orig[x], color, 255);
            }
        }
    });
    m_edit.markDirty(area);
}

void GradientTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_edit.isActive()) return;

    const QPointF vpStart = opts.panOffset + m_startPos * opts.zoom;
    const QPointF vpEnd = opts.panOffset + m_endPos * opts.zoom;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(Qt::white, 2.0));
    painter.drawLine(vpStart, vpEnd);
    painter.setPen(QPen(Qt::black, 1.0, Qt::DashLine));
    painter.drawLine(vpStart, vpEnd);

    painter.setPen(QPen(Qt::black, 1.0));
    painter.setBrush(Qt::white);
    painter.drawEllipse(vpStart, 4.0, 4.0);
    painter.drawEllipse(vpEnd, 4.0, 4.0);
    painter.restore();
}

} // namespace pdn
