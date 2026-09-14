#include "History.h"

namespace pdn {

LayerBitmapUndoCommand::LayerBitmapUndoCommand(Document* doc, int layerIndex, const QImage& oldImage, const QString& text)
    : QUndoCommand(text), m_doc(doc), m_layerIndex(layerIndex), m_oldImage(oldImage) {
    auto l = doc->layer(layerIndex);
    if (l) {
        m_newImage = l->image().copy();
    }
}

void LayerBitmapUndoCommand::undo() {
    auto l = m_doc->layer(m_layerIndex);
    if (l) {
        l->setImage(m_oldImage.copy());
        emit m_doc->documentChanged();
    }
}

void LayerBitmapUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    auto l = m_doc->layer(m_layerIndex);
    if (l) {
        l->setImage(m_newImage.copy());
        emit m_doc->documentChanged();
    }
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
        bottom->setImage(m_oldBottomImage.copy());
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

void LayerPropertyUndoCommand::undo() {
    auto l = m_doc->layer(m_index);
    if (l) {
        l->setName(m_oldName);
        l->setOpacity(m_oldOpacity);
        l->setBlendMode(m_oldMode);
        l->setVisible(m_oldVisible);
        emit m_doc->layerPropertiesChanged(m_index);
        emit m_doc->documentChanged();
    }
}

void LayerPropertyUndoCommand::redo() {
    auto l = m_doc->layer(m_index);
    if (l) {
        l->setName(m_newName);
        l->setOpacity(m_newOpacity);
        l->setBlendMode(m_newMode);
        l->setVisible(m_newVisible);
        emit m_doc->layerPropertiesChanged(m_index);
        emit m_doc->documentChanged();
    }
}

ImageGeometryUndoCommand::ImageGeometryUndoCommand(Document* doc,
                                                   int oldWidth, int oldHeight,
                                                   const QList<QImage>& oldImages,
                                                   const QPainterPath& oldSelectionPath,
                                                   int newWidth, int newHeight,
                                                   const QList<QImage>& newImages,
                                                   const QPainterPath& newSelectionPath,
                                                   const QString& text)
    : QUndoCommand(text), m_doc(doc),
      m_oldWidth(oldWidth), m_oldHeight(oldHeight), m_oldImages(oldImages), m_oldSelectionPath(oldSelectionPath),
      m_newWidth(newWidth), m_newHeight(newHeight), m_newImages(newImages), m_newSelectionPath(newSelectionPath) {
}

void ImageGeometryUndoCommand::undo() {
    m_doc->setDocumentDimensions(m_oldWidth, m_oldHeight);
    for (int i = 0; i < m_doc->layerCount() && i < m_oldImages.size(); ++i) {
        m_doc->layer(i)->setImage(m_oldImages[i].copy());
    }
    m_doc->selection().setPath(m_oldSelectionPath);
    emit m_doc->selectionChanged();
    emit m_doc->documentChanged();
}

void ImageGeometryUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->setDocumentDimensions(m_newWidth, m_newHeight);
    for (int i = 0; i < m_doc->layerCount() && i < m_newImages.size(); ++i) {
        m_doc->layer(i)->setImage(m_newImages[i].copy());
    }
    m_doc->selection().setPath(m_newSelectionPath);
    emit m_doc->selectionChanged();
    emit m_doc->documentChanged();
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
    : QUndoCommand(text), m_doc(doc), m_oldRegion(oldRegion), m_newRegion(newRegion), m_firstRedo(true) {
}

void SelectionUndoCommand::undo() {
    m_doc->selection().setRegion(m_oldRegion);
    emit m_doc->selectionChanged();
    emit m_doc->documentChanged();
}

void SelectionUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->selection().setRegion(m_newRegion);
    emit m_doc->selectionChanged();
    emit m_doc->documentChanged();
}

} // namespace pdn
