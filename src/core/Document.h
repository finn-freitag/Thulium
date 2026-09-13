#pragma once

#include <QObject>
#include <QList>
#include <QUndoStack>
#include <QTransform>
#include <memory>
#include "Layer.h"
#include "Selection.h"
#include "Resampling.h"

namespace pdn {

class Document : public QObject {
    Q_OBJECT
public:
    Document(int width, int height, QObject* parent = nullptr);
    Document(const QImage& initialImage, QObject* parent = nullptr);
    ~Document() override = default;

    int width() const { return m_width; }
    int height() const { return m_height; }
    QSize size() const { return QSize(m_width, m_height); }
    double dpi() const { return m_dpi; }
    void setDpi(double dpi) { m_dpi = dpi; }

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString& path);
    QString fileName() const;
    bool isModified() const;

    QUndoStack* undoStack() { return &m_undoStack; }
    void clearLayers();

    // Layers
    int layerCount() const { return m_layers.size(); }
    std::shared_ptr<Layer> layer(int index) const;
    std::shared_ptr<Layer> activeLayer() const;
    int activeLayerIndex() const { return m_activeLayerIndex; }
    void setActiveLayerIndex(int index);

    std::shared_ptr<Layer> addLayer(const QString& name = QString());
    void insertLayer(int index, std::shared_ptr<Layer> layer);
    std::shared_ptr<Layer> removeLayer(int index);
    std::shared_ptr<Layer> duplicateLayer(int index);
    bool moveLayer(int fromIndex, int toIndex);
    bool mergeLayerDown(int index);

    // Selection
    Selection& selection() { return m_selection; }
    const Selection& selection() const { return m_selection; }
    void clearSelection();

    // Floating selection (hidden temporary layer for paste & moving selected pixels)
    bool hasFloatingSelection() const { return m_hasFloatingSelection; }
    const QImage& floatingImage() const { return m_floatingImage; }
    const QImage& originalFloatingImage() const { return m_originalFloatingImage; }
    const QPointF& floatingOffset() const { return m_floatingOffset; }
    const QTransform& floatingTransform() const { return m_floatingTransform; }
    bool isFloatingLifted() const { return m_floatingIsLifted; }

    void createFloatingSelection(const QImage& image, const QPointF& offset, bool isLifted, const QString& actionName = "Paste");
    void liftSelectionToFloating();
    void moveFloatingSelection(const QPointF& delta);
    void setFloatingTransform(const QTransform& transform);
    void bakeFloatingSelection();
    void cancelFloatingSelection();
    void discardFloatingSelection();

    // Canvas / Image transformation
    void resizeCanvas(int newWidth, int newHeight, Qt::Alignment anchor = Qt::AlignCenter, bool recordUndo = true);
    void resizeImage(int newWidth, int newHeight, ResampleAlgorithm algo = ResampleAlgorithm::Bicubic, bool recordUndo = true);
    void resizeImage(int newWidth, int newHeight, Qt::TransformationMode mode);
    void crop(const QRect& rect, bool recordUndo = true);
    void flipHorizontal(bool recordUndo = true);
    void flipVertical(bool recordUndo = true);
    void rotate90CW(bool recordUndo = true);
    void rotate90CCW(bool recordUndo = true);
    void rotate180(bool recordUndo = true);
    void flatten(bool recordUndo = true);

    void setDocumentDimensions(int width, int height);
    void setLayers(const QList<std::shared_ptr<Layer>>& layers, int activeIndex);

    // Composite all visible layers into a single QImage
    QImage composite() const;
    void compositeInto(QImage& target) const;

signals:
    void documentChanged();
    void layerCountChanged();
    void activeLayerChanged(int index);
    void layerPropertiesChanged(int index);
    void selectionChanged();

private:
    int m_width;
    int m_height;
    double m_dpi = 96.0;
    QString m_filePath;
    QList<std::shared_ptr<Layer>> m_layers;
    int m_activeLayerIndex = 0;
    Selection m_selection;
    QUndoStack m_undoStack;

    // Floating selection state
    bool m_hasFloatingSelection = false;
    QImage m_floatingImage;
    QImage m_originalFloatingImage;
    QTransform m_floatingTransform;
    QPointF m_floatingOffset;
    QImage m_floatingSnapshot;
    int m_floatingLayerIndex = 0;
    bool m_floatingIsLifted = false;
    QString m_floatingActionName;
};

} // namespace pdn
