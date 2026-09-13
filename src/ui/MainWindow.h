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

    ToolManager* toolManager() const { return m_toolMgr; }
    std::shared_ptr<Document> document() const { return m_doc; }

    static QPoint lastCopiedPos() { return s_lastCopiedPos; }
    static QSize lastCopiedSize() { return s_lastCopiedSize; }
    static bool hasLastCopied() { return s_hasLastCopied; }

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

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

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

    static QPoint s_lastCopiedPos;
    static QSize s_lastCopiedSize;
    static bool s_hasLastCopied;
};

} // namespace pdn
