#pragma once

#include <QWidget>
#include <QTimer>
#include <memory>
#include "../core/Document.h"
#include "../rendering/IRenderer.h"
#include "../tools/ToolManager.h"

namespace pdn {

class CanvasView : public QWidget {
    Q_OBJECT
public:
    explicit CanvasView(ToolManager* toolMgr, QWidget* parent = nullptr);
    ~CanvasView() override = default;

    void setDocument(std::shared_ptr<Document> doc);
    std::shared_ptr<Document> document() const { return m_doc; }

    double zoom() const { return m_renderOpts.zoom; }
    void setZoom(double z, const QPointF& centerDocPos = QPointF(-1, -1));
    void zoomIn();
    void zoomOut();
    void zoomActualSize();
    void zoomToWindow();

    void setPanOffset(const QPointF& offset);
    QPointF panOffset() const { return m_renderOpts.panOffset; }
    QPointF clampPanOffset(const QPointF& offset) const;

    void togglePixelGrid(bool enabled);
    void toggleRulers(bool enabled);

    QPointF viewportToDoc(const QPointF& vpPos) const;
    QPointF docToViewport(const QPointF& docPos) const;

    IRenderer* renderer() { return m_renderer.get(); }

signals:
    void cursorMoved(int docX, int docY);
    void zoomChanged(double zoom);
    void statusMessageChanged(const QString& msg);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void drawRulers(QPainter& painter);

    std::shared_ptr<Document> m_doc;
    ToolManager* m_toolMgr;
    std::unique_ptr<IRenderer> m_renderer;
    RenderOptions m_renderOpts;

    QTimer m_marchingAntsTimer;
    bool m_spacePanning = false;
    QPoint m_lastMousePos;
    int m_rulerWidth = 18;
};

} // namespace pdn
