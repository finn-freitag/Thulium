#include "NoiseEffects.h"
#include <random>
#include <cmath>
#include <algorithm>
#include <vector>

namespace pdn {

// ==========================================
// Add Noise
// ==========================================
void AddNoiseEffect::process(QImage& image, const Selection& selection, int intensity, int colorSaturation, int coverage) {
    if (intensity <= 0 || coverage <= 0) return;

    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    // Fixed seed for consistent preview or standard RNG
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> distProb(0.0, 100.0);
    double maxDelta = intensity * 2.55;
    std::uniform_real_distribution<double> distDelta(-maxDelta, maxDelta);

    double satFactor = colorSaturation / 100.0;
    double covThreshold = static_cast<double>(coverage);

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            if (distProb(rng) <= covThreshold) {
                uint32_t px = line[x];
                uint32_t a = px & 0xFF000000;
                double r = (px >> 16) & 0xFF;
                double g = (px >> 8) & 0xFF;
                double b = px & 0xFF;

                double monoDelta = distDelta(rng);
                double rDelta = distDelta(rng);
                double gDelta = distDelta(rng);
                double bDelta = distDelta(rng);

                double finalRDelta = monoDelta * (1.0 - satFactor) + rDelta * satFactor;
                double finalGDelta = monoDelta * (1.0 - satFactor) + gDelta * satFactor;
                double finalBDelta = monoDelta * (1.0 - satFactor) + bDelta * satFactor;

                uint8_t outR = static_cast<uint8_t>(std::clamp(r + finalRDelta, 0.0, 255.0));
                uint8_t outG = static_cast<uint8_t>(std::clamp(g + finalGDelta, 0.0, 255.0));
                uint8_t outB = static_cast<uint8_t>(std::clamp(b + finalBDelta, 0.0, 255.0));

                line[x] = a | (outR << 16) | (outG << 8) | outB;
            }
        }
    }
}

bool AddNoiseEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 64, 100, 100);
    return true;
}

bool AddNoiseEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    AddNoiseDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

AddNoiseDialog::AddNoiseDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Add Noise", parent) {
    addSlider("Intensity:", 0, 100, m_intensity, [this](int val) { m_intensity = val; });
    addSlider("Color Saturation (%):", 0, 100, m_colorSaturation, [this](int val) { m_colorSaturation = val; });
    addSlider("Coverage (%):", 0, 100, m_coverage, [this](int val) { m_coverage = val; });

    setupButtons();
    updatePreview();
}

void AddNoiseDialog::processPreview(QImage& image) {
    AddNoiseEffect::process(image, m_doc->selection(), m_intensity, m_colorSaturation, m_coverage);
}

// ==========================================
// Median
// ==========================================
void MedianEffect::process(QImage& image, const Selection& selection, int radius, int percentile) {
    if (radius <= 0) return;

    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    QImage src = image.copy();
    int r = std::clamp(radius, 1, 20);
    int p = std::clamp(percentile, 0, 100);

    std::vector<uint8_t> valsR;
    std::vector<uint8_t> valsG;
    std::vector<uint8_t> valsB;
    valsR.reserve((2 * r + 1) * (2 * r + 1));
    valsG.reserve((2 * r + 1) * (2 * r + 1));
    valsB.reserve((2 * r + 1) * (2 * r + 1));

    for (int y = 0; y < h; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            valsR.clear();
            valsG.clear();
            valsB.clear();

            for (int dy = -r; dy <= r; ++dy) {
                int sy = std::clamp(y + dy, 0, h - 1);
                const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(src.constScanLine(sy));
                for (int dx = -r; dx <= r; ++dx) {
                    int sx = std::clamp(x + dx, 0, w - 1);
                    uint32_t px = srcLine[sx];
                    valsR.push_back((px >> 16) & 0xFF);
                    valsG.push_back((px >> 8) & 0xFF);
                    valsB.push_back(px & 0xFF);
                }
            }

            size_t k = (valsR.size() - 1) * p / 100;
            std::nth_element(valsR.begin(), valsR.begin() + k, valsR.end());
            std::nth_element(valsG.begin(), valsG.begin() + k, valsG.end());
            std::nth_element(valsB.begin(), valsB.begin() + k, valsB.end());

            uint32_t a = src.pixel(x, y) & 0xFF000000;
            dstLine[x] = a | (valsR[k] << 16) | (valsG[k] << 8) | valsB[k];
        }
    }
}

bool MedianEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 2, 50);
    return true;
}

bool MedianEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    MedianDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

MedianDialog::MedianDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Median", parent) {
    addSlider("Radius (pixels):", 1, 20, m_radius, [this](int val) { m_radius = val; });
    addSlider("Percentile (%):", 0, 100, m_percentile, [this](int val) { m_percentile = val; });

    setupButtons();
    updatePreview();
}

void MedianDialog::processPreview(QImage& image) {
    MedianEffect::process(image, m_doc->selection(), m_radius, m_percentile);
}

} // namespace pdn
