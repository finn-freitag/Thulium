#pragma once

#include "IEffect.h"
#include <QDialog>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>

namespace pdn {

class BrightnessContrastEffect : public IEffect {
public:
    QString name() const override { return "Brightness / Contrast"; }
    QString category() const override { return "Adjustments"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int brightness, int contrast);
};

class BrightnessContrastDialog : public QDialog {
    Q_OBJECT
public:
    BrightnessContrastDialog(Document* doc, QWidget* parent = nullptr);

    int brightness() const { return m_brightnessSlider->value(); }
    int contrast() const { return m_contrastSlider->value(); }

protected:
    void reject() override;

private slots:
    void onValueChanged();
    void onReset();

private:
    Document* m_doc;
    QImage m_originalImage;
    int m_layerIndex;

    QSlider* m_brightnessSlider;
    QSpinBox* m_brightnessSpin;
    QSlider* m_contrastSlider;
    QSpinBox* m_contrastSpin;
};

} // namespace pdn
