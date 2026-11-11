#include "FloodFill.h"
#include "Parallel.h"
#include "Selection.h"
#include <QVector>
#include <algorithm>
#include <mutex>

namespace pdn {

namespace {

constexpr uint8_t kFilled = 255;

// Colour distance test against a fixed target, in squared RGBA space (no sqrt per pixel).
struct ColorMatcher {
    uint32_t target;
    int tr, tg, tb, ta;
    int64_t maxDistSquared;

    ColorMatcher(uint32_t targetPixel, int tolerancePercent) : target(targetPixel) {
        tr = (target >> 16) & 0xFF;
        tg = (target >> 8) & 0xFF;
        tb = target & 0xFF;
        ta = (target >> 24) & 0xFF;
        // 100% tolerance spans the full diagonal of the RGBA cube
        const double maxDist = (std::clamp(tolerancePercent, 0, 100) / 100.0) * 510.0;
        maxDistSquared = static_cast<int64_t>(maxDist * maxDist);
    }

    bool operator()(uint32_t px) const {
        if (px == target) return true;
        const int dr = static_cast<int>((px >> 16) & 0xFF) - tr;
        const int dg = static_cast<int>((px >> 8) & 0xFF) - tg;
        const int db = static_cast<int>(px & 0xFF) - tb;
        const int da = static_cast<int>((px >> 24) & 0xFF) - ta;
        return dr * dr + dg * dg + db * db + da * da <= maxDistSquared;
    }
};

} // namespace

QRegion FloodMask::toRegion() const {
    if (isEmpty()) return QRegion();
    // View the byte buffer as an 8-bit image (no copy) and let the selection code trace it
    const QImage view(filled.data(), width, height, width, QImage::Format_Grayscale8);
    return Selection::regionFromMask(view, QPoint(), bounds);
}

FloodMask floodFill(const QImage& image, const QPoint& seed, int tolerancePercent, bool contiguous,
                    const QImage* selectionMask) {
    FloodMask result;
    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0 || !image.rect().contains(seed)) return result;

    result.width = w;
    result.height = h;
    result.filled.assign(static_cast<size_t>(w) * h, 0);

    auto pixels = [&](int y) { return reinterpret_cast<const uint32_t*>(image.constScanLine(y)); };
    auto allowed = [&](int y) -> const uint8_t* { return selectionMask ? selectionMask->constScanLine(y) : nullptr; };
    const ColorMatcher matches(pixels(seed.y())[seed.x()], tolerancePercent);

    if (!contiguous) {
        std::mutex boundsMutex;
        parallelFor(0, h, 64, [&](int y0, int y1) {
            QRect local;
            for (int y = y0; y < y1; ++y) {
                const uint32_t* line = pixels(y);
                const uint8_t* mask = allowed(y);
                uint8_t* out = result.filled.data() + static_cast<size_t>(y) * w;
                int minX = w, maxX = -1;
                for (int x = 0; x < w; ++x) {
                    if ((!mask || mask[x]) && matches(line[x])) {
                        out[x] = kFilled;
                        minX = std::min(minX, x);
                        maxX = x;
                    }
                }
                if (maxX >= 0) local = local.united(QRect(minX, y, maxX - minX + 1, 1));
            }
            std::lock_guard<std::mutex> lock(boundsMutex);
            result.bounds = result.bounds.united(local);
        });
        return result;
    }

    // Scanline flood fill: fill a whole horizontal run at once, then queue one seed for each
    // run of candidate pixels in the rows directly above and below it.
    if (const uint8_t* mask = allowed(seed.y()); mask && !mask[seed.x()]) return result;

    int minX = w, minY = h, maxX = -1, maxY = -1;
    std::vector<QPoint> stack;
    stack.reserve(1024);
    stack.push_back(seed);

    while (!stack.empty()) {
        const QPoint pt = stack.back();
        stack.pop_back();
        const int y = pt.y();
        uint8_t* fillRow = result.filled.data() + static_cast<size_t>(y) * w;
        if (fillRow[pt.x()]) continue;

        const uint32_t* line = pixels(y);
        const uint8_t* mask = allowed(y);
        auto candidate = [&](int x) { return !fillRow[x] && (!mask || mask[x]) && matches(line[x]); };
        if (!candidate(pt.x())) continue;

        int left = pt.x();
        while (left > 0 && candidate(left - 1)) --left;
        int right = pt.x();
        while (right < w - 1 && candidate(right + 1)) ++right;
        std::fill(fillRow + left, fillRow + right + 1, kFilled);

        minX = std::min(minX, left);
        maxX = std::max(maxX, right);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);

        for (int ny : {y - 1, y + 1}) {
            if (ny < 0 || ny >= h) continue;
            const uint8_t* nFill = result.filled.data() + static_cast<size_t>(ny) * w;
            const uint32_t* nLine = pixels(ny);
            const uint8_t* nMask = allowed(ny);
            bool inRun = false;
            for (int x = left; x <= right; ++x) {
                const bool ok = !nFill[x] && (!nMask || nMask[x]) && matches(nLine[x]);
                if (ok && !inRun) stack.push_back(QPoint(x, ny));
                inRun = ok;
            }
        }
    }

    if (maxX >= minX && maxY >= minY) {
        result.bounds = QRect(QPoint(minX, minY), QPoint(maxX, maxY));
    }
    return result;
}

} // namespace pdn
