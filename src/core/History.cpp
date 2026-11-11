#include "History.h"
#include <QPainter>
#include <cstring>

namespace pdn {

namespace {

// Smallest rectangle (within area) containing every pixel that differs between a and b.
QRect diffRect(const QImage& a, const QImage& b, const QRect& area) {
    if (a.constBits() == b.constBits()) return QRect(); // still sharing the same buffer
    const size_t rowBytes = static_cast<size_t>(area.width()) * 4;
    auto rowDiffers = [&](int y) {
        return std::memcmp(a.constScanLine(y) + area.left() * 4, b.constScanLine(y) + area.left() * 4, rowBytes) != 0;
    };

    int top = area.top();
    while (top <= area.bottom() && !rowDiffers(top)) ++top;
    if (top > area.bottom()) return QRect();
    int bottom = area.bottom();
    while (bottom > top && !rowDiffers(bottom)) --bottom;

    int left = area.right();
    int right = area.left();
    for (int y = top; y <= bottom; ++y) {
        const auto* pa = reinterpret_cast<const uint32_t*>(a.constScanLine(y));
        const auto* pb = reinterpret_cast<const uint32_t*>(b.constScanLine(y));
        int x = area.left();
        while (x < left && pa[x] == pb[x]) ++x;
        left = std::min(left, x);
        x = area.right();
        while (x > right && pa[x] == pb[x]) --x;
        right = std::max(right, x);
    }
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

} // namespace

// --- ImagePatch ---

ImagePatch::ImagePatch(const QImage& before, const QImage& after, const QRect& hint) {
    if (before.size() != after.size() || before.format() != after.format() ||
        before.format() != QImage::Format_ARGB32) {
        m_fullBefore = before;
        m_fullAfter = after;
        return;
    }
    const QRect area = hint.isValid() ? hint.intersected(before.rect()) : before.rect();
    if (area.isEmpty()) return;
    m_rect = diffRect(before, after, area);
    if (!m_rect.isEmpty()) {
        m_before = before.copy(m_rect);
        m_after = after.copy(m_rect);
    }
}

void ImagePatch::apply(Document* doc, int layerIndex, const QImage& patch, const QImage& full) const {
    auto layer = doc->layer(layerIndex);
    if (!layer) return;
    if (!full.isNull()) {
        layer->setImage(full);
        emit doc->documentChanged();
        return;
    }
    if (m_rect.isEmpty() || !layer->image().rect().contains(m_rect)) return;
    const size_t rowBytes = static_cast<size_t>(m_rect.width()) * 4;
    for (int y = 0; y < m_rect.height(); ++y) {
        std::memcpy(layer->scanLine(m_rect.top() + y) + m_rect.left(), patch.constScanLine(y), rowBytes);
    }
    doc->notifyPixelsChanged(m_rect);
}

// --- LayerBitmapUndoCommand ---

LayerBitmapUndoCommand::LayerBitmapUndoCommand(Document* doc, int layerIndex, const QImage& oldImage, const QString& text)
    : LayerBitmapUndoCommand(doc, layerIndex, oldImage, QRect(), text) {
}

LayerBitmapUndoCommand::LayerBitmapUndoCommand(Document* doc, int layerIndex, const QImage& oldImage,
                                               const QRect& changedRect, const QString& text)
    : QUndoCommand(text), m_doc(doc), m_layerIndex(layerIndex) {
    if (auto l = doc->layer(layerIndex)) {
        m_patch = ImagePatch(oldImage, l->image(), changedRect);
    }
}

void LayerBitmapUndoCommand::undo() {
    m_patch.applyBefore(m_doc, m_layerIndex);
}

void LayerBitmapUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_patch.applyAfter(m_doc, m_layerIndex);
}

LayerAddUndoCommand::LayerAddUndoCommand(Document* doc, std::shared_ptr<Layer> layer, int index, const QString& text)
    : QUndoCommand(text), m_doc(doc), m_layer(layer), m_index(index) {
}

void LayerAddUndoCommand::undo() {
    m_doc->removeLayer(m_index, false);
}

void LayerAddUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->insertLayer(m_index, m_layer, false);
}

LayerRemoveUndoCommand::LayerRemoveUndoCommand(Document* doc, int index, const QString& text)
    : QUndoCommand(text), m_doc(doc), m_index(index) {
    m_removedLayer = m_doc->layer(index);
}

LayerRemoveUndoCommand::LayerRemoveUndoCommand(Document* doc, std::shared_ptr<Layer> removedLayer, int index, const QString& text)
    : QUndoCommand(text), m_doc(doc), m_removedLayer(removedLayer), m_index(index) {
}

void LayerRemoveUndoCommand::undo() {
    if (m_removedLayer) {
        m_doc->insertLayer(m_index, m_removedLayer, false);
    }
}

void LayerRemoveUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->removeLayer(m_index, false);
}

LayerMergeDownUndoCommand::LayerMergeDownUndoCommand(Document* doc, int bottomIndex,
                                                     const QImage& oldBottomImage,
                                                     std::shared_ptr<Layer> removedTopLayer,
                                                     const QString& text)
    : QUndoCommand(text), m_doc(doc), m_bottomIndex(bottomIndex),
      m_oldBottomImage(oldBottomImage), m_removedTopLayer(removedTopLayer) {
}

void LayerMergeDownUndoCommand::undo() {
    auto bottom = m_doc->layer(m_bottomIndex);
    if (bottom) {
        bottom->setImage(m_oldBottomImage);
    }
    m_doc->insertLayer(m_bottomIndex + 1, m_removedTopLayer, false);
    emit m_doc->documentChanged();
}

void LayerMergeDownUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->mergeLayerDown(m_bottomIndex + 1, false);
}

LayerMoveUndoCommand::LayerMoveUndoCommand(Document* doc, int fromIndex, int toIndex, const QString& text)
    : QUndoCommand(text), m_doc(doc), m_fromIndex(fromIndex), m_toIndex(toIndex) {
}

void LayerMoveUndoCommand::undo() {
    m_doc->moveLayer(m_toIndex, m_fromIndex, false);
}

void LayerMoveUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->moveLayer(m_fromIndex, m_toIndex, false);
}

LayerPropertyUndoCommand::LayerPropertyUndoCommand(Document* doc, int index,
                                                   const QString& oldName, const QString& newName,
                                                   uint8_t oldOpacity, uint8_t newOpacity,
                                                   BlendMode oldMode, BlendMode newMode,
                                                   bool oldVisible, bool newVisible,
                                                   const QString& text)
    : QUndoCommand(text), m_doc(doc), m_index(index),
      m_oldName(oldName), m_newName(newName),
      m_oldOpacity(oldOpacity), m_newOpacity(newOpacity),
      m_oldMode(oldMode), m_newMode(newMode),
      m_oldVisible(oldVisible), m_newVisible(newVisible) {
}

void LayerPropertyUndoCommand::apply(const QString& name, uint8_t opacity, BlendMode mode, bool visible) {
    auto l = m_doc->layer(m_index);
    if (!l) return;
    l->setName(name);
    l->setOpacity(opacity);
    l->setBlendMode(mode);
    l->setVisible(visible);
    emit m_doc->layerPropertiesChanged(m_index);
    emit m_doc->documentChanged();
}

void LayerPropertyUndoCommand::undo() { apply(m_oldName, m_oldOpacity, m_oldMode, m_oldVisible); }
void LayerPropertyUndoCommand::redo() { apply(m_newName, m_newOpacity, m_newMode, m_newVisible); }

LayerFlipUndoCommand::LayerFlipUndoCommand(Document* doc, int index, Qt::Orientation orientation)
    : QUndoCommand(orientation == Qt::Horizontal ? "Flip Layer Horizontal" : "Flip Layer Vertical"),
      m_doc(doc), m_index(index), m_orientation(orientation) {
}

void LayerFlipUndoCommand::undo() {
    m_doc->flipLayer(m_index, m_orientation, false);
}

void LayerFlipUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->flipLayer(m_index, m_orientation, false);
}

ImageGeometryUndoCommand::ImageGeometryUndoCommand(Document* doc, const GeometrySnapshot& before,
                                                   const GeometrySnapshot& after, const QString& text)
    : QUndoCommand(text), m_doc(doc), m_before(before), m_after(after) {
}

void ImageGeometryUndoCommand::undo() {
    m_doc->restoreGeometry(m_before);
}

void ImageGeometryUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->restoreGeometry(m_after);
}

ImageTransformUndoCommand::ImageTransformUndoCommand(Document* doc, ImageTransform op)
    : QUndoCommand(imageTransformName(op)), m_doc(doc), m_op(op) {
}

void ImageTransformUndoCommand::undo() {
    m_doc->applyImageTransform(inverseTransform(m_op));
}

void ImageTransformUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->applyImageTransform(m_op);
}

FlattenUndoCommand::FlattenUndoCommand(Document* doc,
                                       const QList<std::shared_ptr<Layer>>& oldLayers,
                                       int oldActiveIndex,
                                       const QList<std::shared_ptr<Layer>>& newLayers,
                                       int newActiveIndex,
                                       const QString& text)
    : QUndoCommand(text), m_doc(doc),
      m_oldLayers(oldLayers), m_oldActiveIndex(oldActiveIndex),
      m_newLayers(newLayers), m_newActiveIndex(newActiveIndex) {
}

void FlattenUndoCommand::undo() {
    m_doc->setLayers(m_oldLayers, m_oldActiveIndex);
}

void FlattenUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->setLayers(m_newLayers, m_newActiveIndex);
}

MetadataUndoCommand::MetadataUndoCommand(Document* doc, const Metadata& oldMeta, const Metadata& newMeta, const QString& text)
    : QUndoCommand(text), m_doc(doc), m_oldMeta(oldMeta), m_newMeta(newMeta), m_firstRedo(true) {
}

void MetadataUndoCommand::undo() {
    m_doc->setMetadataInternal(m_oldMeta);
}

void MetadataUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->setMetadataInternal(m_newMeta);
}

SelectionUndoCommand::SelectionUndoCommand(Document* doc, const QRegion& oldRegion, const QRegion& newRegion, const QString& text)
    : QUndoCommand(text), m_doc(doc), m_oldRegion(oldRegion), m_newRegion(newRegion) {
}

void SelectionUndoCommand::apply(const QRegion& region) {
    m_doc->selection().setRegion(region);
    emit m_doc->selectionChanged();
}

void SelectionUndoCommand::undo() { apply(m_oldRegion); }

void SelectionUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    apply(m_newRegion);
}

PasteFloatingUndoCommand::PasteFloatingUndoCommand(Document* doc,
                                                   const QImage& pastedImage,
                                                   const QPoint& pastePos,
                                                   int layerIndex,
                                                   const QRegion& oldSelectionRegion,
                                                   const QString& text)
    : QUndoCommand(text), m_doc(doc), m_pastedImage(pastedImage), m_pastePos(pastePos),
      m_layerIndex(layerIndex), m_oldSelectionRegion(oldSelectionRegion) {
}

void PasteFloatingUndoCommand::undo() {
    m_doc->discardFloatingSelection();
    m_doc->selection().setRegion(m_oldSelectionRegion);
    emit m_doc->selectionChanged();
}

void PasteFloatingUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->setActiveLayerIndex(m_layerIndex);
    m_doc->createFloatingSelection(m_pastedImage, m_pastePos, false, text());
    m_doc->selection().addRect(QRectF(m_pastePos, m_pastedImage.size()), SelectionCombineMode::Replace);
    emit m_doc->selectionChanged();
}

LiftFloatingUndoCommand::LiftFloatingUndoCommand(Document* doc,
                                                int layerIndex,
                                                const QImage& preLiftLayerImage,
                                                const QImage& floatingImage,
                                                const QTransform& newTransform,
                                                const QRegion& initialSelection,
                                                const QRegion& newSelection,
                                                const QString& text)
    : QUndoCommand(text), m_doc(doc), m_layerIndex(layerIndex),
      m_newTransform(newTransform), m_initialSelection(initialSelection), m_newSelection(newSelection) {
    const QRect liftedRect = initialSelection.boundingRect();
    if (auto l = doc->layer(layerIndex)) {
        m_layerPatch = ImagePatch(preLiftLayerImage, l->image(), liftedRect);
    }
    m_liftedState.hasFloating = true;
    m_liftedState.image = floatingImage;
    m_liftedState.originalImage = floatingImage;
    m_liftedState.snapshot = preLiftLayerImage.copy(liftedRect.intersected(preLiftLayerImage.rect()));
    m_liftedState.snapshotOrigin = liftedRect.intersected(preLiftLayerImage.rect()).topLeft();
    m_liftedState.layerIndex = layerIndex;
    m_liftedState.isLifted = true;
    m_liftedState.actionName = text;
}

void LiftFloatingUndoCommand::undo() {
    m_doc->discardFloatingSelection();
    m_layerPatch.applyBefore(m_doc, m_layerIndex);
    m_doc->selection().setRegion(m_initialSelection);
    emit m_doc->selectionChanged();
}

void LiftFloatingUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_layerPatch.applyAfter(m_doc, m_layerIndex);
    FloatingSelectionState state = m_liftedState;
    state.transform = m_newTransform;
    state.offset = m_newTransform.map(QPointF(0, 0));
    m_doc->setFloatingSelectionState(state);
    m_doc->selection().setRegion(m_newSelection);
    emit m_doc->selectionChanged();
}

TransformFloatingUndoCommand::TransformFloatingUndoCommand(Document* doc,
                                                           const QTransform& oldTransform,
                                                           const QTransform& newTransform,
                                                           const QRegion& oldSelection,
                                                           const QRegion& newSelection,
                                                           const QString& text)
    : QUndoCommand(text), m_doc(doc), m_oldTransform(oldTransform), m_newTransform(newTransform),
      m_oldSelection(oldSelection), m_newSelection(newSelection) {
}

void TransformFloatingUndoCommand::apply(const QTransform& transform, const QRegion& selection) {
    m_doc->setFloatingTransform(transform);
    m_doc->selection().setRegion(selection);
    emit m_doc->selectionChanged();
}

void TransformFloatingUndoCommand::undo() { apply(m_oldTransform, m_oldSelection); }

void TransformFloatingUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    apply(m_newTransform, m_newSelection);
}

BakeFloatingUndoCommand::BakeFloatingUndoCommand(Document* doc,
                                                 int layerIndex,
                                                 const QImage& preBakeImage,
                                                 const QRect& affectedRect,
                                                 const FloatingSelectionState& floatingState,
                                                 const QRegion& preSelection,
                                                 const QRegion& postSelection,
                                                 const QString& text)
    : QUndoCommand(text), m_doc(doc), m_layerIndex(layerIndex), m_floatingState(floatingState),
      m_preSelection(preSelection), m_postSelection(postSelection) {
    if (auto l = doc->layer(layerIndex)) {
        m_layerPatch = ImagePatch(preBakeImage, l->image(), affectedRect);
    }
}

void BakeFloatingUndoCommand::undo() {
    m_layerPatch.applyBefore(m_doc, m_layerIndex);
    m_doc->setFloatingSelectionState(m_floatingState);
    m_doc->selection().setRegion(m_preSelection);
    emit m_doc->selectionChanged();
}

void BakeFloatingUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->discardFloatingSelection();
    m_layerPatch.applyAfter(m_doc, m_layerIndex);
    m_doc->selection().setRegion(m_postSelection);
    emit m_doc->selectionChanged();
}

} // namespace pdn
