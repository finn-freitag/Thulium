#include "Selection.h"
#include <QPainter>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace pdn {

Selection::Selection(int docWidth, int docHeight)
    : m_docWidth(docWidth), m_docHeight(docHeight) {
}

void Selection::clear() {
    m_region = QRegion();
    m_path = QPainterPath();
    m_maskDirty = true;
}

void Selection::selectAll(int width, int height) {
    if (width > 0 && height > 0) {
        m_docWidth = width;
        m_docHeight = height;
    }
    int w = m_docWidth > 0 ? m_docWidth : width;
    int h = m_docHeight > 0 ? m_docHeight : height;
    m_region = QRegion(0, 0, w, h);
    updatePathFromRegion();
}

void Selection::setDimensions(int width, int height) {
    m_docWidth = width;
    m_docHeight = height;
    if (m_docWidth > 0 && m_docHeight > 0 && !m_region.isEmpty()) {
        m_region = m_region.intersected(QRegion(0, 0, m_docWidth, m_docHeight));
        updatePathFromRegion();
    }
}

void Selection::updatePathFromRegion() {
    m_path = QPainterPath();
    if (!m_region.isEmpty()) {
        m_path.addRegion(m_region);
        m_path = m_path.simplified();
    }
    m_maskDirty = true;
}

void Selection::combine(const QRegion& newRegion, SelectionCombineMode mode, int docWidth, int docHeight) {
    if (docWidth > 0 && docHeight > 0) {
        m_docWidth = docWidth;
        m_docHeight = docHeight;
    }
    switch (mode) {
        case SelectionCombineMode::Replace:
            m_region = newRegion;
            break;
        case SelectionCombineMode::Union:
            m_region = m_region.united(newRegion);
            break;
        case SelectionCombineMode::Exclude:
            m_region = m_region.subtracted(newRegion);
            break;
        case SelectionCombineMode::Intersect:
            m_region = m_region.intersected(newRegion);
            break;
        case SelectionCombineMode::Invert:
            m_region = m_region.xored(newRegion);
            break;
    }
    if (m_docWidth > 0 && m_docHeight > 0) {
        m_region = m_region.intersected(QRegion(0, 0, m_docWidth, m_docHeight));
    }
    updatePathFromRegion();
}

void Selection::addRect(const QRectF& rect, SelectionCombineMode mode) {
    QRect pixelRect = rect.toAlignedRect();
    if (pixelRect.width() <= 0 || pixelRect.height() <= 0) return;
    combine(QRegion(pixelRect), mode, m_docWidth, m_docHeight);
}

void Selection::addEllipse(const QRectF& rect, SelectionCombineMode mode) {
    QRect pixelRect = rect.toAlignedRect();
    if (pixelRect.width() <= 0 || pixelRect.height() <= 0) return;

    QVector<QRect> spans;
    double cx = pixelRect.x() + pixelRect.width() / 2.0;
    double cy = pixelRect.y() + pixelRect.height() / 2.0;
    double rx = pixelRect.width() / 2.0;
    double ry = pixelRect.height() / 2.0;

    int xMax = pixelRect.left() + pixelRect.width() - 1;
    int yMin = pixelRect.top();
    int yMax = pixelRect.top() + pixelRect.height();
    if (m_docHeight > 0) {
        yMin = std::max(0, yMin);
        yMax = std::min(m_docHeight, yMax);
    }

    for (int y = yMin; y < yMax; ++y) {
        double dy = (y + 0.5) - cy;
        double dy_norm = dy / ry;
        double term = 1.0 - (dy_norm * dy_norm);
        if (term < 0.0) continue;

        double dx_max = rx * std::sqrt(term);
        int x_start = static_cast<int>(std::ceil(cx - dx_max - 0.5));
        int x_end = static_cast<int>(std::floor(cx + dx_max - 0.5));

        x_start = std::max(x_start, pixelRect.left());
        x_end = std::min(x_end, xMax);
        if (m_docWidth > 0) {
            x_start = std::max(0, x_start);
            x_end = std::min(m_docWidth - 1, x_end);
        }

        if (x_start <= x_end) {
            spans.append(QRect(x_start, y, x_end - x_start + 1, 1));
        }
    }

    QRegion ellipseRegion;
    if (!spans.isEmpty()) {
        ellipseRegion.setRects(spans.data(), spans.size());
    }
    combine(ellipseRegion, mode, m_docWidth, m_docHeight);
}

void Selection::addPolygon(const QPolygonF& poly, SelectionCombineMode mode) {
    if (poly.size() < 3) return;
    QRect bounds = poly.boundingRect().toAlignedRect().adjusted(-1, -1, 1, 1);
    if (m_docWidth > 0 && m_docHeight > 0) {
        bounds = bounds.intersected(QRect(0, 0, m_docWidth, m_docHeight));
    }
    if (bounds.width() <= 0 || bounds.height() <= 0) {
        combine(QRegion(), mode, m_docWidth, m_docHeight);
        return;
    }

    QImage maskImg(bounds.size(), QImage::Format_Grayscale8);
    maskImg.fill(0);
    QPainter p(&maskImg);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.translate(-bounds.topLeft());
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawPolygon(poly);
    p.end();

    QVector<QRect> spans;
    for (int y = 0; y < maskImg.height(); ++y) {
        const uint8_t* line = maskImg.scanLine(y);
        int spanStart = -1;
        for (int x = 0; x < maskImg.width(); ++x) {
            if (line[x] > 127) {
                if (spanStart == -1) spanStart = x;
            } else {
                if (spanStart != -1) {
                    spans.append(QRect(bounds.x() + spanStart, bounds.y() + y, x - spanStart, 1));
                    spanStart = -1;
                }
            }
        }
        if (spanStart != -1) {
            spans.append(QRect(bounds.x() + spanStart, bounds.y() + y, maskImg.width() - spanStart, 1));
        }
    }

    QRegion polyRegion;
    if (!spans.isEmpty()) {
        polyRegion.setRects(spans.data(), spans.size());
    }
    combine(polyRegion, mode, m_docWidth, m_docHeight);
}

void Selection::addPath(const QPainterPath& path, SelectionCombineMode mode) {
    if (path.isEmpty()) return;
    QRect bounds = path.boundingRect().toAlignedRect().adjusted(-1, -1, 1, 1);
    if (m_docWidth > 0 && m_docHeight > 0) {
        bounds = bounds.intersected(QRect(0, 0, m_docWidth, m_docHeight));
    }
    if (bounds.width() <= 0 || bounds.height() <= 0) {
        combine(QRegion(), mode, m_docWidth, m_docHeight);
        return;
    }

    QImage maskImg(bounds.size(), QImage::Format_Grayscale8);
    maskImg.fill(0);
    QPainter p(&maskImg);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.translate(-bounds.topLeft());
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.fillPath(path, Qt::white);
    p.end();

    QVector<QRect> spans;
    for (int y = 0; y < maskImg.height(); ++y) {
        const uint8_t* line = maskImg.scanLine(y);
        int spanStart = -1;
        for (int x = 0; x < maskImg.width(); ++x) {
            if (line[x] > 127) {
                if (spanStart == -1) spanStart = x;
            } else {
                if (spanStart != -1) {
                    spans.append(QRect(bounds.x() + spanStart, bounds.y() + y, x - spanStart, 1));
                    spanStart = -1;
                }
            }
        }
        if (spanStart != -1) {
            spans.append(QRect(bounds.x() + spanStart, bounds.y() + y, maskImg.width() - spanStart, 1));
        }
    }

    QRegion pathRegion;
    if (!spans.isEmpty()) {
        pathRegion.setRects(spans.data(), spans.size());
    }
    combine(pathRegion, mode, m_docWidth, m_docHeight);
}

void Selection::addRegion(const QRegion& region, SelectionCombineMode mode) {
    combine(region, mode, m_docWidth, m_docHeight);
}

void Selection::invert(int docWidth, int docHeight) {
    if (docWidth > 0 && docHeight > 0) {
        m_docWidth = docWidth;
        m_docHeight = docHeight;
    }
    int w = m_docWidth > 0 ? m_docWidth : docWidth;
    int h = m_docHeight > 0 ? m_docHeight : docHeight;
    QRegion docRegion(0, 0, w, h);
    m_region = docRegion.subtracted(m_region);
    updatePathFromRegion();
}

void Selection::translate(qreal dx, qreal dy) {
    int idx = static_cast<int>(std::round(dx));
    int idy = static_cast<int>(std::round(dy));
    m_region.translate(idx, idy);
    if (m_docWidth > 0 && m_docHeight > 0) {
        m_region = m_region.intersected(QRegion(0, 0, m_docWidth, m_docHeight));
    }
    updatePathFromRegion();
}

bool Selection::contains(const QPointF& pt) const {
    int px = static_cast<int>(std::floor(pt.x()));
    int py = static_cast<int>(std::floor(pt.y()));
    if (m_docWidth > 0 && m_docHeight > 0) {
        if (px < 0 || py < 0 || px >= m_docWidth || py >= m_docHeight) {
            return false;
        }
    }
    if (m_region.isEmpty()) return true; // No selection means entire canvas is active
    return m_region.contains(QPoint(px, py));
}

bool Selection::containsPixel(int x, int y) const {
    if (m_docWidth > 0 && m_docHeight > 0) {
        if (x < 0 || y < 0 || x >= m_docWidth || y >= m_docHeight) {
            return false;
        }
    }
    if (m_region.isEmpty()) return true;
    return m_region.contains(QPoint(x, y));
}

QRectF Selection::boundingRect() const {
    return QRectF(m_region.boundingRect());
}

void Selection::setRegion(const QRegion& region) {
    m_region = region;
    if (m_docWidth > 0 && m_docHeight > 0) {
        m_region = m_region.intersected(QRegion(0, 0, m_docWidth, m_docHeight));
    }
    updatePathFromRegion();
}

void Selection::setPath(const QPainterPath& path) {
    if (path.isEmpty()) {
        clear();
        return;
    }
    QRect bounds = path.boundingRect().toAlignedRect().adjusted(-1, -1, 1, 1);
    if (m_docWidth > 0 && m_docHeight > 0) {
        bounds = bounds.intersected(QRect(0, 0, m_docWidth, m_docHeight));
    }
    if (bounds.width() <= 0 || bounds.height() <= 0) {
        clear();
        return;
    }
    QImage maskImg(bounds.size(), QImage::Format_Grayscale8);
    maskImg.fill(0);
    QPainter p(&maskImg);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.translate(-bounds.topLeft());
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.fillPath(path, Qt::white);
    p.end();

    QVector<QRect> spans;
    for (int y = 0; y < maskImg.height(); ++y) {
        const uint8_t* line = maskImg.scanLine(y);
        int spanStart = -1;
        for (int x = 0; x < maskImg.width(); ++x) {
            if (line[x] > 127) {
                if (spanStart == -1) spanStart = x;
            } else {
                if (spanStart != -1) {
                    spans.append(QRect(bounds.x() + spanStart, bounds.y() + y, x - spanStart, 1));
                    spanStart = -1;
                }
            }
        }
        if (spanStart != -1) {
            spans.append(QRect(bounds.x() + spanStart, bounds.y() + y, maskImg.width() - spanStart, 1));
        }
    }
    m_region = QRegion();
    if (!spans.isEmpty()) {
        m_region.setRects(spans.data(), spans.size());
    }
    if (m_docWidth > 0 && m_docHeight > 0) {
        m_region = m_region.intersected(QRegion(0, 0, m_docWidth, m_docHeight));
    }
    updatePathFromRegion();
}

void Selection::invalidateMask() {
    m_maskDirty = true;
}

const QImage& Selection::mask(int docWidth, int docHeight) const {
    if (!m_maskDirty && m_cachedMask.width() == docWidth && m_cachedMask.height() == docHeight) {
        return m_cachedMask;
    }

    m_cachedMask = QImage(docWidth, docHeight, QImage::Format_Grayscale8);
    if (m_region.isEmpty()) {
        m_cachedMask.fill(255); // Entire canvas selected
    } else {
        m_cachedMask.fill(0);
        QPainter p(&m_cachedMask);
        p.setRenderHint(QPainter::Antialiasing, false);
        for (const QRect& r : m_region) {
            p.fillRect(r, Qt::white);
        }
    }
    m_maskDirty = false;
    return m_cachedMask;
}

} // namespace pdn
