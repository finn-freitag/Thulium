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

    bool isEmpty() const { return m_path.isEmpty(); }
    void clear();
    void selectAll(int width, int height);

    void setCombineMode(SelectionCombineMode mode) { m_combineMode = mode; }
    SelectionCombineMode combineMode() const { return m_combineMode; }

    void addRect(const QRectF& rect, SelectionCombineMode mode = SelectionCombineMode::Replace);
    void addEllipse(const QRectF& rect, SelectionCombineMode mode = SelectionCombineMode::Replace);
    void addPolygon(const QPolygonF& poly, SelectionCombineMode mode = SelectionCombineMode::Replace);
    void addPath(const QPainterPath& path, SelectionCombineMode mode = SelectionCombineMode::Replace);

    void invert(int docWidth, int docHeight);
    void translate(qreal dx, qreal dy);

    bool contains(const QPointF& pt) const;
    bool containsPixel(int x, int y) const;
    QRectF boundingRect() const;

    const QPainterPath& path() const { return m_path; }
    void setPath(const QPainterPath& path);

    // Raster mask (Format_Grayscale8 or Format_Mono): 255 = selected, 0 = unselected
    const QImage& mask(int docWidth, int docHeight) const;
    void invalidateMask();

private:
    void combine(const QPainterPath& newPath, SelectionCombineMode mode, int docWidth, int docHeight);

    QPainterPath m_path;
    SelectionCombineMode m_combineMode = SelectionCombineMode::Replace;
    mutable QImage m_cachedMask;
    mutable bool m_maskDirty = true;
    int m_docWidth = 0;
    int m_docHeight = 0;
};

} // namespace pdn
