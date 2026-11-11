#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <vector>
#include "../core/Document.h"

namespace pdn {

// One undoable edit of a document layer, shared by all pixel-editing tools.
//
// Usage: begin(), then for every change call prepare(rect) *before* writing to the layer and
// markDirty(rect) after, and finally commit() or cancel().
//
// The pre-edit pixels are saved lazily, tile by tile, the first time prepare() covers them.
// Starting an edit is therefore free and an edit only costs memory for the area it touches,
// instead of duplicating the whole layer (96 MB for a 6000x4000 image) on every mouse press.
class LayerEdit {
public:
    // Starts editing the active layer. Returns false if there is no visible layer to edit.
    bool begin(Document* doc);
    bool isActive() const { return m_doc != nullptr; }

    Layer* layer() const { return m_layer.get(); }
    int layerIndex() const { return m_layerIndex; }
    QRect dirtyRect() const { return m_dirty; }

    // Saves the original pixels of rect. Must be called before the layer is modified there,
    // and before reading originalRow() for rows/columns inside rect.
    void prepare(const QRect& rect);
    // Pre-edit pixels of row y. Only valid inside rectangles passed to prepare().
    const uint32_t* originalRow(int y) const {
        return reinterpret_cast<const uint32_t*>(m_original.constScanLine(y));
    }

    // Records that the layer changed inside rect and schedules a repaint of it.
    void markDirty(const QRect& rect);
    // Puts back the original pixels of everything touched so far (for live previews that redraw).
    void restoreOriginal();

    // Pushes the undo command (if anything changed) and ends the edit.
    void commit(const QString& actionName);
    // Reverts all changes and ends the edit.
    void cancel();

private:
    void end();

    Document* m_doc = nullptr;
    std::shared_ptr<Layer> m_layer;
    int m_layerIndex = -1;
    QImage m_original;                 // layer-sized; only saved tiles hold meaningful data
    std::vector<uint8_t> m_tileSaved;  // one flag per tile
    int m_tilesPerRow = 0;
    QRect m_dirty;
};

} // namespace pdn
