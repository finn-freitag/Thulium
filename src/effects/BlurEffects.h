#pragma once

#include "IEffect.h"
#include "EffectDialog.h"

namespace pdn {

// Motion Blur
class MotionBlurEffect : public IEffect {
public:
    QString name() const override { return "Motion Blur"; }
    QString category() const override { return "Blurs"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int angle, int distance, bool centered);
};

class MotionBlurDialog : public EffectDialog {
    Q_OBJECT
public:
    MotionBlurDialog(Document* doc, QWidget* parent = nullptr);

    int angle() const { return m_angle; }
    int distance() const { return m_distance; }
    bool centered() const { return m_centered; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_angle = 0;
    int m_distance = 10;
    bool m_centered = true;
};

// Radial Blur
class RadialBlurEffect : public IEffect {
public:
    QString name() const override { return "Radial Blur"; }
    QString category() const override { return "Blurs"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int angle);
};

class RadialBlurDialog : public EffectDialog {
    Q_OBJECT
public:
    RadialBlurDialog(Document* doc, QWidget* parent = nullptr);

    int angle() const { return m_angle; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_angle = 10;
};

} // namespace pdn
