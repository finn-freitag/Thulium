#pragma once

#include <QPainterPath>
#include <QRegion>
#include <QRect>
#include <QImage>

namespace pdn {

enum class SelectionCombineMode {
    Replace,
    Union,       // Add
    Exclude,     // Subtract
    Intersect,
    Invert
};

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

    bool contains(const QPointF& pt) const;
    bool containsPixel(int x, int y) const;
    QRectF boundingRect() const;

    const QPainterPath& path() const { return m_path; }
    void setPath(const QPainterPath& path);

    const QRegion& region() const { return m_region; }
    void setRegion(const QRegion& region);

    // Raster mask (Format_Grayscale8): 255 = selected, 0 = unselected
    const QImage& mask(int docWidth, int docHeight) const;
    void invalidateMask();

private:
    void combine(const QRegion& newRegion, SelectionCombineMode mode, int docWidth, int docHeight);
    void updatePathFromRegion();

    QRegion m_region;
    QPainterPath m_path;
    SelectionCombineMode m_combineMode = SelectionCombineMode::Replace;
    mutable QImage m_cachedMask;
    mutable bool m_maskDirty = true;
    int m_docWidth = 0;
    int m_docHeight = 0;
};

} // namespace pdn
