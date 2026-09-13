#pragma once

#include <QMainWindow>
#include <QList>
#include <memory>
#include "../core/Document.h"
#include "../tools/ToolManager.h"
#include "../effects/PluginManager.h"
#include "CanvasView.h"
#include "DocumentStrip.h"
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

    // Multi-document management
    int addDocument(std::shared_ptr<Document> doc, bool makeActive = true);
    void setActiveDocumentIndex(int index);
    int activeDocumentIndex() const { return m_activeDocIndex; }
    std::shared_ptr<Document> activeDocument() const;
    std::shared_ptr<Document> document() const { return activeDocument(); }
    int documentCount() const { return m_documents.size(); }
    std::shared_ptr<Document> documentAt(int index) const;
    int indexOfDocument(const Document* doc) const;

    bool closeDocument(int index);
    bool closeActiveDocument();
    bool closeAllDocuments();

    bool saveDocument(int index);
    bool saveDocumentAs(int index);
    bool saveAllDocuments();

    ToolManager* toolManager() const { return m_toolMgr; }
    CanvasView* canvasView() const { return m_canvasView; }
    DocumentStrip* documentStrip() const { return m_documentStrip; }

    static QPoint lastCopiedPos() { return s_lastCopiedPos; }
    static QSize lastCopiedSize() { return s_lastCopiedSize; }
    static bool hasLastCopied() { return s_hasLastCopied; }
    static void clearLastCopied() {
        s_lastCopiedPos = QPoint(0, 0);
        s_lastCopiedSize = QSize(0, 0);
        s_hasLastCopied = false;
    }

public slots:
    void nextDocument();
    void previousDocument();

private slots:
    // File
    void onNew();
    void onOpen();
    bool onSave();
    bool onSaveAs();
    bool onSaveAll();
    void onClose();
    void onCloseAll();

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
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupMenus();
    void setupToolbars();
    void setupDocks();
    void setupStatusBar();
    void updateTitle();
    void updateWindowMenu();
    bool maybeSaveDocument(int index);
    void closeDocumentWithoutPrompt(int index);
    QString generateUntitledTitle();

    QList<std::shared_ptr<Document>> m_documents;
    int m_activeDocIndex = -1;
    std::shared_ptr<Document> m_doc; // Cached pointer to active document

    ToolManager* m_toolMgr;
    PluginManager* m_pluginMgr;

    DocumentStrip* m_documentStrip;
    CanvasView* m_canvasView;
    ToolOptionsBar* m_toolOptionsBar;
    ToolsDock* m_toolsDock;
    HistoryDock* m_historyDock;
    LayersDock* m_layersDock;
    ColorsDock* m_colorsDock;
    StatusWidget* m_statusWidget;

    QMenu* m_windowMenu = nullptr;
    QList<QAction*> m_windowDocActions;

    static QPoint s_lastCopiedPos;
    static QSize s_lastCopiedSize;
    static bool s_hasLastCopied;
};

} // namespace pdn
