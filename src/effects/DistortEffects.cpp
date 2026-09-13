#include "DistortEffects.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace pdn {

// ==========================================
// Pixelate
// ==========================================
void PixelateEffect::process(QImage& image, const Selection& selection, int cellSize) {
    if (cellSize <= 1) return;

    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    for (int by = 0; by < h; by += cellSize) {
        int cellH = std::min(cellSize, h - by);
        for (int bx = 0; bx < w; bx += cellSize) {
            int cellW = std::min(cellSize, w - bx);

            uint32_t sumA = 0, sumR = 0, sumG = 0, sumB = 0;
            int count = 0;

            for (int dy = 0; dy < cellH; ++dy) {
                const uint32_t* line = reinterpret_cast<const uint32_t*>(image.constScanLine(by + dy));
                for (int dx = 0; dx < cellW; ++dx) {
                    int pxX = bx + dx;
                    int pxY = by + dy;
                    if (!selection.isEmpty() && !selection.containsPixel(pxX, pxY)) continue;

                    uint32_t px = line[pxX];
                    sumA += (px >> 24) & 0xFF;
                    sumR += (px >> 16) & 0xFF;
                    sumG += (px >> 8) & 0xFF;
                    sumB += px & 0xFF;
                    count++;
                }
            }

            if (count > 0) {
                uint32_t avgA = sumA / count;
                uint32_t avgR = sumR / count;
                uint32_t avgG = sumG / count;
                uint32_t avgB = sumB / count;
                uint32_t finalPx = (avgA << 24) | (avgR << 16) | (avgG << 8) | avgB;

                for (int dy = 0; dy < cellH; ++dy) {
                    uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(by + dy));
                    for (int dx = 0; dx < cellW; ++dx) {
                        int pxX = bx + dx;
                        int pxY = by + dy;
                        if (!selection.isEmpty() && !selection.containsPixel(pxX, pxY)) continue;
                        line[pxX] = finalPx;
                    }
                }
            }
        }
    }
}

bool PixelateEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 4);
    return true;
}

bool PixelateEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    PixelateDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

PixelateDialog::PixelateDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Pixelate", parent) {
    addSlider("Cell size (pixels):", 1, 100, m_cellSize, [this](int val) { m_cellSize = val; });

    setupButtons();
    updatePreview();
}

void PixelateDialog::processPreview(QImage& image) {
    PixelateEffect::process(image, m_doc->selection(), m_cellSize);
}

// ==========================================
// Twist
// ==========================================
void TwistEffect::process(QImage& image, const Selection& selection, int amount, int size, const QPointF& center) {
    if (amount == 0 || size <= 0) return;

    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    QImage src = image.copy();
    double cx = (center.x() >= 0.0) ? center.x() : (w * 0.5);
    double cy = (center.y() >= 0.0) ? center.y() : (h * 0.5);
    double maxDim = std::max(w, h);
    double radius = (maxDim * 0.5) * (std::clamp(size, 1, 100) / 100.0);
    double maxAngle = (amount / 100.0) * M_PI * 2.0;

    for (int y = 0; y < h; ++y) {
        uint32_t* dstLine = reinterpret_cast<uint32_t*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            if (!selection.isEmpty() && !selection.containsPixel(x, y)) continue;

            double dx = x - cx;
            double dy = y - cy;
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist < radius) {
                double factor = (1.0 - dist / radius);
                double theta = -maxAngle * factor * factor; // negative for intuitive clockwise
                double cosT = std::cos(theta);
                double sinT = std::sin(theta);

                double rx = dx * cosT - dy * sinT;
                double ry = dx * sinT + dy * cosT;

                int sx = std::clamp(static_cast<int>(std::round(cx + rx)), 0, w - 1);
                int sy = std::clamp(static_cast<int>(std::round(cy + ry)), 0, h - 1);

                const uint32_t* srcLine = reinterpret_cast<const uint32_t*>(src.constScanLine(sy));
                dstLine[x] = srcLine[sx];
            }
        }
    }
}

bool TwistEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 45, 50);
    return true;
}

bool TwistEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    TwistDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

TwistDialog::TwistDialog(Document* doc, QWidget* parent)
    : EffectDialog(doc, "Twist", parent) {
    int docW = doc ? doc->width() : 800;
    int docH = doc ? doc->height() : 600;
    m_center = QPointF(docW * 0.5, docH * 0.5);

    addSlider("Amount / Direction:", -100, 100, m_amount, [this](int val) {
        m_amount = val;
        if (m_bridge) m_bridge->requestCanvasRepaint();
    });
    addSlider("Size (%):", 1, 100, m_size, [this](int val) {
        m_size = val;
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

void TwistDialog::onCanvasPointPicked(const QPointF& docPos) {
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

void TwistDialog::drawCanvasOverlay(QPainter& painter, const RenderOptions& opts) {
    int docW = m_doc ? m_doc->width() : 800;
    int docH = m_doc ? m_doc->height() : 600;
    double maxDim = std::max(docW, docH);
    double radius = (maxDim * 0.5) * (std::clamp(m_size, 1, 100) / 100.0);
    drawCrosshairOverlay(painter, m_center, opts.zoom, opts.panOffset, radius);
}

void TwistDialog::onReset() {
    int docW = m_doc ? m_doc->width() : 800;
    int docH = m_doc ? m_doc->height() : 600;
    m_center = QPointF(docW * 0.5, docH * 0.5);
    if (m_centerXCtrl.slider) m_centerXCtrl.slider->setValue(static_cast<int>(m_center.x()));
    if (m_centerYCtrl.slider) m_centerYCtrl.slider->setValue(static_cast<int>(m_center.y()));
    if (m_bridge) m_bridge->requestCanvasRepaint();
}

void TwistDialog::processPreview(QImage& image) {
    TwistEffect::process(image, m_doc->selection(), m_amount, m_size, m_center);
}

} // namespace pdn
