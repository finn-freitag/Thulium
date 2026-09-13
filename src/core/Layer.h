#pragma once

#include <QImage>
#include <QString>
#include <memory>
#include "BlendModes.h"

namespace pdn {

class Layer {
public:
    Layer(int width, int height, const QString& name = "Layer", bool isBackground = false);
    Layer(const QImage& image, const QString& name = "Layer", bool isBackground = false);
    ~Layer() = default;

    std::shared_ptr<Layer> clone() const;

    int width() const { return m_image.width(); }
    int height() const { return m_image.height(); }
    QSize size() const { return m_image.size(); }

    QImage& image() { return m_image; }
    const QImage& image() const { return m_image; }
    void setImage(const QImage& img);

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

    uint8_t opacity() const { return m_opacity; }
    void setOpacity(uint8_t opacity) { m_opacity = opacity; }

    BlendMode blendMode() const { return m_blendMode; }
    void setBlendMode(BlendMode mode) { m_blendMode = mode; }

    bool isBackground() const { return m_isBackground; }
    void setIsBackground(bool bg) { m_isBackground = bg; }

    // Layer operations
    void resize(int width, int height);
    void crop(const QRect& rect);
    void flipHorizontal();
    void flipVertical();
    void rotate90CW();
    void rotate90CCW();
    void rotate180();
    void clear();
    void fill(const QColor& color);

    // Direct pixel access (Format_ARGB32: 0xAARRGGBB)
    uint32_t* scanLine(int y) { return reinterpret_cast<uint32_t*>(m_image.scanLine(y)); }
    const uint32_t* scanLine(int y) const { return reinterpret_cast<const uint32_t*>(m_image.constScanLine(y)); }
    uint32_t* bits() { return reinterpret_cast<uint32_t*>(m_image.bits()); }
    const uint32_t* bits() const { return reinterpret_cast<const uint32_t*>(m_image.constBits()); }

private:
    QImage m_image;
    QString m_name;
    bool m_visible = true;
    uint8_t m_opacity = 255;
    BlendMode m_blendMode = BlendMode::Normal;
    bool m_isBackground = false;
};

} // namespace pdn
