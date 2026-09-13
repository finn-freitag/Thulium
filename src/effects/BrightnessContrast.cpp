#include "BrightnessContrast.h"
#include "../core/History.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
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
    : QDialog(parent), m_doc(doc) {
    setWindowTitle("Brightness / Contrast");
    setFixedSize(360, 220);

    m_layerIndex = doc->activeLayerIndex();
    m_originalImage = doc->activeLayer()->image().copy();

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Brightness row
    QLabel* bLabel = new QLabel("Brightness:", this);
    QHBoxLayout* bLayout = new QHBoxLayout();
    m_brightnessSlider = new QSlider(Qt::Horizontal, this);
    m_brightnessSlider->setRange(-100, 100);
    m_brightnessSlider->setValue(0);
    m_brightnessSpin = new QSpinBox(this);
    m_brightnessSpin->setRange(-100, 100);
    m_brightnessSpin->setValue(0);
    bLayout->addWidget(m_brightnessSlider);
    bLayout->addWidget(m_brightnessSpin);

    // Contrast row
    QLabel* cLabel = new QLabel("Contrast:", this);
    QHBoxLayout* cLayout = new QHBoxLayout();
    m_contrastSlider = new QSlider(Qt::Horizontal, this);
    m_contrastSlider->setRange(-100, 100);
    m_contrastSlider->setValue(0);
    m_contrastSpin = new QSpinBox(this);
    m_contrastSpin->setRange(-100, 100);
    m_contrastSpin->setValue(0);
    cLayout->addWidget(m_contrastSlider);
    cLayout->addWidget(m_contrastSpin);

    // Sync sliders and spins
    connect(m_brightnessSlider, &QSlider::valueChanged, m_brightnessSpin, &QSpinBox::setValue);
    connect(m_brightnessSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_brightnessSlider, &QSlider::setValue);
    connect(m_contrastSlider, &QSlider::valueChanged, m_contrastSpin, &QSpinBox::setValue);
    connect(m_contrastSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_contrastSlider, &QSlider::setValue);

    connect(m_brightnessSlider, &QSlider::valueChanged, this, &BrightnessContrastDialog::onValueChanged);
    connect(m_contrastSlider, &QSlider::valueChanged, this, &BrightnessContrastDialog::onValueChanged);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* resetBtn = new QPushButton("Reset", this);
    connect(resetBtn, &QPushButton::clicked, this, &BrightnessContrastDialog::onReset);

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bbox, &QDialogButtonBox::accepted, this, [this]() {
        // Push undo command
        m_doc->undoStack()->push(new LayerBitmapUndoCommand(m_doc, m_layerIndex, m_originalImage, "Brightness / Contrast"));
        accept();
    });
    connect(bbox, &QDialogButtonBox::rejected, this, &BrightnessContrastDialog::reject);

    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(bbox);

    mainLayout->addWidget(bLabel);
    mainLayout->addLayout(bLayout);
    mainLayout->addWidget(cLabel);
    mainLayout->addLayout(cLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(btnLayout);
}

void BrightnessContrastDialog::onValueChanged() {
    auto layer = m_doc->layer(m_layerIndex);
    if (!layer) return;

    QImage copy = m_originalImage.copy();
    BrightnessContrastEffect::process(copy, m_doc->selection(), m_brightnessSlider->value(), m_contrastSlider->value());
    layer->setImage(copy);
    emit m_doc->documentChanged();
}

void BrightnessContrastDialog::onReset() {
    m_brightnessSlider->setValue(0);
    m_contrastSlider->setValue(0);
}

void BrightnessContrastDialog::reject() {
    auto layer = m_doc->layer(m_layerIndex);
    if (layer) {
        layer->setImage(m_originalImage.copy());
        emit m_doc->documentChanged();
    }
    QDialog::reject();
}

} // namespace pdn
