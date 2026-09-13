#include "BrightnessContrast.h"
#include <algorithm>
#include <cmath>

namespace pdn {

void BrightnessContrastEffect::process(QImage& image, const Selection& selection, int brightness, int contrast) {
    // contrast in [-100, 100]
    // factor F = (259 * (contrast + 255)) / (255 * (259 - contrast))
    double c = std::clamp(contrast, -100, 100);
    double factor = (259.0 * (c + 255.0)) / (255.0 * (259.0 - c));

    // Precompute LUT for speed
    uint8_t lut[256];
    for (int i = 0; i < 256; ++i) {
        double val = factor * (i - 128 + brightness) + 128;
        lut[i] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
    }

    int w = image.width();
    int h = image.height();

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) {
                continue;
            }
            uint32_t px = line[x];
            uint32_t a = (px >> 24) & 0xFF;
            if (a == 0) continue;

            uint8_t r = lut[(px >> 16) & 0xFF];
            uint8_t g = lut[(px >> 8) & 0xFF];
            uint8_t b = lut[px & 0xFF];

            line[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

bool BrightnessContrastEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 20, 20);
    return true;
}

bool BrightnessContrastEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    BrightnessContrastDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

BrightnessContrastDialog::BrightnessContrastDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Brightness / Contrast", parent) {
    addSlider("Brightness:", -100, 100, m_brightness, [this](int val) { m_brightness = val; });
    addSlider("Contrast:", -100, 100, m_contrast, [this](int val) { m_contrast = val; });

    setupButtons();
    updatePreview();
}

void BrightnessContrastDialog::processPreview(QImage& image) {
    BrightnessContrastEffect::process(image, m_doc->selection(), m_brightness, m_contrast);
}

} // namespace pdn
