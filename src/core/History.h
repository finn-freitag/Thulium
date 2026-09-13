#pragma once

#include <QUndoCommand>
#include <QImage>
#include <memory>
#include "Document.h"

namespace pdn {

class LayerBitmapUndoCommand : public QUndoCommand {
public:
    LayerBitmapUndoCommand(Document* doc, int layerIndex, const QImage& oldImage, const QString& text = "Paint");

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_layerIndex;
    QImage m_oldImage;
    QImage m_newImage;
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

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    std::shared_ptr<Layer> m_removedLayer;
    int m_index;
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
    Document* m_doc;
    int m_index;
    QString m_oldName, m_newName;
    uint8_t m_oldOpacity, m_newOpacity;
    BlendMode m_oldMode, m_newMode;
    bool m_oldVisible, m_newVisible;
};

} // namespace pdn
