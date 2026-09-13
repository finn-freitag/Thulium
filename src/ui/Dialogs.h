#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QButtonGroup>
#include <memory>
#include "../core/Document.h"

namespace pdn {

class LayerPropertiesDialog : public QDialog {
    Q_OBJECT
public:
    LayerPropertiesDialog(Document* doc, int layerIndex, QWidget* parent = nullptr);

    QString layerName() const;
    bool isLayerVisible() const;
    uint8_t opacity() const;
    BlendMode blendMode() const;

private slots:
    void applyChanges();

private:
    Document* m_doc;
    int m_layerIndex;

    QLineEdit* m_nameEdit;
    QCheckBox* m_visibleCheck;
    QSlider* m_opacitySlider;
    QSpinBox* m_opacitySpin;
    QComboBox* m_blendModeCombo;

    QString m_origName;
    bool m_origVisible;
    uint8_t m_origOpacity;
    BlendMode m_origBlendMode;
};

class ResizeImageDialog : public QDialog {
    Q_OBJECT
public:
    ResizeImageDialog(int currentWidth, int currentHeight, QWidget* parent = nullptr);

    int newWidth() const;
    int newHeight() const;

private slots:
    void onWidthChanged(int w);
    void onHeightChanged(int h);

private:
    int m_origWidth;
    int m_origHeight;
    double m_aspectRatio;

    QSpinBox* m_widthSpin;
    QSpinBox* m_heightSpin;
    QCheckBox* m_maintainAspectCheck;
    bool m_updating = false;
};

class CanvasSizeDialog : public QDialog {
    Q_OBJECT
public:
    CanvasSizeDialog(int currentWidth, int currentHeight, QWidget* parent = nullptr);

    int newWidth() const;
    int newHeight() const;
    Qt::Alignment anchor() const;

private:
    int m_origWidth;
    int m_origHeight;

    QSpinBox* m_widthSpin;
    QSpinBox* m_heightSpin;
    QButtonGroup* m_anchorGroup;
};

class NewImageDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewImageDialog(QWidget* parent = nullptr);

    int imageWidth() const;
    int imageHeight() const;

private slots:
    void onPresetChanged(int index);

private:
    QSpinBox* m_widthSpin;
    QSpinBox* m_heightSpin;
    QComboBox* m_presetCombo;
};

} // namespace pdn
