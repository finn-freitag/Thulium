#pragma once

#include "IEffect.h"
#include "EffectDialog.h"

namespace pdn {

// Add Noise
class AddNoiseEffect : public IEffect {
public:
    QString name() const override { return "Add Noise"; }
    QString category() const override { return "Noise"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int intensity, int colorSaturation, int coverage);
};

class AddNoiseDialog : public EffectDialog {
    Q_OBJECT
public:
    AddNoiseDialog(Document* doc, QWidget* parent = nullptr);

    int intensity() const { return m_intensity; }
    int colorSaturation() const { return m_colorSaturation; }
    int coverage() const { return m_coverage; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_intensity = 64;
    int m_colorSaturation = 100;
    int m_coverage = 100;
};

// Median
class MedianEffect : public IEffect {
public:
    QString name() const override { return "Median"; }
    QString category() const override { return "Noise"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int radius, int percentile);
};

class MedianDialog : public EffectDialog {
    Q_OBJECT
public:
    MedianDialog(Document* doc, QWidget* parent = nullptr);

    int radius() const { return m_radius; }
    int percentile() const { return m_percentile; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_radius = 2;
    int m_percentile = 50;
};

} // namespace pdn
