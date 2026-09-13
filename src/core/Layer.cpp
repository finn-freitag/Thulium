#include "Layer.h"
#include <QPainter>

namespace pdn {

Layer::Layer(int width, int height, const QString& name, bool isBackground)
    : m_image(width, height, QImage::Format_ARGB32),
      m_name(name),
      m_visible(true),
      m_opacity(255),
      m_blendMode(BlendMode::Normal),
      m_isBackground(isBackground) {
    m_image.fill(Qt::transparent);
    if (isBackground) {
        m_image.fill(Qt::white);
    }
}

Layer::Layer(const QImage& image, const QString& name, bool isBackground)
    : m_image(image.convertToFormat(QImage::Format_ARGB32)),
      m_name(name),
      m_visible(true),
      m_opacity(255),
      m_blendMode(BlendMode::Normal),
      m_isBackground(isBackground) {
}

std::shared_ptr<Layer> Layer::clone() const {
    auto copy = std::make_shared<Layer>(m_image, m_name, m_isBackground);
    copy->setVisible(m_visible);
    copy->setOpacity(m_opacity);
    copy->setBlendMode(m_blendMode);
    return copy;
}

void Layer::setImage(const QImage& img) {
    if (img.format() == QImage::Format_ARGB32) {
        m_image = img;
    } else {
        m_image = img.convertToFormat(QImage::Format_ARGB32);
    }
}

void Layer::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    m_image = m_image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void Layer::crop(const QRect& rect) {
    m_image = m_image.copy(rect);
}

void Layer::flipHorizontal() {
    m_image = m_image.mirrored(true, false);
}

void Layer::flipVertical() {
    m_image = m_image.mirrored(false, true);
}

void Layer::rotate90CW() {
    QTransform transform;
    transform.rotate(90);
    m_image = m_image.transformed(transform);
}

void Layer::rotate90CCW() {
    QTransform transform;
    transform.rotate(270);
    m_image = m_image.transformed(transform);
}

void Layer::rotate180() {
    QTransform transform;
    transform.rotate(180);
    m_image = m_image.transformed(transform);
}

void Layer::clear() {
    m_image.fill(Qt::transparent);
}

void Layer::fill(const QColor& color) {
    m_image.fill(color);
}

} // namespace pdn
