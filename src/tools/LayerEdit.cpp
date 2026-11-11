#include "LayerEdit.h"
#include "../core/History.h"
#include <cstring>

namespace pdn {

namespace {
constexpr int kTileShift = 8; // 256 x 256 pixel tiles
constexpr int kTileSize = 1 << kTileShift;
}

bool LayerEdit::begin(Document* doc) {
    end();
    if (!doc) return false;
    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible() || layer->image().isNull()) return false;

    m_doc = doc;
    m_layer = layer;
    m_layerIndex = doc->activeLayerIndex();
    // Allocated but deliberately left uninitialised: pages are only touched as tiles get saved.
    m_original = QImage(layer->size(), QImage::Format_ARGB32);
    m_tilesPerRow = (layer->width() + kTileSize - 1) >> kTileShift;
    const int tileRows = (layer->height() + kTileSize - 1) >> kTileShift;
    m_tileSaved.assign(static_cast<size_t>(m_tilesPerRow) * tileRows, 0);
    m_dirty = QRect();
    return true;
}

void LayerEdit::prepare(const QRect& rect) {
    if (!m_doc) return;
    const QRect clipped = rect.intersected(m_layer->image().rect());
    if (clipped.isEmpty()) return;

    const Layer& source = *m_layer; // const access: must not detach the layer image
    for (int ty = clipped.top() >> kTileShift; ty <= clipped.bottom() >> kTileShift; ++ty) {
        for (int tx = clipped.left() >> kTileShift; tx <= clipped.right() >> kTileShift; ++tx) {
            uint8_t& saved = m_tileSaved[static_cast<size_t>(ty) * m_tilesPerRow + tx];
            if (saved) continue;
            saved = 1;
            const int x0 = tx << kTileShift;
            const int y0 = ty << kTileShift;
            const int w = std::min(kTileSize, source.width() - x0);
            const int h = std::min(kTileSize, source.height() - y0);
            for (int y = y0; y < y0 + h; ++y) {
                std::memcpy(reinterpret_cast<uint32_t*>(m_original.scanLine(y)) + x0, source.scanLine(y) + x0,
                            static_cast<size_t>(w) * 4);
            }
        }
    }
}

void LayerEdit::markDirty(const QRect& rect) {
    if (!m_doc) return;
    const QRect clipped = rect.intersected(m_layer->image().rect());
    if (clipped.isEmpty()) return;
    m_dirty = m_dirty.united(clipped);
    m_doc->notifyPixelsChanged(clipped);
}

void LayerEdit::restoreOriginal() {
    if (!m_doc || m_dirty.isEmpty()) return;
    // Tiles inside the bounding box that were never written still match the layer.
    prepare(m_dirty);
    const size_t rowBytes = static_cast<size_t>(m_dirty.width()) * 4;
    for (int y = m_dirty.top(); y <= m_dirty.bottom(); ++y) {
        std::memcpy(m_layer->scanLine(y) + m_dirty.left(), originalRow(y) + m_dirty.left(), rowBytes);
    }
    m_doc->notifyPixelsChanged(m_dirty);
    m_dirty = QRect();
}

void LayerEdit::commit(const QString& actionName) {
    if (!m_doc) return;
    if (!m_dirty.isEmpty()) {
        prepare(m_dirty); // make the whole bounding box of the "before" image valid
        m_doc->undoStack()->push(new LayerBitmapUndoCommand(m_doc, m_layerIndex, m_original, m_dirty, actionName));
    }
    end();
}

void LayerEdit::cancel() {
    restoreOriginal();
    end();
}

void LayerEdit::end() {
    m_doc = nullptr;
    m_layer.reset();
    m_layerIndex = -1;
    m_original = QImage();
    m_tileSaved.clear();
    m_dirty = QRect();
}

} // namespace pdn
