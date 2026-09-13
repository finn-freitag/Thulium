#include "Adjustments.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace pdn {

// ==========================================
// 1. Auto-Level
// ==========================================
void AutoLevelEffect::process(QImage& image, const Selection& selection) {
    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    int histR[256] = {0};
    int histG[256] = {0};
    int histB[256] = {0};
    int totalCount = 0;

    for (int y = 0; y < h; ++y) {
        const uint32_t* line = reinterpret_cast<const uint32_t*>(image.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;
            uint32_t px = line[x];
            if (((px >> 24) & 0xFF) == 0) continue; // skip transparent
            histR[(px >> 16) & 0xFF]++;
            histG[(px >> 8) & 0xFF]++;
            histB[px & 0xFF]++;
            totalCount++;
        }
    }

    if (totalCount == 0) return;

    int clipLow = static_cast<int>(totalCount * 0.005);
    int clipHigh = static_cast<int>(totalCount * 0.995);

    auto findBounds = [&](const int* hist, int& minVal, int& maxVal) {
        int sum = 0;
        minVal = 0;
        for (int i = 0; i < 256; ++i) {
            sum += hist[i];
            if (sum >= clipLow) {
                minVal = i;
                break;
            }
        }
        sum = 0;
        maxVal = 255;
        for (int i = 255; i >= 0; --i) {
            sum += hist[i];
            if (sum >= (totalCount - clipHigh)) {
                maxVal = i;
                break;
            }
        }
        if (minVal >= maxVal) {
            minVal = 0;
            maxVal = 255;
        }
    };

    int minR, maxR, minG, maxG, minB, maxB;
    findBounds(histR, minR, maxR);
    findBounds(histG, minG, maxG);
    findBounds(histB, minB, maxB);

    uint8_t lutR[256], lutG[256], lutB[256];
    for (int i = 0; i < 256; ++i) {
        lutR[i] = static_cast<uint8_t>(std::clamp((i - minR) * 255.0 / (maxR - minR), 0.0, 255.0));
        lutG[i] = static_cast<uint8_t>(std::clamp((i - minG) * 255.0 / (maxG - minG), 0.0, 255.0));
        lutB[i] = static_cast<uint8_t>(std::clamp((i - minB) * 255.0 / (maxB - minB), 0.0, 255.0));
    }

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;
            uint32_t px = line[x];
            uint32_t a = px & 0xFF000000;
            uint8_t r = lutR[(px >> 16) & 0xFF];
            uint8_t g = lutG[(px >> 8) & 0xFF];
            uint8_t b = lutB[px & 0xFF];
            line[x] = a | (r << 16) | (g << 8) | b;
        }
    }
}

bool AutoLevelEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection);
    return true;
}

bool AutoLevelEffect::showDialog(QWidget*, Document* doc) {
    return applyInstantEffect(doc, name(), [](QImage& img, const Selection& sel) {
        process(img, sel);
    });
}

// ==========================================
// 2. Black and White
// ==========================================
void BlackAndWhiteEffect::process(QImage& image, const Selection& selection) {
    int w = image.width();
    int h = image.height();

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;
            uint32_t px = line[x];
            uint32_t a = px & 0xFF000000;
            uint32_t r = (px >> 16) & 0xFF;
            uint32_t g = (px >> 8) & 0xFF;
            uint32_t b = px & 0xFF;
            uint32_t luma = (299 * r + 587 * g + 114 * b + 500) / 1000;
            line[x] = a | (luma << 16) | (luma << 8) | luma;
        }
    }
}

bool BlackAndWhiteEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection);
    return true;
}

bool BlackAndWhiteEffect::showDialog(QWidget*, Document* doc) {
    return applyInstantEffect(doc, name(), [](QImage& img, const Selection& sel) {
        process(img, sel);
    });
}

// ==========================================
// 3. Invert Colors
// ==========================================
void InvertColorsEffect::process(QImage& image, const Selection& selection) {
    int w = image.width();
    int h = image.height();

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;
            uint32_t px = line[x];
            uint32_t a = px & 0xFF000000;
            uint32_t rgb = (~px) & 0x00FFFFFF;
            line[x] = a | rgb;
        }
    }
}

bool InvertColorsEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection);
    return true;
}

bool InvertColorsEffect::showDialog(QWidget*, Document* doc) {
    return applyInstantEffect(doc, name(), [](QImage& img, const Selection& sel) {
        process(img, sel);
    });
}

// ==========================================
// 4. Invert Alpha
// ==========================================
void InvertAlphaEffect::process(QImage& image, const Selection& selection) {
    int w = image.width();
    int h = image.height();

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;
            uint32_t px = line[x];
            uint32_t invA = (255 - ((px >> 24) & 0xFF)) << 24;
            line[x] = invA | (px & 0x00FFFFFF);
        }
    }
}

bool InvertAlphaEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection);
    return true;
}

bool InvertAlphaEffect::showDialog(QWidget*, Document* doc) {
    return applyInstantEffect(doc, name(), [](QImage& img, const Selection& sel) {
        process(img, sel);
    });
}

// ==========================================
// 5. Sepia
// ==========================================
void SepiaEffect::process(QImage& image, const Selection& selection) {
    uint8_t lutR[256], lutG[256], lutB[256];
    for (int i = 0; i < 256; ++i) {
        lutR[i] = static_cast<uint8_t>(std::clamp((i * 120 + 50) / 100, 0, 255));
        lutG[i] = static_cast<uint8_t>(std::clamp((i * 100) / 100, 0, 255));
        lutB[i] = static_cast<uint8_t>(std::clamp((i * 75) / 100, 0, 255));
    }

    int w = image.width();
    int h = image.height();

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;
            uint32_t px = line[x];
            uint32_t a = px & 0xFF000000;
            uint32_t r = (px >> 16) & 0xFF;
            uint32_t g = (px >> 8) & 0xFF;
            uint32_t b = px & 0xFF;
            uint32_t luma = (299 * r + 587 * g + 114 * b + 500) / 1000;
            line[x] = a | (lutR[luma] << 16) | (lutG[luma] << 8) | lutB[luma];
        }
    }
}

bool SepiaEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection);
    return true;
}

bool SepiaEffect::showDialog(QWidget*, Document* doc) {
    return applyInstantEffect(doc, name(), [](QImage& img, const Selection& sel) {
        process(img, sel);
    });
}

// ==========================================
// 6. Posterize
// ==========================================
void PosterizeEffect::process(QImage& image, const Selection& selection, int levels) {
    int lv = std::clamp(levels, 2, 64);
    uint8_t lut[256];
    for (int i = 0; i < 256; ++i) {
        double bin = std::round(i * (lv - 1.0) / 255.0);
        lut[i] = static_cast<uint8_t>(std::clamp(std::round(bin * 255.0 / (lv - 1.0)), 0.0, 255.0));
    }

    int w = image.width();
    int h = image.height();

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;
            uint32_t px = line[x];
            uint32_t a = px & 0xFF000000;
            uint8_t r = lut[(px >> 16) & 0xFF];
            uint8_t g = lut[(px >> 8) & 0xFF];
            uint8_t b = lut[px & 0xFF];
            line[x] = a | (r << 16) | (g << 8) | b;
        }
    }
}

bool PosterizeEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 16);
    return true;
}

bool PosterizeEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    PosterizeDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

PosterizeDialog::PosterizeDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Posterize", parent) {
    addSlider("Linked Levels:", 2, 64, m_levels, [this](int val) {
        m_levels = val;
    });
    setupButtons();
    updatePreview();
}

void PosterizeDialog::processPreview(QImage& image) {
    PosterizeEffect::process(image, m_doc->selection(), m_levels);
}

// ==========================================
// 7. Hue / Saturation
// ==========================================
static void rgbToHsl(uint8_t r, uint8_t g, uint8_t b, double& h, double& s, double& l) {
    double rf = r / 255.0;
    double gf = g / 255.0;
    double bf = b / 255.0;
    double maxVal = std::max({rf, gf, bf});
    double minVal = std::min({rf, gf, bf});
    double delta = maxVal - minVal;

    l = (maxVal + minVal) / 2.0;
    if (delta < 1e-6) {
        h = 0.0;
        s = 0.0;
    } else {
        s = (l > 0.5) ? (delta / (2.0 - maxVal - minVal)) : (delta / (maxVal + minVal));
        if (maxVal == rf) {
            h = (gf - bf) / delta + (gf < bf ? 6.0 : 0.0);
        } else if (maxVal == gf) {
            h = (bf - rf) / delta + 2.0;
        } else {
            h = (rf - gf) / delta + 4.0;
        }
        h *= 60.0;
    }
}

static void hslToRgb(double h, double s, double l, uint8_t& r, uint8_t& g, uint8_t& b) {
    auto hue2rgb = [](double p, double q, double t) {
        if (t < 0.0) t += 1.0;
        if (t > 1.0) t -= 1.0;
        if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
        if (t < 1.0 / 2.0) return q;
        if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
        return p;
    };

    if (s < 1e-6) {
        uint8_t val = static_cast<uint8_t>(std::clamp(l * 255.0 + 0.5, 0.0, 255.0));
        r = g = b = val;
    } else {
        double q = (l < 0.5) ? (l * (1.0 + s)) : (l + s - l * s);
        double p = 2.0 * l - q;
        double hk = h / 360.0;
        r = static_cast<uint8_t>(std::clamp(hue2rgb(p, q, hk + 1.0 / 3.0) * 255.0 + 0.5, 0.0, 255.0));
        g = static_cast<uint8_t>(std::clamp(hue2rgb(p, q, hk) * 255.0 + 0.5, 0.0, 255.0));
        b = static_cast<uint8_t>(std::clamp(hue2rgb(p, q, hk - 1.0 / 3.0) * 255.0 + 0.5, 0.0, 255.0));
    }
}

void HueSaturationEffect::process(QImage& image, const Selection& selection, int hue, int saturation, int lightness) {
    int w = image.width();
    int h = image.height();
    double satFactor = saturation / 100.0;
    double lightFactor = lightness / 100.0;

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;
            uint32_t px = line[x];
            uint32_t a = px & 0xFF000000;
            if (a == 0) continue;

            uint8_t r = (px >> 16) & 0xFF;
            uint8_t g = (px >> 8) & 0xFF;
            uint8_t b = px & 0xFF;

            double hDeg, sVal, lVal;
            rgbToHsl(r, g, b, hDeg, sVal, lVal);

            // Apply adjustments
            hDeg = std::fmod(hDeg + hue + 360.0, 360.0);
            sVal = std::clamp(sVal * satFactor, 0.0, 1.0);
            if (lightFactor > 0.0) {
                lVal = lVal + (1.0 - lVal) * lightFactor;
            } else if (lightFactor < 0.0) {
                lVal = lVal + lVal * lightFactor;
            }
            lVal = std::clamp(lVal, 0.0, 1.0);

            hslToRgb(hDeg, sVal, lVal, r, g, b);
            line[x] = a | (r << 16) | (g << 8) | b;
        }
    }
}

bool HueSaturationEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 0, 100, 0);
    return true;
}

bool HueSaturationEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    HueSaturationDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

HueSaturationDialog::HueSaturationDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Hue / Saturation", parent) {
    addSlider("Hue:", -180, 180, m_hue, [this](int val) { m_hue = val; });
    addSlider("Saturation (%):", 0, 200, m_saturation, [this](int val) { m_saturation = val; });
    addSlider("Lightness (%):", -100, 100, m_lightness, [this](int val) { m_lightness = val; });

    setupButtons();
    updatePreview();
}

void HueSaturationDialog::processPreview(QImage& image) {
    HueSaturationEffect::process(image, m_doc->selection(), m_hue, m_saturation, m_lightness);
}

// ==========================================
// 8. Temperature / Tint
// ==========================================
void TemperatureTintEffect::process(QImage& image, const Selection& selection, int temperature, int tint) {
    int w = image.width();
    int h = image.height();

    // Precompute per-channel offsets
    // Temp: warm increases R, decreases B; cool increases B, decreases R
    double rTemp = (temperature > 0) ? (temperature * 0.6) : (temperature * 0.3);
    double bTemp = (temperature > 0) ? (-temperature * 0.6) : (-temperature * 0.3);

    // Tint: magenta increases R & B, decreases G; green increases G, decreases R & B
    double rTint = tint * 0.3;
    double gTint = -tint * 0.6;
    double bTint = tint * 0.3;

    int rOff = static_cast<int>(std::round(rTemp + rTint));
    int gOff = static_cast<int>(std::round(gTint));
    int bOff = static_cast<int>(std::round(bTemp + bTint));

    uint8_t lutR[256], lutG[256], lutB[256];
    for (int i = 0; i < 256; ++i) {
        lutR[i] = static_cast<uint8_t>(std::clamp(i + rOff, 0, 255));
        lutG[i] = static_cast<uint8_t>(std::clamp(i + gOff, 0, 255));
        lutB[i] = static_cast<uint8_t>(std::clamp(i + bOff, 0, 255));
    }

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;
            uint32_t px = line[x];
            uint32_t a = px & 0xFF000000;
            uint8_t r = lutR[(px >> 16) & 0xFF];
            uint8_t g = lutG[(px >> 8) & 0xFF];
            uint8_t b = lutB[px & 0xFF];
            line[x] = a | (r << 16) | (g << 8) | b;
        }
    }
}

bool TemperatureTintEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 0, 0);
    return true;
}

bool TemperatureTintEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    TemperatureTintDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

TemperatureTintDialog::TemperatureTintDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Temperature / Tint", parent) {
    addSlider("Temperature (Cool / Warm):", -100, 100, m_temperature, [this](int val) { m_temperature = val; });
    addSlider("Tint (Green / Magenta):", -100, 100, m_tint, [this](int val) { m_tint = val; });

    setupButtons();
    updatePreview();
}

void TemperatureTintDialog::processPreview(QImage& image) {
    TemperatureTintEffect::process(image, m_doc->selection(), m_temperature, m_tint);
}

} // namespace pdn
