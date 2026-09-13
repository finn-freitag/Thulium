#pragma once

#include "IEffect.h"
#include <QDialog>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>

namespace pdn {

class GaussianBlurEffect : public IEffect {
public:
    QString name() const override { return "Gaussian Blur"; }
    QString category() const override { return "Blurs"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int radius);
};

class GaussianBlurDialog : public QDialog {
    Q_OBJECT
public:
    GaussianBlurDialog(Document* doc, QWidget* parent = nullptr);

    int radius() const { return m_radiusSlider->value(); }

protected:
    void reject() override;

private slots:
    void onValueChanged();
    void onReset();

private:
    Document* m_doc;
    QImage m_originalImage;
    int m_layerIndex;

    QSlider* m_radiusSlider;
    QSpinBox* m_radiusSpin;
};

} // namespace pdn
