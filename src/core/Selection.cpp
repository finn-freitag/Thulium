#include "Selection.h"
#include <QMutexLocker>
#include <QPainter>
#include <algorithm>
#include <climits>
#include <cmath>

namespace pdn {

namespace {

// Builds a QRegion from horizontal spans delivered row by row, top to bottom. Consecutive rows
// with identical spans are merged into taller rectangles, which yields QRegion's canonical
// banded form: equal areas compare equal, and plain shapes stay a handful of rectangles.
class BandedRegionBuilder {
public:
    // spans holds sorted [left, rightExclusive) pairs of disjoint, non-touching intervals.
    void addRow(int y, const QVector<int>& spans) {
        if (!spans.isEmpty() && y == m_lastY + 1 && spans == m_bandSpans) {
            for (int i = m_bandStart; i < m_rects.size(); ++i) m_rects[i].setBottom(y);
        } else {
            m_bandStart = m_rects.size();
            m_bandSpans = spans;
            for (int i = 0; i + 1 < spans.size(); i += 2) {
                m_rects.append(QRect(spans[i], y, spans[i + 1] - spans[i], 1));
            }
        }
        m_lastY = y;
    }

    QRegion build() const {
        QRegion region;
        if (!m_rects.isEmpty()) {
            region.setRects(m_rects.constData(), m_rects.size());
        }
        return region;
    }

private:
    QVector<QRect> m_rects;
    QVector<int> m_bandSpans;
    int m_bandStart = 0;
    int m_lastY = INT_MIN;
};

// Appends the horizontal boundary between two rows of x-intervals. Each list holds the sorted
// [left, rightExclusive) endpoints of disjoint intervals; the boundary is their symmetric
// difference, which is simply the gaps between consecutive pairs of the merged endpoints.
void appendHorizontalEdges(const QVector<int>& above, const QVector<int>& below, int y,
                           QVector<int>& scratch, QVector<QLine>& out) {
    scratch.resize(above.size() + below.size());
    std::merge(above.cbegin(), above.cend(), below.cbegin(), below.cend(), scratch.begin());
    for (int i = 0; i + 1 < scratch.size(); i += 2) {
        if (scratch[i] != scratch[i + 1]) {
            out.append(QLine(scratch[i], y, scratch[i + 1], y));
        }
    }
}

} // namespace

QRegion Selection::regionFromMask(const QImage& coverage, const QPoint& origin, const QRect& area) {
    const QRect scan = area.isValid() ? area.intersected(coverage.rect()) : coverage.rect();
    BandedRegionBuilder builder;
    QVector<int> spans;
    for (int y = scan.top(); y <= scan.bottom(); ++y) {
        const uint8_t* line = coverage.constScanLine(y);
        spans.clear();
        int spanStart = -1;
        for (int x = scan.left(); x <= scan.right() + 1; ++x) {
            const bool inside = x <= scan.right() && line[x] > 127;
            if (inside && spanStart < 0) {
                spanStart = x;
            } else if (!inside && spanStart >= 0) {
                spans << origin.x() + spanStart << origin.x() + x;
                spanStart = -1;
            }
        }
        builder.addRow(origin.y() + y, spans);
    }
    return builder.build();
}

Selection::Selection(int docWidth, int docHeight)
    : m_docWidth(docWidth), m_docHeight(docHeight) {
}

void Selection::regionChanged() {
    if (hasDocSize() && !m_region.isEmpty()) {
        m_region = m_region.intersected(docRect());
    }
    ++m_revision;
    m_pathDirty = true;
    m_outlineDirty = true;
    m_maskDirty = true;
}

void Selection::clear() {
    m_region = QRegion();
    regionChanged();
}

void Selection::selectAll(int width, int height) {
    if (width > 0 && height > 0) {
        m_docWidth = width;
        m_docHeight = height;
    }
    m_region = QRegion(0, 0, m_docWidth, m_docHeight);
    regionChanged();
}

void Selection::setDimensions(int width, int height) {
    m_docWidth = width;
    m_docHeight = height;
    regionChanged();
}

void Selection::combine(const QRegion& newRegion, SelectionCombineMode mode) {
    switch (mode) {
        case SelectionCombineMode::Replace:   m_region = newRegion; break;
        case SelectionCombineMode::Union:     m_region = m_region.united(newRegion); break;
        case SelectionCombineMode::Exclude:   m_region = m_region.subtracted(newRegion); break;
        case SelectionCombineMode::Intersect: m_region = m_region.intersected(newRegion); break;
        case SelectionCombineMode::Invert:    m_region = m_region.xored(newRegion); break;
    }
    regionChanged();
}

QRegion Selection::regionFromEllipse(const QRectF& rect) {
    const QRect pixelRect = rect.toAlignedRect();
    if (pixelRect.width() <= 0 || pixelRect.height() <= 0) return QRegion();

    // A pixel belongs to the ellipse when its centre lies inside it.
    const double cx = pixelRect.x() + pixelRect.width() / 2.0;
    const double cy = pixelRect.y() + pixelRect.height() / 2.0;
    const double rx = pixelRect.width() / 2.0;
    const double ry = pixelRect.height() / 2.0;

    BandedRegionBuilder builder;
    for (int y = pixelRect.top(); y <= pixelRect.bottom(); ++y) {
        const double dyNorm = ((y + 0.5) - cy) / ry;
        const double term = 1.0 - dyNorm * dyNorm;
        QVector<int> spans;
        if (term >= 0.0) {
            const double dxMax = rx * std::sqrt(term);
            const int xStart = std::max(static_cast<int>(std::ceil(cx - dxMax - 0.5)), pixelRect.left());
            const int xEnd = std::min(static_cast<int>(std::floor(cx + dxMax - 0.5)), pixelRect.right());
            if (xStart <= xEnd) spans << xStart << xEnd + 1;
        }
        builder.addRow(y, spans);
    }
    return builder.build();
}

QRegion Selection::regionFromPath(const QPainterPath& path, const QRect& clip) {
    if (path.isEmpty()) return QRegion();
    QRect bounds = path.boundingRect().toAlignedRect().adjusted(-1, -1, 1, 1);
    if (clip.isValid()) {
        bounds = bounds.intersected(clip);
    }
    if (bounds.width() <= 0 || bounds.height() <= 0) return QRegion();

    QImage coverage(bounds.size(), QImage::Format_Grayscale8);
    coverage.fill(0);
    QPainter p(&coverage);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.translate(-bounds.topLeft());
    p.fillPath(path, Qt::white);
    p.end();

    return regionFromMask(coverage, bounds.topLeft());
}

void Selection::addRect(const QRectF& rect, SelectionCombineMode mode) {
    const QRect pixelRect = rect.toAlignedRect();
    if (pixelRect.width() <= 0 || pixelRect.height() <= 0) return;
    combine(QRegion(pixelRect), mode);
}

void Selection::addEllipse(const QRectF& rect, SelectionCombineMode mode) {
    const QRect pixelRect = rect.toAlignedRect();
    if (pixelRect.width() <= 0 || pixelRect.height() <= 0) return;
    combine(regionFromEllipse(rect), mode);
}

void Selection::addPolygon(const QPolygonF& poly, SelectionCombineMode mode) {
    if (poly.size() < 3) return;
    QPainterPath path;
    path.addPolygon(poly);
    path.closeSubpath();
    combine(regionFromPath(path, hasDocSize() ? docRect() : QRect()), mode);
}

void Selection::addPath(const QPainterPath& path, SelectionCombineMode mode) {
    if (path.isEmpty()) return;
    combine(regionFromPath(path, hasDocSize() ? docRect() : QRect()), mode);
}

void Selection::addRegion(const QRegion& region, SelectionCombineMode mode) {
    combine(region, mode);
}

void Selection::invert(int docWidth, int docHeight) {
    if (docWidth > 0 && docHeight > 0) {
        m_docWidth = docWidth;
        m_docHeight = docHeight;
    }
    m_region = QRegion(docRect()).subtracted(m_region);
    regionChanged();
}

void Selection::translate(qreal dx, qreal dy) {
    m_region.translate(static_cast<int>(std::round(dx)), static_cast<int>(std::round(dy)));
    regionChanged();
}

bool Selection::contains(const QPointF& pt) const {
    return containsPixel(static_cast<int>(std::floor(pt.x())), static_cast<int>(std::floor(pt.y())));
}

bool Selection::containsPixel(int x, int y) const {
    if (!hasDocSize()) {
        return m_region.isEmpty() || m_region.contains(QPoint(x, y));
    }
    if (x < 0 || y < 0 || x >= m_docWidth || y >= m_docHeight) return false;
    if (m_region.isEmpty()) return true;

    // QRegion::contains() walks the rectangle list; the mask makes per-pixel queries O(1),
    // which matters for effects and brushes that test every pixel they touch.
    ensureLookupMask();
    return m_cachedMask.constScanLine(y)[x] != 0;
}

QRectF Selection::boundingRect() const {
    return QRectF(m_region.boundingRect());
}

void Selection::setRegion(const QRegion& region) {
    m_region = region;
    regionChanged();
}

void Selection::setPath(const QPainterPath& path) {
    m_region = regionFromPath(path, hasDocSize() ? docRect() : QRect());
    regionChanged();
}

const QPainterPath& Selection::path() const {
    if (m_pathDirty) {
        m_path = QPainterPath();
        if (!m_region.isEmpty()) {
            m_path.addRegion(m_region);
            m_path = m_path.simplified();
        }
        m_pathDirty = false;
    }
    return m_path;
}

const QVector<QLine>& Selection::outlineSegments() const {
    if (!m_outlineDirty) return m_outline;
    m_outline.clear();
    m_outlineDirty = false;
    if (m_region.isEmpty()) return m_outline;

    // QRegion stores its rectangles in y-x bands: every rectangle in a band shares the same
    // top and bottom, and bands are sorted top to bottom. Vertical edges come straight from the
    // rectangles; horizontal edges are the difference between vertically adjacent bands.
    QVector<int> prevBand, curBand, scratch;
    int prevBottom = 0; // exclusive bottom of the previous band
    bool havePrev = false;

    auto it = m_region.begin();
    const auto end = m_region.end();
    while (it != end) {
        const int top = it->top();
        const int bottom = it->bottom() + 1;
        curBand.clear();
        for (; it != end && it->top() == top; ++it) {
            const int left = it->left();
            const int right = it->right() + 1;
            m_outline.append(QLine(left, top, left, bottom));
            m_outline.append(QLine(right, top, right, bottom));
            // Rectangles within a band never touch, so the endpoint list stays strictly sorted.
            curBand.append(left);
            curBand.append(right);
        }

        if (havePrev && prevBottom == top) {
            appendHorizontalEdges(prevBand, curBand, top, scratch, m_outline);
        } else {
            if (havePrev) appendHorizontalEdges(prevBand, {}, prevBottom, scratch, m_outline);
            appendHorizontalEdges({}, curBand, top, scratch, m_outline);
        }
        prevBand.swap(curBand);
        prevBottom = bottom;
        havePrev = true;
    }
    appendHorizontalEdges(prevBand, {}, prevBottom, scratch, m_outline);
    return m_outline;
}

void Selection::invalidateMask() {
    m_maskDirty = true;
}

void Selection::ensureLookupMask() const {
    if (!m_maskDirty.load(std::memory_order_acquire) &&
        m_cachedMask.width() == m_docWidth && m_cachedMask.height() == m_docHeight) {
        return;
    }
    mask(m_docWidth, m_docHeight);
}

const QImage& Selection::mask(int docWidth, int docHeight) const {
    QMutexLocker lock(&m_maskMutex);
    if (!m_maskDirty.load(std::memory_order_relaxed) &&
        m_cachedMask.width() == docWidth && m_cachedMask.height() == docHeight) {
        return m_cachedMask;
    }

    QImage maskImage(docWidth, docHeight, QImage::Format_Grayscale8);
    if (m_region.isEmpty()) {
        maskImage.fill(255); // Nothing selected: the whole canvas is editable
    } else {
        maskImage.fill(0);
        const QRect bounds(0, 0, docWidth, docHeight);
        for (const QRect& r : m_region) {
            const QRect c = r.intersected(bounds);
            if (c.isEmpty()) continue;
            for (int y = c.top(); y <= c.bottom(); ++y) {
                std::fill_n(maskImage.scanLine(y) + c.left(), c.width(), uint8_t(255));
            }
        }
    }
    m_cachedMask = maskImage;
    m_maskDirty.store(false, std::memory_order_release);
    return m_cachedMask;
}

} // namespace pdn
