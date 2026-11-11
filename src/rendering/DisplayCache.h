#pragma once

#include <QImage>
#include <QRegion>
#include <vector>

namespace pdn {

class Document;

// Display-ready copy of a document's composite, kept as a mip pyramid:
//   level 0 = full resolution, level k = 1/2^k resolution (2x2 box filtered from level k-1).
// All levels are premultiplied ARGB so QPainter can blit them without per-pixel conversion.
//
// Nothing is recomputed eagerly. invalidate() only records dirty rectangles; level() brings
// just the requested rectangle of the requested level up to date. Painting therefore costs
// O(visible pixels) regardless of document size, and an edit only recomposites what it touched.
class DisplayCache {
public:
    // Binds the cache to a document, discarding all levels if the document or its size changed.
    void bind(const Document* doc);
    void clear();
    const Document* document() const { return m_doc; }

    void invalidate(const QRect& docRect);

    int levelCount() const { return static_cast<int>(m_levels.size()); }
    // Coarsest level that still has at least one pixel per device pixel at the given scale.
    int levelForScale(double scale) const;
    // Document pixels covered by one pixel of the given level.
    static int levelStep(int level) { return 1 << level; }

    // Returns the level image, guaranteed current inside levelRect (level coordinates).
    const QImage& level(int index, const QRect& levelRect);

private:
    struct Level {
        QImage image;   // allocated on first use
        QRegion dirty;  // level coordinates
        QSize size;
    };

    void updateLevel(int index, const QRect& levelRect);
    void compositeLevel0(const QRect& rect);
    void downsample(int index, const QRect& rect);

    const Document* m_doc = nullptr;
    QSize m_docSize;
    std::vector<Level> m_levels;
};

} // namespace pdn
