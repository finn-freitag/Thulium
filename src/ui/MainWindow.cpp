#include "MainWindow.h"
#include "../io/ImageIO.h"
#include "Dialogs.h"
#include "../effects/BrightnessContrast.h"
#include "../effects/GaussianBlur.h"
#include "../core/History.h"
#include "../tools/TextTool.h"
#include "../tools/ShapeTools.h"
#include "../tools/MoveTools.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
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
#include <QVBoxLayout>
#include <QFileInfo>
#include <QUrl>
#include <algorithm>

static void initResources() {
    static bool inited = false;
    if (!inited) {
        Q_INIT_RESOURCE(resources);
        inited = true;
    }
}

namespace pdn {

QPoint MainWindow::s_lastCopiedPos = QPoint(0, 0);
QSize MainWindow::s_lastCopiedSize = QSize(0, 0);
bool MainWindow::s_hasLastCopied = false;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_toolMgr(new ToolManager(this)),
      m_pluginMgr(new PluginManager(this)) {
    initResources();
    setWindowTitle("Paint.NET Clone");
    resize(1200, 800);
    setAcceptDrops(true);

    qApp->installEventFilter(this);

    m_documentStrip = new DocumentStrip(this);
    m_canvasView = new CanvasView(m_toolMgr, this);

    QWidget* centralContainer = new QWidget(this);
    QVBoxLayout* centralLayout = new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_documentStrip);
    centralLayout->addWidget(m_canvasView, 1);
    setCentralWidget(centralContainer);

    connect(m_documentStrip, &DocumentStrip::documentSelected, this, &MainWindow::setActiveDocumentIndex);
    connect(m_documentStrip, &DocumentStrip::closeRequested, this, &MainWindow::closeDocument);
    connect(m_documentStrip, &DocumentStrip::newDocumentRequested, this, &MainWindow::onNew);
    connect(m_documentStrip, &DocumentStrip::saveRequested, this, [this](int idx) { saveDocument(idx); });
    connect(m_documentStrip, &DocumentStrip::saveAsRequested, this, [this](int idx) { saveDocumentAs(idx); });
    connect(m_documentStrip, &DocumentStrip::closeAllRequested, this, &MainWindow::onCloseAll);
    connect(m_documentStrip, &DocumentStrip::closeOthersRequested, this, [this](int keepIdx) {
        if (keepIdx < 0 || keepIdx >= m_documents.size()) return;
        auto keepDoc = m_documents[keepIdx];
        for (int i = m_documents.size() - 1; i >= 0; --i) {
            if (m_documents[i] != keepDoc) {
                if (!closeDocument(i)) {
                    break;
                }
            }
        }
    });

    setupMenus();
    setupToolbars();
    setupDocks();
    setupStatusBar();

    // Default document: 800 x 600
    newDocument(800, 600);
}

QString MainWindow::generateUntitledTitle() {
    if (m_documents.isEmpty()) {
        return "Untitled";
    }
    bool hasUntitled1 = false;
    for (const auto& d : m_documents) {
        if (d->fileName() == "Untitled") {
            hasUntitled1 = true;
            break;
        }
    }
    if (!hasUntitled1) {
        return "Untitled";
    }
    for (int n = 2; ; ++n) {
        QString candidate = QString("Untitled %1").arg(n);
        bool used = false;
        for (const auto& d : m_documents) {
            if (d->fileName() == candidate) {
                used = true;
                break;
            }
        }
        if (!used) return candidate;
    }
}

int MainWindow::addDocument(std::shared_ptr<Document> doc, bool makeActive) {
    if (!doc) return -1;

    int newIdx = m_documents.size();
    m_documents.append(doc);
    m_documentStrip->addDocument(doc);

    connect(doc.get(), &Document::documentChanged, this, [this, doc]() {
        if (m_doc == doc) {
            updateTitle();
        }
        int idx = indexOfDocument(doc.get());
        if (idx >= 0) {
            m_documentStrip->updateDocument(idx);
            updateWindowMenu();
        }
    });

    if (doc->undoStack()) {
        connect(doc->undoStack(), &QUndoStack::cleanChanged, this, [this, doc](bool) {
            if (m_doc == doc) {
                updateTitle();
            }
            int idx = indexOfDocument(doc.get());
            if (idx >= 0) {
                m_documentStrip->updateDocument(idx);
                updateWindowMenu();
            }
        });
    }

    if (makeActive || m_activeDocIndex < 0) {
        setActiveDocumentIndex(newIdx);
    } else {
        updateWindowMenu();
    }

    return newIdx;
}

void MainWindow::setActiveDocumentIndex(int index) {
    if (index < 0 || index >= m_documents.size()) return;
    if (index == m_activeDocIndex && m_doc == m_documents[index]) return;

    m_activeDocIndex = index;
    m_doc = m_documents[m_activeDocIndex];

    m_toolMgr->setDocument(m_doc.get());
    m_canvasView->setDocument(m_doc);
    m_historyDock->setDocument(m_doc);
    m_layersDock->setDocument(m_doc);

    m_documentStrip->setCurrentIndex(m_activeDocIndex);
    updateTitle();
    updateWindowMenu();
}

std::shared_ptr<Document> MainWindow::activeDocument() const {
    if (m_activeDocIndex >= 0 && m_activeDocIndex < m_documents.size()) {
        return m_documents[m_activeDocIndex];
    }
    return nullptr;
}

std::shared_ptr<Document> MainWindow::documentAt(int index) const {
    if (index >= 0 && index < m_documents.size()) {
        return m_documents[index];
    }
    return nullptr;
}

int MainWindow::indexOfDocument(const Document* doc) const {
    for (int i = 0; i < m_documents.size(); ++i) {
        if (m_documents[i].get() == doc) return i;
    }
    return -1;
}

void MainWindow::closeDocumentWithoutPrompt(int index) {
    if (index < 0 || index >= m_documents.size()) return;
    auto doc = m_documents[index];

    disconnect(doc.get(), nullptr, this, nullptr);
    m_canvasView->removeDocumentViewState(doc.get());

    m_documents.removeAt(index);
    m_documentStrip->removeDocument(index);

    if (m_documents.isEmpty()) {
        m_activeDocIndex = -1;
        m_doc = nullptr;
    } else {
        int newIdx = m_activeDocIndex;
        if (newIdx == index) {
            newIdx = std::clamp(index, 0, static_cast<int>(m_documents.size()) - 1);
            m_activeDocIndex = -1; // force switch
            setActiveDocumentIndex(newIdx);
        } else if (newIdx > index) {
            m_activeDocIndex--;
            m_documentStrip->setCurrentIndex(m_activeDocIndex);
            updateWindowMenu();
        }
    }
}

bool MainWindow::maybeSaveDocument(int index) {
    if (index < 0 || index >= m_documents.size()) return true;
    auto doc = m_documents[index];
    if (!doc->isModified()) return true;

    // Switch to the document so the user can inspect it
    setActiveDocumentIndex(index);

    QMessageBox box(this);
    box.setWindowTitle("Save Changes?");
    box.setText(QString("Save changes to \"%1\" before closing?").arg(doc->fileName()));
    box.setIcon(QMessageBox::Question);
    box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Save);

    int ret = box.exec();
    if (ret == QMessageBox::Save) {
        return saveDocument(index);
    } else if (ret == QMessageBox::Discard) {
        return true;
    } else { // Cancel
        return false;
    }
}

bool MainWindow::closeDocument(int index) {
    if (index < 0 || index >= m_documents.size()) return false;

    if (!maybeSaveDocument(index)) {
        return false;
    }

    closeDocumentWithoutPrompt(index);

    if (m_documents.isEmpty()) {
        newDocument(800, 600);
    }

    return true;
}

bool MainWindow::closeActiveDocument() {
    return closeDocument(m_activeDocIndex);
}

bool MainWindow::closeAllDocuments() {
    while (!m_documents.isEmpty()) {
        // If there is only 1 pristine default document remaining, we're done
        if (m_documents.size() == 1 && !m_documents[0]->isModified() && m_documents[0]->filePath().isEmpty()) {
            break;
        }

        int lastIdx = m_documents.size() - 1;
        if (!closeDocument(lastIdx)) {
            return false;
        }
    }
    return true;
}

void MainWindow::newDocument(int width, int height) {
    clearLastCopied();

    auto doc = std::make_shared<Document>(width, height);
    QString title = generateUntitledTitle();
    doc->setTitle(title);

    addDocument(doc, true);
}

bool MainWindow::openFile(const QString& filePath) {
    // Check if already open
    QFileInfo newFi(filePath);
    for (int i = 0; i < m_documents.size(); ++i) {
        if (!m_documents[i]->filePath().isEmpty()) {
            QFileInfo existingFi(m_documents[i]->filePath());
            if (existingFi.canonicalFilePath() == newFi.canonicalFilePath()) {
                setActiveDocumentIndex(i);
                return true;
            }
        }
    }

    QString err;
    auto doc = ImageIO::openDocument(filePath, &err);
    if (!doc) {
        QMessageBox::critical(this, "Error Opening File", err);
        return false;
    }

    clearLastCopied();

    // If currently there is only 1 untouched default document, replace it
    if (m_documents.size() == 1 &&
        m_documents[0]->filePath().isEmpty() &&
        !m_documents[0]->isModified() &&
        m_documents[0]->undoStack()->count() == 0) {
        closeDocumentWithoutPrompt(0);
    }

    addDocument(doc, true);
    return true;
}

void MainWindow::updateTitle() {
    QString title = "Paint.NET Clone - ";
    if (m_doc) {
        title += m_doc->fileName();
        if (m_doc->isModified()) {
            title += "*";
        }
        if (m_statusWidget) {
            m_statusWidget->setDocumentSize(m_doc->width(), m_doc->height());
        }
    } else {
        title += "Untitled";
    }
    setWindowTitle(title);
}

void MainWindow::updateWindowMenu() {
    if (!m_windowMenu) return;

    for (QAction* act : m_windowDocActions) {
        m_windowMenu->removeAction(act);
        delete act;
    }
    m_windowDocActions.clear();

    for (int i = 0; i < m_documents.size(); ++i) {
        auto doc = m_documents[i];
        QString text = QString("&%1 %2").arg(i + 1).arg(doc->fileName());
        if (doc->isModified()) {
            text += "*";
        }
        QAction* act = new QAction(text, m_windowMenu);
        act->setCheckable(true);
        act->setChecked(i == m_activeDocIndex);
        connect(act, &QAction::triggered, this, [this, i]() {
            setActiveDocumentIndex(i);
        });
        m_windowMenu->addAction(act);
        m_windowDocActions.append(act);
    }
}

void MainWindow::nextDocument() {
    if (m_documents.size() <= 1) return;
    int nextIdx = (m_activeDocIndex + 1) % m_documents.size();
    setActiveDocumentIndex(nextIdx);
}

void MainWindow::previousDocument() {
    if (m_documents.size() <= 1) return;
    int prevIdx = (m_activeDocIndex - 1 + m_documents.size()) % m_documents.size();
    setActiveDocumentIndex(prevIdx);
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
    fileMenu->addAction("Save Al&l", this, &MainWindow::onSaveAll, QKeySequence("Ctrl+Alt+S"));
    fileMenu->addSeparator();
    fileMenu->addAction("&Close", this, &MainWindow::onClose, QKeySequence::Close);
    fileMenu->addAction("Close &All", this, &MainWindow::onCloseAll, QKeySequence("Ctrl+Shift+W"));
    fileMenu->addSeparator();
    fileMenu->addAction("&Metadata...", this, &MainWindow::onMetadata);
    fileMenu->addSeparator();
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
    layersMenu->addAction(QIcon(":/icons/layer-add.svg"), "&Add New Layer", this, &MainWindow::onAddLayer, QKeySequence("Ctrl+Shift+N"));
    QAction* delLayerAct = layersMenu->addAction(QIcon(":/icons/layer-delete.svg"), "&Delete Layer", this, &MainWindow::onDeleteLayer);
    delLayerAct->setShortcut(QKeySequence("Ctrl+Shift+Delete"));
    layersMenu->addAction(QIcon(":/icons/layer-duplicate.svg"), "&Duplicate Layer", this, &MainWindow::onDuplicateLayer, QKeySequence("Ctrl+Shift+D"));
    layersMenu->addAction(QIcon(":/icons/layer-merge-down.svg"), "&Merge Layer Down", this, &MainWindow::onMergeDown, QKeySequence("Ctrl+M"));
    layersMenu->addSeparator();
    layersMenu->addAction(QIcon(":/icons/layer-properties.svg"), "Layer &Properties...", this, &MainWindow::onLayerProperties, QKeySequence("F4"));

    // --- Adjustments Menu ---
    QMenu* adjMenu = mb->addMenu("&Adjustments");
    adjMenu->addAction("&Brightness / Contrast...", this, &MainWindow::onBrightnessContrast, QKeySequence("Ctrl+Shift+T"));

    // --- Effects Menu ---
    QMenu* fxMenu = mb->addMenu("&Effects");
    QMenu* blursMenu = fxMenu->addMenu("&Blurs");
    blursMenu->addAction("&Gaussian Blur...", this, &MainWindow::onGaussianBlur);

    // --- Window Menu ---
    m_windowMenu = mb->addMenu("&Window");
    m_windowMenu->addAction("&Reset Window Locations", this, &MainWindow::onResetWindowLocations);
    m_windowMenu->addSeparator();

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

    // Add toggles and image items to Window menu
    if (m_windowMenu) {
        QAction* tAct = m_windowMenu->addAction("&Tools", m_toolsDock, &QDockWidget::setVisible);
        tAct->setCheckable(true); tAct->setShortcut(QKeySequence("F5")); tAct->setChecked(true);
        connect(m_toolsDock, &QDockWidget::visibilityChanged, tAct, &QAction::setChecked);

        QAction* hAct = m_windowMenu->addAction("&History", m_historyDock, &QDockWidget::setVisible);
        hAct->setCheckable(true); hAct->setShortcut(QKeySequence("F6")); hAct->setChecked(true);
        connect(m_historyDock, &QDockWidget::visibilityChanged, hAct, &QAction::setChecked);

        QAction* lAct = m_windowMenu->addAction("&Layers", m_layersDock, &QDockWidget::setVisible);
        lAct->setCheckable(true); lAct->setShortcut(QKeySequence("F7")); lAct->setChecked(true);
        connect(m_layersDock, &QDockWidget::visibilityChanged, lAct, &QAction::setChecked);

        QAction* cAct = m_windowMenu->addAction("&Colors", m_colorsDock, &QDockWidget::setVisible);
        cAct->setCheckable(true); cAct->setShortcut(QKeySequence("F8")); cAct->setChecked(true);
        connect(m_colorsDock, &QDockWidget::visibilityChanged, cAct, &QAction::setChecked);

        m_windowMenu->addSeparator();
        QAction* nextAct = m_windowMenu->addAction("&Next Image", this, &MainWindow::nextDocument, QKeySequence("Ctrl+Tab"));
        nextAct->setShortcuts({QKeySequence("Ctrl+Tab"), QKeySequence("Ctrl+PageDown")});
        QAction* prevAct = m_windowMenu->addAction("&Previous Image", this, &MainWindow::previousDocument, QKeySequence("Ctrl+Shift+Tab"));
        prevAct->setShortcuts({QKeySequence("Ctrl+Shift+Tab"), QKeySequence("Ctrl+PageUp")});
        m_windowMenu->addAction("&Close", this, &MainWindow::onClose, QKeySequence::Close);
        m_windowMenu->addAction("Close &All", this, &MainWindow::onCloseAll, QKeySequence("Ctrl+Shift+W"));
        m_windowMenu->addSeparator();
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

bool MainWindow::saveDocument(int index) {
    if (index < 0 || index >= m_documents.size()) return false;
    auto doc = m_documents[index];
    if (doc->filePath().isEmpty()) {
        return saveDocumentAs(index);
    }
    QString err;
    if (!ImageIO::saveDocument(*doc, doc->filePath(), &err)) {
        QMessageBox::critical(this, "Error Saving File", err);
        return false;
    }
    doc->undoStack()->setClean();
    if (m_doc == doc) updateTitle();
    m_documentStrip->updateDocument(index);
    updateWindowMenu();
    return true;
}

bool MainWindow::saveDocumentAs(int index) {
    if (index < 0 || index >= m_documents.size()) return false;
    auto doc = m_documents[index];
    QString defaultName = doc->filePath().isEmpty() ? (doc->fileName() + ".pdn") : doc->filePath();
    QString file = QFileDialog::getSaveFileName(this, "Save Image As", defaultName, ImageIO::saveFileFilter());
    if (file.isEmpty()) return false;

    QString err;
    if (!ImageIO::saveDocument(*doc, file, &err)) {
        QMessageBox::critical(this, "Error Saving File", err);
        return false;
    }
    doc->setFilePath(file);
    doc->undoStack()->setClean();
    if (m_doc == doc) updateTitle();
    m_documentStrip->updateDocument(index);
    updateWindowMenu();
    return true;
}

bool MainWindow::saveAllDocuments() {
    bool allSuccess = true;
    for (int i = 0; i < m_documents.size(); ++i) {
        if (m_documents[i]->isModified()) {
            if (!saveDocument(i)) {
                allSuccess = false;
                break;
            }
        }
    }
    return allSuccess;
}

bool MainWindow::onSave() {
    return saveDocument(m_activeDocIndex);
}

bool MainWindow::onSaveAs() {
    return saveDocumentAs(m_activeDocIndex);
}

bool MainWindow::onSaveAll() {
    return saveAllDocuments();
}

void MainWindow::onClose() {
    closeActiveDocument();
}

void MainWindow::onCloseAll() {
    closeAllDocuments();
}

void MainWindow::onMetadata() {
    if (!m_doc) return;

    MetadataDialog dlg(m_doc.get(), this);
    if (dlg.exec() == QDialog::Accepted) {
        Metadata newMeta = dlg.metadata();
        if (newMeta != m_doc->metadata()) {
            m_doc->setMetadata(newMeta, true);
        }
    }
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
    m_doc->clearSelection();
}

void MainWindow::onCopy() {
    if (!m_doc) return;

    if (m_doc->hasFloatingSelection()) {
        const QImage& baseImg = !m_doc->originalFloatingImage().isNull() ? m_doc->originalFloatingImage() : m_doc->floatingImage();
        if (baseImg.isNull()) return;

        QRectF mappedRect = m_doc->floatingTransform().mapRect(QRectF(0, 0, baseImg.width(), baseImg.height()));
        QRect aligned = mappedRect.toAlignedRect();
        if (aligned.isEmpty()) return;

        QImage cropped(aligned.size(), QImage::Format_ARGB32);
        cropped.fill(Qt::transparent);
        QPainter p(&cropped);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.translate(-aligned.topLeft());
        p.setTransform(m_doc->floatingTransform(), true);
        p.drawImage(0, 0, baseImg);
        p.end();

        s_lastCopiedPos = aligned.topLeft();
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

QImage MainWindow::getClipboardImage(QString* outFileName) const {
    const QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) return QImage();
    const QMimeData* mimeData = clipboard->mimeData();
    if (!mimeData) return QImage();

    // 1. First check if URLs contain local image files (e.g. copied from file explorer)
    if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                QString localPath = url.toLocalFile();
                if (ImageIO::isImageFile(localPath)) {
                    QImage img = ImageIO::loadImage(localPath);
                    if (!img.isNull()) {
                        if (outFileName) {
                            *outFileName = QFileInfo(localPath).completeBaseName();
                        }
                        return img;
                    }
                }
            }
        }
    }

    // 2. Check if clipboard has raw image
    if (mimeData->hasImage()) {
        QImage img = qvariant_cast<QImage>(clipboard->image());
        if (!img.isNull()) {
            if (img.format() != QImage::Format_ARGB32) {
                img = img.convertToFormat(QImage::Format_ARGB32);
            }
            if (outFileName) {
                *outFileName = QString();
            }
            return img;
        }
    }

    // 3. Fallback: check if text contains local file path
    if (mimeData->hasText()) {
        QString text = mimeData->text().trimmed();
        if (text.startsWith("file://", Qt::CaseInsensitive)) {
            QUrl u(text);
            if (u.isLocalFile()) {
                QString localPath = u.toLocalFile();
                if (ImageIO::isImageFile(localPath)) {
                    QImage img = ImageIO::loadImage(localPath);
                    if (!img.isNull()) {
                        if (outFileName) *outFileName = QFileInfo(localPath).completeBaseName();
                        return img;
                    }
                }
            }
        } else if (QFileInfo::exists(text) && QFileInfo(text).isFile()) {
            if (ImageIO::isImageFile(text)) {
                QImage img = ImageIO::loadImage(text);
                if (!img.isNull()) {
                    if (outFileName) *outFileName = QFileInfo(text).completeBaseName();
                    return img;
                }
            }
        }
    }

    return QImage();
}

CanvasExpandChoice MainWindow::askCanvasExpand(bool isPaste) {
    QMessageBox box(this);
    box.setWindowTitle("Paint.NET Clone");
    box.setIcon(QMessageBox::Question);
    if (isPaste) {
        box.setText("The image being pasted is larger than the canvas size.\n\nWould you like to expand the canvas size?");
    } else {
        box.setText("The image being added is larger than the canvas size.\n\nWould you like to expand the canvas size?");
    }
    QPushButton* expandBtn = box.addButton("Expand canvas", QMessageBox::AcceptRole);
    QPushButton* keepBtn = box.addButton("Keep canvas size", QMessageBox::ActionRole);
    QPushButton* cancelBtn = box.addButton("Cancel", QMessageBox::RejectRole);
    box.setDefaultButton(expandBtn);
    box.setEscapeButton(cancelBtn);

    box.exec();

    if (box.clickedButton() == expandBtn) {
        return CanvasExpandChoice::ExpandCanvas;
    } else if (box.clickedButton() == keepBtn) {
        return CanvasExpandChoice::KeepCanvasSize;
    } else {
        return CanvasExpandChoice::Cancel;
    }
}

DropActionChoice MainWindow::askDropAction(const QStringList& filePaths) {
    QMessageBox box(this);
    box.setWindowTitle("Paint.NET Clone");
    box.setIcon(QMessageBox::Question);
    if (filePaths.size() == 1) {
        box.setText(QString("Do you want to open \"%1\" as a new document, or add it as a new layer to the current document?")
                        .arg(QFileInfo(filePaths[0]).fileName()));
    } else {
        box.setText("Do you want to open the images as new documents, or add them as new layers to the current document?");
    }
    QPushButton* openBtn = box.addButton("Open", QMessageBox::AcceptRole);
    QPushButton* addLayerBtn = box.addButton(filePaths.size() == 1 ? "Add as Layer" : "Add as Layers", QMessageBox::ActionRole);
    QPushButton* cancelBtn = box.addButton("Cancel", QMessageBox::RejectRole);
    box.setDefaultButton(openBtn);
    box.setEscapeButton(cancelBtn);

    box.exec();

    if (box.clickedButton() == openBtn) {
        return DropActionChoice::Open;
    } else if (box.clickedButton() == addLayerBtn) {
        return DropActionChoice::AddAsLayer;
    } else {
        return DropActionChoice::Cancel;
    }
}

bool MainWindow::pasteImage(const QImage& img, CanvasExpandChoice expandChoice, const QString& /*sourceName*/) {
    if (img.isNull() || !m_doc) return false;

    bool exceeds = (img.width() > m_doc->width() || img.height() > m_doc->height());
    if (exceeds) {
        CanvasExpandChoice choice = expandChoice;
        if (choice == CanvasExpandChoice::Prompt) {
            choice = askCanvasExpand(true);
        }
        if (choice == CanvasExpandChoice::Cancel) return false;
        if (choice == CanvasExpandChoice::ExpandCanvas) {
            int newW = std::max(m_doc->width(), img.width());
            int newH = std::max(m_doc->height(), img.height());
            m_doc->resizeCanvas(newW, newH, Qt::AlignLeft | Qt::AlignTop);
            if (m_statusWidget) {
                m_statusWidget->setDocumentSize(m_doc->width(), m_doc->height());
            }
            updateTitle();
        }
    }

    QPoint pastePos(0, 0);
    if (!exceeds && s_hasLastCopied && img.size() == s_lastCopiedSize) {
        pastePos = s_lastCopiedPos;
    }

    if (m_toolMgr) {
        m_toolMgr->setActiveTool(ToolType::MoveSelectedPixels);
    }

    if (m_doc->hasFloatingSelection()) {
        m_doc->bakeFloatingSelection();
    }

    m_doc->createFloatingSelection(img, pastePos, false, "Paste");

    m_doc->selection().clear();
    m_doc->selection().addRect(QRectF(pastePos, img.size()), SelectionCombineMode::Replace);

    emit m_doc->selectionChanged();
    emit m_doc->documentChanged();
    return true;
}

bool MainWindow::pasteImageIntoNewLayer(const QImage& img, CanvasExpandChoice expandChoice, const QString& sourceName) {
    if (img.isNull() || !m_doc) return false;

    bool exceeds = (img.width() > m_doc->width() || img.height() > m_doc->height());
    if (exceeds) {
        CanvasExpandChoice choice = expandChoice;
        if (choice == CanvasExpandChoice::Prompt) {
            choice = askCanvasExpand(true);
        }
        if (choice == CanvasExpandChoice::Cancel) return false;
        if (choice == CanvasExpandChoice::ExpandCanvas) {
            int newW = std::max(m_doc->width(), img.width());
            int newH = std::max(m_doc->height(), img.height());
            m_doc->resizeCanvas(newW, newH, Qt::AlignLeft | Qt::AlignTop);
            if (m_statusWidget) {
                m_statusWidget->setDocumentSize(m_doc->width(), m_doc->height());
            }
            updateTitle();
        }
    }

    QPoint pastePos(0, 0);
    if (!exceeds && s_hasLastCopied && img.size() == s_lastCopiedSize) {
        pastePos = s_lastCopiedPos;
    }

    if (m_doc->hasFloatingSelection()) {
        m_doc->bakeFloatingSelection();
    }

    QString layerName = sourceName.isEmpty() ? "Pasted Layer" : sourceName;
    auto newLayer = m_doc->addLayer(layerName);
    if (!newLayer) return false;

    if (m_toolMgr) {
        m_toolMgr->setActiveTool(ToolType::MoveSelectedPixels);
    }

    m_doc->createFloatingSelection(img, pastePos, false, "Paste into New Layer");

    m_doc->selection().clear();
    m_doc->selection().addRect(QRectF(pastePos, img.size()), SelectionCombineMode::Replace);

    emit m_doc->selectionChanged();
    emit m_doc->documentChanged();
    return true;
}

bool MainWindow::addImageAsLayer(const QImage& img, const QString& layerName, CanvasExpandChoice expandChoice) {
    if (img.isNull() || !m_doc) return false;

    bool exceeds = (img.width() > m_doc->width() || img.height() > m_doc->height());
    if (exceeds) {
        CanvasExpandChoice choice = expandChoice;
        if (choice == CanvasExpandChoice::Prompt) {
            choice = askCanvasExpand(false);
        }
        if (choice == CanvasExpandChoice::Cancel) return false;
        if (choice == CanvasExpandChoice::ExpandCanvas) {
            int newW = std::max(m_doc->width(), img.width());
            int newH = std::max(m_doc->height(), img.height());
            m_doc->resizeCanvas(newW, newH, Qt::AlignLeft | Qt::AlignTop);
            if (m_statusWidget) {
                m_statusWidget->setDocumentSize(m_doc->width(), m_doc->height());
            }
            updateTitle();
        }
    }

    if (m_doc->hasFloatingSelection()) {
        m_doc->bakeFloatingSelection();
    }

    QString lName = layerName.isEmpty() ? "Layer" : layerName;
    auto newLayer = m_doc->addLayer(lName);
    if (!newLayer) return false;

    if (m_toolMgr) {
        m_toolMgr->setActiveTool(ToolType::MoveSelectedPixels);
    }

    m_doc->createFloatingSelection(img, QPoint(0, 0), false, "Add Layer From File");

    m_doc->selection().clear();
    m_doc->selection().addRect(QRectF(0, 0, img.width(), img.height()), SelectionCombineMode::Replace);

    emit m_doc->selectionChanged();
    emit m_doc->documentChanged();
    return true;
}

void MainWindow::onPaste() {
    if (!m_doc) return;
    QString sourceName;
    QImage img = getClipboardImage(&sourceName);
    if (img.isNull()) return;

    pasteImage(img, CanvasExpandChoice::Prompt, sourceName);
}

void MainWindow::onPasteIntoNewLayer() {
    if (!m_doc) return;
    QString sourceName;
    QImage img = getClipboardImage(&sourceName);
    if (img.isNull()) return;

    pasteImageIntoNewLayer(img, CanvasExpandChoice::Prompt, sourceName);
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
        m_doc->resizeImage(dlg.newWidth(), dlg.newHeight(), dlg.algorithm());
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
        "<li>Multiple images / documents in one window with thumbnail strip</li>"
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

void MainWindow::closeEvent(QCloseEvent* event) {
    for (int i = m_documents.size() - 1; i >= 0; --i) {
        if (!maybeSaveDocument(i)) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData && (mimeData->hasUrls() || mimeData->hasImage())) {
        event->acceptProposedAction();
    } else {
        QMainWindow::dragEnterEvent(event);
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (!mimeData) return;

    QStringList imageFiles;
    if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                if (ImageIO::isImageFile(path)) {
                    imageFiles.append(path);
                }
            }
        }
    }

    if (imageFiles.isEmpty()) {
        if (mimeData->hasImage()) {
            QImage img = qvariant_cast<QImage>(mimeData->imageData());
            if (!img.isNull()) {
                if (img.format() != QImage::Format_ARGB32) {
                    img = img.convertToFormat(QImage::Format_ARGB32);
                }
                if (!m_doc || m_documents.isEmpty()) {
                    auto newDoc = std::make_shared<Document>(img);
                    addDocument(newDoc, true);
                } else {
                    DropActionChoice choice = askDropAction({ "Dropped Image" });
                    if (choice == DropActionChoice::Open) {
                        auto newDoc = std::make_shared<Document>(img);
                        addDocument(newDoc, true);
                    } else if (choice == DropActionChoice::AddAsLayer) {
                        addImageAsLayer(img, "Dropped Image", CanvasExpandChoice::Prompt);
                    }
                }
                event->acceptProposedAction();
                return;
            }
        }
        QMainWindow::dropEvent(event);
        return;
    }

    if (!m_doc || m_documents.isEmpty()) {
        for (const QString& file : imageFiles) {
            openFile(file);
        }
        event->acceptProposedAction();
        return;
    }

    DropActionChoice choice = askDropAction(imageFiles);
    if (choice == DropActionChoice::Cancel) {
        event->acceptProposedAction();
        return;
    }

    if (choice == DropActionChoice::Open) {
        for (const QString& file : imageFiles) {
            openFile(file);
        }
    } else if (choice == DropActionChoice::AddAsLayer) {
        for (const QString& file : imageFiles) {
            QImage img = ImageIO::loadImage(file);
            if (!img.isNull()) {
                QString layerName = QFileInfo(file).completeBaseName();
                addImageAsLayer(img, layerName, CanvasExpandChoice::Prompt);
            } else {
                QMessageBox::critical(this, "Error Opening Image", QString("Could not load image from %1").arg(file));
            }
        }
    }

    event->acceptProposedAction();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        // Document navigation shortcuts: Ctrl+Tab / Ctrl+Shift+Tab / Ctrl+PageDown / Ctrl+PageUp
        if (keyEvent->modifiers() & Qt::ControlModifier) {
            if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_PageDown) {
                if (keyEvent->modifiers() & Qt::ShiftModifier) {
                    previousDocument();
                } else {
                    nextDocument();
                }
                return true;
            }
            if (keyEvent->key() == Qt::Key_Backtab || keyEvent->key() == Qt::Key_PageUp) {
                previousDocument();
                return true;
            }
            if (keyEvent->key() == Qt::Key_W || keyEvent->key() == Qt::Key_F4) {
                closeActiveDocument();
                return true;
            }
        }

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
