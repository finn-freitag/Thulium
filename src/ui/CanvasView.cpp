#include "CanvasView.h"
#include "../rendering/VulkanRenderer.h"
#include "../tools/ViewTools.h"
#include <QPainter>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>

namespace pdn {

CanvasView::CanvasView(ToolManager* toolMgr, QWidget* parent)
    : QWidget(parent), m_toolMgr(toolMgr) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Initialize renderer with Vulkan GPU acceleration
    m_renderer = std::make_unique<VulkanRenderer>();
    m_renderer->initialize();

    // Setup Marching Ants timer for selection
    connect(&m_marchingAntsTimer, &QTimer::timeout, this, [this]() {
        if (m_doc && !m_doc->selection().isEmpty()) {
            m_renderOpts.marchingAntsOffset = (m_renderOpts.marchingAntsOffset + 1) % 8;
            update();
        }
    });
    m_marchingAntsTimer.start(50);

    // Wire up Pan & Zoom tool callbacks
    auto panTool = std::dynamic_pointer_cast<PanTool>(m_toolMgr->tool(ToolType::Pan));
    if (panTool) {
        panTool->onPan = [this](const QPointF& delta) {
            m_renderOpts.panOffset += delta;
            update();
        };
    }
    auto zoomTool = std::dynamic_pointer_cast<ZoomTool>(m_toolMgr->tool(ToolType::Zoom));
    if (zoomTool) {
        zoomTool->onZoom = [this](bool zoomIn, const QPointF& center) {
            double factor = zoomIn ? 1.25 : 0.8;
            setZoom(m_renderOpts.zoom * factor, center);
        };
    }
}

void CanvasView::setDocument(std::shared_ptr<Document> doc) {
    m_doc = doc;
    if (m_doc) {
        connect(m_doc.get(), &Document::documentChanged, this, QOverload<>::of(&CanvasView::update));
        connect(m_doc.get(), &Document::selectionChanged, this, QOverload<>::of(&CanvasView::update));
        zoomToWindow();
    }
    update();
}

QPointF CanvasView::viewportToDoc(const QPointF& vpPos) const {
    qreal rx = vpPos.x() - (m_renderOpts.showRulers ? m_rulerWidth : 0);
    qreal ry = vpPos.y() - (m_renderOpts.showRulers ? m_rulerWidth : 0);
    qreal x = (rx - m_renderOpts.panOffset.x()) / m_renderOpts.zoom;
    qreal y = (ry - m_renderOpts.panOffset.y()) / m_renderOpts.zoom;
    return QPointF(x, y);
}

QPointF CanvasView::docToViewport(const QPointF& docPos) const {
    qreal rOffset = (m_renderOpts.showRulers ? m_rulerWidth : 0);
    qreal x = m_renderOpts.panOffset.x() + docPos.x() * m_renderOpts.zoom + rOffset;
    qreal y = m_renderOpts.panOffset.y() + docPos.y() * m_renderOpts.zoom + rOffset;
    return QPointF(x, y);
}

void CanvasView::setZoom(double z, const QPointF& centerDocPos) {
    double oldZoom = m_renderOpts.zoom;
    double newZoom = std::clamp(z, 0.05, 32.0);
    if (std::abs(newZoom - oldZoom) < 0.0001) return;

    QPointF center = centerDocPos;
    if (center.x() < 0 && m_doc) {
        center = QPointF(m_doc->width() / 2.0, m_doc->height() / 2.0);
    }

    // Zoom centered around center
    qreal rOffset = (m_renderOpts.showRulers ? m_rulerWidth : 0);
    qreal vpCenterX = m_renderOpts.panOffset.x() + center.x() * oldZoom;
    qreal vpCenterY = m_renderOpts.panOffset.y() + center.y() * oldZoom;

    m_renderOpts.zoom = newZoom;
    m_renderOpts.panOffset.setX(vpCenterX - center.x() * newZoom);
    m_renderOpts.panOffset.setY(vpCenterY - center.y() * newZoom);

    emit zoomChanged(m_renderOpts.zoom);
    update();
}

void CanvasView::zoomIn() {
    setZoom(m_renderOpts.zoom * 1.25);
}

void CanvasView::zoomOut() {
    setZoom(m_renderOpts.zoom * 0.8);
}

void CanvasView::zoomActualSize() {
    setZoom(1.0);
}

void CanvasView::zoomToWindow() {
    if (!m_doc) return;
    int rOffset = m_renderOpts.showRulers ? m_rulerWidth : 0;
    int availW = width() - rOffset - 40;
    int availH = height() - rOffset - 40;
    if (availW <= 0 || availH <= 0) return;

    double scaleX = static_cast<double>(availW) / m_doc->width();
    double scaleY = static_cast<double>(availH) / m_doc->height();
    double fitZoom = std::min(scaleX, scaleY);
    if (fitZoom > 1.0) fitZoom = 1.0;

    m_renderOpts.zoom = fitZoom;
    m_renderOpts.panOffset.setX((availW + 40 - m_doc->width() * fitZoom) / 2.0);
    m_renderOpts.panOffset.setY((availH + 40 - m_doc->height() * fitZoom) / 2.0);

    emit zoomChanged(m_renderOpts.zoom);
    update();
}

void CanvasView::setPanOffset(const QPointF& offset) {
    m_renderOpts.panOffset = offset;
    update();
}

void CanvasView::togglePixelGrid(bool enabled) {
    m_renderOpts.showPixelGrid = enabled;
    update();
}

void CanvasView::toggleRulers(bool enabled) {
    m_renderOpts.showRulers = enabled;
    update();
}

void CanvasView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    int rOffset = m_renderOpts.showRulers ? m_rulerWidth : 0;
    m_renderer->resize(width() - rOffset, height() - rOffset);
}

void CanvasView::drawRulers(QPainter& painter) {
    if (!m_renderOpts.showRulers || !m_doc) return;

    int rW = m_rulerWidth;
    painter.save();

    // Ruler backgrounds
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

    // Horizontal ruler ticks
    int step = 50;
    if (m_renderOpts.zoom >= 2.0) step = 10;
    else if (m_renderOpts.zoom <= 0.25) step = 200;

    for (int docX = 0; docX <= m_doc->width(); docX += step) {
        qreal vpX = rW + m_renderOpts.panOffset.x() + docX * m_renderOpts.zoom;
        if (vpX >= rW && vpX < width()) {
            painter.drawLine(QPointF(vpX, rW - 6), QPointF(vpX, rW - 1));
            if (docX % (step * 2) == 0) {
                painter.drawText(QPointF(vpX + 2, rW - 4), QString::number(docX));
            }
        }
    }

    // Vertical ruler ticks
    for (int docY = 0; docY <= m_doc->height(); docY += step) {
        qreal vpY = rW + m_renderOpts.panOffset.y() + docY * m_renderOpts.zoom;
        if (vpY >= rW && vpY < height()) {
            painter.drawLine(QPointF(rW - 6, vpY), QPointF(rW - 1, vpY));
            if (docY % (step * 2) == 0) {
                painter.save();
                painter.translate(rW - 4, vpY - 2);
                painter.rotate(-90);
                painter.drawText(0, 0, QString::number(docY));
                painter.restore();
            }
        }
    }

    // Cursor indicator ticks
    QPointF docPos = viewportToDoc(mapFromGlobal(QCursor::pos()));
    qreal curVpX = rW + m_renderOpts.panOffset.x() + docPos.x() * m_renderOpts.zoom;
    qreal curVpY = rW + m_renderOpts.panOffset.y() + docPos.y() * m_renderOpts.zoom;

    painter.setPen(QPen(Qt::red, 1.0));
    if (curVpX >= rW && curVpX < width()) {
        painter.drawLine(QPointF(curVpX, 0), QPointF(curVpX, rW - 1));
    }
    if (curVpY >= rW && curVpY < height()) {
        painter.drawLine(QPointF(0, curVpY), QPointF(rW - 1, curVpY));
    }

    painter.restore();
}

void CanvasView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    int rOffset = m_renderOpts.showRulers ? m_rulerWidth : 0;

    // Render workspace background & document through renderer
    painter.save();
    painter.translate(rOffset, rOffset);

    if (m_doc) {
        m_renderer->render(painter, *m_doc, m_renderOpts);

        // Tool overlay preview
        auto tool = m_toolMgr->activeTool();
        if (tool) {
            tool->drawOverlay(painter, m_renderOpts);
        }
    } else {
        painter.fillRect(0, 0, width() - rOffset, height() - rOffset, QColor(160, 160, 160));
    }
    painter.restore();

    // Draw rulers on top
    drawRulers(painter);
}

void CanvasView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton || (event->modifiers() & Qt::AltModifier)) {
        m_spacePanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (!m_doc) return;
    QPointF docPos = viewportToDoc(event->pos());
    auto tool = m_toolMgr->activeTool();
    if (tool) {
        tool->mousePress(event, m_doc.get(), docPos, m_toolMgr->context());
    }
    update();
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
    if (m_spacePanning) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        m_renderOpts.panOffset += QPointF(delta.x(), delta.y());
        update();
        return;
    }

    if (!m_doc) return;
    QPointF docPos = viewportToDoc(event->pos());
    int docX = static_cast<int>(docPos.x());
    int docY = static_cast<int>(docPos.y());
    emit cursorMoved(docX, docY);

    auto tool = m_toolMgr->activeTool();
    if (tool) {
        setCursor(tool->cursor());
        tool->mouseMove(event, m_doc.get(), docPos, m_toolMgr->context());
    }
    update();
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_spacePanning && (event->button() == Qt::MiddleButton || !(event->modifiers() & Qt::AltModifier))) {
        m_spacePanning = false;
        auto tool = m_toolMgr->activeTool();
        setCursor(tool ? tool->cursor() : Qt::ArrowCursor);
        return;
    }

    if (!m_doc) return;
    QPointF docPos = viewportToDoc(event->pos());
    auto tool = m_toolMgr->activeTool();
    if (tool) {
        tool->mouseRelease(event, m_doc.get(), docPos, m_toolMgr->context());
    }
    update();
}

void CanvasView::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom on mouse position
        QPointF docPos = viewportToDoc(event->position());
        double factor = (event->angleDelta().y() > 0) ? 1.15 : 0.85;
        setZoom(m_renderOpts.zoom * factor, docPos);
    } else {
        // Pan
        m_renderOpts.panOffset += QPointF(event->angleDelta().x() / 2.0, event->angleDelta().y() / 2.0);
        update();
    }
}

void CanvasView::keyPressEvent(QKeyEvent* event) {
    if (!m_doc) return;
    auto tool = m_toolMgr->activeTool();
    if (tool) {
        tool->keyPress(event, m_doc.get(), m_toolMgr->context());
        update();
    }
    QWidget::keyPressEvent(event);
}

} // namespace pdn
