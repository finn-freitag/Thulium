#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QButtonGroup>
#include <QTextEdit>
#include <memory>
#include "../core/Document.h"

namespace pdn {

enum class CanvasExpandChoice {
    Prompt,
    ExpandCanvas,
    KeepCanvasSize,
    Cancel
};

enum class DropActionChoice {
    Prompt,
    Open,
    AddAsLayer,
    Cancel
};

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
    ResampleAlgorithm algorithm() const;

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
    QComboBox* m_resampleCombo;
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
    bool isTransparentBackground() const;
    QRadioButton* whiteRadioButton() const { return m_whiteRadio; }
    QRadioButton* transparentRadioButton() const { return m_transparentRadio; }

private slots:
    void onPresetChanged(int index);

private:
    QSpinBox* m_widthSpin;
    QSpinBox* m_heightSpin;
    QComboBox* m_presetCombo;
    QRadioButton* m_whiteRadio;
    QRadioButton* m_transparentRadio;
};

class MetadataDialog : public QDialog {
    Q_OBJECT
public:
    explicit MetadataDialog(const Document* doc, QWidget* parent = nullptr);

    Metadata metadata() const;

public slots:
    void onClearAll();
    void onSetCurrentDateTime();

private:
    QLineEdit* m_titleEdit = nullptr;
    QLineEdit* m_authorEdit = nullptr;
    QLineEdit* m_copyrightEdit = nullptr;
    QTextEdit* m_descriptionEdit = nullptr;
    QLineEdit* m_creationDateEdit = nullptr;
    QLineEdit* m_softwareEdit = nullptr;

    Metadata m_initialMetadata;
};

} // namespace pdn
