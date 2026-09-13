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
    Document* m_doc;
    int m_index;
    QString m_oldName, m_newName;
    uint8_t m_oldOpacity, m_newOpacity;
    BlendMode m_oldMode, m_newMode;
    bool m_oldVisible, m_newVisible;
};

class ImageGeometryUndoCommand : public QUndoCommand {
public:
    ImageGeometryUndoCommand(Document* doc,
                            int oldWidth, int oldHeight,
                            const QList<QImage>& oldImages,
                            const QPainterPath& oldSelectionPath,
                            int newWidth, int newHeight,
                            const QList<QImage>& newImages,
                            const QPainterPath& newSelectionPath,
                            const QString& text);

    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_oldWidth;
    int m_oldHeight;
    QList<QImage> m_oldImages;
    QPainterPath m_oldSelectionPath;
    int m_newWidth;
    int m_newHeight;
    QList<QImage> m_newImages;
    QPainterPath m_newSelectionPath;
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

} // namespace pdn
