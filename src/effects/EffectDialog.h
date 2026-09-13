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
#include "ICanvasInteraction.h"

namespace pdn {

class EffectDialog : public QDialog, public ICanvasPointReceiver, public ICanvasOverlayProvider {
    Q_OBJECT
public:
    EffectDialog(Document* doc, const QString& effectName, QWidget* parent = nullptr);
    ~EffectDialog() override;

    int exec() override;

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

    // Canvas coordinate picking and interaction
    void enablePointPicking(bool enable = true);
    bool isPointPickingEnabled() const { return m_pointPickingEnabled; }

    void setCanvasBridge(ICanvasInteractionBridge* bridge) { m_bridge = bridge; }
    ICanvasInteractionBridge* canvasBridge() const { return m_bridge; }

    static void setGlobalCanvasBridge(ICanvasInteractionBridge* bridge) { s_globalBridge = bridge; }
    static ICanvasInteractionBridge* globalCanvasBridge() { return s_globalBridge; }

    // ICanvasPointReceiver
    void onCanvasPointPicked(const QPointF& /*docPos*/) override {}

    // ICanvasOverlayProvider
    void drawCanvasOverlay(QPainter& /*painter*/, const RenderOptions& /*opts*/) override {}

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

    ICanvasInteractionBridge* m_bridge = nullptr;
    bool m_pointPickingEnabled = false;

    static ICanvasInteractionBridge* s_globalBridge;
};

// Helper to run an effect without showing a dialog
bool applyInstantEffect(Document* doc, const QString& name,
                        const std::function<void(QImage&, const Selection&)>& processFunc);

} // namespace pdn
