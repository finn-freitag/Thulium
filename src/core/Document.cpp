#include "Document.h"
#include <QFileInfo>
#include <QPainter>
#include <algorithm>

namespace pdn {

Document::Document(int width, int height, QObject* parent)
    : QObject(parent), m_width(width), m_height(height), m_selection(width, height) {
    // Default background layer
    auto bg = std::make_shared<Layer>(width, height, "Background", true);
    m_layers.append(bg);
    m_activeLayerIndex = 0;
}

Document::Document(const QImage& initialImage, QObject* parent)
    : QObject(parent),
      m_width(initialImage.width()),
      m_height(initialImage.height()),
      m_selection(initialImage.width(), initialImage.height()) {
    auto bg = std::make_shared<Layer>(initialImage, "Background", true);
    m_layers.append(bg);
    m_activeLayerIndex = 0;
}

void Document::setFilePath(const QString& path) {
    m_filePath = path;
    emit documentChanged();
}

QString Document::fileName() const {
    if (m_filePath.isEmpty()) {
        return "Untitled";
    }
    return QFileInfo(m_filePath).fileName();
}

bool Document::isModified() const {
    return !m_undoStack.isClean();
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
        m_activeLayerIndex = index;
        emit activeLayerChanged(m_activeLayerIndex);
    }
}

std::shared_ptr<Layer> Document::addLayer(const QString& name) {
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
    return newLayer;
}

void Document::insertLayer(int index, std::shared_ptr<Layer> layer) {
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

std::shared_ptr<Layer> Document::removeLayer(int index) {
    if (m_layers.size() <= 1 || index < 0 || index >= m_layers.size()) {
        return nullptr; // Cannot remove last layer
    }
    auto removed = m_layers.takeAt(index);
    if (m_activeLayerIndex >= m_layers.size()) {
        m_activeLayerIndex = m_layers.size() - 1;
    }
    emit layerCountChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentChanged();
    return removed;
}

std::shared_ptr<Layer> Document::duplicateLayer(int index) {
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
    return dup;
}

bool Document::moveLayer(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= m_layers.size() ||
        toIndex < 0 || toIndex >= m_layers.size() ||
        fromIndex == toIndex) {
        return false;
    }
    m_layers.move(fromIndex, toIndex);
    m_activeLayerIndex = toIndex;
    emit layerCountChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentChanged();
    return true;
}

bool Document::mergeLayerDown(int index) {
    if (index <= 0 || index >= m_layers.size()) {
        return false; // Can't merge down bottom layer
    }
    auto topLayer = m_layers.at(index);
    auto bottomLayer = m_layers.at(index - 1);

    // Blend topLayer onto bottomLayer
    int count = m_width * m_height;
    blendImages(bottomLayer->bits(), topLayer->bits(), count, topLayer->blendMode(), topLayer->opacity());

    // Remove top layer
    m_layers.removeAt(index);
    m_activeLayerIndex = index - 1;

    emit layerCountChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit documentChanged();
    return true;
}

void Document::clearSelection() {
    m_selection.clear();
    emit selectionChanged();
    emit documentChanged();
}

void Document::resizeCanvas(int newWidth, int newHeight, Qt::Alignment anchor) {
    if (newWidth <= 0 || newHeight <= 0) return;
    int dx = 0;
    int dy = 0;

    if (anchor & Qt::AlignRight) {
        dx = newWidth - m_width;
    } else if (anchor & Qt::AlignHCenter) {
        dx = (newWidth - m_width) / 2;
    }

    if (anchor & Qt::AlignBottom) {
        dy = newHeight - m_height;
    } else if (anchor & Qt::AlignVCenter) {
        dy = (newHeight - m_height) / 2;
    }

    for (auto& l : m_layers) {
        QImage newImg(newWidth, newHeight, QImage::Format_ARGB32);
        newImg.fill(l->isBackground() ? Qt::white : Qt::transparent);
        QPainter p(&newImg);
        p.drawImage(dx, dy, l->image());
        p.end();
        l->setImage(newImg);
    }

    m_width = newWidth;
    m_height = newHeight;
    m_selection.clear();
    emit documentChanged();
}

void Document::resizeImage(int newWidth, int newHeight, Qt::TransformationMode mode) {
    if (newWidth <= 0 || newHeight <= 0) return;
    for (auto& l : m_layers) {
        l->setImage(l->image().scaled(newWidth, newHeight, Qt::IgnoreAspectRatio, mode));
    }
    m_width = newWidth;
    m_height = newHeight;
    m_selection.clear();
    emit documentChanged();
}

void Document::crop(const QRect& rect) {
    QRect validRect = rect.intersected(QRect(0, 0, m_width, m_height));
    if (validRect.isEmpty()) return;

    for (auto& l : m_layers) {
        l->crop(validRect);
    }
    m_width = validRect.width();
    m_height = validRect.height();
    m_selection.clear();
    emit documentChanged();
}

void Document::flipHorizontal() {
    for (auto& l : m_layers) l->flipHorizontal();
    emit documentChanged();
}

void Document::flipVertical() {
    for (auto& l : m_layers) l->flipVertical();
    emit documentChanged();
}

void Document::rotate90CW() {
    for (auto& l : m_layers) l->rotate90CW();
    std::swap(m_width, m_height);
    m_selection.clear();
    emit documentChanged();
}

void Document::rotate90CCW() {
    for (auto& l : m_layers) l->rotate90CCW();
    std::swap(m_width, m_height);
    m_selection.clear();
    emit documentChanged();
}

void Document::rotate180() {
    for (auto& l : m_layers) l->rotate180();
    emit documentChanged();
}

void Document::flatten() {
    if (m_layers.size() <= 1) return;
    QImage compositeImage = composite();
    m_layers.clear();
    auto flat = std::make_shared<Layer>(compositeImage, "Background", true);
    m_layers.append(flat);
    m_activeLayerIndex = 0;
    emit layerCountChanged();
    emit activeLayerChanged(0);
    emit documentChanged();
}

QImage Document::composite() const {
    QImage result(m_width, m_height, QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    compositeInto(result);
    return result;
}

void Document::compositeInto(QImage& target) const {
    if (target.width() != m_width || target.height() != m_height || target.format() != QImage::Format_ARGB32) {
        target = QImage(m_width, m_height, QImage::Format_ARGB32);
    }
    target.fill(Qt::transparent);

    int pixelCount = m_width * m_height;
    uint32_t* targetBits = reinterpret_cast<uint32_t*>(target.bits());

    for (const auto& layer : m_layers) {
        if (!layer->isVisible()) continue;
        blendImages(targetBits, layer->bits(), pixelCount, layer->blendMode(), layer->opacity());
    }
}

} // namespace pdn
