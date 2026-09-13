#include "Selection.h"
#include <QPainter>

namespace pdn {

Selection::Selection(int docWidth, int docHeight)
    : m_docWidth(docWidth), m_docHeight(docHeight) {
}

void Selection::clear() {
    m_path = QPainterPath();
    m_maskDirty = true;
}

void Selection::selectAll(int width, int height) {
    m_docWidth = width;
    m_docHeight = height;
    m_path = QPainterPath();
    m_path.addRect(0, 0, width, height);
    m_maskDirty = true;
}

void Selection::combine(const QPainterPath& newPath, SelectionCombineMode mode, int docWidth, int docHeight) {
    m_docWidth = docWidth;
    m_docHeight = docHeight;
    switch (mode) {
        case SelectionCombineMode::Replace:
            m_path = newPath;
            break;
        case SelectionCombineMode::Union:
            m_path = m_path.united(newPath);
            break;
        case SelectionCombineMode::Exclude:
            m_path = m_path.subtracted(newPath);
            break;
        case SelectionCombineMode::Intersect:
            m_path = m_path.intersected(newPath);
            break;
        case SelectionCombineMode::Invert: {
            QPainterPath docPath;
            docPath.addRect(0, 0, docWidth > 0 ? docWidth : 10000, docHeight > 0 ? docHeight : 10000);
            QPainterPath invertedNew = docPath.subtracted(newPath);
            m_path = m_path.intersected(invertedNew);
            break;
        }
    }
    m_maskDirty = true;
}

void Selection::addRect(const QRectF& rect, SelectionCombineMode mode) {
    QPainterPath p;
    p.addRect(rect);
    combine(p, mode, m_docWidth, m_docHeight);
}

void Selection::addEllipse(const QRectF& rect, SelectionCombineMode mode) {
    QPainterPath p;
    p.addEllipse(rect);
    combine(p, mode, m_docWidth, m_docHeight);
}

void Selection::addPolygon(const QPolygonF& poly, SelectionCombineMode mode) {
    QPainterPath p;
    p.addPolygon(poly);
    p.closeSubpath();
    combine(p, mode, m_docWidth, m_docHeight);
}

void Selection::addPath(const QPainterPath& path, SelectionCombineMode mode) {
    combine(path, mode, m_docWidth, m_docHeight);
}

void Selection::invert(int docWidth, int docHeight) {
    m_docWidth = docWidth;
    m_docHeight = docHeight;
    QPainterPath docPath;
    docPath.addRect(0, 0, docWidth, docHeight);
    m_path = docPath.subtracted(m_path);
    m_maskDirty = true;
}

void Selection::translate(qreal dx, qreal dy) {
    m_path.translate(dx, dy);
    m_maskDirty = true;
}

bool Selection::contains(const QPointF& pt) const {
    if (m_path.isEmpty()) return true; // No selection means entire canvas is active
    return m_path.contains(pt);
}

bool Selection::containsPixel(int x, int y) const {
    if (m_path.isEmpty()) return true;
    return m_path.contains(QPointF(x + 0.5, y + 0.5));
}

QRectF Selection::boundingRect() const {
    return m_path.boundingRect();
}

void Selection::setPath(const QPainterPath& path) {
    m_path = path;
    m_maskDirty = true;
}

void Selection::invalidateMask() {
    m_maskDirty = true;
}

const QImage& Selection::mask(int docWidth, int docHeight) const {
    if (!m_maskDirty && m_cachedMask.width() == docWidth && m_cachedMask.height() == docHeight) {
        return m_cachedMask;
    }

    m_cachedMask = QImage(docWidth, docHeight, QImage::Format_Grayscale8);
    if (m_path.isEmpty()) {
        m_cachedMask.fill(255); // Entire canvas selected
    } else {
        m_cachedMask.fill(0);
        QPainter p(&m_cachedMask);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.fillPath(m_path, Qt::white);
    }
    m_maskDirty = false;
    return m_cachedMask;
}

} // namespace pdn
