#pragma once

#include "IEffect.h"
#include "EffectDialog.h"

namespace pdn {

// Pixelate
class PixelateEffect : public IEffect {
public:
    QString name() const override { return "Pixelate"; }
    QString category() const override { return "Distort"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int cellSize);
};

class PixelateDialog : public EffectDialog {
    Q_OBJECT
public:
    PixelateDialog(Document* doc, QWidget* parent = nullptr);

    int cellSize() const { return m_cellSize; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_cellSize = 4;
};

// Twist
class TwistEffect : public IEffect {
public:
    QString name() const override { return "Twist"; }
    QString category() const override { return "Distort"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int amount, int size);
};

class TwistDialog : public EffectDialog {
    Q_OBJECT
public:
    TwistDialog(Document* doc, QWidget* parent = nullptr);

    int amount() const { return m_amount; }
    int size() const { return m_size; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_amount = 45;
    int m_size = 50;
};

} // namespace pdn
