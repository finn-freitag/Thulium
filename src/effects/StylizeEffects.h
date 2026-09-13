#pragma once

#include "IEffect.h"
#include "EffectDialog.h"

namespace pdn {

// Edge Detect
class EdgeDetectEffect : public IEffect {
public:
    QString name() const override { return "Edge Detect"; }
    QString category() const override { return "Stylize"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int sensitivity);
};

class EdgeDetectDialog : public EffectDialog {
    Q_OBJECT
public:
    EdgeDetectDialog(Document* doc, QWidget* parent = nullptr);

    int sensitivity() const { return m_sensitivity; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_sensitivity = 5;
};

// Emboss
class EmbossEffect : public IEffect {
public:
    QString name() const override { return "Emboss"; }
    QString category() const override { return "Stylize"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int angle);
};

class EmbossDialog : public EffectDialog {
    Q_OBJECT
public:
    EmbossDialog(Document* doc, QWidget* parent = nullptr);

    int angle() const { return m_angle; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_angle = 45;
};

// Oil Painting (Artistic)
class OilPaintingEffect : public IEffect {
public:
    QString name() const override { return "Oil Painting"; }
    QString category() const override { return "Artistic"; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

    static void process(QImage& image, const Selection& selection, int brushSize, int coarseness);
};

class OilPaintingDialog : public EffectDialog {
    Q_OBJECT
public:
    OilPaintingDialog(Document* doc, QWidget* parent = nullptr);

    int brushSize() const { return m_brushSize; }
    int coarseness() const { return m_coarseness; }

protected:
    void processPreview(QImage& image) override;

private:
    int m_brushSize = 3;
    int m_coarseness = 50;
};

} // namespace pdn
