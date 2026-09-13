#include "PhotoEffects.h"
#include "GaussianBlur.h"
#include "BrightnessContrast.h"
#include <cmath>
#include <algorithm>

namespace pdn {

// ==========================================
// Sharpen
// ==========================================
void SharpenEffect::process(QImage& image, const Selection& selection, int amount) {
    if (amount <= 0) return;

    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    QImage src = image.copy();
    double weight = amount / 4.0;
    double centerW = 1.0 + 4.0 * weight;
    double edgeW = -weight;

    for (int y = 0; y < h; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(image.scanLine(y));
        int yUp = std::max(0, y - 1);
        int yDown = std::min(h - 1, y + 1);

        const uint32_t* srcMid = reinterpret_cast<const uint32_t*>(src.constScanLine(y));
        const uint32_t* srcUp = reinterpret_cast<const uint32_t*>(src.constScanLine(yUp));
        const uint32_t* srcDown = reinterpret_cast<const uint32_t*>(src.constScanLine(yDown));

        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            int xLeft = std::max(0, x - 1);
            int xRight = std::min(w - 1, x + 1);

            uint32_t mid = srcMid[x];
            uint32_t up = srcUp[x];
            uint32_t down = srcDown[x];
            uint32_t left = srcMid[xLeft];
            uint32_t right = srcMid[xRight];

            double r = ((mid >> 16) & 0xFF) * centerW +
                       (((up >> 16) & 0xFF) + ((down >> 16) & 0xFF) + ((left >> 16) & 0xFF) + ((right >> 16) & 0xFF)) * edgeW;
            double g = ((mid >> 8) & 0xFF) * centerW +
                       (((up >> 8) & 0xFF) + ((down >> 8) & 0xFF) + ((left >> 8) & 0xFF) + ((right >> 8) & 0xFF)) * edgeW;
            double b = (mid & 0xFF) * centerW +
                       ((up & 0xFF) + (down & 0xFF) + (left & 0xFF) + (right & 0xFF)) * edgeW;

            uint8_t outR = static_cast<uint8_t>(std::clamp(r, 0.0, 255.0));
            uint8_t outG = static_cast<uint8_t>(std::clamp(g, 0.0, 255.0));
            uint8_t outB = static_cast<uint8_t>(std::clamp(b, 0.0, 255.0));

            dstLine[x] = (mid & 0xFF000000) | (outR << 16) | (outG << 8) | outB;
        }
    }
}

bool SharpenEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 2);
    return true;
}

bool SharpenEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    SharpenDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

SharpenDialog::SharpenDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Sharpen", parent) {
    addSlider("Amount:", 1, 20, m_amount, [this](int val) { m_amount = val; });

    setupButtons();
    updatePreview();
}

void SharpenDialog::processPreview(QImage& image) {
    SharpenEffect::process(image, m_doc->selection(), m_amount);
}

// ==========================================
// Glow
// ==========================================
void GlowEffect::process(QImage& image, const Selection& selection, int radius, int brightness, int contrast) {
    if (radius <= 0) return;

    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    QImage blurred = image.copy();
    GaussianBlurEffect::process(blurred, selection, radius);
    BrightnessContrastEffect::process(blurred, selection, brightness, contrast);

    // Screen blend: dst = 255 - ((255 - a) * (255 - b)) / 255
    for (int y = 0; y < h; ++y) {
        uint32_t* baseLine = reinterpret_cast<uint32_t*>(image.scanLine(y));
        const uint32_t* glowLine = reinterpret_cast<const uint32_t*>(blurred.constScanLine(y));

        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            uint32_t basePx = baseLine[x];
            uint32_t glowPx = glowLine[x];

            uint32_t a = basePx & 0xFF000000;
            int rA = (basePx >> 16) & 0xFF;
            int gA = (basePx >> 8) & 0xFF;
            int bA = basePx & 0xFF;

            int rB = (glowPx >> 16) & 0xFF;
            int gB = (glowPx >> 8) & 0xFF;
            int bB = glowPx & 0xFF;

            uint8_t outR = static_cast<uint8_t>(255 - ((255 - rA) * (255 - rB)) / 255);
            uint8_t outG = static_cast<uint8_t>(255 - ((255 - gA) * (255 - gB)) / 255);
            uint8_t outB = static_cast<uint8_t>(255 - ((255 - bA) * (255 - bB)) / 255);

            baseLine[x] = a | (outR << 16) | (outG << 8) | outB;
        }
    }
}

bool GlowEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 6, 10, 10);
    return true;
}

bool GlowEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    GlowDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

GlowDialog::GlowDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Glow", parent) {
    addSlider("Radius (pixels):", 1, 20, m_radius, [this](int val) { m_radius = val; });
    addSlider("Brightness:", -100, 100, m_brightness, [this](int val) { m_brightness = val; });
    addSlider("Contrast:", -100, 100, m_contrast, [this](int val) { m_contrast = val; });

    setupButtons();
    updatePreview();
}

void GlowDialog::processPreview(QImage& image) {
    GlowEffect::process(image, m_doc->selection(), m_radius, m_brightness, m_contrast);
}

// ==========================================
// Vignette
// ==========================================
void VignetteEffect::process(QImage& image, const Selection& selection, int radius, int density, const QPointF& center) {
    if (density <= 0) return;

    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    double cx = (center.x() >= 0.0) ? center.x() : (w * 0.5);
    double cy = (center.y() >= 0.0) ? center.y() : (h * 0.5);
    double maxDist = std::max({ std::hypot(cx, cy), std::hypot(w - cx, cy),
                                 std::hypot(cx, h - cy), std::hypot(w - cx, h - cy) });
    double innerDist = maxDist * (radius / 100.0);
    double range = maxDist - innerDist;
    if (range < 1.0) range = 1.0;
    double maxDarkness = density / 100.0;

    for (int y = 0; y < h; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            double dx = x - cx;
            double dy = y - cy;
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist > innerDist) {
                double t = std::clamp((dist - innerDist) / range, 0.0, 1.0);
                double darkFactor = 1.0 - (t * maxDarkness);

                uint32_t px = line[x];
                uint32_t a = px & 0xFF000000;
                uint8_t r = static_cast<uint8_t>(((px >> 16) & 0xFF) * darkFactor);
                uint8_t g = static_cast<uint8_t>(((px >> 8) & 0xFF) * darkFactor);
                uint8_t b = static_cast<uint8_t>((px & 0xFF) * darkFactor);

                line[x] = a | (r << 16) | (g << 8) | b;
            }
        }
    }
}

bool VignetteEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 50, 50);
    return true;
}

bool VignetteEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    VignetteDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

VignetteDialog::VignetteDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Vignette", parent) {
    int docW = doc ? doc->width() : 800;
    int docH = doc ? doc->height() : 600;
    m_center = QPointF(docW * 0.5, docH * 0.5);

    addSlider("Radius (%):", 10, 100, m_radius, [this](int val) {
        m_radius = val;
        if (m_bridge) m_bridge->requestCanvasRepaint();
    });
    addSlider("Density (%):", 0, 100, m_density, [this](int val) {
        m_density = val;
        if (m_bridge) m_bridge->requestCanvasRepaint();
    });

    m_centerXCtrl = addSlider("Center X (pixels):", 0, docW, static_cast<int>(m_center.x()), [this](int val) {
        m_center.setX(val);
        if (m_bridge) m_bridge->requestCanvasRepaint();
    });
    m_centerYCtrl = addSlider("Center Y (pixels):", 0, docH, static_cast<int>(m_center.y()), [this](int val) {
        m_center.setY(val);
        if (m_bridge) m_bridge->requestCanvasRepaint();
    });

    QLabel* tip = new QLabel("Tip: Click or drag on the canvas to set center point.", this);
    tip->setStyleSheet("color: #777; font-style: italic;");
    addCustomWidget(tip);

    enablePointPicking(true);
    setupButtons();
    updatePreview();
}

void VignetteDialog::onCanvasPointPicked(const QPointF& docPos) {
    int docW = m_doc ? m_doc->width() : 1000;
    int docH = m_doc ? m_doc->height() : 1000;
    m_center = QPointF(std::clamp(docPos.x(), 0.0, static_cast<double>(docW)),
                       std::clamp(docPos.y(), 0.0, static_cast<double>(docH)));

    if (m_centerXCtrl.slider && m_centerXCtrl.spin) {
        m_centerXCtrl.slider->blockSignals(true);
        m_centerXCtrl.spin->blockSignals(true);
        m_centerXCtrl.slider->setValue(static_cast<int>(std::round(m_center.x())));
        m_centerXCtrl.spin->setValue(static_cast<int>(std::round(m_center.x())));
        m_centerXCtrl.slider->blockSignals(false);
        m_centerXCtrl.spin->blockSignals(false);
    }

    if (m_centerYCtrl.slider && m_centerYCtrl.spin) {
        m_centerYCtrl.slider->blockSignals(true);
        m_centerYCtrl.spin->blockSignals(true);
        m_centerYCtrl.slider->setValue(static_cast<int>(std::round(m_center.y())));
        m_centerYCtrl.spin->setValue(static_cast<int>(std::round(m_center.y())));
        m_centerYCtrl.slider->blockSignals(false);
        m_centerYCtrl.spin->blockSignals(false);
    }

    updatePreview();
    if (m_bridge) {
        m_bridge->requestCanvasRepaint();
    }
}

void VignetteDialog::drawCanvasOverlay(QPainter& painter, const RenderOptions& opts) {
    drawCrosshairOverlay(painter, m_center, opts.zoom, opts.panOffset);
}

void VignetteDialog::onReset() {
    int docW = m_doc ? m_doc->width() : 800;
    int docH = m_doc ? m_doc->height() : 600;
    m_center = QPointF(docW * 0.5, docH * 0.5);
    if (m_centerXCtrl.slider) m_centerXCtrl.slider->setValue(static_cast<int>(m_center.x()));
    if (m_centerYCtrl.slider) m_centerYCtrl.slider->setValue(static_cast<int>(m_center.y()));
    if (m_bridge) m_bridge->requestCanvasRepaint();
}

void VignetteDialog::processPreview(QImage& image) {
    VignetteEffect::process(image, m_doc->selection(), m_radius, m_density, m_center);
}

} // namespace pdn
