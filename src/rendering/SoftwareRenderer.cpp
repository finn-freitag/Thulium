#include "SoftwareRenderer.h"
#include <QPen>
#include <cmath>

namespace pdn {

SoftwareRenderer::SoftwareRenderer() {
    // 16x16 alternating checkerboard pattern
    m_checkerPixmap = QPixmap(16, 16);
    QPainter p(&m_checkerPixmap);
    p.fillRect(0, 0, 8, 8, QColor(255, 255, 255));
    p.fillRect(8, 0, 8, 8, QColor(220, 220, 220));
    p.fillRect(0, 8, 8, 8, QColor(220, 220, 220));
    p.fillRect(8, 8, 8, 8, QColor(255, 255, 255));
}

bool SoftwareRenderer::initialize() {
    return true;
}

void SoftwareRenderer::cleanup() {
}

void SoftwareRenderer::resize(int viewportWidth, int viewportHeight) {
    m_viewportWidth = viewportWidth;
    m_viewportHeight = viewportHeight;
}

void SoftwareRenderer::drawCheckerboard(QPainter& painter, const QRectF& docRect, double /*zoom*/) {
    painter.save();
    painter.setClipRect(docRect);
    painter.drawTiledPixmap(docRect, m_checkerPixmap);
    painter.restore();
}

void SoftwareRenderer::drawPixelGrid(QPainter& painter, const QRectF& docRect, double zoom) {
    if (zoom < 4.0) return;

    painter.save();
    QPen gridPen(QColor(180, 180, 180, 100), 1.0);
    painter.setPen(gridPen);

    int w = static_cast<int>(docRect.width() / zoom);
    int h = static_cast<int>(docRect.height() / zoom);

    for (int x = 0; x <= w; ++x) {
        qreal px = docRect.left() + x * zoom;
        painter.drawLine(QPointF(px, docRect.top()), QPointF(px, docRect.bottom()));
    }
    for (int y = 0; y <= h; ++y) {
        qreal py = docRect.top() + y * zoom;
        painter.drawLine(QPointF(docRect.left(), py), QPointF(docRect.right(), py));
    }
    painter.restore();
}

void SoftwareRenderer::drawSelection(QPainter& painter, const Selection& selection, double zoom, const QPointF& offset, int marchingAntsOffset) {
    if (selection.isEmpty()) return;

    painter.save();
    QTransform transform;
    transform.translate(offset.x(), offset.y());
    transform.scale(zoom, zoom);

    QPainterPath transformedPath = transform.map(selection.path());

    // Paint.NET marching ants: alternating black and white dashes
    QPen blackPen(Qt::black, 1.0, Qt::CustomDashLine);
    QList<qreal> dashes;
    dashes << 4.0 << 4.0;
    blackPen.setDashPattern(dashes);
    blackPen.setDashOffset(marchingAntsOffset);

    QPen whitePen(Qt::white, 1.0, Qt::CustomDashLine);
    whitePen.setDashPattern(dashes);
    whitePen.setDashOffset(marchingAntsOffset + 4.0);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(blackPen);
    painter.drawPath(transformedPath);
    painter.setPen(whitePen);
    painter.drawPath(transformedPath);

    painter.restore();
}

void SoftwareRenderer::render(QPainter& painter, const Document& doc, const RenderOptions& opts) {
    // 1. Fill viewport background with Paint.NET workspace dark gray
    painter.fillRect(0, 0, m_viewportWidth, m_viewportHeight, QColor(160, 160, 160));

    // 2. Compute document rectangle in viewport coordinates
    qreal docW = doc.width() * opts.zoom;
    qreal docH = doc.height() * opts.zoom;
    QRectF docRect(opts.panOffset.x(), opts.panOffset.y(), docW, docH);

    // 3. Draw drop shadow
    painter.save();
    painter.fillRect(docRect.adjusted(3, 3, 3, 3), QColor(0, 0, 0, 80));
    painter.restore();

    // 4. Draw checkerboard transparency
    drawCheckerboard(painter, docRect, opts.zoom);

    // 5. Draw document image
    QImage composite = doc.composite();
    painter.save();
    if (opts.zoom >= 4.0) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    } else {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    }
    painter.drawImage(docRect, composite);
    painter.restore();

    // 6. Draw pixel grid if enabled
    if (opts.showPixelGrid) {
        drawPixelGrid(painter, docRect, opts.zoom);
    }

    // 7. Draw canvas border
    painter.save();
    painter.setPen(QPen(QColor(80, 80, 80), 1.0));
    painter.drawRect(docRect);
    painter.restore();

    // 8. Draw selection marching ants
    drawSelection(painter, doc.selection(), opts.zoom, opts.panOffset, opts.marchingAntsOffset);
}

} // namespace pdn
