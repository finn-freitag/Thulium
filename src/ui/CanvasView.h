#pragma once

#include <QWidget>
#include <QTimer>
#include <QMap>
#include <memory>
#include "../core/Document.h"
#include "../rendering/IRenderer.h"
#include "../tools/ToolManager.h"
#include "../effects/ICanvasInteraction.h"

namespace pdn {

class CanvasView : public QWidget, public ICanvasInteractionBridge {
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

    void removeDocumentViewState(const Document* doc);

    IRenderer* renderer() { return m_renderer.get(); }

    // ICanvasInteractionBridge
    void setPointReceiver(ICanvasPointReceiver* receiver) override;
    ICanvasPointReceiver* pointReceiver() const override { return m_pointReceiver; }

    void setOverlayProvider(ICanvasOverlayProvider* provider) override;
    ICanvasOverlayProvider* overlayProvider() const override { return m_overlayProvider; }

    void requestCanvasRepaint() override;

    void setInteractionBlocked(bool blocked);
    bool isInteractionBlocked() const { return m_interactionBlocked; }

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
    QMap<const Document*, RenderOptions> m_viewStates;

    QTimer m_marchingAntsTimer;
    bool m_spacePanning = false;
    QPoint m_lastMousePos;
    int m_rulerWidth = 18;

    ICanvasPointReceiver* m_pointReceiver = nullptr;
    ICanvasOverlayProvider* m_overlayProvider = nullptr;
    bool m_pickingActive = false;
    bool m_interactionBlocked = false;
};

} // namespace pdn
