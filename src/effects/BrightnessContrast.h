#pragma once

#include "IEffect.h"
#include "EffectDialog.h"

namespace pdn {

class BrightnessContrastEffect : public IEffect {
public:
    QString name() const override { return "Brightness / Contrast"; }
    QString category() const override { return "Adjustments"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int brightness, int contrast);
};

class BrightnessContrastDialog : public EffectDialog {
    Q_OBJECT
public:
    BrightnessContrastDialog(Document* doc, QWidget* parent = nullptr);

    int brightness() const { return m_brightness; }
    int contrast() const { return m_contrast; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_brightness = 0;
    int m_contrast = 0;
};

} // namespace pdn
