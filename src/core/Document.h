#pragma once

#include <QObject>
#include <QList>
#include <QUndoStack>
#include <QTransform>
#include <memory>
#include "Layer.h"
#include "Selection.h"
#include "Resampling.h"
#include "Metadata.h"

namespace pdn {

// Reversible whole-image transforms. Undo applies the inverse instead of storing pixel copies.
enum class ImageTransform {
    FlipHorizontal,
    FlipVertical,
    Rotate90CW,
    Rotate90CCW,
    Rotate180
};

ImageTransform inverseTransform(ImageTransform op);
QString imageTransformName(ImageTransform op);

// Everything a canvas geometry change (resize, crop, canvas size) can alter.
// The images are implicitly shared with the layers, so capturing a snapshot is cheap.
struct GeometrySnapshot {
    int width = 0;
    int height = 0;
    QList<QImage> layerImages;
    QRegion selection;
};

struct FloatingSelectionState {
    bool hasFloating = false;
    QImage image;
    QImage originalImage;
    QTransform transform;
    QPointF offset;
    // Pixels that were under a lifted selection (restored on cancel), and where they belong.
    QImage snapshot;
    QPoint snapshotOrigin;
    int layerIndex = 0;
    bool isLifted = false;
    QString actionName;
};

class Document : public QObject {
    Q_OBJECT
public:
    Document(int width, int height, QObject* parent = nullptr);
    Document(int width, int height, bool transparentBackground, QObject* parent = nullptr);
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
    QString customTitle() const { return m_customTitle; }
    void setTitle(const QString& title);
    bool isModified() const;

    const Metadata& metadata() const { return m_metadata; }
    void setMetadata(const Metadata& meta, bool recordUndo = true);
    void setMetadataInternal(const Metadata& meta);

    QUndoStack* undoStack() { return &m_undoStack; }
    void clearLayers();

    // Layers
    int layerCount() const { return m_layers.size(); }
    std::shared_ptr<Layer> layer(int index) const;
    std::shared_ptr<Layer> activeLayer() const;
    int activeLayerIndex() const { return m_activeLayerIndex; }
    void setActiveLayerIndex(int index);

    std::shared_ptr<Layer> addLayer(const QString& name = QString(), bool recordUndo = true);
    void insertLayer(int index, std::shared_ptr<Layer> layer, bool recordUndo = false);
    std::shared_ptr<Layer> removeLayer(int index, bool recordUndo = true);
    std::shared_ptr<Layer> duplicateLayer(int index, bool recordUndo = true);
    bool moveLayer(int fromIndex, int toIndex, bool recordUndo = true);
    bool mergeLayerDown(int index, bool recordUndo = true);

    // Selection
    Selection& selection() { return m_selection; }
    const Selection& selection() const { return m_selection; }
    void clearSelection();
    void selectAll();

    // Floating selection (hidden temporary layer for paste & moving selected pixels)
    bool hasFloatingSelection() const { return m_hasFloatingSelection; }
    const QImage& floatingImage() const { return m_floatingImage; }
    const QImage& originalFloatingImage() const { return m_originalFloatingImage; }
    const QPointF& floatingOffset() const { return m_floatingOffset; }
    const QTransform& floatingTransform() const { return m_floatingTransform; }
    bool isFloatingLifted() const { return m_floatingIsLifted; }

    FloatingSelectionState floatingSelectionState() const;
    void setFloatingSelectionState(const FloatingSelectionState& state);

    void createFloatingSelection(const QImage& image, const QPointF& offset, bool isLifted, const QString& actionName = "Paste");
    void liftSelectionToFloating();
    void moveFloatingSelection(const QPointF& delta);
    void setFloatingTransform(const QTransform& transform);
    // Document-space bounding box of the transformed floating image (empty if none).
    QRect floatingBounds() const;
    void bakeFloatingSelection(bool recordUndo = true, const QString& actionName = "Deselect");
    void cancelFloatingSelection();
    void discardFloatingSelection();

    // Canvas / Image transformation
    void resizeCanvas(int newWidth, int newHeight, Qt::Alignment anchor = Qt::AlignCenter, bool recordUndo = true);
    void resizeImage(int newWidth, int newHeight, ResampleAlgorithm algo = ResampleAlgorithm::Bicubic, bool recordUndo = true);
    void resizeImage(int newWidth, int newHeight, Qt::TransformationMode mode);
    void crop(const QRect& rect, bool recordUndo = true);
    // Crops to the selection's bounding box and clears everything outside the selected shape.
    void cropToSelection(bool recordUndo = true);
    void flipHorizontal(bool recordUndo = true);
    void flipVertical(bool recordUndo = true);
    void rotate90CW(bool recordUndo = true);
    void rotate90CCW(bool recordUndo = true);
    void rotate180(bool recordUndo = true);
    void flatten(bool recordUndo = true);

    // Single-layer transforms (Layers menu)
    void flipLayer(int index, Qt::Orientation orientation, bool recordUndo = true);

    // Low-level state access used by undo commands and file loaders (no undo recording)
    void applyImageTransform(ImageTransform op);
    GeometrySnapshot geometrySnapshot() const;
    void restoreGeometry(const GeometrySnapshot& snapshot);
    void setDocumentDimensions(int width, int height);
    void setLayers(const QList<std::shared_ptr<Layer>>& layers, int activeIndex);

    // Composite all visible layers (and the floating selection) into straight-alpha ARGB32.
    QImage composite() const;
    void compositeInto(QImage& target) const;
    // Composites only rect into a caller-provided buffer of rect.height() rows of rect.width() pixels.
    void compositeRect(const QRect& rect, uint32_t* dst, qsizetype bytesPerLine) const;
    QRgb compositePixel(int x, int y) const;
    // Fast, point-sampled preview that fits within bounds (for tab and layer thumbnails).
    QImage thumbnail(const QSize& bounds) const;

    // Call after modifying layer pixels inside rect. Cheaper than documentChanged() because
    // views only recomposite and repaint that rectangle.
    void notifyPixelsChanged(const QRect& rect);

signals:
    // Anything about the document changed (structure, properties, or all pixels).
    // Always followed by pixelsChanged() for the full canvas.
    void documentChanged();
    // The composited result changed inside rect (document coordinates).
    void pixelsChanged(const QRect& rect);
    void metadataChanged();
    void layerCountChanged();
    void activeLayerChanged(int index);
    void layerPropertiesChanged(int index);
    void selectionChanged();

private:
    QRect bounds() const { return QRect(0, 0, m_width, m_height); }
    void resetFloatingState();
    void pushGeometryUndo(const GeometrySnapshot& before, const QString& text);

    int m_width;
    int m_height;
    double m_dpi = 96.0;
    QString m_filePath;
    QString m_customTitle;
    Metadata m_metadata;
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
    QPoint m_floatingSnapshotOrigin;
    int m_floatingLayerIndex = 0;
    bool m_floatingIsLifted = false;
    QString m_floatingActionName;
};

} // namespace pdn
