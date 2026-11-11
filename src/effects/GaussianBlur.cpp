#include "GaussianBlur.h"
#include "../core/Parallel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <array>
#include <cmath>
#include <algorithm>

namespace pdn {

namespace {

constexpr int kBoxPasses = 3; // three box blurs approximate a Gaussian closely

// Box radii whose successive application approximates a Gaussian of the given sigma
// (Kovesi, "Fast Almost-Gaussian Filtering").
std::array<int, kBoxPasses> boxRadiiForSigma(double sigma) {
    const double idealWidth = std::sqrt(12.0 * sigma * sigma / kBoxPasses + 1.0);
    int lower = static_cast<int>(std::floor(idealWidth));
    if (lower % 2 == 0) --lower;
    const int upper = lower + 2;
    const double idealLowerCount =
        (12.0 * sigma * sigma - kBoxPasses * lower * lower - 4.0 * kBoxPasses * lower - 3.0 * kBoxPasses) /
        (-4.0 * lower - 4.0);
    const int lowerCount = static_cast<int>(std::round(idealLowerCount));
    std::array<int, kBoxPasses> radii;
    for (int i = 0; i < kBoxPasses; ++i) {
        radii[i] = std::max(0, ((i < lowerCount ? lower : upper) - 1) / 2);
    }
    return radii;
}

// Box-blurs one line of n premultiplied pixels (rows: stride 1, columns: stride = pixels per row).
// The window slides along the line with edge pixels repeated, so any radius is safe for any n.
void boxBlurLine(const uint32_t* src, uint32_t* dst, int n, qsizetype stride, int radius) {
    const float scale = 1.0f / (2 * radius + 1);
    auto at = [&](int i) { return src[std::clamp(i, 0, n - 1) * stride]; };

    int sumA = 0, sumR = 0, sumG = 0, sumB = 0;
    auto add = [&](uint32_t px, int sign) {
        sumA += sign * static_cast<int>(px >> 24);
        sumR += sign * static_cast<int>((px >> 16) & 0xFF);
        sumG += sign * static_cast<int>((px >> 8) & 0xFF);
        sumB += sign * static_cast<int>(px & 0xFF);
    };
    for (int i = -radius; i <= radius; ++i) add(at(i), +1);

    for (int i = 0; i < n; ++i) {
        dst[i * stride] = (static_cast<uint32_t>(sumA * scale + 0.5f) << 24) |
                          (static_cast<uint32_t>(sumR * scale + 0.5f) << 16) |
                          (static_cast<uint32_t>(sumG * scale + 0.5f) << 8) |
                          static_cast<uint32_t>(sumB * scale + 0.5f);
        add(at(i + radius + 1), +1);
        add(at(i - radius), -1);
    }
}

} // namespace

void GaussianBlurEffect::process(QImage& image, const Selection& selection, int radius) {
    if (radius <= 0) return;
    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) return;

    // Blur premultiplied pixels: otherwise the (arbitrary) colour of fully transparent pixels
    // bleeds into their visible neighbours and produces dark or coloured fringes.
    QImage work = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage scratch(w, h, QImage::Format_ARGB32_Premultiplied);
    auto* a = reinterpret_cast<uint32_t*>(work.bits());
    auto* b = reinterpret_cast<uint32_t*>(scratch.bits());
    const qsizetype stride = work.bytesPerLine() / 4;

    // The slider value is the blur radius in pixels, i.e. roughly two standard deviations.
    for (int boxRadius : boxRadiiForSigma(std::max(0.5, radius / 2.0))) {
        if (boxRadius == 0) continue;
        parallelFor(0, h, 32, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) boxBlurLine(a + y * stride, b + y * stride, w, 1, boxRadius);
        });
        parallelFor(0, w, 32, [&](int x0, int x1) {
            for (int x = x0; x < x1; ++x) boxBlurLine(b + x, a + x, h, stride, boxRadius);
        });
    }

    const QImage blurred = work.convertToFormat(QImage::Format_ARGB32);
    image.detach();
    const QImage* mask = selection.isEmpty() ? nullptr : &selection.mask(w, h);
    parallelRows(h, [&](int y) {
        auto* dst = reinterpret_cast<uint32_t*>(image.scanLine(y));
        const auto* src = reinterpret_cast<const uint32_t*>(blurred.constScanLine(y));
        if (!mask) {
            std::copy(src, src + w, dst);
            return;
        }
        // Only selected pixels take the blurred value
        const uint8_t* maskRow = mask->constScanLine(y);
        for (int x = 0; x < w; ++x) {
            if (maskRow[x]) dst[x] = src[x];
        }
    });
}

bool GaussianBlurEffect::apply(QImage& image, const Selection& selection) {
    if (applyRemembered(image, selection)) return true;
    process(image, selection, 5);
    return true;
}

bool GaussianBlurEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    GaussianBlurDialog dlg(doc, parent);
    return runDialog(dlg);
}

GaussianBlurDialog::GaussianBlurDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Gaussian Blur", parent) {
    addSlider("Radius (pixels):", 1, 100, m_radius, [this](int val) { m_radius = val; });

    setupButtons();
    updatePreview();
}

void GaussianBlurDialog::processPreview(QImage& image) {
    GaussianBlurEffect::process(image, m_doc->selection(), m_radius);
}

EffectApplier GaussianBlurDialog::applier() const {
    return [p0 = m_radius](QImage& image, const Selection& selection) {
        GaussianBlurEffect::process(image, selection, p0);
    };
}

} // namespace pdn
