#pragma once

#include <QMainWindow>
#include <memory>
#include "../core/Document.h"
#include "../tools/ToolManager.h"
#include "../effects/PluginManager.h"
#include "CanvasView.h"
#include "ToolOptionsBar.h"
#include "ToolsDock.h"
#include "HistoryDock.h"
#include "LayersDock.h"
#include "ColorsDock.h"
#include "StatusWidget.h"

namespace pdn {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    bool openFile(const QString& filePath);
    void newDocument(int width, int height);

private slots:
    // File
    void onNew();
    void onOpen();
    bool onSave();
    bool onSaveAs();
    void onClose();

    // Edit
    void onCut();
    void onCopy();
    void onPaste();
    void onPasteIntoNewLayer();
    void onEraseSelection();
    void onFillSelection();
    void onInvertSelection();
    void onSelectAll();
    void onDeselect();

    // Image
    void onCropToSelection();
    void onResizeImage();
    void onResizeCanvas();
    void onFlipHorizontal();
    void onFlipVertical();
    void onRotate90CW();
    void onRotate90CCW();
    void onRotate180();
    void onFlatten();

    // Layers
    void onAddLayer();
    void onDeleteLayer();
    void onDuplicateLayer();
    void onMergeDown();
    void onLayerProperties();

    // Adjustments & Effects
    void onBrightnessContrast();
    void onGaussianBlur();

    // Window
    void onResetWindowLocations();

    // Help
    void onAbout();

private:
    void setupMenus();
    void setupToolbars();
    void setupDocks();
    void setupStatusBar();
    void updateTitle();

    std::shared_ptr<Document> m_doc;
    ToolManager* m_toolMgr;
    PluginManager* m_pluginMgr;

    CanvasView* m_canvasView;
    ToolOptionsBar* m_toolOptionsBar;
    ToolsDock* m_toolsDock;
    HistoryDock* m_historyDock;
    LayersDock* m_layersDock;
    ColorsDock* m_colorsDock;
    StatusWidget* m_statusWidget;
};

} // namespace pdn
