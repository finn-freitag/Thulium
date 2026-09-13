#pragma once

#include "IEffect.h"
#include "EffectDialog.h"

namespace pdn {

class GaussianBlurEffect : public IEffect {
public:
    QString name() const override { return "Gaussian Blur"; }
    QString category() const override { return "Blurs"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int radius);
};

class GaussianBlurDialog : public EffectDialog {
    Q_OBJECT
public:
    GaussianBlurDialog(Document* doc, QWidget* parent = nullptr);

    int radius() const { return m_radius; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_radius = 2;
};

} // namespace pdn
