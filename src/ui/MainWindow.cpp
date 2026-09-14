#include "MainWindow.h"
#include "../io/ImageIO.h"
#include "Dialogs.h"
#include "../effects/BrightnessContrast.h"
#include "../effects/GaussianBlur.h"
#include "../effects/Adjustments.h"
#include "../effects/BlurEffects.h"
#include "../effects/DistortEffects.h"
#include "../effects/NoiseEffects.h"
#include "../effects/PhotoEffects.h"
#include "../effects/StylizeEffects.h"
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
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QApplication>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QUrl>
#include <QLabel>
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
    setWindowTitle("Thulium");
    setWindowIcon(QIcon(":/icons/logo.png"));
    resize(1200, 800);
    setAcceptDrops(true);

    qApp->installEventFilter(this);

    m_documentStrip = new DocumentStrip(this);
    m_canvasView = new CanvasView(m_toolMgr, this);
    EffectDialog::setGlobalCanvasBridge(m_canvasView);

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

    if (m_doc) {
        disconnect(m_doc.get(), &Document::selectionChanged, this, &MainWindow::updateSelectionStatus);
    }

    m_activeDocIndex = index;
    m_doc = m_documents[m_activeDocIndex];

    if (m_doc) {
        connect(m_doc.get(), &Document::selectionChanged, this, &MainWindow::updateSelectionStatus);
    }

    m_toolMgr->setDocument(m_doc.get());
    m_canvasView->setDocument(m_doc);
    m_historyDock->setDocument(m_doc);
    m_layersDock->setDocument(m_doc);

    m_documentStrip->setCurrentIndex(m_activeDocIndex);
    updateTitle();
    updateWindowMenu();
    updateSelectionStatus();
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
        updateSelectionStatus();
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

void MainWindow::newDocument(int width, int height, bool transparentBackground) {
    clearLastCopied();

    auto doc = std::make_shared<Document>(width, height, transparentBackground);
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
    QString title = "Thulium - ";
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
    adjMenu->addAction("&Auto-Level", this, &MainWindow::onAutoLevel, QKeySequence("Ctrl+Shift+L"));
    adjMenu->addAction("&Black and White", this, &MainWindow::onBlackAndWhite, QKeySequence("Ctrl+Shift+G"));
    adjMenu->addAction("&Brightness / Contrast...", this, &MainWindow::onBrightnessContrast, QKeySequence("Ctrl+Shift+T"));
    adjMenu->addAction("&Hue / Saturation...", this, &MainWindow::onHueSaturation, QKeySequence("Ctrl+Shift+U"));
    adjMenu->addAction("&Invert Colors", this, &MainWindow::onInvertColors, QKeySequence("Ctrl+Shift+I"));
    adjMenu->addAction("Invert &Alpha", this, &MainWindow::onInvertAlpha);
    adjMenu->addAction("&Posterize...", this, &MainWindow::onPosterize, QKeySequence("Ctrl+Shift+P"));
    adjMenu->addAction("&Sepia", this, &MainWindow::onSepia);
    adjMenu->addAction("&Temperature / Tint...", this, &MainWindow::onTemperatureTint);

    // --- Effects Menu ---
    QMenu* fxMenu = mb->addMenu("&Effects");
    m_repeatEffectAct = fxMenu->addAction("&Repeat Effect", this, &MainWindow::onRepeatLastEffect, QKeySequence("Ctrl+F"));
    m_repeatEffectAct->setEnabled(false);
    fxMenu->addSeparator();

    // Artistic
    QMenu* artisticMenu = fxMenu->addMenu("&Artistic");
    artisticMenu->addAction("&Oil Painting...", this, &MainWindow::onOilPainting);

    // Blurs
    QMenu* blursMenu = fxMenu->addMenu("&Blurs");
    blursMenu->addAction("&Gaussian Blur...", this, &MainWindow::onGaussianBlur);
    blursMenu->addAction("&Motion Blur...", this, &MainWindow::onMotionBlur);
    blursMenu->addAction("&Radial Blur...", this, &MainWindow::onRadialBlur);

    // Distort
    QMenu* distortMenu = fxMenu->addMenu("&Distort");
    distortMenu->addAction("&Pixelate...", this, &MainWindow::onPixelate);
    distortMenu->addAction("&Twist...", this, &MainWindow::onTwist);

    // Noise
    QMenu* noiseMenu = fxMenu->addMenu("&Noise");
    noiseMenu->addAction("&Add Noise...", this, &MainWindow::onAddNoise);
    noiseMenu->addAction("&Median...", this, &MainWindow::onMedian);

    // Photo
    QMenu* photoMenu = fxMenu->addMenu("&Photo");
    photoMenu->addAction("&Glow...", this, &MainWindow::onGlow);
    photoMenu->addAction("&Sharpen...", this, &MainWindow::onSharpen);
    photoMenu->addAction("&Vignette...", this, &MainWindow::onVignette);

    // Stylize
    QMenu* stylizeMenu = fxMenu->addMenu("&Stylize");
    stylizeMenu->addAction("&Edge Detect...", this, &MainWindow::onEdgeDetect);
    stylizeMenu->addAction("&Emboss...", this, &MainWindow::onEmboss);

    // --- Window Menu ---
    m_windowMenu = mb->addMenu("&Window");
    m_windowMenu->addAction("&Reset Window Locations", this, &MainWindow::onResetWindowLocations);
    m_windowMenu->addSeparator();

    // --- Help Menu ---
    QMenu* helpMenu = mb->addMenu("&Help");
    helpMenu->addAction("&About Thulium...", this, &MainWindow::onAbout);
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
        act->setShortcutContext(Qt::WindowShortcut);
        m_toolsDock->addAction(act);
        m_historyDock->addAction(act);
        m_layersDock->addAction(act);
        m_colorsDock->addAction(act);
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
    updateSelectionStatus();
}

void MainWindow::updateSelectionStatus() {
    if (!m_statusWidget) return;
    if (m_doc && !m_doc->selection().isEmpty()) {
        QRect bounds = m_doc->selection().region().boundingRect();
        m_statusWidget->setSelectionBounds(true, bounds.x(), bounds.y(), bounds.width(), bounds.height());
    } else {
        m_statusWidget->setSelectionBounds(false, 0, 0, 0, 0);
    }
}

// --- Slot Implementations ---

void MainWindow::onNew() {
    NewImageDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        newDocument(dlg.imageWidth(), dlg.imageHeight(), dlg.isTransparentBackground());
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
        m_doc->undoStack()->beginMacro("Cut");
        QRegion oldRegion = m_doc->selection().region();
        m_doc->discardFloatingSelection();
        m_doc->clearSelection();
        if (!oldRegion.isEmpty()) {
            m_doc->undoStack()->push(new SelectionUndoCommand(m_doc.get(), oldRegion, QRegion(), "Deselect"));
        }
        m_doc->undoStack()->endMacro();
        return;
    }

    onCopy();
    m_doc->undoStack()->beginMacro("Cut");
    auto layer = m_doc->activeLayer();
    if (layer) {
        QImage oldImg = layer->image().copy();
        QPainter p(&layer->image());
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        if (!m_doc->selection().isEmpty()) {
            p.fillPath(m_doc->selection().path(), Qt::transparent);
        } else {
            layer->clear();
        }
        p.end();

        m_doc->undoStack()->push(new LayerBitmapUndoCommand(m_doc.get(), m_doc->activeLayerIndex(), oldImg, "Cut"));
    }

    QRegion oldRegion = m_doc->selection().region();
    m_doc->clearSelection();
    if (!oldRegion.isEmpty()) {
        m_doc->undoStack()->push(new SelectionUndoCommand(m_doc.get(), oldRegion, QRegion(), "Deselect"));
    }
    m_doc->undoStack()->endMacro();

    emit m_doc->documentChanged();
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

    QImage cropped(srcRect.size(), QImage::Format_ARGB32);
    cropped.fill(Qt::transparent);

    QPainter p(&cropped);
    if (!m_doc->selection().isEmpty()) {
        p.setClipPath(m_doc->selection().path().translated(-srcRect.x(), -srcRect.y()));
    }
    p.drawImage(-srcRect.x(), -srcRect.y(), layer->image());
    p.end();

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
    box.setWindowTitle("Thulium");
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
    box.setWindowTitle("Thulium");
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
        m_doc->bakeFloatingSelection(true, "Deselect");
    }

    QRegion oldRegion = m_doc->selection().region();

    m_doc->createFloatingSelection(img, pastePos, false, "Paste");

    m_doc->selection().clear();
    m_doc->selection().addRect(QRectF(pastePos, img.size()), SelectionCombineMode::Replace);

    m_doc->undoStack()->push(new PasteFloatingUndoCommand(m_doc.get(), img, pastePos, m_doc->activeLayerIndex(), oldRegion, "Paste"));

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
        m_doc->bakeFloatingSelection(true, "Deselect");
    }

    QString layerName = sourceName.isEmpty() ? "Pasted Layer" : sourceName;
    auto newLayer = m_doc->addLayer(layerName);
    if (!newLayer) return false;

    if (m_toolMgr) {
        m_toolMgr->setActiveTool(ToolType::MoveSelectedPixels);
    }

    QRegion oldRegion = m_doc->selection().region();

    m_doc->createFloatingSelection(img, pastePos, false, "Paste into New Layer");

    m_doc->selection().clear();
    m_doc->selection().addRect(QRectF(pastePos, img.size()), SelectionCombineMode::Replace);

    m_doc->undoStack()->push(new PasteFloatingUndoCommand(m_doc.get(), img, pastePos, m_doc->activeLayerIndex(), oldRegion, "Paste into New Layer"));

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
        m_doc->bakeFloatingSelection(true, "Deselect");
    }

    QString lName = layerName.isEmpty() ? "Layer" : layerName;
    auto newLayer = m_doc->addLayer(lName);
    if (!newLayer) return false;

    if (m_toolMgr) {
        m_toolMgr->setActiveTool(ToolType::MoveSelectedPixels);
    }

    QRegion oldRegion = m_doc->selection().region();

    m_doc->createFloatingSelection(img, QPoint(0, 0), false, "Add Layer From File");

    m_doc->selection().clear();
    m_doc->selection().addRect(QRectF(0, 0, img.width(), img.height()), SelectionCombineMode::Replace);

    m_doc->undoStack()->push(new PasteFloatingUndoCommand(m_doc.get(), img, QPoint(0, 0), m_doc->activeLayerIndex(), oldRegion, "Add Layer From File"));

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
        QRegion oldRegion = m_doc->selection().region();
        m_doc->selection().invert(m_doc->width(), m_doc->height());
        QRegion newRegion = m_doc->selection().region();
        if (oldRegion != newRegion) {
            m_doc->undoStack()->push(new SelectionUndoCommand(m_doc.get(), oldRegion, newRegion, "Invert Selection"));
        }
        emit m_doc->selectionChanged();
    }
}

void MainWindow::onSelectAll() {
    if (m_doc) {
        if (m_doc->hasFloatingSelection()) {
            m_doc->bakeFloatingSelection();
        }
        QRegion oldRegion = m_doc->selection().region();
        m_doc->selection().selectAll(m_doc->width(), m_doc->height());
        QRegion newRegion = m_doc->selection().region();
        if (oldRegion != newRegion) {
            m_doc->undoStack()->push(new SelectionUndoCommand(m_doc.get(), oldRegion, newRegion, "Select All"));
        }
        emit m_doc->selectionChanged();
        emit m_doc->documentChanged();
    }
}

void MainWindow::onDeselect() {
    if (!m_doc) return;
    if (m_doc->hasFloatingSelection()) {
        m_doc->clearSelection();
    } else if (!m_doc->selection().isEmpty()) {
        QRegion oldRegion = m_doc->selection().region();
        m_doc->clearSelection();
        m_doc->undoStack()->push(new SelectionUndoCommand(m_doc.get(), oldRegion, QRegion(), "Deselect"));
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

ICanvasInteractionBridge* MainWindow::canvasBridge() {
    return m_canvasView;
}

void MainWindow::runEffect(const std::shared_ptr<IEffect>& effect) {
    if (!m_doc || !effect) return;

    if (m_canvasView) {
        m_canvasView->setInteractionBlocked(true);
    }
    menuBar()->setEnabled(false);
    m_toolsDock->setEnabled(false);
    m_historyDock->setEnabled(false);
    m_layersDock->setEnabled(false);
    m_colorsDock->setEnabled(false);

    bool accepted = effect->showDialog(this, m_doc.get());

    menuBar()->setEnabled(true);
    m_toolsDock->setEnabled(true);
    m_historyDock->setEnabled(true);
    m_layersDock->setEnabled(true);
    m_colorsDock->setEnabled(true);
    if (m_canvasView) {
        m_canvasView->setInteractionBlocked(false);
    }

    if (accepted) {
        m_lastEffect = effect;
        if (m_repeatEffectAct) {
            m_repeatEffectAct->setEnabled(true);
            m_repeatEffectAct->setText("&Repeat " + effect->name());
        }
    }
}

void MainWindow::onRepeatLastEffect() {
    if (!m_doc || !m_doc->activeLayer() || !m_lastEffect) return;
    int layerIdx = m_doc->activeLayerIndex();
    QImage original = m_doc->activeLayer()->image().copy();
    QImage copy = original.copy();
    m_lastEffect->apply(copy, m_doc->selection());
    m_doc->activeLayer()->setImage(copy);
    m_doc->undoStack()->push(new LayerBitmapUndoCommand(m_doc.get(), layerIdx, original, m_lastEffect->name()));
    emit m_doc->documentChanged();
}

void MainWindow::onAutoLevel() { runEffect(std::make_shared<AutoLevelEffect>()); }
void MainWindow::onBlackAndWhite() { runEffect(std::make_shared<BlackAndWhiteEffect>()); }
void MainWindow::onBrightnessContrast() { runEffect(std::make_shared<BrightnessContrastEffect>()); }
void MainWindow::onHueSaturation() { runEffect(std::make_shared<HueSaturationEffect>()); }
void MainWindow::onInvertColors() { runEffect(std::make_shared<InvertColorsEffect>()); }
void MainWindow::onInvertAlpha() { runEffect(std::make_shared<InvertAlphaEffect>()); }
void MainWindow::onPosterize() { runEffect(std::make_shared<PosterizeEffect>()); }
void MainWindow::onSepia() { runEffect(std::make_shared<SepiaEffect>()); }
void MainWindow::onTemperatureTint() { runEffect(std::make_shared<TemperatureTintEffect>()); }

void MainWindow::onOilPainting() { runEffect(std::make_shared<OilPaintingEffect>()); }
void MainWindow::onGaussianBlur() { runEffect(std::make_shared<GaussianBlurEffect>()); }
void MainWindow::onMotionBlur() { runEffect(std::make_shared<MotionBlurEffect>()); }
void MainWindow::onRadialBlur() { runEffect(std::make_shared<RadialBlurEffect>()); }
void MainWindow::onPixelate() { runEffect(std::make_shared<PixelateEffect>()); }
void MainWindow::onTwist() { runEffect(std::make_shared<TwistEffect>()); }
void MainWindow::onAddNoise() { runEffect(std::make_shared<AddNoiseEffect>()); }
void MainWindow::onMedian() { runEffect(std::make_shared<MedianEffect>()); }
void MainWindow::onGlow() { runEffect(std::make_shared<GlowEffect>()); }
void MainWindow::onSharpen() { runEffect(std::make_shared<SharpenEffect>()); }
void MainWindow::onVignette() { runEffect(std::make_shared<VignetteEffect>()); }
void MainWindow::onEdgeDetect() { runEffect(std::make_shared<EdgeDetectEffect>()); }
void MainWindow::onEmboss() { runEffect(std::make_shared<EmbossEffect>()); }

void MainWindow::onResetWindowLocations() {
    m_toolsDock->setVisible(true);
    m_historyDock->setVisible(true);
    m_layersDock->setVisible(true);
    m_colorsDock->setVisible(true);
}

void MainWindow::onAbout() {
    QMessageBox box(this);
    box.setWindowTitle("About Thulium");
    box.setWindowIcon(QIcon(":/icons/logo.png"));
    box.setIconPixmap(QPixmap(":/icons/logo.png").scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    box.setTextFormat(Qt::RichText);
    box.setTextInteractionFlags(Qt::TextBrowserInteraction);
    box.setText(
        "<h3>Thulium</h3>"
        "<p>A fast, lightweight, and cross-platform raster image editor.</p>"
        "<p><b>Architecture & Technology:</b></p>"
        "<ul>"
        "<li><b>Modern C++17 Core:</b> Native performance and compact memory footprint without managed runtime overhead.</li>"
        "<li><b>Qt 6 UI:</b> Responsive, modular interface with dockable palettes and multi-document workflows.</li>"
        "<li><b>Dual Rendering Pipeline:</b> Hardware-accelerated Vulkan GPU backend alongside an optimized CPU fallback.</li>"
        "<li><b>Clean-Room Compatibility:</b> Direct read/write support for Paint.NET (<code>.pdn</code>) documents, plus PNG, JPEG, WebP, BMP, and GIF.</li>"
        "</ul>"
        "<p>"
        "Project: <a href=\"https://github.com/finn-freitag/Thulium\">https://github.com/finn-freitag/Thulium</a><br>"
        "Author: <a href=\"https://finnfreitag.com?ref=thulium\">finnfreitag.com</a>"
        "</p>"
        "<hr>"
        "<p><small>"
        "Copyright &copy; 2026 Finn Freitag. Released under the MIT License.<br><br>"
        "<b>Trademarks & Acknowledgments:</b><br>"
        "Paint.NET is a registered trademark of Rick Brewster and dotPDN LLC. Thulium is an independent, clean-room project and is not affiliated with, endorsed by, or sponsored by dotPDN LLC.<br>"
        "Qt is a registered trademark of The Qt Company Ltd. and its subsidiaries.<br>"
        "Vulkan and the Vulkan logo are registered trademarks of the Khronos Group Inc."
        "</small></p>"
    );

    for (QLabel* label : box.findChildren<QLabel*>()) {
        label->setOpenExternalLinks(true);
    }

    box.exec();
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

bool MainWindow::isComponentOfMainWindow(QObject* obj) const {
    if (!obj) return false;
    QWidget* w = qobject_cast<QWidget*>(obj);
    if (!w) return false;

    if (w == this || this->isAncestorOf(w)) {
        return true;
    }

    const QDockWidget* docks[] = { m_toolsDock, m_historyDock, m_layersDock, m_colorsDock };
    for (const QDockWidget* dock : docks) {
        if (dock && (dock == w || dock->isAncestorOf(w))) {
            return true;
        }
    }

    return false;
}

bool MainWindow::isMainWindowActive() const {
    QWidget* activeWin = QApplication::activeWindow();
    if (!activeWin) return false;
    if (activeWin == this) return true;

    const QDockWidget* docks[] = { m_toolsDock, m_historyDock, m_layersDock, m_colorsDock };
    for (const QDockWidget* dock : docks) {
        if (dock && (activeWin == dock || dock->isAncestorOf(activeWin))) {
            return true;
        }
    }
    return false;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        QWidget* watchedWidget = qobject_cast<QWidget*>(watched);
        if (!watchedWidget) {
            return QMainWindow::eventFilter(watched, event);
        }

        // Keybinds must only apply when MainWindow (or one of its docks) is active,
        // and must never apply to other windows/dialogs (e.g. file save dialog).
        if (!isMainWindowActive() || !isComponentOfMainWindow(watchedWidget)) {
            return QMainWindow::eventFilter(watched, event);
        }

        // 1. If focus is inside a text input widget or line edit, allow standard typing
        QWidget* fw = QApplication::focusWidget();
        auto isTextInput = [](QObject* obj) -> bool {
            if (!obj) return false;
            for (QObject* cur = obj; cur != nullptr; cur = cur->parent()) {
                if (qobject_cast<QLineEdit*>(cur) ||
                    qobject_cast<QTextEdit*>(cur) ||
                    qobject_cast<QPlainTextEdit*>(cur) ||
                    qobject_cast<QAbstractSpinBox*>(cur)) {
                    return true;
                }
                if (auto cb = qobject_cast<QComboBox*>(cur)) {
                    if (cb->isEditable()) return true;
                }
                if (qobject_cast<QWidget*>(cur) && static_cast<QWidget*>(cur)->isWindow()) {
                    break;
                }
            }
            return false;
        };

        if (isTextInput(fw) || isTextInput(watchedWidget)) {
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

        // 4. Document navigation shortcuts: Ctrl+Tab / Ctrl+Shift+Tab / Ctrl+PageDown / Ctrl+PageUp
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

        // 5. Try ToolManager global shortcuts (tool selection, cycling, X, D, [, ], arrows, Esc)
        if (m_toolMgr && m_toolMgr->handleKeyPress(keyEvent)) {
            if (m_canvasView) {
                m_canvasView->update();
            }
            return true; // Event consumed for MainWindow component!
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    QWidget* fw = focusWidget();
    auto isTextInput = [](QObject* obj) -> bool {
        if (!obj) return false;
        for (QObject* cur = obj; cur != nullptr; cur = cur->parent()) {
            if (qobject_cast<QLineEdit*>(cur) ||
                qobject_cast<QTextEdit*>(cur) ||
                qobject_cast<QPlainTextEdit*>(cur) ||
                qobject_cast<QAbstractSpinBox*>(cur)) {
                return true;
            }
            if (auto cb = qobject_cast<QComboBox*>(cur)) {
                if (cb->isEditable()) return true;
            }
            if (qobject_cast<QWidget*>(cur) && static_cast<QWidget*>(cur)->isWindow()) {
                break;
            }
        }
        return false;
    };

    if (isTextInput(fw)) {
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
