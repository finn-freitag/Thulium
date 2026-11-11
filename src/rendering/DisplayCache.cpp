#include "DisplayCache.h"
#include "../core/Document.h"
#include "../core/Parallel.h"
#include <QRgb>
#include <algorithm>
#include <cmath>

namespace pdn {

namespace {

constexpr int kMinLevelDimension = 256;   // stop halving once a level is this small
constexpr int kMaxDirtyRects = 48;        // beyond this, collapse to the bounding box

inline void premultiplyRow(uint32_t* row, int count) {
    for (int i = 0; i < count; ++i) {
        const uint32_t px = row[i];
        const uint32_t a = px >> 24;
        if (a == 255) continue;
        row[i] = a == 0 ? 0u : qPremultiply(px);
    }
}

// Rounded average of four premultiplied pixels, two channels at a time.
inline uint32_t average4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    const uint32_t rb = (((a & 0x00FF00FFu) + (b & 0x00FF00FFu) + (c & 0x00FF00FFu) + (d & 0x00FF00FFu) + 0x00020002u) >> 2) & 0x00FF00FFu;
    const uint32_t ag = ((((a >> 8) & 0x00FF00FFu) + ((b >> 8) & 0x00FF00FFu) + ((c >> 8) & 0x00FF00FFu) + ((d >> 8) & 0x00FF00FFu) + 0x00020002u) >> 2) & 0x00FF00FFu;
    return rb | (ag << 8);
}

} // namespace

void DisplayCache::clear() {
    m_doc = nullptr;
    m_docSize = QSize();
    m_levels.clear();
}

void DisplayCache::bind(const Document* doc) {
    if (doc == m_doc && doc && doc->size() == m_docSize) return;
    clear();
    if (!doc || doc->width() <= 0 || doc->height() <= 0) return;
    m_doc = doc;
    m_docSize = doc->size();

    QSize size = m_docSize;
    while (true) {
        Level level;
        level.size = size;
        level.dirty = QRect(QPoint(0, 0), size);
        m_levels.push_back(level);
        if (std::min(size.width(), size.height()) <= kMinLevelDimension) break;
        size = QSize((size.width() + 1) / 2, (size.height() + 1) / 2);
    }
}

void DisplayCache::invalidate(const QRect& docRect) {
    const QRect clipped = docRect.intersected(QRect(QPoint(0, 0), m_docSize));
    if (clipped.isEmpty()) return;
    for (int i = 0; i < levelCount(); ++i) {
        Level& level = m_levels[i];
        const QRect r(QPoint(clipped.left() >> i, clipped.top() >> i),
                      QPoint(clipped.right() >> i, clipped.bottom() >> i));
        level.dirty += r;
        if (level.dirty.rectCount() > kMaxDirtyRects) {
            level.dirty = level.dirty.boundingRect();
        }
    }
}

int DisplayCache::levelForScale(double scale) const {
    if (scale >= 1.0 || m_levels.empty()) return 0;
    const int level = static_cast<int>(std::floor(std::log2(1.0 / scale)));
    return std::clamp(level, 0, levelCount() - 1);
}

const QImage& DisplayCache::level(int index, const QRect& levelRect) {
    updateLevel(index, levelRect);
    return m_levels[index].image;
}

void DisplayCache::updateLevel(int index, const QRect& levelRect) {
    Level& level = m_levels[index];
    if (level.image.isNull()) {
        level.image = QImage(level.size, QImage::Format_ARGB32_Premultiplied);
        level.dirty = QRect(QPoint(0, 0), level.size);
    }
    const QRect wanted = levelRect.intersected(QRect(QPoint(0, 0), level.size));
    const QRegion todo = level.dirty.intersected(wanted);
    if (todo.isEmpty()) return;

    for (const QRect& r : todo) {
        if (index == 0) {
            compositeLevel0(r);
        } else {
            const QSize parentSize = m_levels[index - 1].size;
            const QRect parentRect = QRect(r.left() * 2, r.top() * 2, r.width() * 2, r.height() * 2)
                                         .intersected(QRect(QPoint(0, 0), parentSize));
            updateLevel(index - 1, parentRect);
            downsample(index, r);
        }
    }
    m_levels[index].dirty -= todo;
}

void DisplayCache::compositeLevel0(const QRect& rect) {
    QImage& image = m_levels[0].image;
    uchar* bits = image.bits();
    const qsizetype bpl = image.bytesPerLine();
    const Document* doc = m_doc;
    parallelFor(rect.top(), rect.bottom() + 1, 64, [&](int y0, int y1) {
        auto* dst = reinterpret_cast<uint32_t*>(bits + y0 * bpl) + rect.left();
        doc->compositeRect(QRect(rect.left(), y0, rect.width(), y1 - y0), dst, bpl);
        for (int y = y0; y < y1; ++y) {
            premultiplyRow(reinterpret_cast<uint32_t*>(bits + y * bpl) + rect.left(), rect.width());
        }
    });
}

void DisplayCache::downsample(int index, const QRect& rect) {
    const QImage& src = m_levels[index - 1].image;
    QImage& dst = m_levels[index].image;
    uchar* dstBits = dst.bits();
    const qsizetype dstBpl = dst.bytesPerLine();
    const int srcW = src.width();
    const int srcH = src.height();
    parallelFor(rect.top(), rect.bottom() + 1, 64, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            // Odd-sized parents have no partner row/column at the far edge: reuse the last one.
            const auto* s0 = reinterpret_cast<const uint32_t*>(src.constScanLine(2 * y));
            const auto* s1 = reinterpret_cast<const uint32_t*>(src.constScanLine(std::min(2 * y + 1, srcH - 1)));
            auto* d = reinterpret_cast<uint32_t*>(dstBits + y * dstBpl);
            for (int x = rect.left(); x <= rect.right(); ++x) {
                const int x0 = 2 * x;
                const int x1 = std::min(x0 + 1, srcW - 1);
                d[x] = average4(s0[x0], s0[x1], s1[x0], s1[x1]);
            }
        }
    });
}

} // namespace pdn
