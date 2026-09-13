#include "EffectDialog.h"

namespace pdn {

EffectDialog::EffectDialog(Document* doc, const QString& effectName, QWidget* parent)
    : QDialog(parent), m_doc(doc), m_effectName(effectName) {
    setWindowTitle(effectName);
    setMinimumWidth(380);

    m_layerIndex = m_doc ? m_doc->activeLayerIndex() : -1;
    if (m_doc && m_doc->activeLayer()) {
        m_originalImage = m_doc->activeLayer()->image().copy();
    }

    m_mainLayout = new QVBoxLayout(this);
}

EffectDialog::SliderControls EffectDialog::addSlider(const QString& labelText, int minVal, int maxVal,
                                                     int defaultVal, std::function<void(int)> onChanged) {
    QLabel* label = new QLabel(labelText, this);
    QHBoxLayout* row = new QHBoxLayout();
    QSlider* slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(minVal, maxVal);
    slider->setValue(defaultVal);

    QSpinBox* spin = new QSpinBox(this);
    spin->setRange(minVal, maxVal);
    spin->setValue(defaultVal);

    row->addWidget(slider);
    row->addWidget(spin);

    connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);

    connect(slider, &QSlider::valueChanged, this, [this, onChanged](int val) {
        if (onChanged) onChanged(val);
        updatePreview();
    });

    m_mainLayout->addWidget(label);
    m_mainLayout->addLayout(row);

    SliderControls ctrl{slider, spin, defaultVal};
    m_sliders.push_back(ctrl);
    return ctrl;
}

QCheckBox* EffectDialog::addCheckBox(const QString& labelText, bool defaultChecked,
                                     std::function<void(bool)> onChanged) {
    QCheckBox* cb = new QCheckBox(labelText, this);
    cb->setChecked(defaultChecked);

    connect(cb, &QCheckBox::toggled, this, [this, onChanged](bool checked) {
        if (onChanged) onChanged(checked);
        updatePreview();
    });

    m_mainLayout->addWidget(cb);
    m_checkBoxes.push_back({cb, defaultChecked});
    return cb;
}

void EffectDialog::addCustomWidget(QWidget* widget) {
    if (widget) {
        m_mainLayout->addWidget(widget);
    }
}

void EffectDialog::setupButtons() {
    if (m_buttonsAdded) return;
    m_buttonsAdded = true;

    m_mainLayout->addStretch();

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* resetBtn = new QPushButton("Reset", this);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        for (const auto& s : m_sliders) {
            s.slider->setValue(s.defaultValue);
        }
        for (const auto& cb : m_checkBoxes) {
            cb.first->setChecked(cb.second);
        }
        onReset();
        updatePreview();
    });

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bbox, &QDialogButtonBox::accepted, this, &EffectDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &EffectDialog::reject);

    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(bbox);

    m_mainLayout->addLayout(btnLayout);
}

void EffectDialog::onReset() {
    // Default implementation does nothing extra
}

void EffectDialog::updatePreview() {
    if (!m_doc || m_layerIndex < 0) return;
    auto layer = m_doc->layer(m_layerIndex);
    if (!layer) return;

    QImage copy = m_originalImage.copy();
    processPreview(copy);
    layer->setImage(copy);
    emit m_doc->documentChanged();
}

void EffectDialog::reject() {
    if (m_doc && m_layerIndex >= 0) {
        auto layer = m_doc->layer(m_layerIndex);
        if (layer) {
            layer->setImage(m_originalImage.copy());
            emit m_doc->documentChanged();
        }
    }
    QDialog::reject();
}

void EffectDialog::accept() {
    if (m_doc && m_layerIndex >= 0) {
        m_doc->undoStack()->push(new LayerBitmapUndoCommand(m_doc, m_layerIndex, m_originalImage, m_effectName));
    }
    QDialog::accept();
}

bool applyInstantEffect(Document* doc, const QString& name,
                        const std::function<void(QImage&, const Selection&)>& processFunc) {
    if (!doc || !doc->activeLayer()) return false;
    int layerIdx = doc->activeLayerIndex();
    QImage original = doc->activeLayer()->image().copy();
    QImage copy = original.copy();
    processFunc(copy, doc->selection());
    doc->activeLayer()->setImage(copy);
    doc->undoStack()->push(new LayerBitmapUndoCommand(doc, layerIdx, original, name));
    emit doc->documentChanged();
    return true;
}

} // namespace pdn
