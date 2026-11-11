#pragma once

#include <QImage>
#include <QLine>
#include <QMutex>
#include <QPainterPath>
#include <QRect>
#include <QRegion>
#include <QTransform>
#include <QVector>
#include <atomic>

namespace pdn {

enum class SelectionCombineMode {
    Replace,
    Union,       // Add
    Exclude,     // Subtract
    Intersect,
    Invert
};

// A pixel-exact selection. The QRegion is the single source of truth; the vector outline,
// the painter path and the raster mask are derived from it lazily and cached.
//
// An empty selection means "nothing is selected", which every editing operation treats as
// "the whole canvas is editable" (see contains()/containsPixel()).
class Selection {
public:
    Selection(int docWidth = 0, int docHeight = 0);

    bool isEmpty() const { return m_region.isEmpty(); }
    void clear();
    void selectAll(int width, int height);

    void setDimensions(int width, int height);
    int docWidth() const { return m_docWidth; }
    int docHeight() const { return m_docHeight; }

    void setCombineMode(SelectionCombineMode mode) { m_combineMode = mode; }
    SelectionCombineMode combineMode() const { return m_combineMode; }

    void addRect(const QRectF& rect, SelectionCombineMode mode = SelectionCombineMode::Replace);
    void addEllipse(const QRectF& rect, SelectionCombineMode mode = SelectionCombineMode::Replace);
    void addPolygon(const QPolygonF& poly, SelectionCombineMode mode = SelectionCombineMode::Replace);
    void addPath(const QPainterPath& path, SelectionCombineMode mode = SelectionCombineMode::Replace);
    void addRegion(const QRegion& region, SelectionCombineMode mode = SelectionCombineMode::Replace);

    void invert(int docWidth, int docHeight);
    void translate(qreal dx, qreal dy);

    // True if pt is editable: inside the selection, or anywhere on the canvas when nothing is selected.
    bool contains(const QPointF& pt) const;
    bool containsPixel(int x, int y) const;
    QRectF boundingRect() const;

    // Simplified vector outline of the region. Built on first use; prefer region() for clipping
    // and outlineSegments() for drawing, both of which are much cheaper for complex selections.
    const QPainterPath& path() const;
    void setPath(const QPainterPath& path);

    const QRegion& region() const { return m_region; }
    void setRegion(const QRegion& region);

    // Boundary of the region as axis-aligned segments on the pixel grid (end coordinates exclusive).
    const QVector<QLine>& outlineSegments() const;

    // Raster mask (Format_Grayscale8): 255 = selected, 0 = unselected. All 255 when nothing is selected.
    const QImage& mask(int docWidth, int docHeight) const;
    void invalidateMask();

    // Pixel-snapped region covered by a shape. These do not modify the selection.
    static QRegion regionFromEllipse(const QRectF& rect);
    static QRegion regionFromPath(const QPainterPath& path, const QRect& clip = QRect());
    // Region of all pixels > 127 in a Format_Grayscale8 image, translated by origin.
    // Region of all pixels > 127 in a Format_Grayscale8 image (optionally only inside area),
    // translated by origin.
    static QRegion regionFromMask(const QImage& grayscale, const QPoint& origin = QPoint(),
                                  const QRect& area = QRect());

    // Bumped on every change; lets caches (e.g. the renderer's outline cache) detect staleness.
    quint64 revision() const { return m_revision; }

private:
    void combine(const QRegion& newRegion, SelectionCombineMode mode);
    void regionChanged();
    QRect docRect() const { return QRect(0, 0, m_docWidth, m_docHeight); }
    bool hasDocSize() const { return m_docWidth > 0 && m_docHeight > 0; }
    void ensureLookupMask() const;

    QRegion m_region;
    SelectionCombineMode m_combineMode = SelectionCombineMode::Replace;
    int m_docWidth = 0;
    int m_docHeight = 0;
    quint64 m_revision = 0;

    mutable QPainterPath m_path;
    mutable bool m_pathDirty = false;
    mutable QVector<QLine> m_outline;
    mutable bool m_outlineDirty = false;

    // Lazily built from worker threads too (effects query containsPixel() in parallel), hence the lock.
    mutable QImage m_cachedMask;
    mutable std::atomic<bool> m_maskDirty{true};
    mutable QMutex m_maskMutex;
};

} // namespace pdn
