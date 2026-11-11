#pragma once

#include <QUndoCommand>
#include <QImage>
#include <memory>
#include "Document.h"

namespace pdn {

// The part of a layer image that an edit changed, before and after.
// Storing just this rectangle (instead of two full copies of the layer) keeps the undo
// history small: a brush stroke on a 6000x4000 layer costs kilobytes, not ~190 MB.
class ImagePatch {
public:
    ImagePatch() = default;
    // Captures the difference between before and after. If hint is valid, only that
    // rectangle is examined (callers that know what they touched avoid a full scan).
    ImagePatch(const QImage& before, const QImage& after, const QRect& hint = QRect());

    bool isEmpty() const { return m_rect.isEmpty() && m_fullBefore.isNull(); }
    QRect rect() const { return m_rect; }

    // Writes the before/after pixels into the layer and notifies the document.
    void applyBefore(Document* doc, int layerIndex) const { apply(doc, layerIndex, m_before, m_fullBefore); }
    void applyAfter(Document* doc, int layerIndex) const { apply(doc, layerIndex, m_after, m_fullAfter); }

private:
    void apply(Document* doc, int layerIndex, const QImage& patch, const QImage& full) const;

    QRect m_rect;
    QImage m_before;
    QImage m_after;
    // Fallback when the image size changed and a rectangle patch cannot describe the edit.
    QImage m_fullBefore;
    QImage m_fullAfter;
};

class LayerBitmapUndoCommand : public QUndoCommand {
public:
    // oldImage is the layer content before the edit; the layer's current content is the result.
    LayerBitmapUndoCommand(Document* doc, int layerIndex, const QImage& oldImage, const QString& text = "Paint");
    LayerBitmapUndoCommand(Document* doc, int layerIndex, const QImage& oldImage, const QRect& changedRect,
                           const QString& text);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    ImagePatch m_patch;
    bool m_firstRedo = true;
};

class LayerAddUndoCommand : public QUndoCommand {
public:
    LayerAddUndoCommand(Document* doc, std::shared_ptr<Layer> layer, int index, const QString& text = "Add Layer");

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    std::shared_ptr<Layer> m_layer;
    int m_index;
    bool m_firstRedo = true;
};

class LayerRemoveUndoCommand : public QUndoCommand {
public:
    LayerRemoveUndoCommand(Document* doc, int index, const QString& text = "Delete Layer");
    LayerRemoveUndoCommand(Document* doc, std::shared_ptr<Layer> removedLayer, int index, const QString& text = "Delete Layer");

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    std::shared_ptr<Layer> m_removedLayer;
    int m_index;
    bool m_firstRedo = true;
};

class LayerMergeDownUndoCommand : public QUndoCommand {
public:
    LayerMergeDownUndoCommand(Document* doc, int bottomIndex,
                             const QImage& oldBottomImage,
                             std::shared_ptr<Layer> removedTopLayer,
                             const QString& text = "Merge Layer Down");

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_bottomIndex;
    QImage m_oldBottomImage;
    std::shared_ptr<Layer> m_removedTopLayer;
    bool m_firstRedo = true;
};

class LayerMoveUndoCommand : public QUndoCommand {
public:
    LayerMoveUndoCommand(Document* doc, int fromIndex, int toIndex, const QString& text = "Move Layer");

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_fromIndex;
    int m_toIndex;
    bool m_firstRedo = true;
};

class LayerPropertyUndoCommand : public QUndoCommand {
public:
    LayerPropertyUndoCommand(Document* doc, int index,
                            const QString& oldName, const QString& newName,
                            uint8_t oldOpacity, uint8_t newOpacity,
                            BlendMode oldMode, BlendMode newMode,
                            bool oldVisible, bool newVisible,
                            const QString& text = "Layer Properties");

    void undo() override;
    void redo() override;

private:
    void apply(const QString& name, uint8_t opacity, BlendMode mode, bool visible);

    Document* m_doc;
    int m_index;
    QString m_oldName, m_newName;
    uint8_t m_oldOpacity, m_newOpacity;
    BlendMode m_oldMode, m_newMode;
    bool m_oldVisible, m_newVisible;
};

// Flips one layer; a flip is its own inverse, so no pixel data is stored.
class LayerFlipUndoCommand : public QUndoCommand {
public:
    LayerFlipUndoCommand(Document* doc, int index, Qt::Orientation orientation);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_index;
    Qt::Orientation m_orientation;
    bool m_firstRedo = true;
};

// Resize / canvas size / crop: swaps complete before and after states.
class ImageGeometryUndoCommand : public QUndoCommand {
public:
    ImageGeometryUndoCommand(Document* doc, const GeometrySnapshot& before, const GeometrySnapshot& after,
                             const QString& text);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    GeometrySnapshot m_before;
    GeometrySnapshot m_after;
    bool m_firstRedo = true;
};

// Whole-image flip / rotation: undone by applying the inverse transform.
class ImageTransformUndoCommand : public QUndoCommand {
public:
    ImageTransformUndoCommand(Document* doc, ImageTransform op);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    ImageTransform m_op;
    bool m_firstRedo = true;
};

class FlattenUndoCommand : public QUndoCommand {
public:
    FlattenUndoCommand(Document* doc,
                      const QList<std::shared_ptr<Layer>>& oldLayers,
                      int oldActiveIndex,
                      const QList<std::shared_ptr<Layer>>& newLayers,
                      int newActiveIndex,
                      const QString& text = "Flatten Image");

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    QList<std::shared_ptr<Layer>> m_oldLayers;
    int m_oldActiveIndex;
    QList<std::shared_ptr<Layer>> m_newLayers;
    int m_newActiveIndex;
    bool m_firstRedo = true;
};

class MetadataUndoCommand : public QUndoCommand {
public:
    MetadataUndoCommand(Document* doc, const Metadata& oldMeta, const Metadata& newMeta, const QString& text = "Edit Metadata");

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    Metadata m_oldMeta;
    Metadata m_newMeta;
    bool m_firstRedo = true;
};

class SelectionUndoCommand : public QUndoCommand {
public:
    SelectionUndoCommand(Document* doc, const QRegion& oldRegion, const QRegion& newRegion, const QString& text = "Selection");

    void undo() override;
    void redo() override;

private:
    void apply(const QRegion& region);

    Document* m_doc;
    QRegion m_oldRegion;
    QRegion m_newRegion;
    bool m_firstRedo = true;
};

class PasteFloatingUndoCommand : public QUndoCommand {
public:
    PasteFloatingUndoCommand(Document* doc,
                             const QImage& pastedImage,
                             const QPoint& pastePos,
                             int layerIndex,
                             const QRegion& oldSelectionRegion,
                             const QString& text = "Paste");

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    QImage m_pastedImage;
    QPoint m_pastePos;
    int m_layerIndex;
    QRegion m_oldSelectionRegion;
    bool m_firstRedo = true;
};

// Lifting the selected pixels off the layer into a floating selection, plus its first transform.
class LiftFloatingUndoCommand : public QUndoCommand {
public:
    // preLiftLayerImage is the layer before lifting; the layer's current content is the post-lift state.
    LiftFloatingUndoCommand(Document* doc,
                            int layerIndex,
                            const QImage& preLiftLayerImage,
                            const QImage& floatingImage,
                            const QTransform& newTransform,
                            const QRegion& initialSelection,
                            const QRegion& newSelection,
                            const QString& text = "Move Pixels");

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    ImagePatch m_layerPatch;
    FloatingSelectionState m_liftedState;
    QTransform m_newTransform;
    QRegion m_initialSelection;
    QRegion m_newSelection;
    bool m_firstRedo = true;
};

class TransformFloatingUndoCommand : public QUndoCommand {
public:
    TransformFloatingUndoCommand(Document* doc,
                                 const QTransform& oldTransform,
                                 const QTransform& newTransform,
                                 const QRegion& oldSelection,
                                 const QRegion& newSelection,
                                 const QString& text = "Move Pixels");

    void undo() override;
    void redo() override;

private:
    void apply(const QTransform& transform, const QRegion& selection);

    Document* m_doc;
    QTransform m_oldTransform;
    QTransform m_newTransform;
    QRegion m_oldSelection;
    QRegion m_newSelection;
    bool m_firstRedo = true;
};

// Merging the floating selection back into its layer.
class BakeFloatingUndoCommand : public QUndoCommand {
public:
    // preBakeImage is the layer before baking; the layer's current content is the baked result.
    BakeFloatingUndoCommand(Document* doc,
                            int layerIndex,
                            const QImage& preBakeImage,
                            const QRect& affectedRect,
                            const FloatingSelectionState& floatingState,
                            const QRegion& preSelection,
                            const QRegion& postSelection,
                            const QString& text = "Deselect");

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    ImagePatch m_layerPatch;
    FloatingSelectionState m_floatingState;
    QRegion m_preSelection;
    QRegion m_postSelection;
    bool m_firstRedo = true;
};

} // namespace pdn
