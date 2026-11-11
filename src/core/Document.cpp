#include "Document.h"
#include "History.h"
#include "Parallel.h"
#include <QFileInfo>
#include <QPainter>
#include <algorithm>
#include <cstring>

namespace pdn {

ImageTransform inverseTransform(ImageTransform op) {
    switch (op) {
        case ImageTransform::Rotate90CW:  return ImageTransform::Rotate90CCW;
        case ImageTransform::Rotate90CCW: return ImageTransform::Rotate90CW;
        default:                          return op; // flips and 180° are their own inverse
    }
}

QString imageTransformName(ImageTransform op) {
    switch (op) {
        case ImageTransform::FlipHorizontal: return "Flip Horizontal";
        case ImageTransform::FlipVertical:   return "Flip Vertical";
        case ImageTransform::Rotate90CW:     return "Rotate 90° Clockwise";
        case ImageTransform::Rotate90CCW:    return "Rotate 90° Counter-Clockwise";
        case ImageTransform::Rotate180:      return "Rotate 180°";
    }
    return "Transform";
}

Document::Document(int width, int height, QObject* parent)
    : Document(width, height, false, parent) {
}

Document::Document(int width, int height, bool transparentBackground, QObject* parent)
    : QObject(parent), m_width(width), m_height(height), m_selection(width, height) {
    // Default background layer
    auto bg = std::make_shared<Layer>(width, height, "Background", true);
    if (transparentBackground) {
        bg->clear();
    }
    m_layers.append(bg);
    m_activeLayerIndex = 0;
    connect(this, &Document::documentChanged, this, [this]() { emit pixelsChanged(bounds()); });
}

Document::Document(const QImage& initialImage, QObject* parent)
    : QObject(parent),
      m_width(initialImage.width()),
      m_height(initialImage.height()),
      m_selection(initialImage.width(), initialImage.height()) {
    auto bg = std::make_shared<Layer>(initialImage, "Background", true);
    m_layers.append(bg);
    m_activeLayerIndex = 0;
    connect(this, &Document::documentChanged, this, [this]() { emit pixelsChanged(bounds()); });
}

void Document::notifyPixelsChanged(const QRect& rect) {
    const QRect clipped = rect.intersected(bounds());
    if (!clipped.isEmpty()) {
        emit pixelsChanged(clipped);
    }
}

void Document::setFilePath(const QString& path) {
    m_filePath = path;
    emit documentChanged();
}

void Document::setTitle(const QString& title) {
    m_customTitle = title;
    emit documentChanged();
}

QString Document::fileName() const {
    if (!m_filePath.isEmpty()) {
        return QFileInfo(m_filePath).fileName();
    }
    if (!m_customTitle.isEmpty()) {
        return m_customTitle;
    }
    return "Untitled";
}

bool Document::isModified() const {
    return !m_undoStack.isClean();
}

void Document::setMetadata(const Metadata& meta, bool recordUndo) {
    if (m_metadata == meta) return;
    if (recordUndo) {
        Metadata oldMeta = m_metadata;
        setMetadataInternal(meta);
        m_undoStack.push(new MetadataUndoCommand(this, oldMeta, meta));
    } else {
        setMetadataInternal(meta);
    }
}

void Document::setMetadataInternal(const Metadata& meta) {
    m_metadata = meta;
    emit metadataChanged();
    emit documentChanged();
}

std::shared_ptr<Layer> Document::layer(int index) const {
    if (index >= 0 && index < m_layers.size()) {
        return m_layers.at(index);
    }
    return nullptr;
}

std::shared_ptr<Layer> Document::activeLayer() const {
    return layer(m_activeLayerIndex);
}

void Document::setActiveLayerIndex(int index) {
    if (index >= 0 && index < m_layers.size() && index != m_activeLayerIndex) {
        if (m_hasFloatingSelection) {
            bakeFloatingSelection();
        }
        m_activeLayerIndex = index;
        emit activeLayerChanged(m_activeLayerIndex);
    }
}

std::shared_ptr<Layer> Document::addLayer(const QString& name, bool recordUndo) {
    QString layerName = name;
    if (layerName.isEmpty()) {
        layerName = QString("Layer %1").arg(m_layers.size() + 1);
    }
    auto newLayer = std::make_shared<Layer>(m_width, m_height, layerName, false);
    // Insert above currently active layer
    int insertIndex = m_activeLayerIndex + 1;
    if (insertIndex > m_layers.size()) insertIndex = m_layers.size();
    m_layers.insert(insertIndex, newLayer);
    m_activeLayerIndex = insertIndex;

    emit layerCountChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentChanged();

    if (recordUndo) {
        m_undoStack.push(new LayerAddUndoCommand(this, newLayer, insertIndex, "Add Layer"));
    }
    return newLayer;
}

void Document::insertLayer(int index, std::shared_ptr<Layer> layer, bool /*recordUndo*/) {
    if (!layer) return;
    int idx = std::clamp(index, 0, static_cast<int>(m_layers.size()));
    m_layers.insert(idx, layer);
    m_activeLayerIndex = idx;
    emit layerCountChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentChanged();
}

void Document::clearLayers() {
    m_layers.clear();
    m_activeLayerIndex = 0;
    emit layerCountChanged();
    emit activeLayerChanged(0);
    emit documentChanged();
}

std::shared_ptr<Layer> Document::removeLayer(int index, bool recordUndo) {
    if (m_layers.size() <= 1 || index < 0 || index >= m_layers.size()) {
        return nullptr; // Cannot remove last layer
    }
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }
    auto removed = m_layers.takeAt(index);
    if (m_activeLayerIndex >= m_layers.size()) {
        m_activeLayerIndex = m_layers.size() - 1;
    }
    emit layerCountChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentChanged();

    if (recordUndo) {
        m_undoStack.push(new LayerRemoveUndoCommand(this, removed, index, "Delete Layer"));
    }
    return removed;
}

std::shared_ptr<Layer> Document::duplicateLayer(int index, bool recordUndo) {
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }
    auto src = layer(index);
    if (!src) return nullptr;
    auto dup = src->clone();
    dup->setName(src->name() + " (Copy)");
    dup->setIsBackground(false);
    m_layers.insert(index + 1, dup);
    m_activeLayerIndex = index + 1;

    emit layerCountChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentChanged();

    if (recordUndo) {
        m_undoStack.push(new LayerAddUndoCommand(this, dup, index + 1, "Duplicate Layer"));
    }
    return dup;
}

bool Document::moveLayer(int fromIndex, int toIndex, bool recordUndo) {
    if (fromIndex < 0 || fromIndex >= m_layers.size() ||
        toIndex < 0 || toIndex >= m_layers.size() ||
        fromIndex == toIndex) {
        return false;
    }
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }
    m_layers.move(fromIndex, toIndex);
    m_activeLayerIndex = toIndex;
    emit layerCountChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentChanged();

    if (recordUndo) {
        QString text = (toIndex > fromIndex) ? "Move Layer Up" : "Move Layer Down";
        m_undoStack.push(new LayerMoveUndoCommand(this, fromIndex, toIndex, text));
    }
    return true;
}

bool Document::mergeLayerDown(int index, bool recordUndo) {
    if (index <= 0 || index >= m_layers.size()) {
        return false; // Can't merge down bottom layer
    }
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }
    auto topLayer = m_layers.at(index);
    auto bottomLayer = m_layers.at(index - 1);

    // Implicitly shared: becomes the "before" copy when the blend below detaches the layer image.
    const QImage oldBottomImage = bottomLayer->image();

    const Layer& top = *topLayer;
    Layer& bottom = *bottomLayer;
    bottom.bits(); // detach once, before the worker threads touch the scanlines
    const int rowWidth = std::min(top.width(), bottom.width());
    parallelRows(std::min(top.height(), bottom.height()), [&](int y) {
        blendImages(bottom.scanLine(y), top.scanLine(y), rowWidth, top.blendMode(), top.opacity());
    });

    // Remove top layer
    m_layers.removeAt(index);
    m_activeLayerIndex = index - 1;

    emit layerCountChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentChanged();

    if (recordUndo) {
        m_undoStack.push(new LayerMergeDownUndoCommand(this, index - 1, oldBottomImage, topLayer, "Merge Layer Down"));
    }
    return true;
}

FloatingSelectionState Document::floatingSelectionState() const {
    FloatingSelectionState state;
    state.hasFloating = m_hasFloatingSelection;
    state.image = m_floatingImage;
    state.originalImage = m_originalFloatingImage;
    state.transform = m_floatingTransform;
    state.offset = m_floatingOffset;
    state.snapshot = m_floatingSnapshot;
    state.snapshotOrigin = m_floatingSnapshotOrigin;
    state.layerIndex = m_floatingLayerIndex;
    state.isLifted = m_floatingIsLifted;
    state.actionName = m_floatingActionName;
    return state;
}

void Document::setFloatingSelectionState(const FloatingSelectionState& state) {
    m_hasFloatingSelection = state.hasFloating;
    m_floatingImage = state.image;
    m_originalFloatingImage = state.originalImage;
    m_floatingTransform = state.transform;
    m_floatingOffset = state.offset;
    m_floatingSnapshot = state.snapshot;
    m_floatingSnapshotOrigin = state.snapshotOrigin;
    m_floatingLayerIndex = state.layerIndex;
    m_floatingIsLifted = state.isLifted;
    m_floatingActionName = state.actionName;
    emit documentChanged();
}

void Document::resetFloatingState() {
    m_hasFloatingSelection = false;
    m_floatingImage = QImage();
    m_originalFloatingImage = QImage();
    m_floatingSnapshot = QImage();
    m_floatingTransform = QTransform();
    m_floatingIsLifted = false;
}

QRect Document::floatingBounds() const {
    if (!m_hasFloatingSelection) return QRect();
    const QImage& img = !m_originalFloatingImage.isNull() ? m_originalFloatingImage : m_floatingImage;
    if (img.isNull()) return QRect();
    // One extra pixel covers the antialiased edge of a rotated/scaled image.
    return m_floatingTransform.mapRect(QRectF(img.rect())).toAlignedRect().adjusted(-1, -1, 1, 1);
}

void Document::createFloatingSelection(const QImage& image, const QPointF& offset, bool isLifted, const QString& actionName) {
    if (m_hasFloatingSelection) {
        bakeFloatingSelection(false);
    }
    auto layer = activeLayer();
    if (!layer) return;

    // QImage is implicitly shared, so these do not duplicate the pixel data.
    m_floatingImage = image.convertToFormat(QImage::Format_ARGB32);
    m_originalFloatingImage = m_floatingImage;
    m_floatingOffset = offset;
    m_floatingTransform = QTransform::fromTranslate(offset.x(), offset.y());
    m_floatingLayerIndex = m_activeLayerIndex;
    m_floatingSnapshot = QImage(); // nothing was removed from the layer, so nothing to restore
    m_floatingIsLifted = isLifted;
    m_floatingActionName = actionName;
    m_hasFloatingSelection = true;
    notifyPixelsChanged(floatingBounds());
}

void Document::liftSelectionToFloating() {
    if (m_hasFloatingSelection || m_selection.isEmpty()) return;
    auto layer = activeLayer();
    if (!layer) return;

    const QRect srcRect = m_selection.region().boundingRect().intersected(bounds());
    if (srcRect.isEmpty()) return;

    m_floatingSnapshot = layer->image().copy(srcRect);
    m_floatingSnapshotOrigin = srcRect.topLeft();
    m_floatingLayerIndex = m_activeLayerIndex;
    m_floatingIsLifted = true;
    m_floatingActionName = "Move Pixels";

    // Copy the selected pixels, strictly clipped to the selection shape
    m_floatingImage = QImage(srcRect.size(), QImage::Format_ARGB32);
    m_floatingImage.fill(Qt::transparent);
    {
        QPainter p(&m_floatingImage);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.setClipRegion(m_selection.region().translated(-srcRect.topLeft()));
        p.drawImage(QPoint(0, 0), layer->image(), srcRect);
    }

    // ...and leave a transparent hole behind on the layer
    {
        QPainter p(&layer->image());
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.setClipRegion(m_selection.region());
        p.fillRect(srcRect, Qt::transparent);
    }

    m_originalFloatingImage = m_floatingImage;
    m_floatingOffset = srcRect.topLeft();
    m_floatingTransform = QTransform::fromTranslate(srcRect.x(), srcRect.y());
    m_hasFloatingSelection = true;

    notifyPixelsChanged(srcRect);
}

void Document::moveFloatingSelection(const QPointF& delta) {
    if (!m_hasFloatingSelection) return;
    const QRect before = floatingBounds();
    m_floatingOffset += delta;
    m_floatingTransform = m_floatingTransform * QTransform::fromTranslate(delta.x(), delta.y());
    m_selection.translate(delta.x(), delta.y());
    emit selectionChanged();
    notifyPixelsChanged(before.united(floatingBounds()));
}

void Document::setFloatingTransform(const QTransform& transform) {
    if (!m_hasFloatingSelection) return;
    const QRect before = floatingBounds();
    m_floatingTransform = transform;
    m_floatingOffset = m_floatingTransform.map(QPointF(0, 0));
    notifyPixelsChanged(before.united(floatingBounds()));
}

void Document::bakeFloatingSelection(bool recordUndo, const QString& actionName) {
    if (!m_hasFloatingSelection) return;
    auto layer = this->layer(m_floatingLayerIndex);
    if (!layer) {
        resetFloatingState();
        emit documentChanged();
        return;
    }

    const QRect affected = floatingBounds().intersected(bounds());
    const QImage preBakeImage = layer->image(); // shared; detached by the painter below
    const QImage& srcImg = !m_originalFloatingImage.isNull() ? m_originalFloatingImage : m_floatingImage;
    if (!srcImg.isNull()) {
        QPainter p(&layer->image());
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setTransform(m_floatingTransform);
        p.drawImage(0, 0, srcImg);
    }

    const FloatingSelectionState state = floatingSelectionState();
    const QRegion preSelection = m_selection.region();
    const int layerIndex = m_floatingLayerIndex;
    resetFloatingState();

    const QString act = actionName.isEmpty() ? "Deselect" : actionName;
    if (act == "Deselect") {
        m_selection.clear();
        emit selectionChanged();
    }

    if (recordUndo) {
        m_undoStack.push(new BakeFloatingUndoCommand(this, layerIndex, preBakeImage, affected, state,
                                                     preSelection, m_selection.region(), act));
    }

    notifyPixelsChanged(affected);
}

void Document::cancelFloatingSelection() {
    if (!m_hasFloatingSelection) return;
    auto layer = this->layer(m_floatingLayerIndex);
    if (layer && m_floatingIsLifted && !m_floatingSnapshot.isNull()) {
        QPainter p(&layer->image());
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(m_floatingSnapshotOrigin, m_floatingSnapshot);
    }
    resetFloatingState();
    m_selection.clear();
    emit selectionChanged();
    emit documentChanged();
}

void Document::discardFloatingSelection() {
    if (!m_hasFloatingSelection) return;
    const QRect affected = floatingBounds();
    resetFloatingState();
    notifyPixelsChanged(affected);
}

void Document::clearSelection() {
    if (m_hasFloatingSelection) {
        bakeFloatingSelection(true, "Deselect");
        return;
    }
    if (m_selection.isEmpty()) return;
    m_selection.clear();
    emit selectionChanged();
}

void Document::selectAll() {
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }
    m_selection.selectAll(m_width, m_height);
    emit selectionChanged();
}

void Document::setDocumentDimensions(int width, int height) {
    m_width = width;
    m_height = height;
    m_selection.setDimensions(width, height);
}

void Document::setLayers(const QList<std::shared_ptr<Layer>>& layers, int activeIndex) {
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }
    m_layers = layers;
    m_activeLayerIndex = std::clamp(activeIndex, 0, std::max(0, static_cast<int>(m_layers.size()) - 1));
    emit layerCountChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentChanged();
}

// --- Canvas geometry ---

GeometrySnapshot Document::geometrySnapshot() const {
    GeometrySnapshot snap;
    snap.width = m_width;
    snap.height = m_height;
    snap.selection = m_selection.region();
    snap.layerImages.reserve(m_layers.size());
    for (const auto& l : m_layers) {
        snap.layerImages.append(l->image());
    }
    return snap;
}

void Document::restoreGeometry(const GeometrySnapshot& snapshot) {
    setDocumentDimensions(snapshot.width, snapshot.height);
    for (int i = 0; i < m_layers.size() && i < snapshot.layerImages.size(); ++i) {
        m_layers[i]->setImage(snapshot.layerImages[i]);
    }
    m_selection.setRegion(snapshot.selection);
    emit selectionChanged();
    emit documentChanged();
}

void Document::pushGeometryUndo(const GeometrySnapshot& before, const QString& text) {
    m_undoStack.push(new ImageGeometryUndoCommand(this, before, geometrySnapshot(), text));
}

void Document::resizeCanvas(int newWidth, int newHeight, Qt::Alignment anchor, bool recordUndo) {
    if (newWidth <= 0 || newHeight <= 0) return;
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }
    const GeometrySnapshot before = geometrySnapshot();

    int dx = 0;
    int dy = 0;
    if (anchor & Qt::AlignRight) dx = newWidth - m_width;
    else if (anchor & Qt::AlignHCenter) dx = (newWidth - m_width) / 2;
    if (anchor & Qt::AlignBottom) dy = newHeight - m_height;
    else if (anchor & Qt::AlignVCenter) dy = (newHeight - m_height) / 2;

    for (auto& l : m_layers) {
        QImage newImg(newWidth, newHeight, QImage::Format_ARGB32);
        newImg.fill(l->isBackground() ? Qt::white : Qt::transparent);
        QPainter p(&newImg);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(dx, dy, l->image());
        p.end();
        l->setImage(newImg);
    }

    setDocumentDimensions(newWidth, newHeight);
    m_selection.clear();
    emit selectionChanged();
    emit documentChanged();

    if (recordUndo) pushGeometryUndo(before, "Canvas Size");
}

void Document::resizeImage(int newWidth, int newHeight, ResampleAlgorithm algo, bool recordUndo) {
    if (newWidth <= 0 || newHeight <= 0) return;
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }
    const GeometrySnapshot before = geometrySnapshot();

    for (auto& l : m_layers) {
        l->setImage(Resampling::resample(l->image(), newWidth, newHeight, algo));
    }
    setDocumentDimensions(newWidth, newHeight);
    m_selection.clear();
    emit selectionChanged();
    emit documentChanged();

    if (recordUndo) pushGeometryUndo(before, "Resize");
}

void Document::resizeImage(int newWidth, int newHeight, Qt::TransformationMode mode) {
    resizeImage(newWidth, newHeight,
                (mode == Qt::FastTransformation ? ResampleAlgorithm::NearestNeighbor : ResampleAlgorithm::Bilinear),
                true);
}

void Document::crop(const QRect& rect, bool recordUndo) {
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }
    const QRect validRect = rect.intersected(bounds());
    if (validRect.isEmpty()) return;
    const GeometrySnapshot before = geometrySnapshot();

    for (auto& l : m_layers) {
        l->crop(validRect);
    }
    setDocumentDimensions(validRect.width(), validRect.height());
    m_selection.clear();
    emit selectionChanged();
    emit documentChanged();

    if (recordUndo) pushGeometryUndo(before, "Crop to Selection");
}

void Document::cropToSelection(bool recordUndo) {
    if (m_hasFloatingSelection) {
        bakeFloatingSelection(true, "Finish Move Pixels");
    }
    if (m_selection.isEmpty()) return;
    const QRect cropRect = m_selection.region().boundingRect().intersected(bounds());
    if (cropRect.isEmpty()) return;
    const GeometrySnapshot before = geometrySnapshot();

    // Everything inside the bounding box but outside the selected shape becomes transparent.
    const QRegion outside = QRegion(cropRect).subtracted(m_selection.region()).translated(-cropRect.topLeft());
    for (auto& l : m_layers) {
        l->crop(cropRect);
        if (!outside.isEmpty()) {
            QPainter p(&l->image());
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            p.setClipRegion(outside);
            p.fillRect(l->image().rect(), Qt::transparent);
        }
    }
    setDocumentDimensions(cropRect.width(), cropRect.height());
    m_selection.clear();
    emit selectionChanged();
    emit documentChanged();

    if (recordUndo) pushGeometryUndo(before, "Crop to Selection");
}

void Document::applyImageTransform(ImageTransform op) {
    for (auto& l : m_layers) {
        switch (op) {
            case ImageTransform::FlipHorizontal: l->flipHorizontal(); break;
            case ImageTransform::FlipVertical:   l->flipVertical(); break;
            case ImageTransform::Rotate90CW:     l->rotate90CW(); break;
            case ImageTransform::Rotate90CCW:    l->rotate90CCW(); break;
            case ImageTransform::Rotate180:      l->rotate180(); break;
        }
    }

    // The selection follows the pixels it was drawn around: run its mask through the same transform.
    QRegion newSelection;
    if (!m_selection.isEmpty()) {
        Layer maskLayer(m_selection.mask(m_width, m_height).convertToFormat(QImage::Format_ARGB32));
        switch (op) {
            case ImageTransform::FlipHorizontal: maskLayer.flipHorizontal(); break;
            case ImageTransform::FlipVertical:   maskLayer.flipVertical(); break;
            case ImageTransform::Rotate90CW:     maskLayer.rotate90CW(); break;
            case ImageTransform::Rotate90CCW:    maskLayer.rotate90CCW(); break;
            case ImageTransform::Rotate180:      maskLayer.rotate180(); break;
        }
        newSelection = Selection::regionFromMask(maskLayer.image().convertToFormat(QImage::Format_Grayscale8));
    }
    if (op == ImageTransform::Rotate90CW || op == ImageTransform::Rotate90CCW) {
        setDocumentDimensions(m_height, m_width);
    }
    m_selection.setRegion(newSelection);
    emit selectionChanged();
    emit documentChanged();
}

static void transformWithUndo(Document* doc, ImageTransform op, bool recordUndo) {
    if (doc->hasFloatingSelection()) {
        doc->bakeFloatingSelection();
    }
    doc->applyImageTransform(op);
    if (recordUndo) {
        doc->undoStack()->push(new ImageTransformUndoCommand(doc, op));
    }
}

void Document::flipHorizontal(bool recordUndo) { transformWithUndo(this, ImageTransform::FlipHorizontal, recordUndo); }
void Document::flipVertical(bool recordUndo)   { transformWithUndo(this, ImageTransform::FlipVertical, recordUndo); }
void Document::rotate90CW(bool recordUndo)     { transformWithUndo(this, ImageTransform::Rotate90CW, recordUndo); }
void Document::rotate90CCW(bool recordUndo)    { transformWithUndo(this, ImageTransform::Rotate90CCW, recordUndo); }
void Document::rotate180(bool recordUndo)      { transformWithUndo(this, ImageTransform::Rotate180, recordUndo); }

void Document::flipLayer(int index, Qt::Orientation orientation, bool recordUndo) {
    auto l = layer(index);
    if (!l) return;
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }
    if (orientation == Qt::Horizontal) l->flipHorizontal();
    else l->flipVertical();
    emit documentChanged();
    if (recordUndo) {
        m_undoStack.push(new LayerFlipUndoCommand(this, index, orientation));
    }
}

void Document::flatten(bool recordUndo) {
    if (m_layers.size() <= 1 && !m_hasFloatingSelection) return;
    if (m_hasFloatingSelection) {
        bakeFloatingSelection();
    }

    QList<std::shared_ptr<Layer>> oldLayers = m_layers;
    int oldActiveIndex = m_activeLayerIndex;

    QImage compositeImage = composite();
    m_layers.clear();
    auto flat = std::make_shared<Layer>(compositeImage, "Background", true);
    m_layers.append(flat);
    m_activeLayerIndex = 0;
    emit layerCountChanged();
    emit activeLayerChanged(0);
    emit documentChanged();

    if (recordUndo) {
        m_undoStack.push(new FlattenUndoCommand(this, oldLayers, oldActiveIndex, m_layers, 0, "Flatten Image"));
    }
}

// --- Compositing ---

void Document::compositeRect(const QRect& rect, uint32_t* dst, qsizetype bytesPerLine) const {
    const int w = rect.width();
    const int h = rect.height();
    auto dstRow = [&](int row) {
        return reinterpret_cast<uint32_t*>(reinterpret_cast<uchar*>(dst) + row * bytesPerLine);
    };
    for (int row = 0; row < h; ++row) {
        std::memset(dstRow(row), 0, static_cast<size_t>(w) * sizeof(uint32_t));
    }

    for (int i = 0; i < m_layers.size(); ++i) {
        const Layer& layer = *m_layers.at(i);
        if (!layer.isVisible()) continue;

        // Layers normally match the canvas size; clip defensively in case one does not.
        const QRect src = rect.intersected(layer.image().rect());
        if (!src.isEmpty() && layer.opacity() > 0) {
            for (int y = src.top(); y <= src.bottom(); ++y) {
                blendImages(dstRow(y - rect.top()) + (src.left() - rect.left()),
                            layer.scanLine(y) + src.left(), src.width(),
                            layer.blendMode(), layer.opacity());
            }
        }

        // The floating selection sits directly above the layer it belongs to
        if (m_hasFloatingSelection && i == m_floatingLayerIndex && rect.intersects(floatingBounds())) {
            const QImage& srcImg = !m_originalFloatingImage.isNull() ? m_originalFloatingImage : m_floatingImage;
            QImage view(reinterpret_cast<uchar*>(dst), w, h, bytesPerLine, QImage::Format_ARGB32);
            QPainter p(&view);
            p.setOpacity(layer.opacity() / 255.0);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.translate(-rect.topLeft());
            p.setTransform(m_floatingTransform, true);
            p.drawImage(0, 0, srcImg);
        }
    }
}

QImage Document::composite() const {
    QImage result;
    compositeInto(result);
    return result;
}

void Document::compositeInto(QImage& target) const {
    if (target.width() != m_width || target.height() != m_height || target.format() != QImage::Format_ARGB32) {
        target = QImage(m_width, m_height, QImage::Format_ARGB32);
    }
    if (target.isNull()) return;
    uchar* bits = target.bits(); // detach before handing rows to worker threads
    const qsizetype bpl = target.bytesPerLine();
    parallelFor(0, m_height, 64, [&](int y0, int y1) {
        compositeRect(QRect(0, y0, m_width, y1 - y0), reinterpret_cast<uint32_t*>(bits + y0 * bpl), bpl);
    });
}

QRgb Document::compositePixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= m_width || y >= m_height) return 0;
    uint32_t px = 0;
    compositeRect(QRect(x, y, 1, 1), &px, sizeof(px));
    return px;
}

QImage Document::thumbnail(const QSize& maxSize) const {
    if (m_width <= 0 || m_height <= 0 || maxSize.isEmpty()) return QImage();
    const QSize size = QSize(m_width, m_height).scaled(maxSize, Qt::KeepAspectRatio).expandedTo(QSize(1, 1));

    // Point-sample each layer down to thumbnail size first, then blend the tiny images.
    // That costs O(thumbnail pixels) per layer instead of a full-resolution composite.
    QImage result(size, QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    for (int i = 0; i < m_layers.size(); ++i) {
        const Layer& layer = *m_layers.at(i);
        if (!layer.isVisible() || layer.opacity() == 0) continue;
        QImage small = layer.image().scaled(size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        if (m_hasFloatingSelection && i == m_floatingLayerIndex) {
            const QImage& srcImg = !m_originalFloatingImage.isNull() ? m_originalFloatingImage : m_floatingImage;
            QPainter p(&small);
            p.scale(static_cast<qreal>(size.width()) / m_width, static_cast<qreal>(size.height()) / m_height);
            p.setTransform(m_floatingTransform, true);
            p.drawImage(0, 0, srcImg);
        }
        for (int y = 0; y < size.height(); ++y) {
            blendImages(reinterpret_cast<uint32_t*>(result.scanLine(y)),
                        reinterpret_cast<const uint32_t*>(small.constScanLine(y)),
                        size.width(), layer.blendMode(), layer.opacity());
        }
    }
    return result;
}

} // namespace pdn
