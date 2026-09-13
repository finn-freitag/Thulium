#pragma once

#include "IEffect.h"
#include "EffectDialog.h"

namespace pdn {

// Sharpen
class SharpenEffect : public IEffect {
public:
    QString name() const override { return "Sharpen"; }
    QString category() const override { return "Photo"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int amount);
};

class SharpenDialog : public EffectDialog {
    Q_OBJECT
public:
    SharpenDialog(Document* doc, QWidget* parent = nullptr);

    int amount() const { return m_amount; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_amount = 2;
};

// Glow
class GlowEffect : public IEffect {
public:
    QString name() const override { return "Glow"; }
    QString category() const override { return "Photo"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int radius, int brightness, int contrast);
};

class GlowDialog : public EffectDialog {
    Q_OBJECT
public:
    GlowDialog(Document* doc, QWidget* parent = nullptr);

    int radius() const { return m_radius; }
    int brightness() const { return m_brightness; }
    int contrast() const { return m_contrast; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_radius = 6;
    int m_brightness = 10;
    int m_contrast = 10;
};

// Vignette
class VignetteEffect : public IEffect {
public:
    QString name() const override { return "Vignette"; }
    QString category() const override { return "Photo"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int radius, int density,
                        const QPointF& center = QPointF(-1, -1));
};

class VignetteDialog : public EffectDialog {
    Q_OBJECT
public:
    VignetteDialog(Document* doc, QWidget* parent = nullptr);

    int radius() const { return m_radius; }
    int density() const { return m_density; }
    QPointF center() const { return m_center; }

    void onCanvasPointPicked(const QPointF& docPos) override;
    void drawCanvasOverlay(QPainter& painter, const RenderOptions& opts) override;

protected:
    void processPreview(QImage& image) override;
    void onReset() override;

private:
    int m_radius = 50;
    int m_density = 50;
    QPointF m_center;
    SliderControls m_centerXCtrl;
    SliderControls m_centerYCtrl;
};

} // namespace pdn
