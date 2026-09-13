#include "BlurEffects.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace pdn {

// ==========================================
// Motion Blur
// ==========================================
void MotionBlurEffect::process(QImage& image, const Selection& selection, int angle, int distance, bool centered) {
    if (distance <= 1) return;

    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    QImage src = image.copy();
    double rad = angle * M_PI / 180.0;
    double dirX = std::cos(rad);
    double dirY = std::sin(rad);

    int numSteps = std::max(1, distance);
    double startT = centered ? (-distance / 2.0) : 0.0;
    double stepSize = static_cast<double>(distance) / numSteps;

    for (int y = 0; y < h; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            uint32_t sumA = 0, sumR = 0, sumG = 0, sumB = 0;
            int count = 0;

            for (int i = 0; i <= numSteps; ++i) {
                double t = startT + i * stepSize;
                int sx = std::clamp(static_cast<int>(std::round(x + t * dirX)), 0, w - 1);
                int sy = std::clamp(static_cast<int>(std::round(y + t * dirY)), 0, h - 1);

                const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(src.constScanLine(sy));
                uint32_t px = srcLine[sx];

                sumA += (px >> 24) & 0xFF;
                sumR += (px >> 16) & 0xFF;
                sumG += (px >> 8) & 0xFF;
                sumB += px & 0xFF;
                count++;
            }

            if (count > 0) {
                uint32_t avgA = sumA / count;
                uint32_t avgR = sumR / count;
                uint32_t avgG = sumG / count;
                uint32_t avgB = sumB / count;
                dstLine[x] = (avgA << 24) | (avgR << 16) | (avgG << 8) | avgB;
            }
        }
    }
}

bool MotionBlurEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 0, 10, true);
    return true;
}

bool MotionBlurEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    MotionBlurDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

MotionBlurDialog::MotionBlurDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Motion Blur", parent) {
    addSlider("Angle:", -180, 180, m_angle, [this](int val) { m_angle = val; });
    addSlider("Distance (pixels):", 1, 100, m_distance, [this](int val) { m_distance = val; });
    addCheckBox("Centered", m_centered, [this](bool val) { m_centered = val; });

    setupButtons();
    updatePreview();
}

void MotionBlurDialog::processPreview(QImage& image) {
    MotionBlurEffect::process(image, m_doc->selection(), m_angle, m_distance, m_centered);
}

// ==========================================
// Radial Blur
// ==========================================
void RadialBlurEffect::process(QImage& image, const Selection& selection, int angle, const QPointF& center) {
    if (angle <= 0) return;

    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    QImage src = image.copy();
    double cx = (center.x() >= 0.0) ? center.x() : (w * 0.5);
    double cy = (center.y() >= 0.0) ? center.y() : (h * 0.5);

    double maxRad = (angle * M_PI / 180.0) * 0.15;
    const int numSamples = 11;

    for (int y = 0; y < h; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            double dx = x - cx;
            double dy = y - cy;

            uint32_t sumA = 0, sumR = 0, sumG = 0, sumB = 0;

            for (int i = 0; i < numSamples; ++i) {
                double t = -1.0 + 2.0 * i / (numSamples - 1.0);
                double theta = t * maxRad;
                double cosT = std::cos(theta);
                double sinT = std::sin(theta);

                double rx = dx * cosT - dy * sinT;
                double ry = dx * sinT + dy * cosT;

                int sx = std::clamp(static_cast<int>(std::round(cx + rx)), 0, w - 1);
                int sy = std::clamp(static_cast<int>(std::round(cy + ry)), 0, h - 1);

                const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(src.constScanLine(sy));
                uint32_t px = srcLine[sx];

                sumA += (px >> 24) & 0xFF;
                sumR += (px >> 16) & 0xFF;
                sumG += (px >> 8) & 0xFF;
                sumB += px & 0xFF;
            }

            uint32_t avgA = sumA / numSamples;
            uint32_t avgR = sumR / numSamples;
            uint32_t avgG = sumG / numSamples;
            uint32_t avgB = sumB / numSamples;
            dstLine[x] = (avgA << 24) | (avgR << 16) | (avgG << 8) | avgB;
        }
    }
}

bool RadialBlurEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 10);
    return true;
}

bool RadialBlurEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    RadialBlurDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

RadialBlurDialog::RadialBlurDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Radial Blur", parent) {
    int docW = doc ? doc->width() : 800;
    int docH = doc ? doc->height() : 600;
    m_center = QPointF(docW * 0.5, docH * 0.5);

    addSlider("Angle:", 1, 100, m_angle, [this](int val) {
        m_angle = val;
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

void RadialBlurDialog::onCanvasPointPicked(const QPointF& docPos) {
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

void RadialBlurDialog::drawCanvasOverlay(QPainter& painter, const RenderOptions& opts) {
    drawCrosshairOverlay(painter, m_center, opts.zoom, opts.panOffset);
}

void RadialBlurDialog::onReset() {
    int docW = m_doc ? m_doc->width() : 800;
    int docH = m_doc ? m_doc->height() : 600;
    m_center = QPointF(docW * 0.5, docH * 0.5);
    if (m_centerXCtrl.slider) m_centerXCtrl.slider->setValue(static_cast<int>(m_center.x()));
    if (m_centerYCtrl.slider) m_centerYCtrl.slider->setValue(static_cast<int>(m_center.y()));
    if (m_bridge) m_bridge->requestCanvasRepaint();
}

void RadialBlurDialog::processPreview(QImage& image) {
    RadialBlurEffect::process(image, m_doc->selection(), m_angle, m_center);
}

} // namespace pdn
