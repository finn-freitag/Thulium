#include "GaussianBlur.h"
#include "../core/History.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <vector>
#include <cmath>
#include <algorithm>

namespace pdn {

// Fast box blur horizontal pass
static void boxBlurH(const uint32_t* src, uint32_t* dst, int w, int h, int r) {
    float iarr = 1.0f / (r + r + 1);
    for (int i = 0; i < h; ++i) {
        int ti = i * w;
        int li = ti;
        int ri = ti + r;

        uint32_t fv = src[ti];
        uint32_t lv = src[ti + w - 1];

        int valA = ((fv >> 24) & 0xFF) * (r + 1);
        int valR = ((fv >> 16) & 0xFF) * (r + 1);
        int valG = ((fv >> 8) & 0xFF) * (r + 1);
        int valB = (fv & 0xFF) * (r + 1);

        for (int j = 0; j < r; ++j) {
            uint32_t cur = src[ti + std::min(j, w - 1)];
            valA += (cur >> 24) & 0xFF;
            valR += (cur >> 16) & 0xFF;
            valG += (cur >> 8) & 0xFF;
            valB += cur & 0xFF;
        }

        for (int j = 0; j <= r; ++j) {
            uint32_t cur = src[std::min(ri++, ti + w - 1)];
            valA += ((cur >> 24) & 0xFF) - ((fv >> 24) & 0xFF);
            valR += ((cur >> 16) & 0xFF) - ((fv >> 16) & 0xFF);
            valG += ((cur >> 8) & 0xFF) - ((fv >> 8) & 0xFF);
            valB += (cur & 0xFF) - (fv & 0xFF);

            dst[ti++] = (static_cast<uint32_t>(valA * iarr + 0.5f) << 24) |
                        (static_cast<uint32_t>(valR * iarr + 0.5f) << 16) |
                        (static_cast<uint32_t>(valG * iarr + 0.5f) << 8) |
                        static_cast<uint32_t>(valB * iarr + 0.5f);
        }

        for (int j = r + 1; j < w - r; ++j) {
            uint32_t curR = src[ri++];
            uint32_t curL = src[li++];
            valA += ((curR >> 24) & 0xFF) - ((curL >> 24) & 0xFF);
            valR += ((curR >> 16) & 0xFF) - ((curL >> 16) & 0xFF);
            valG += ((curR >> 8) & 0xFF) - ((curL >> 8) & 0xFF);
            valB += (curR & 0xFF) - (curL & 0xFF);

            dst[ti++] = (static_cast<uint32_t>(valA * iarr + 0.5f) << 24) |
                        (static_cast<uint32_t>(valR * iarr + 0.5f) << 16) |
                        (static_cast<uint32_t>(valG * iarr + 0.5f) << 8) |
                        static_cast<uint32_t>(valB * iarr + 0.5f);
        }

        for (int j = w - r; j < w; ++j) {
            uint32_t curL = src[li++];
            valA += ((lv >> 24) & 0xFF) - ((curL >> 24) & 0xFF);
            valR += ((lv >> 16) & 0xFF) - ((curL >> 16) & 0xFF);
            valG += ((lv >> 8) & 0xFF) - ((curL >> 8) & 0xFF);
            valB += (lv & 0xFF) - (curL & 0xFF);

            dst[ti++] = (static_cast<uint32_t>(valA * iarr + 0.5f) << 24) |
                        (static_cast<uint32_t>(valR * iarr + 0.5f) << 16) |
                        (static_cast<uint32_t>(valG * iarr + 0.5f) << 8) |
                        static_cast<uint32_t>(valB * iarr + 0.5f);
        }
    }
}

// Fast box blur vertical pass
static void boxBlurT(const uint32_t* src, uint32_t* dst, int w, int h, int r) {
    float iarr = 1.0f / (r + r + 1);
    for (int i = 0; i < w; ++i) {
        int ti = i;
        int li = ti;
        int ri = ti + r * w;

        uint32_t fv = src[ti];
        uint32_t lv = src[ti + w * (h - 1)];

        int valA = ((fv >> 24) & 0xFF) * (r + 1);
        int valR = ((fv >> 16) & 0xFF) * (r + 1);
        int valG = ((fv >> 8) & 0xFF) * (r + 1);
        int valB = (fv & 0xFF) * (r + 1);

        for (int j = 0; j < r; ++j) {
            uint32_t cur = src[ti + std::min(j, h - 1) * w];
            valA += (cur >> 24) & 0xFF;
            valR += (cur >> 16) & 0xFF;
            valG += (cur >> 8) & 0xFF;
            valB += cur & 0xFF;
        }

        for (int j = 0; j <= r; ++j) {
            uint32_t cur = src[std::min(ri, ti + (h - 1) * w)];
            ri += w;
            valA += ((cur >> 24) & 0xFF) - ((fv >> 24) & 0xFF);
            valR += ((cur >> 16) & 0xFF) - ((fv >> 16) & 0xFF);
            valG += ((cur >> 8) & 0xFF) - ((fv >> 8) & 0xFF);
            valB += (cur & 0xFF) - (fv & 0xFF);

            dst[ti] = (static_cast<uint32_t>(valA * iarr + 0.5f) << 24) |
                      (static_cast<uint32_t>(valR * iarr + 0.5f) << 16) |
                      (static_cast<uint32_t>(valG * iarr + 0.5f) << 8) |
                      static_cast<uint32_t>(valB * iarr + 0.5f);
            ti += w;
        }

        for (int j = r + 1; j < h - r; ++j) {
            uint32_t curR = src[ri];
            uint32_t curL = src[li];
            ri += w;
            li += w;
            valA += ((curR >> 24) & 0xFF) - ((curL >> 24) & 0xFF);
            valR += ((curR >> 16) & 0xFF) - ((curL >> 16) & 0xFF);
            valG += ((curR >> 8) & 0xFF) - ((curL >> 8) & 0xFF);
            valB += (curR & 0xFF) - (curL & 0xFF);

            dst[ti] = (static_cast<uint32_t>(valA * iarr + 0.5f) << 24) |
                      (static_cast<uint32_t>(valR * iarr + 0.5f) << 16) |
                      (static_cast<uint32_t>(valG * iarr + 0.5f) << 8) |
                      static_cast<uint32_t>(valB * iarr + 0.5f);
            ti += w;
        }

        for (int j = h - r; j < h; ++j) {
            uint32_t curL = src[li];
            li += w;
            valA += ((lv >> 24) & 0xFF) - ((curL >> 24) & 0xFF);
            valR += ((lv >> 16) & 0xFF) - ((curL >> 16) & 0xFF);
            valG += ((lv >> 8) & 0xFF) - ((curL >> 8) & 0xFF);
            valB += (lv & 0xFF) - (curL & 0xFF);

            dst[ti] = (static_cast<uint32_t>(valA * iarr + 0.5f) << 24) |
                      (static_cast<uint32_t>(valR * iarr + 0.5f) << 16) |
                      (static_cast<uint32_t>(valG * iarr + 0.5f) << 8) |
                      static_cast<uint32_t>(valB * iarr + 0.5f);
            ti += w;
        }
    }
}

void GaussianBlurEffect::process(QImage& image, const Selection& selection, int radius) {
    if (radius <= 0) return;
    int w = image.width();
    int h = image.height();
    if (w <= 0 || h <= 0) return;

    std::vector<uint32_t> buf(w * h);
    const uint32_t* src = reinterpret_cast<const uint32_t*>(image.constBits());
    uint32_t* dst = buf.data();

    // 3 passes of box blur = excellent approximation of Gaussian blur
    boxBlurH(src, dst, w, h, radius);
    boxBlurT(dst, reinterpret_cast<uint32_t*>(image.bits()), w, h, radius);

    // Apply back only inside selection if selection is active
    if (!selection.isEmpty()) {
        uint32_t* imgBits = reinterpret_cast<uint32_t*>(image.bits());
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (!selection.containsPixel(x, y)) {
                    imgBits[y * w + x] = src[y * w + x];
                }
            }
        }
    }
}

bool GaussianBlurEffect::apply(QImage& image, const Selection& selection) {
    process(image, selection, 5);
    return true;
}

bool GaussianBlurEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    GaussianBlurDialog dlg(doc, parent);
    return dlg.exec() == QDialog::Accepted;
}

GaussianBlurDialog::GaussianBlurDialog(Document* doc, QWidget* parent)
    : QDialog(parent), m_doc(doc) {
    setWindowTitle("Gaussian Blur");
    setFixedSize(360, 160);

    m_layerIndex = doc->activeLayerIndex();
    m_originalImage = doc->activeLayer()->image().copy();

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* rLabel = new QLabel("Radius (pixels):", this);
    QHBoxLayout* rLayout = new QHBoxLayout();
    m_radiusSlider = new QSlider(Qt::Horizontal, this);
    m_radiusSlider->setRange(1, 100);
    m_radiusSlider->setValue(2);
    m_radiusSpin = new QSpinBox(this);
    m_radiusSpin->setRange(1, 100);
    m_radiusSpin->setValue(2);
    rLayout->addWidget(m_radiusSlider);
    rLayout->addWidget(m_radiusSpin);

    connect(m_radiusSlider, &QSlider::valueChanged, m_radiusSpin, &QSpinBox::setValue);
    connect(m_radiusSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_radiusSlider, &QSlider::setValue);
    connect(m_radiusSlider, &QSlider::valueChanged, this, &GaussianBlurDialog::onValueChanged);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* resetBtn = new QPushButton("Reset", this);
    connect(resetBtn, &QPushButton::clicked, this, &GaussianBlurDialog::onReset);

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bbox, &QDialogButtonBox::accepted, this, [this]() {
        m_doc->undoStack()->push(new LayerBitmapUndoCommand(m_doc, m_layerIndex, m_originalImage, "Gaussian Blur"));
        accept();
    });
    connect(bbox, &QDialogButtonBox::rejected, this, &GaussianBlurDialog::reject);

    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(bbox);

    mainLayout->addWidget(rLabel);
    mainLayout->addLayout(rLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // Initial preview
    onValueChanged();
}

void GaussianBlurDialog::onValueChanged() {
    auto layer = m_doc->layer(m_layerIndex);
    if (!layer) return;

    QImage copy = m_originalImage.copy();
    GaussianBlurEffect::process(copy, m_doc->selection(), m_radiusSlider->value());
    layer->setImage(copy);
    emit m_doc->documentChanged();
}

void GaussianBlurDialog::onReset() {
    m_radiusSlider->setValue(2);
}

void GaussianBlurDialog::reject() {
    auto layer = m_doc->layer(m_layerIndex);
    if (layer) {
        layer->setImage(m_originalImage.copy());
        emit m_doc->documentChanged();
    }
    QDialog::reject();
}

} // namespace pdn
