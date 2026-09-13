#include "StylizeEffects.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace pdn {

// ==========================================
// Edge Detect
// ==========================================
void EdgeDetectEffect::process(QImage& image, const Selection& selection, int sensitivity) {
    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    QImage src = image.copy();
    double sens = sensitivity / 2.0;

    for (int y = 0; y < h; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(image.scanLine(y));
        int y0 = std::max(0, y - 1);
        int y1 = y;
        int y2 = std::min(h - 1, y + 1);

        const uint32_t* line0 = reinterpret_cast<const uint32_t*>(src.constScanLine(y0));
        const uint32_t* line1 = reinterpret_cast<const uint32_t*>(src.constScanLine(y1));
        const uint32_t* line2 = reinterpret_cast<const uint32_t*>(src.constScanLine(y2));

        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            int x0 = std::max(0, x - 1);
            int x2 = std::min(w - 1, x + 1);

            // Pixels in 3x3
            uint32_t p00 = line0[x0], p01 = line0[x], p02 = line0[x2];
            uint32_t p10 = line1[x0],                 p12 = line1[x2];
            uint32_t p20 = line2[x0], p21 = line2[x], p22 = line2[x2];

            auto getRGB = [](uint32_t px, int& r, int& g, int& b) {
                r = (px >> 16) & 0xFF;
                g = (px >> 8) & 0xFF;
                b = px & 0xFF;
            };

            int r00, g00, b00, r01, g01, b01, r02, g02, b02;
            int r10, g10, b10,                 r12, g12, b12;
            int r20, g20, b20, r21, g21, b21, r22, g22, b22;

            getRGB(p00, r00, g00, b00); getRGB(p01, r01, g01, b01); getRGB(p02, r02, g02, b02);
            getRGB(p10, r10, g10, b10);                              getRGB(p12, r12, g12, b12);
            getRGB(p20, r20, g20, b20); getRGB(p21, r21, g21, b21); getRGB(p22, r22, g22, b22);

            // Sobel X
            int gxR = -r00 + r02 - 2 * r10 + 2 * r12 - r20 + r22;
            int gxG = -g00 + g02 - 2 * g10 + 2 * g12 - g20 + g22;
            int gxB = -b00 + b02 - 2 * b10 + 2 * b12 - b20 + b22;

            // Sobel Y
            int gyR = -r00 - 2 * r01 - r02 + r20 + 2 * r21 + r22;
            int gyG = -g00 - 2 * g01 - g02 + g20 + 2 * g21 + g22;
            int gyB = -b00 - 2 * b01 - b02 + b20 + 2 * b21 + b22;

            uint8_t outR = static_cast<uint8_t>(std::clamp(std::sqrt(gxR * gxR + gyR * gyR) * sens, 0.0, 255.0));
            uint8_t outG = static_cast<uint8_t>(std::clamp(std::sqrt(gxG * gxG + gyG * gyG) * sens, 0.0, 255.0));
            uint8_t outB = static_cast<uint8_t>(std::clamp(std::sqrt(gxB * gxB + gyB * gyB) * sens, 0.0, 255.0));

            uint32_t a = line1[x] & 0xFF000000;
            dstLine[x] = a | (outR << 16) | (outG << 8) | outB;
        }
    }
}

bool EdgeDetectEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 5);
    return true;
}

bool EdgeDetectEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    EdgeDetectDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

EdgeDetectDialog::EdgeDetectDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Edge Detect", parent) {
    addSlider("Sensitivity:", 1, 10, m_sensitivity, [this](int val) { m_sensitivity = val; });

    setupButtons();
    updatePreview();
}

void EdgeDetectDialog::processPreview(QImage& image) {
    EdgeDetectEffect::process(image, m_doc->selection(), m_sensitivity);
}

// ==========================================
// Emboss
// ==========================================
void EmbossEffect::process(QImage& image, const Selection& selection, int angle) {
    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    QImage src = image.copy();
    double rad = angle * M_PI / 180.0;
    int dx = static_cast<int>(std::round(std::cos(rad)));
    int dy = static_cast<int>(std::round(std::sin(rad)));

    for (int y = 0; y < h; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(image.scanLine(y));

        int yP = std::clamp(y + dy, 0, h - 1);
        int yM = std::clamp(y - dy, 0, h - 1);
        const uint32_t* lineP = reinterpret_cast<const uint32_t*>(src.constScanLine(yP));
        const uint32_t* lineM = reinterpret_cast<const uint32_t*>(src.constScanLine(yM));

        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            int xP = std::clamp(x + dx, 0, w - 1);
            int xM = std::clamp(x - dx, 0, w - 1);

            uint32_t pxP = lineP[xP];
            uint32_t pxM = lineM[xM];

            int rP = (pxP >> 16) & 0xFF;
            int gP = (pxP >> 8) & 0xFF;
            int bP = pxP & 0xFF;

            int rM = (pxM >> 16) & 0xFF;
            int gM = (pxM >> 8) & 0xFF;
            int bM = pxM & 0xFF;

            int lumaP = (299 * rP + 587 * gP + 114 * bP + 500) / 1000;
            int lumaM = (299 * rM + 587 * gM + 114 * bM + 500) / 1000;

            int diff = lumaP - lumaM;
            uint8_t outVal = static_cast<uint8_t>(std::clamp(128 + diff * 2, 0, 255));

            uint32_t a = src.pixel(x, y) & 0xFF000000;
            dstLine[x] = a | (outVal << 16) | (outVal << 8) | outVal;
        }
    }
}

bool EmbossEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 45);
    return true;
}

bool EmbossEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    EmbossDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

EmbossDialog::EmbossDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Emboss", parent) {
    addSlider("Angle:", 0, 360, m_angle, [this](int val) { m_angle = val; });

    setupButtons();
    updatePreview();
}

void EmbossDialog::processPreview(QImage& image) {
    EmbossEffect::process(image, m_doc->selection(), m_angle);
}

// ==========================================
// Oil Painting (Artistic)
// ==========================================
void OilPaintingEffect::process(QImage& image, const Selection& selection, int brushSize, int coarseness) {
    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    QImage src = image.copy();
    int r = std::clamp(brushSize, 1, 8);
    int numBins = std::clamp(coarseness, 3, 100);

    std::vector<int> counts(numBins + 1);
    std::vector<uint32_t> sumR(numBins + 1);
    std::vector<uint32_t> sumG(numBins + 1);
    std::vector<uint32_t> sumB(numBins + 1);

    for (int y = 0; y < h; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            std::fill(counts.begin(), counts.end(), 0);
            std::fill(sumR.begin(), sumR.end(), 0);
            std::fill(sumG.begin(), sumG.end(), 0);
            std::fill(sumB.begin(), sumB.end(), 0);

            for (int dy = -r; dy <= r; ++dy) {
                int sy = std::clamp(y + dy, 0, h - 1);
                const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(src.constScanLine(sy));

                for (int dx = -r; dx <= r; ++dx) {
                    int sx = std::clamp(x + dx, 0, w - 1);
                    uint32_t px = srcLine[sx];

                    uint32_t cr = (px >> 16) & 0xFF;
                    uint32_t cg = (px >> 8) & 0xFF;
                    uint32_t cb = px & 0xFF;

                    uint32_t luma = (299 * cr + 587 * cg + 114 * cb + 500) / 1000;
                    int bin = std::clamp(static_cast<int>(luma * (numBins - 1) / 255), 0, numBins - 1);

                    counts[bin]++;
                    sumR[bin] += cr;
                    sumG[bin] += cg;
                    sumB[bin] += cb;
                }
            }

            int maxBin = 0;
            int maxCount = 0;
            for (int b = 0; b < numBins; ++b) {
                if (counts[b] > maxCount) {
                    maxCount = counts[b];
                    maxBin = b;
                }
            }

            if (maxCount > 0) {
                uint8_t outR = static_cast<uint8_t>(sumR[maxBin] / maxCount);
                uint8_t outG = static_cast<uint8_t>(sumG[maxBin] / maxCount);
                uint8_t outB = static_cast<uint8_t>(sumB[maxBin] / maxCount);
                uint32_t a = src.pixel(x, y) & 0xFF000000;
                dstLine[x] = a | (outR << 16) | (outG << 8) | outB;
            }
        }
    }
}

bool OilPaintingEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 3, 50);
    return true;
}

bool OilPaintingEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    OilPaintingDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

OilPaintingDialog::OilPaintingDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Oil Painting", parent) {
    addSlider("Brush size:", 1, 8, m_brushSize, [this](int val) { m_brushSize = val; });
    addSlider("Coarseness:", 3, 100, m_coarseness, [this](int val) { m_coarseness = val; });

    setupButtons();
    updatePreview();
}

void OilPaintingDialog::processPreview(QImage& image) {
    OilPaintingEffect::process(image, m_doc->selection(), m_brushSize, m_coarseness);
}

} // namespace pdn
