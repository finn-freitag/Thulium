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
    m_doc->removeLayer(m_index);
}

void LayerAddUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->insertLayer(m_index, m_layer);
}

LayerRemoveUndoCommand::LayerRemoveUndoCommand(Document* doc, int index, const QString& text)
    : QUndoCommand(text), m_doc(doc), m_index(index) {
    m_removedLayer = m_doc->layer(index);
}

void LayerRemoveUndoCommand::undo() {
    if (m_removedLayer) {
        m_doc->insertLayer(m_index, m_removedLayer);
    }
}

void LayerRemoveUndoCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }
    m_doc->removeLayer(m_index);
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

} // namespace pdn
