#pragma once

#include "IEffect.h"
#include "EffectDialog.h"

namespace pdn {

// 1. Auto-Level
class AutoLevelEffect : public IEffect {
public:
    QString name() const override { return "Auto-Level"; }
    QString category() const override { return "Adjustments"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection);
};

// 2. Black and White
class BlackAndWhiteEffect : public IEffect {
public:
    QString name() const override { return "Black and White"; }
    QString category() const override { return "Adjustments"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection);
};

// 3. Invert Colors
class InvertColorsEffect : public IEffect {
public:
    QString name() const override { return "Invert Colors"; }
    QString category() const override { return "Adjustments"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection);
};

// 4. Invert Alpha
class InvertAlphaEffect : public IEffect {
public:
    QString name() const override { return "Invert Alpha"; }
    QString category() const override { return "Adjustments"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection);
};

// 5. Sepia
class SepiaEffect : public IEffect {
public:
    QString name() const override { return "Sepia"; }
    QString category() const override { return "Adjustments"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection);
};

// 6. Posterize
class PosterizeEffect : public IEffect {
public:
    QString name() const override { return "Posterize"; }
    QString category() const override { return "Adjustments"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int levels);
};

class PosterizeDialog : public EffectDialog {
    Q_OBJECT
public:
    PosterizeDialog(Document* doc, QWidget* parent = nullptr);

    int levels() const { return m_levels; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_levels = 16;
};

// 7. Hue / Saturation
class HueSaturationEffect : public IEffect {
public:
    QString name() const override { return "Hue / Saturation"; }
    QString category() const override { return "Adjustments"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int hue, int saturation, int lightness);
};

class HueSaturationDialog : public EffectDialog {
    Q_OBJECT
public:
    HueSaturationDialog(Document* doc, QWidget* parent = nullptr);

    int hue() const { return m_hue; }
    int saturation() const { return m_saturation; }
    int lightness() const { return m_lightness; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_hue = 0;
    int m_saturation = 100;
    int m_lightness = 0;
};

// 8. Temperature / Tint
class TemperatureTintEffect : public IEffect {
public:
    QString name() const override { return "Temperature / Tint"; }
    QString category() const override { return "Adjustments"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int temperature, int tint);
};

class TemperatureTintDialog : public EffectDialog {
    Q_OBJECT
public:
    TemperatureTintDialog(Document* doc, QWidget* parent = nullptr);

    int temperature() const { return m_temperature; }
    int tint() const { return m_tint; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_temperature = 0;
    int m_tint = 0;
};

} // namespace pdn
