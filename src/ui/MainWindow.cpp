#include "MainWindow.h"
#include "../io/ImageIO.h"
#include "Dialogs.h"
#include "../effects/BrightnessContrast.h"
#include "../effects/GaussianBlur.h"
#include "../core/History.h"
#include "../tools/TextTool.h"
#include "../tools/ShapeTools.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QApplication>

namespace pdn {

QPoint MainWindow::s_lastCopiedPos = QPoint(0, 0);
QSize MainWindow::s_lastCopiedSize = QSize(0, 0);
bool MainWindow::s_hasLastCopied = false;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_toolMgr(new ToolManager(this)),
      m_pluginMgr(new PluginManager(this)) {
    setWindowTitle("Paint.NET Clone");
    resize(1200, 800);

    qApp->installEventFilter(this);

    m_canvasView = new CanvasView(m_toolMgr, this);
    setCentralWidget(m_canvasView);

    setupMenus();
    setupToolbars();
    setupDocks();
    setupStatusBar();

    // Default document: 800 x 600
    newDocument(800, 600);
}

void MainWindow::newDocument(int width, int height) {
    m_doc = std::make_shared<Document>(width, height);
    m_toolMgr->setDocument(m_doc.get());
    m_canvasView->setDocument(m_doc);
    m_historyDock->setDocument(m_doc);
    m_layersDock->setDocument(m_doc);

    connect(m_doc.get(), &Document::documentChanged, this, &MainWindow::updateTitle);
    updateTitle();
    m_statusWidget->setDocumentSize(width, height);
}

bool MainWindow::openFile(const QString& filePath) {
    QString err;
    auto doc = ImageIO::openDocument(filePath, &err);
    if (!doc) {
        QMessageBox::critical(this, "Error Opening File", err);
        return false;
    }

    m_doc = doc;
    m_toolMgr->setDocument(m_doc.get());
    m_canvasView->setDocument(m_doc);
    m_historyDock->setDocument(m_doc);
    m_layersDock->setDocument(m_doc);

    connect(m_doc.get(), &Document::documentChanged, this, &MainWindow::updateTitle);
    updateTitle();
    m_statusWidget->setDocumentSize(m_doc->width(), m_doc->height());
    return true;
}

void MainWindow::updateTitle() {
    QString title = "Paint.NET Clone - ";
    if (m_doc) {
        title += m_doc->fileName();
        if (m_doc->isModified()) {
            title += "*";
        }
    } else {
        title += "Untitled";
    }
    setWindowTitle(title);
}

void MainWindow::setupMenus() {
    QMenuBar* mb = menuBar();

    // --- File Menu ---
    QMenu* fileMenu = mb->addMenu("&File");
    fileMenu->addAction("&New...", this, &MainWindow::onNew, QKeySequence::New);
    fileMenu->addAction("&Open...", this, &MainWindow::onOpen, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("&Save", this, &MainWindow::onSave, QKeySequence::Save);
    fileMenu->addAction("Save &As...", this, &MainWindow::onSaveAs, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction("&Close", this, &MainWindow::onClose, QKeySequence::Close);
    fileMenu->addAction("E&xit", this, &QWidget::close, QKeySequence::Quit);

    // --- Edit Menu ---
    QMenu* editMenu = mb->addMenu("&Edit");
    QAction* undoAct = editMenu->addAction("&Undo", this, [this]() {
        if (m_doc) {
            if (m_doc->hasFloatingSelection()) {
                m_doc->cancelFloatingSelection();
                return;
            }
            m_doc->undoStack()->undo();
        }
    }, QKeySequence::Undo);
    QAction* redoAct = editMenu->addAction("&Redo", this, [this]() { if (m_doc) m_doc->undoStack()->redo(); });
    redoAct->setShortcuts({QKeySequence("Ctrl+Y"), QKeySequence("Ctrl+Shift+Z")});
    editMenu->addSeparator();
    editMenu->addAction("Cu&t", this, &MainWindow::onCut, QKeySequence::Cut);
    editMenu->addAction("&Copy", this, &MainWindow::onCopy, QKeySequence::Copy);
    editMenu->addAction("&Paste", this, &MainWindow::onPaste, QKeySequence::Paste);
    editMenu->addAction("Paste into &New Layer", this, &MainWindow::onPasteIntoNewLayer, QKeySequence("Ctrl+Shift+V"));
    editMenu->addSeparator();
    editMenu->addAction("&Erase Selection", this, &MainWindow::onEraseSelection, QKeySequence::Delete);
    editMenu->addAction("&Fill Selection", this, &MainWindow::onFillSelection, QKeySequence(Qt::Key_Backspace));
    editMenu->addAction("&Invert Selection", this, &MainWindow::onInvertSelection, QKeySequence("Ctrl+I"));
    editMenu->addAction("Select &All", this, &MainWindow::onSelectAll, QKeySequence::SelectAll);
    QAction* deselectAct = editMenu->addAction("&Deselect", this, &MainWindow::onDeselect);
    deselectAct->setShortcuts({QKeySequence("Ctrl+D"), QKeySequence("Ctrl+Shift+A")});

    // --- View Menu ---
    QMenu* viewMenu = mb->addMenu("&View");
    QAction* zoomInAct = viewMenu->addAction("Zoom &In", m_canvasView, &CanvasView::zoomIn);
    zoomInAct->setShortcuts({QKeySequence::ZoomIn, QKeySequence("Ctrl+=")});
    QAction* zoomOutAct = viewMenu->addAction("Zoom &Out", m_canvasView, &CanvasView::zoomOut);
    zoomOutAct->setShortcuts({QKeySequence::ZoomOut, QKeySequence("Ctrl+-")});
    QAction* actualSizeAct = viewMenu->addAction("&Actual Size", m_canvasView, &CanvasView::zoomActualSize);
    actualSizeAct->setShortcuts({QKeySequence("Ctrl+0"), QKeySequence("Ctrl+Alt+0")});
    viewMenu->addAction("Zoom to &Window", m_canvasView, &CanvasView::zoomToWindow, QKeySequence("Ctrl+B"));
    viewMenu->addSeparator();
    QAction* gridAct = viewMenu->addAction("&Grid", m_canvasView, &CanvasView::togglePixelGrid);
    gridAct->setCheckable(true);
    QAction* rulersAct = viewMenu->addAction("&Rulers", m_canvasView, &CanvasView::toggleRulers);
    rulersAct->setCheckable(true);
    rulersAct->setChecked(true);

    // --- Image Menu ---
    QMenu* imageMenu = mb->addMenu("&Image");
    imageMenu->addAction("&Crop to Selection", this, &MainWindow::onCropToSelection, QKeySequence("Ctrl+Shift+X"));
    imageMenu->addAction("&Resize...", this, &MainWindow::onResizeImage, QKeySequence("Ctrl+R"));
    imageMenu->addAction("&Canvas Size...", this, &MainWindow::onResizeCanvas, QKeySequence("Ctrl+Shift+R"));
    imageMenu->addSeparator();
    imageMenu->addAction("Flip &Horizontal", this, &MainWindow::onFlipHorizontal);
    imageMenu->addAction("Flip &Vertical", this, &MainWindow::onFlipVertical);
    imageMenu->addAction("Rotate 90° &Clockwise", this, &MainWindow::onRotate90CW, QKeySequence("Ctrl+H"));
    imageMenu->addAction("Rotate 90° &Counter-Clockwise", this, &MainWindow::onRotate90CCW, QKeySequence("Ctrl+G"));
    imageMenu->addAction("Rotate &180°", this, &MainWindow::onRotate180, QKeySequence("Ctrl+J"));
    imageMenu->addSeparator();
    imageMenu->addAction("&Flatten", this, &MainWindow::onFlatten, QKeySequence("Ctrl+Shift+F"));

    // --- Layers Menu ---
    QMenu* layersMenu = mb->addMenu("&Layers");
    layersMenu->addAction("&Add New Layer", this, &MainWindow::onAddLayer, QKeySequence("Ctrl+Shift+N"));
    QAction* delLayerAct = layersMenu->addAction("&Delete Layer", this, &MainWindow::onDeleteLayer);
    delLayerAct->setShortcut(QKeySequence("Ctrl+Shift+Delete"));
    layersMenu->addAction("&Duplicate Layer", this, &MainWindow::onDuplicateLayer, QKeySequence("Ctrl+Shift+D"));
    layersMenu->addAction("&Merge Layer Down", this, &MainWindow::onMergeDown, QKeySequence("Ctrl+M"));
    layersMenu->addSeparator();
    layersMenu->addAction("Layer &Properties...", this, &MainWindow::onLayerProperties, QKeySequence("F4"));

    // --- Adjustments Menu ---
    QMenu* adjMenu = mb->addMenu("&Adjustments");
    adjMenu->addAction("&Brightness / Contrast...", this, &MainWindow::onBrightnessContrast, QKeySequence("Ctrl+Shift+T"));

    // --- Effects Menu ---
    QMenu* fxMenu = mb->addMenu("&Effects");
    QMenu* blursMenu = fxMenu->addMenu("&Blurs");
    blursMenu->addAction("&Gaussian Blur...", this, &MainWindow::onGaussianBlur);

    // --- Window Menu ---
    QMenu* windowMenu = mb->addMenu("&Window");
    windowMenu->addAction("&Reset Window Locations", this, &MainWindow::onResetWindowLocations);
    windowMenu->addSeparator();

    // --- Help Menu ---
    QMenu* helpMenu = mb->addMenu("&Help");
    helpMenu->addAction("&About Paint.NET Clone...", this, &MainWindow::onAbout);
}

void MainWindow::setupToolbars() {
    m_toolOptionsBar = new ToolOptionsBar(m_toolMgr, this);
    addToolBar(Qt::TopToolBarArea, m_toolOptionsBar);
}

void MainWindow::setupDocks() {
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks);

    // 1. Tools Dock (F5)
    m_toolsDock = new ToolsDock(m_toolMgr, this);
    addDockWidget(Qt::LeftDockWidgetArea, m_toolsDock);

    // 2. History Dock (F6)
    m_historyDock = new HistoryDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_historyDock);

    // 3. Layers Dock (F7)
    m_layersDock = new LayersDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_layersDock);

    // 4. Colors Dock (F8)
    m_colorsDock = new ColorsDock(m_toolMgr, this);
    addDockWidget(Qt::LeftDockWidgetArea, m_colorsDock);

    // Add toggles to Window menu
    QMenu* winMenu = findChild<QMenu*>("Window");
    if (!winMenu) {
        for (auto m : menuBar()->findChildren<QMenu*>()) {
            if (m->title().contains("Window")) {
                winMenu = m;
                break;
            }
        }
    }
    if (winMenu) {
        QAction* tAct = winMenu->addAction("&Tools", m_toolsDock, &QDockWidget::setVisible);
        tAct->setCheckable(true); tAct->setShortcut(QKeySequence("F5")); tAct->setChecked(true);
        connect(m_toolsDock, &QDockWidget::visibilityChanged, tAct, &QAction::setChecked);

        QAction* hAct = winMenu->addAction("&History", m_historyDock, &QDockWidget::setVisible);
        hAct->setCheckable(true); hAct->setShortcut(QKeySequence("F6")); hAct->setChecked(true);
        connect(m_historyDock, &QDockWidget::visibilityChanged, hAct, &QAction::setChecked);

        QAction* lAct = winMenu->addAction("&Layers", m_layersDock, &QDockWidget::setVisible);
        lAct->setCheckable(true); lAct->setShortcut(QKeySequence("F7")); lAct->setChecked(true);
        connect(m_layersDock, &QDockWidget::visibilityChanged, lAct, &QAction::setChecked);

        QAction* cAct = winMenu->addAction("&Colors", m_colorsDock, &QDockWidget::setVisible);
        cAct->setCheckable(true); cAct->setShortcut(QKeySequence("F8")); cAct->setChecked(true);
        connect(m_colorsDock, &QDockWidget::visibilityChanged, cAct, &QAction::setChecked);
    }

    for (QAction* act : findChildren<QAction*>()) {
        act->setShortcutContext(Qt::ApplicationShortcut);
    }
}

void MainWindow::setupStatusBar() {
    m_statusWidget = new StatusWidget(this);
    statusBar()->addPermanentWidget(m_statusWidget, 1);

    connect(m_canvasView, &CanvasView::cursorMoved, m_statusWidget, &StatusWidget::setCursorPos);
    connect(m_canvasView, &CanvasView::zoomChanged, m_statusWidget, &StatusWidget::setZoom);

    if (m_canvasView->renderer()) {
        m_statusWidget->setRendererInfo(m_canvasView->renderer()->rendererName());
    }
}

// --- Slot Implementations ---

void MainWindow::onNew() {
    NewImageDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        newDocument(dlg.imageWidth(), dlg.imageHeight());
    }
}

void MainWindow::onOpen() {
    QString file = QFileDialog::getOpenFileName(this, "Open Image", QString(), ImageIO::openFileFilter());
    if (!file.isEmpty()) {
        openFile(file);
    }
}

bool MainWindow::onSave() {
    if (!m_doc) return false;
    if (m_doc->filePath().isEmpty()) {
        return onSaveAs();
    }
    QString err;
    if (!ImageIO::saveDocument(*m_doc, m_doc->filePath(), &err)) {
        QMessageBox::critical(this, "Error Saving File", err);
        return false;
    }
    m_doc->undoStack()->setClean();
    updateTitle();
    return true;
}

bool MainWindow::onSaveAs() {
    if (!m_doc) return false;
    QString file = QFileDialog::getSaveFileName(this, "Save Image As",
                                               m_doc->filePath().isEmpty() ? "Untitled.pdn" : m_doc->filePath(),
                                               ImageIO::saveFileFilter());
    if (file.isEmpty()) return false;

    QString err;
    if (!ImageIO::saveDocument(*m_doc, file, &err)) {
        QMessageBox::critical(this, "Error Saving File", err);
        return false;
    }
    m_doc->setFilePath(file);
    m_doc->undoStack()->setClean();
    updateTitle();
    return true;
}

void MainWindow::onClose() {
    newDocument(800, 600);
}

void MainWindow::onCut() {
    if (!m_doc) return;

    if (m_doc->hasFloatingSelection()) {
        onCopy();
        m_doc->discardFloatingSelection();
        m_doc->clearSelection();
        return;
    }

    onCopy();
    onEraseSelection();
    // Requirement 1: When I cut something, the selection should disappear.
    m_doc->clearSelection();
}

void MainWindow::onCopy() {
    if (!m_doc) return;

    if (m_doc->hasFloatingSelection()) {
        QImage cropped = m_doc->floatingImage();
        if (cropped.isNull()) return;
        s_lastCopiedPos = m_doc->floatingOffset().toPoint();
        s_lastCopiedSize = cropped.size();
        s_hasLastCopied = true;

        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setImage(cropped);
        return;
    }

    auto layer = m_doc->activeLayer();
    if (!layer) return;

    QRectF bounds = m_doc->selection().boundingRect();
    if (bounds.isEmpty()) {
        bounds = QRectF(0, 0, m_doc->width(), m_doc->height());
    }
    QRect srcRect = bounds.toAlignedRect().intersected(QRect(0, 0, m_doc->width(), m_doc->height()));
    if (srcRect.isEmpty()) return;

    QImage cropped = layer->image().copy(srcRect);

    if (!m_doc->selection().isEmpty()) {
        QPainter p(&cropped);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.translate(-srcRect.x(), -srcRect.y());
        p.fillPath(m_doc->selection().path(), Qt::black);
        p.end();
    }

    s_lastCopiedPos = srcRect.topLeft();
    s_lastCopiedSize = cropped.size();
    s_hasLastCopied = true;

    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setImage(cropped);
}

void MainWindow::onPaste() {
    const QClipboard* clipboard = QGuiApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    if (!mimeData || !mimeData->hasImage() || !m_doc) return;

    QImage img = qvariant_cast<QImage>(clipboard->image());
    if (img.isNull()) return;

    // Requirement 4: When something is cut or copied, the position and size should be saved.
    // When the next paste contains an image, with the same size as the previous cut/copy,
    // then it should be pasted at that exact position.
    QPoint pastePos(0, 0);
    if (s_hasLastCopied && img.size() == s_lastCopiedSize) {
        pastePos = s_lastCopiedPos;
    }

    // Switch tool to MoveSelectedPixels first
    if (m_toolMgr) {
        m_toolMgr->setActiveTool(ToolType::MoveSelectedPixels);
    }

    // Bake any existing floating selection before starting new paste
    if (m_doc->hasFloatingSelection()) {
        m_doc->bakeFloatingSelection();
    }

    // Requirement 3: Paste creates a hidden temporary layer (floating selection)
    // with isLifted = false so the layer underneath is NOT punched with a hole!
    m_doc->createFloatingSelection(img, pastePos, false, "Paste");

    // Requirement 2: When I paste something, it should have a selection around the pasted content.
    m_doc->selection().clear();
    m_doc->selection().addRect(QRectF(pastePos, img.size()), SelectionCombineMode::Replace);
    emit m_doc->selectionChanged();
    emit m_doc->documentChanged();
}

void MainWindow::onPasteIntoNewLayer() {
    const QClipboard* clipboard = QGuiApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    if (!mimeData || !mimeData->hasImage() || !m_doc) return;

    QImage img = qvariant_cast<QImage>(clipboard->image());
    if (img.isNull()) return;

    if (m_doc->hasFloatingSelection()) {
        m_doc->bakeFloatingSelection();
    }

    QPoint pastePos(0, 0);
    if (s_hasLastCopied && img.size() == s_lastCopiedSize) {
        pastePos = s_lastCopiedPos;
    }

    auto newLayer = m_doc->addLayer("Pasted Layer");
    if (newLayer) {
        QPainter p(&newLayer->image());
        p.drawImage(pastePos, img);
        p.end();

        m_doc->selection().clear();
        m_doc->selection().addRect(QRectF(pastePos, img.size()), SelectionCombineMode::Replace);
        emit m_doc->selectionChanged();
        emit m_doc->documentChanged();
    }
}

void MainWindow::onEraseSelection() {
    if (!m_doc) return;

    if (m_doc->hasFloatingSelection()) {
        m_doc->discardFloatingSelection();
        m_doc->clearSelection();
        return;
    }

    auto layer = m_doc->activeLayer();
    if (!layer) return;

    QImage oldImg = layer->image().copy();
    QPainter p(&layer->image());
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    if (!m_doc->selection().isEmpty()) {
        p.fillPath(m_doc->selection().path(), Qt::transparent);
    } else {
        layer->clear();
    }
    p.end();

    m_doc->undoStack()->push(new LayerBitmapUndoCommand(m_doc.get(), m_doc->activeLayerIndex(), oldImg, "Erase Selection"));
    emit m_doc->documentChanged();
}

void MainWindow::onFillSelection() {
    if (!m_doc) return;
    auto layer = m_doc->activeLayer();
    if (!layer) return;

    QImage oldImg = layer->image().copy();
    QPainter p(&layer->image());
    if (!m_doc->selection().isEmpty()) {
        p.fillPath(m_doc->selection().path(), m_toolMgr->context().primaryColor);
    } else {
        p.fillRect(QRect(0, 0, m_doc->width(), m_doc->height()), m_toolMgr->context().primaryColor);
    }
    p.end();

    m_doc->undoStack()->push(new LayerBitmapUndoCommand(m_doc.get(), m_doc->activeLayerIndex(), oldImg, "Fill Selection"));
    emit m_doc->documentChanged();
}

void MainWindow::onInvertSelection() {
    if (m_doc) {
        m_doc->selection().invert(m_doc->width(), m_doc->height());
        emit m_doc->selectionChanged();
    }
}

void MainWindow::onSelectAll() {
    if (m_doc) {
        m_doc->selection().selectAll(m_doc->width(), m_doc->height());
        emit m_doc->selectionChanged();
    }
}

void MainWindow::onDeselect() {
    if (m_doc) {
        m_doc->clearSelection();
    }
}

void MainWindow::onCropToSelection() {
    if (!m_doc || m_doc->selection().isEmpty()) return;
    QRect cropRect = m_doc->selection().boundingRect().toAlignedRect();
    m_doc->crop(cropRect);
    m_statusWidget->setDocumentSize(m_doc->width(), m_doc->height());
}

void MainWindow::onResizeImage() {
    if (!m_doc) return;
    ResizeImageDialog dlg(m_doc->width(), m_doc->height(), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_doc->resizeImage(dlg.newWidth(), dlg.newHeight());
        m_statusWidget->setDocumentSize(m_doc->width(), m_doc->height());
    }
}

void MainWindow::onResizeCanvas() {
    if (!m_doc) return;
    CanvasSizeDialog dlg(m_doc->width(), m_doc->height(), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_doc->resizeCanvas(dlg.newWidth(), dlg.newHeight(), dlg.anchor());
        m_statusWidget->setDocumentSize(m_doc->width(), m_doc->height());
    }
}

void MainWindow::onFlipHorizontal() { if (m_doc) m_doc->flipHorizontal(); }
void MainWindow::onFlipVertical() { if (m_doc) m_doc->flipVertical(); }
void MainWindow::onRotate90CW() { if (m_doc) { m_doc->rotate90CW(); m_statusWidget->setDocumentSize(m_doc->width(), m_doc->height()); } }
void MainWindow::onRotate90CCW() { if (m_doc) { m_doc->rotate90CCW(); m_statusWidget->setDocumentSize(m_doc->width(), m_doc->height()); } }
void MainWindow::onRotate180() { if (m_doc) m_doc->rotate180(); }
void MainWindow::onFlatten() { if (m_doc) m_doc->flatten(); }

void MainWindow::onAddLayer() { if (m_doc) m_doc->addLayer(); }
void MainWindow::onDeleteLayer() { if (m_doc) m_doc->removeLayer(m_doc->activeLayerIndex()); }
void MainWindow::onDuplicateLayer() { if (m_doc) m_doc->duplicateLayer(m_doc->activeLayerIndex()); }
void MainWindow::onMergeDown() { if (m_doc) m_doc->mergeLayerDown(m_doc->activeLayerIndex()); }
void MainWindow::onLayerProperties() {
    if (m_doc) {
        LayerPropertiesDialog dlg(m_doc.get(), m_doc->activeLayerIndex(), this);
        dlg.exec();
    }
}

void MainWindow::onBrightnessContrast() {
    if (!m_doc) return;
    BrightnessContrastEffect fx;
    fx.showDialog(this, m_doc.get());
}

void MainWindow::onGaussianBlur() {
    if (!m_doc) return;
    GaussianBlurEffect fx;
    fx.showDialog(this, m_doc.get());
}

void MainWindow::onResetWindowLocations() {
    m_toolsDock->setVisible(true);
    m_historyDock->setVisible(true);
    m_layersDock->setVisible(true);
    m_colorsDock->setVisible(true);
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About Paint.NET Clone",
        "<h3>Paint.NET Clone</h3>"
        "<p>Cross-platform Paint.NET clone written in C++ with Qt 6 and native Vulkan GPU rendering.</p>"
        "<p>Features:"
        "<ul>"
        "<li>PDN3 format reader/writer (100% Paint.NET compatible)</li>"
        "<li>PNG, JPG, BMP, GIF, WebP support</li>"
        "<li>19 Paint.NET tools with options toolbar</li>"
        "<li>Paint.NET Color Picker with HSV Wheel & Swatches</li>"
        "<li>Multi-layer editing with all 14 blend modes</li>"
        "<li>Undo / Redo History stack</li>"
        "<li>Adjustments: Brightness / Contrast</li>"
        "<li>Effects: Gaussian Blur</li>"
        "<li>Vulkan GPU hardware rendering</li>"
        "<li>Plugin-ready extensible architecture</li>"
        "</ul>"
        "</p>");
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        // 1. If focus is inside a text input widget or line edit, allow standard typing
        QWidget* fw = QApplication::focusWidget();
        auto isTextInput = [](QObject* obj) -> bool {
            return obj && (qobject_cast<QLineEdit*>(obj) ||
                           qobject_cast<QTextEdit*>(obj) ||
                           qobject_cast<QPlainTextEdit*>(obj) ||
                           qobject_cast<QSpinBox*>(obj) ||
                           qobject_cast<QDoubleSpinBox*>(obj));
        };

        if (isTextInput(fw) || isTextInput(watched)) {
            return QMainWindow::eventFilter(watched, event);
        }

        // 2. If TextTool is active on canvas and currently typing text, let it handle the typing
        if (m_toolMgr && m_toolMgr->activeToolType() == ToolType::Text) {
            auto textTool = std::dynamic_pointer_cast<TextTool>(m_toolMgr->activeTool());
            if (textTool && textTool->isEditing()) {
                return QMainWindow::eventFilter(watched, event);
            }
        }

        // 3. If LineCurveTool is adjusting and user presses Enter or Escape, let LineCurveTool handle it
        if (m_toolMgr && m_toolMgr->activeToolType() == ToolType::LineCurve) {
            auto lineTool = std::dynamic_pointer_cast<LineCurveTool>(m_toolMgr->activeTool());
            if (lineTool && lineTool->isEditing()) {
                if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Escape) {
                    return QMainWindow::eventFilter(watched, event);
                }
            }
        }

        // 4. Try ToolManager global shortcuts (tool selection, cycling, X, D, [, ], arrows, Esc)
        if (m_toolMgr && m_toolMgr->handleKeyPress(keyEvent)) {
            if (m_canvasView) {
                m_canvasView->update();
            }
            return true; // Event consumed globally!
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    QWidget* fw = focusWidget();
    if (fw && (qobject_cast<QLineEdit*>(fw) || qobject_cast<QTextEdit*>(fw) || qobject_cast<QSpinBox*>(fw))) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    if (m_toolMgr && m_toolMgr->handleKeyPress(event)) {
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

} // namespace pdn
