#pragma once

#include <QDialog>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <functional>
#include <vector>
#include "../core/Document.h"
#include "../core/History.h"

namespace pdn {

class EffectDialog : public QDialog {
    Q_OBJECT
public:
    EffectDialog(Document* doc, const QString& effectName, QWidget* parent = nullptr);
    ~EffectDialog() override = default;

    struct SliderControls {
        QSlider* slider = nullptr;
        QSpinBox* spin = nullptr;
        int defaultValue = 0;
    };

    SliderControls addSlider(const QString& labelText, int minVal, int maxVal, int defaultVal,
                             std::function<void(int)> onChanged = nullptr);

    QCheckBox* addCheckBox(const QString& labelText, bool defaultChecked,
                           std::function<void(bool)> onChanged = nullptr);

    void addCustomWidget(QWidget* widget);

    void setupButtons();

protected:
    virtual void processPreview(QImage& image) = 0;
    virtual void onReset();

    void updatePreview();

    void reject() override;
    void accept() override;

    Document* m_doc;
    int m_layerIndex;
    QImage m_originalImage;
    QString m_effectName;

    QVBoxLayout* m_mainLayout;
    std::vector<SliderControls> m_sliders;
    std::vector<std::pair<QCheckBox*, bool>> m_checkBoxes;
    bool m_buttonsAdded = false;
};

// Helper to run an effect without showing a dialog
bool applyInstantEffect(Document* doc, const QString& name,
                        const std::function<void(QImage&, const Selection&)>& processFunc);

} // namespace pdn
