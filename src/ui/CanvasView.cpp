#include "CanvasView.h"
#include "../rendering/SoftwareRenderer.h"
#include "../tools/ViewTools.h"
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace pdn {

namespace {

constexpr double kMinZoom = 0.01;
constexpr double kMaxZoom = 64.0;
constexpr int kFitMargin = 40;          // free space kept around the image by "zoom to window"
constexpr int kMarchingAntsIntervalMs = 80;

// Zoom steps used by zoom in / zoom out, like Paint.NET's zoom levels.
const double kZoomLevels[] = {
    0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.08, 0.12, 0.16, 0.25, 0.33, 0.50, 0.66, 0.75,
    1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 12.0, 16.0, 24.0, 32.0, 48.0, 64.0
};

// Ruler tick spacing in document pixels: the smallest 1/2/5 x 10^n step that is at least minPixels apart on screen.
int rulerStep(double zoom, double minPixels) {
    const double raw = minPixels / zoom;
    double step = std::pow(10.0, std::floor(std::log10(std::max(raw, 1e-9))));
    if (step < raw) step *= 2;
    if (step < raw) step *= 2.5;
    if (step < raw) step *= 2;
    return std::max(1, static_cast<int>(std::ceil(step)));
}

} // namespace

CanvasView::CanvasView(ToolManager* toolMgr, QWidget* parent)
    : QWidget(parent), m_toolMgr(toolMgr) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    // The renderer paints every pixel of the widget itself.
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_renderer = std::make_unique<SoftwareRenderer>();
    m_renderer->initialize();

    m_hScroll = new QScrollBar(Qt::Horizontal, this);
    m_vScroll = new QScrollBar(Qt::Vertical, this);
    for (QScrollBar* bar : {m_hScroll, m_vScroll}) {
        bar->setCursor(Qt::ArrowCursor);
        bar->setFocusPolicy(Qt::NoFocus);
        bar->hide();
    }
    connect(m_hScroll, &QScrollBar::valueChanged, this, [this](int value) {
        if (m_syncingScrollBars) return;
        setPanOffset(QPointF(m_hScroll->property("maxPan").toDouble() - value, m_renderOpts.panOffset.y()));
    });
    connect(m_vScroll, &QScrollBar::valueChanged, this, [this](int value) {
        if (m_syncingScrollBars) return;
        setPanOffset(QPointF(m_renderOpts.panOffset.x(), m_vScroll->property("maxPan").toDouble() - value));
    });

    // Marching ants only animate (and only repaint the selection's bounding box) while a selection exists.
    m_marchingAntsTimer.setInterval(kMarchingAntsIntervalMs);
    connect(&m_marchingAntsTimer, &QTimer::timeout, this, [this]() {
        if (!m_doc || m_doc->selection().isEmpty()) {
            m_marchingAntsTimer.stop();
            return;
        }
        m_renderOpts.marchingAntsOffset = (m_renderOpts.marchingAntsOffset + 1) % 8;
        const QRectF b = m_doc->selection().boundingRect();
        const QRectF vp(docToViewport(b.topLeft()), docToViewport(b.bottomRight()));
        update(vp.toAlignedRect().adjusted(-3, -3, 3, 3));
    });

    // Wire up Pan & Zoom tool callbacks
    auto panTool = std::dynamic_pointer_cast<PanTool>(m_toolMgr->tool(ToolType::Pan));
    if (panTool) {
        panTool->onPan = [this](const QPointF& delta) {
            setPanOffset(m_renderOpts.panOffset + delta);
        };
    }
    auto zoomTool = std::dynamic_pointer_cast<ZoomTool>(m_toolMgr->tool(ToolType::Zoom));
    if (zoomTool) {
        zoomTool->onZoom = [this](bool zoomIn, const QPointF& center) {
            double factor = zoomIn ? 1.25 : 0.8;
            setZoom(m_renderOpts.zoom * factor, center);
        };
        zoomTool->onZoomToRect = [this](const QRectF& docRect) { zoomToRect(docRect); };
    }

    connect(m_toolMgr, &ToolManager::activeToolChanged, this, [this](ToolType) {
        updateToolCursor();
        update();
    });
    connect(m_toolMgr, &ToolManager::contextChanged, this, QOverload<>::of(&CanvasView::update));
}

CanvasView::~CanvasView() = default;

void CanvasView::setDocument(std::shared_ptr<Document> doc) {
    if (m_doc == doc) return;

    if (m_doc) {
        m_viewStates[m_doc.get()] = m_renderOpts;
        disconnect(m_doc.get(), nullptr, this, nullptr);
        m_renderer->releaseDocument(m_doc.get());
    }

    m_doc = doc;
    if (m_doc) {
        m_toolMgr->setDocument(m_doc.get());
        connect(m_doc.get(), &Document::documentChanged, this, [this]() {
            // Structure or size may have changed: revalidate the view, then repaint everything.
            m_renderOpts.panOffset = clampPanOffset(m_renderOpts.panOffset);
            updateScrollBars();
            update();
        });
        connect(m_doc.get(), &Document::pixelsChanged, this, &CanvasView::onPixelsChanged);
        connect(m_doc.get(), &Document::selectionChanged, this, &CanvasView::onSelectionChanged);

        if (m_viewStates.contains(m_doc.get())) {
            const RenderOptions saved = m_viewStates[m_doc.get()];
            m_renderOpts.zoom = saved.zoom;
            m_renderOpts.panOffset = clampPanOffset(saved.panOffset);
            emit zoomChanged(m_renderOpts.zoom);
        } else {
            zoomToWindow();
            m_viewStates[m_doc.get()] = m_renderOpts;
        }
        onSelectionChanged();
    } else {
        m_marchingAntsTimer.stop();
    }
    updateScrollBars();
    update();
}

void CanvasView::removeDocumentViewState(const Document* doc) {
    m_viewStates.remove(doc);
    m_renderer->releaseDocument(doc);
}

void CanvasView::onPixelsChanged(const QRect& docRect) {
    if (!m_doc) return;
    RenderOptions opts = m_renderOpts;
    opts.devicePixelRatio = devicePixelRatioF();
    const QRect dirty = m_renderer->invalidate(*m_doc, docRect, opts);
    if (!dirty.isEmpty()) {
        update(dirty.translated(rulerOffset(), rulerOffset()));
    }
}

void CanvasView::onSelectionChanged() {
    if (m_doc && !m_doc->selection().isEmpty()) {
        if (!m_marchingAntsTimer.isActive()) m_marchingAntsTimer.start();
    } else {
        m_marchingAntsTimer.stop();
    }
    update();
}

QSize CanvasView::viewportSize() const {
    return QSize(width() - rulerOffset(), height() - rulerOffset());
}

QPointF CanvasView::viewportToDoc(const QPointF& vpPos) const {
    const qreal r = rulerOffset();
    return QPointF((vpPos.x() - r - m_renderOpts.panOffset.x()) / m_renderOpts.zoom,
                   (vpPos.y() - r - m_renderOpts.panOffset.y()) / m_renderOpts.zoom);
}

QPointF CanvasView::docToViewport(const QPointF& docPos) const {
    const qreal r = rulerOffset();
    return QPointF(m_renderOpts.panOffset.x() + docPos.x() * m_renderOpts.zoom + r,
                   m_renderOpts.panOffset.y() + docPos.y() * m_renderOpts.zoom + r);
}

QPointF CanvasView::viewportCenterDocPos() const {
    const QSize vp = viewportSize();
    return viewportToDoc(QPointF(rulerOffset() + vp.width() / 2.0, rulerOffset() + vp.height() / 2.0));
}

void CanvasView::applyView(double zoom, const QPointF& pan) {
    const bool zoomChangedNow = zoom != m_renderOpts.zoom;
    m_renderOpts.zoom = zoom;
    m_renderOpts.panOffset = clampPanOffset(pan);
    updateScrollBars();
    if (zoomChangedNow) emit zoomChanged(m_renderOpts.zoom);
    update();
}

void CanvasView::setZoom(double z, const QPointF& centerDocPos) {
    const double oldZoom = m_renderOpts.zoom;
    const double newZoom = std::clamp(z, kMinZoom, kMaxZoom);
    if (std::abs(newZoom - oldZoom) < 1e-9) return;

    QPointF center = centerDocPos;
    if (center.x() < 0 && m_doc) {
        // Keep whatever is in the middle of the viewport there, unless the image does not reach it.
        center = viewportCenterDocPos();
        center.setX(std::clamp(center.x(), 0.0, static_cast<double>(m_doc->width())));
        center.setY(std::clamp(center.y(), 0.0, static_cast<double>(m_doc->height())));
    }

    // The viewport position of `center` must not move
    const QPointF anchor = m_renderOpts.panOffset + center * oldZoom;
    applyView(newZoom, anchor - center * newZoom);
}

void CanvasView::zoomIn() {
    for (double level : kZoomLevels) {
        if (level > m_renderOpts.zoom * 1.0001) {
            setZoom(level);
            return;
        }
    }
}

void CanvasView::zoomOut() {
    for (int i = static_cast<int>(std::size(kZoomLevels)) - 1; i >= 0; --i) {
        if (kZoomLevels[i] < m_renderOpts.zoom * 0.9999) {
            setZoom(kZoomLevels[i]);
            return;
        }
    }
}

void CanvasView::zoomActualSize() {
    setZoom(1.0);
}

void CanvasView::zoomToWindow() {
    if (!m_doc) return;
    zoomToRect(QRectF(0, 0, m_doc->width(), m_doc->height()));
    if (m_renderOpts.zoom > 1.0) {
        // Small images are shown at 100% rather than blown up
        const QSize vp = viewportSize();
        applyView(1.0, QPointF((vp.width() - m_doc->width()) / 2.0, (vp.height() - m_doc->height()) / 2.0));
    }
}

void CanvasView::zoomToRect(const QRectF& docRect) {
    if (!m_doc || docRect.isEmpty()) return;
    const QSize vp = viewportSize();
    const int availW = vp.width() - kFitMargin;
    const int availH = vp.height() - kFitMargin;
    if (availW <= 0 || availH <= 0) {
        m_fitPending = true; // retried from resizeEvent
        return;
    }
    m_fitPending = false;

    const double fit = std::clamp(std::min(availW / docRect.width(), availH / docRect.height()), kMinZoom, kMaxZoom);
    const QPointF pan(vp.width() / 2.0 - docRect.center().x() * fit,
                      vp.height() / 2.0 - docRect.center().y() * fit);
    applyView(fit, pan);
}

void CanvasView::setPanOffset(const QPointF& offset) {
    const QPointF clamped = clampPanOffset(offset);
    if (clamped == m_renderOpts.panOffset) return;
    m_renderOpts.panOffset = clamped;
    updateScrollBars();
    update();
}

QPointF CanvasView::clampPanOffset(const QPointF& offset) const {
    if (!m_doc) return offset;

    const QSize vp = viewportSize();
    if (vp.width() <= 0 || vp.height() <= 0) return offset;

    // The image edge may be dragged at most to the centre of the viewport
    const qreal docW = m_doc->width() * m_renderOpts.zoom;
    const qreal docH = m_doc->height() * m_renderOpts.zoom;
    return QPointF(std::clamp(offset.x(), vp.width() / 2.0 - docW, vp.width() / 2.0),
                   std::clamp(offset.y(), vp.height() / 2.0 - docH, vp.height() / 2.0));
}

void CanvasView::updateScrollBars() {
    if (!m_doc) {
        m_hScroll->hide();
        m_vScroll->hide();
        return;
    }
    const QSize vp = viewportSize();
    const int r = rulerOffset();
    const int hThickness = m_hScroll->sizeHint().height();
    const int vThickness = m_vScroll->sizeHint().width();
    const qreal docW = m_doc->width() * m_renderOpts.zoom;
    const qreal docH = m_doc->height() * m_renderOpts.zoom;
    const bool needH = docW > vp.width();
    const bool needV = docH > vp.height();

    m_syncingScrollBars = true;
    auto sync = [](QScrollBar* bar, bool needed, qreal docExtent, int viewExtent, qreal pan) {
        bar->setVisible(needed);
        if (!needed) return;
        const qreal maxPan = viewExtent / 2.0;
        const qreal minPan = viewExtent / 2.0 - docExtent;
        bar->setProperty("maxPan", maxPan);
        bar->setRange(0, static_cast<int>(std::ceil(maxPan - minPan)));
        bar->setPageStep(viewExtent);
        bar->setSingleStep(std::max(1, viewExtent / 20));
        bar->setValue(static_cast<int>(std::lround(maxPan - pan)));
    };
    sync(m_hScroll, needH, docW, vp.width(), m_renderOpts.panOffset.x());
    sync(m_vScroll, needV, docH, vp.height(), m_renderOpts.panOffset.y());
    m_hScroll->setGeometry(r, height() - hThickness, width() - r - (needV ? vThickness : 0), hThickness);
    m_vScroll->setGeometry(width() - vThickness, r, vThickness, height() - r - (needH ? hThickness : 0));
    m_syncingScrollBars = false;
}

void CanvasView::togglePixelGrid(bool enabled) {
    m_renderOpts.showPixelGrid = enabled;
    update();
}

void CanvasView::toggleRulers(bool enabled) {
    m_renderOpts.showRulers = enabled;
    m_renderer->resize(viewportSize().width(), viewportSize().height());
    m_renderOpts.panOffset = clampPanOffset(m_renderOpts.panOffset);
    updateScrollBars();
    update();
}

void CanvasView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_renderer->resize(viewportSize().width(), viewportSize().height());
    if (m_fitPending && m_doc) {
        // The document was assigned before the view had a usable size (e.g. at startup)
        zoomToWindow();
    }
    m_renderOpts.panOffset = clampPanOffset(m_renderOpts.panOffset);
    updateScrollBars();
}

void CanvasView::drawRulers(QPainter& painter) {
    if (!m_renderOpts.showRulers || !m_doc) return;

    const int rW = m_rulerWidth;
    const double zoom = m_renderOpts.zoom;
    painter.save();

    painter.fillRect(0, 0, width(), rW, QColor(240, 240, 240));
    painter.fillRect(0, 0, rW, height(), QColor(240, 240, 240));
    painter.fillRect(0, 0, rW, rW, QColor(220, 220, 220));

    painter.setPen(QColor(160, 160, 160));
    painter.drawLine(0, rW - 1, width(), rW - 1);
    painter.drawLine(rW - 1, 0, rW - 1, height());

    painter.setPen(QColor(100, 100, 100));
    QFont f = painter.font();
    f.setPixelSize(9);
    painter.setFont(f);

    // Labelled major ticks roughly every 60 px on screen, with unlabelled ticks at a fifth of that.
    const int major = rulerStep(zoom, 60.0);
    const int minor = std::max(1, major / 5);

    const QPointF topLeftDoc = viewportToDoc(QPointF(rW, rW));
    const QPointF bottomRightDoc = viewportToDoc(QPointF(width(), height()));

    const int firstX = static_cast<int>(std::floor(topLeftDoc.x() / minor)) * minor;
    for (int docX = firstX; docX <= bottomRightDoc.x(); docX += minor) {
        const qreal vpX = rW + m_renderOpts.panOffset.x() + docX * zoom;
        if (vpX < rW) continue;
        const bool isMajor = docX % major == 0;
        painter.drawLine(QPointF(vpX, isMajor ? 2 : rW - 5), QPointF(vpX, rW - 1));
        if (isMajor) painter.drawText(QPointF(vpX + 2, 9), QString::number(docX));
    }

    const int firstY = static_cast<int>(std::floor(topLeftDoc.y() / minor)) * minor;
    for (int docY = firstY; docY <= bottomRightDoc.y(); docY += minor) {
        const qreal vpY = rW + m_renderOpts.panOffset.y() + docY * zoom;
        if (vpY < rW) continue;
        const bool isMajor = docY % major == 0;
        painter.drawLine(QPointF(isMajor ? 2 : rW - 5, vpY), QPointF(rW - 1, vpY));
        if (isMajor) {
            painter.save();
            painter.translate(9, vpY - 2);
            painter.rotate(-90);
            painter.drawText(0, 0, QString::number(docY));
            painter.restore();
        }
    }

    // Cursor position markers
    if (m_mouseInside) {
        painter.setPen(QPen(Qt::red, 1.0));
        if (m_lastMousePos.x() >= rW) painter.drawLine(m_lastMousePos.x(), 0, m_lastMousePos.x(), rW - 1);
        if (m_lastMousePos.y() >= rW) painter.drawLine(0, m_lastMousePos.y(), rW - 1, m_lastMousePos.y());
    }

    painter.restore();
}

bool CanvasView::activeToolUsesBrushOutline() const {
    switch (m_toolMgr->activeToolType()) {
        case ToolType::Paintbrush:
        case ToolType::Eraser:
        case ToolType::CloneStamp:
        case ToolType::Recolor:
            return true;
        default:
            return false;
    }
}

QRect CanvasView::brushOutlineRect(const QPoint& widgetPos) const {
    const int radius = static_cast<int>(std::ceil(m_toolMgr->context().brushWidth * m_renderOpts.zoom / 2.0)) + 3;
    return QRect(widgetPos.x() - radius, widgetPos.y() - radius, 2 * radius, 2 * radius);
}

// Shows the brush footprint at the cursor, like Paint.NET does for brush-type tools.
void CanvasView::drawBrushOutline(QPainter& painter) {
    if (!m_mouseInside || m_panning || m_pointReceiver || !activeToolUsesBrushOutline()) return;
    const qreal radius = m_toolMgr->context().brushWidth * m_renderOpts.zoom / 2.0;
    if (radius < 3.0) return; // smaller than the crosshair cursor itself

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);
    const QPointF center = QPointF(m_lastMousePos) + QPointF(0.5, 0.5);
    painter.setPen(QPen(QColor(255, 255, 255, 220), 2.0));
    painter.drawEllipse(center, radius, radius);
    painter.setPen(QPen(QColor(0, 0, 0, 220), 1.0));
    painter.drawEllipse(center, radius, radius);
    painter.restore();
}

void CanvasView::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    const int rOffset = rulerOffset();

    painter.save();
    painter.translate(rOffset, rOffset);

    if (m_doc) {
        RenderOptions opts = m_renderOpts;
        opts.devicePixelRatio = devicePixelRatioF();
        opts.exposedRect = event->rect().translated(-rOffset, -rOffset);
        m_renderer->render(painter, *m_doc, opts);

        // Tool overlay preview
        auto tool = m_toolMgr->activeTool();
        if (tool && !m_overlayProvider) {
            tool->drawOverlay(painter, m_renderOpts);
        }

        // Effect interactive overlay (e.g. crosshair, center point, radius)
        if (m_overlayProvider) {
            m_overlayProvider->drawCanvasOverlay(painter, m_renderOpts);
        }
    } else {
        painter.fillRect(0, 0, width() - rOffset, height() - rOffset, QColor(160, 160, 160));
    }
    painter.restore();

    if (m_doc) drawBrushOutline(painter);
    drawRulers(painter);
}

void CanvasView::updateToolCursor() {
    if (m_panning) {
        setCursor(Qt::ClosedHandCursor);
    } else if (m_spaceHeld) {
        setCursor(Qt::OpenHandCursor);
    } else if (m_pointReceiver) {
        setCursor(Qt::CrossCursor);
    } else {
        auto tool = m_toolMgr->activeTool();
        setCursor(tool ? tool->cursor() : Qt::ArrowCursor);
    }
}

void CanvasView::beginPan(const QPoint& pos) {
    m_panning = true;
    m_lastMousePos = pos;
    updateToolCursor();
}

void CanvasView::endPan() {
    m_panning = false;
    updateToolCursor();
}

void CanvasView::mousePressEvent(QMouseEvent* event) {
    setFocus();
    if (event->button() == Qt::MiddleButton || (event->modifiers() & Qt::AltModifier) || m_spaceHeld) {
        beginPan(event->pos());
        return;
    }

    if (!m_doc) return;

    if (m_pointReceiver && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)) {
        m_pickingActive = true;
        QPointF docPos = viewportToDoc(event->pos());
        m_pointReceiver->onCanvasPointPicked(docPos);
        update();
        return;
    }

    if (m_interactionBlocked) return;

    QPointF docPos = viewportToDoc(event->pos());
    auto tool = m_toolMgr->activeTool();
    if (tool) {
        tool->mousePress(event, m_doc.get(), docPos, m_toolMgr->context());
    }
    update();
}

void CanvasView::updateRulerMarkers() {
    if (!m_renderOpts.showRulers) return;
    update(0, 0, width(), m_rulerWidth);
    update(0, 0, m_rulerWidth, height());
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
    const QPoint previousPos = m_lastMousePos;
    m_lastMousePos = event->pos();
    m_mouseInside = true;

    if (m_panning) {
        const QPoint delta = event->pos() - previousPos;
        setPanOffset(m_renderOpts.panOffset + QPointF(delta.x(), delta.y()));
        return;
    }

    if (!m_doc) return;
    QPointF docPos = viewportToDoc(event->pos());
    emit cursorMoved(static_cast<int>(std::floor(docPos.x())), static_cast<int>(std::floor(docPos.y())));
    updateRulerMarkers();

    if (m_pointReceiver) {
        setCursor(Qt::CrossCursor);
        if (m_pickingActive && (event->buttons() & Qt::LeftButton)) {
            m_pointReceiver->onCanvasPointPicked(docPos);
            update();
        }
        return;
    }

    if (m_interactionBlocked) return;

    auto tool = m_toolMgr->activeTool();
    if (tool) {
        tool->mouseMove(event, m_doc.get(), docPos, m_toolMgr->context());
        if (!m_spaceHeld) setCursor(tool->cursor());
    }

    // Pixel edits repaint themselves through pixelsChanged(). What is left is overlay state:
    // a full repaint while dragging (tool previews), otherwise just the brush outline.
    if (event->buttons() != Qt::NoButton || !activeToolUsesBrushOutline()) {
        update();
    } else {
        update(brushOutlineRect(previousPos).united(brushOutlineRect(m_lastMousePos)));
    }
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_panning) {
        if (event->buttons() == Qt::NoButton) endPan();
        return;
    }

    if (m_pointReceiver && m_pickingActive) {
        m_pickingActive = false;
        setCursor(Qt::CrossCursor);
        return;
    }

    if (m_interactionBlocked) return;

    if (!m_doc) return;
    QPointF docPos = viewportToDoc(event->pos());
    auto tool = m_toolMgr->activeTool();
    if (tool) {
        tool->mouseRelease(event, m_doc.get(), docPos, m_toolMgr->context());
        setCursor(tool->cursor());
    }
    update();
}

void CanvasView::leaveEvent(QEvent* event) {
    m_mouseInside = false;
    update();
    QWidget::leaveEvent(event);
}

void CanvasView::wheelEvent(QWheelEvent* event) {
    const QPoint angle = event->angleDelta();
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom around the mouse position
        if (angle.y() == 0) return;
        const QPointF docPos = viewportToDoc(event->position());
        setZoom(m_renderOpts.zoom * std::pow(1.15, angle.y() / 120.0), docPos);
    } else if (event->modifiers() & Qt::ShiftModifier) {
        // Horizontal scroll with Shift+Scroll
        const qreal delta = angle.y() != 0 ? angle.y() : angle.x();
        setPanOffset(QPointF(m_renderOpts.panOffset.x() + delta / 2.0, m_renderOpts.panOffset.y()));
    } else {
        setPanOffset(m_renderOpts.panOffset + QPointF(angle.x() / 2.0, angle.y() / 2.0));
    }
}

void CanvasView::keyPressEvent(QKeyEvent* event) {
    if (!m_doc) return;
    auto tool = m_toolMgr->activeTool();
    if (tool) {
        event->ignore();
        tool->keyPress(event, m_doc.get(), m_toolMgr->context());
        if (event->isAccepted()) {
            update();
            return;
        }
    }

    if (m_toolMgr && m_toolMgr->handleKeyPress(event)) {
        event->accept();
        update();
        return;
    }

    // Hold Space to pan with the mouse, whatever tool is active
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceHeld = true;
        updateToolCursor();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void CanvasView::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat() && m_spaceHeld) {
        m_spaceHeld = false;
        updateToolCursor();
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void CanvasView::setPointReceiver(ICanvasPointReceiver* receiver) {
    m_pointReceiver = receiver;
    updateToolCursor();
    update();
}

void CanvasView::setOverlayProvider(ICanvasOverlayProvider* provider) {
    m_overlayProvider = provider;
    update();
}

void CanvasView::requestCanvasRepaint() {
    update();
}

void CanvasView::setInteractionBlocked(bool blocked) {
    m_interactionBlocked = blocked;
    if (blocked) {
        m_pickingActive = false;
    }
}

} // namespace pdn
