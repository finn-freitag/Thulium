#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QRegion>
#include <vector>

namespace pdn {

// Result of a colour-similarity fill: one byte per pixel (non-zero = filled) plus the filled bounds.
struct FloodMask {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> filled;
    QRect bounds;

    bool isEmpty() const { return bounds.isEmpty(); }
    const uint8_t* row(int y) const { return filled.data() + static_cast<size_t>(y) * width; }
    QRegion toRegion() const;
};

// Finds the pixels of image (Format_ARGB32) whose colour is within tolerancePercent (0..100)
// of the pixel at seed.
//   contiguous = true : only pixels connected to the seed (4-neighbourhood), like a paint bucket
//   contiguous = false: every matching pixel in the image ("global" flood mode)
// If selectionMask (Format_Grayscale8, same size) is given, pixels where it is 0 are never filled.
FloodMask floodFill(const QImage& image, const QPoint& seed, int tolerancePercent, bool contiguous,
                    const QImage* selectionMask = nullptr);

} // namespace pdn
