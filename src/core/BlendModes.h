#pragma once

#include <cstdint>
#include <QString>
#include <QVector>

namespace pdn {

enum class BlendMode : int {
    Normal = 0,
    Multiply = 1,
    Add = 2,
    ColorBurn = 3,
    ColorDodge = 4,
    Reflect = 5,
    Glow = 6,
    Overlay = 7,
    Difference = 8,
    Negation = 9,
    Lighten = 10,
    Darken = 11,
    Screen = 12,
    Xor = 13
};

struct BlendModeInfo {
    BlendMode mode;
    QString name;
};

const QVector<BlendModeInfo>& getAvailableBlendModes();
QString getBlendModeName(BlendMode mode);

// Blend single channel (0..255)
uint8_t blendChannel(BlendMode mode, uint8_t dst, uint8_t src);

// Blend 32-bit ARGB/BGRA pixels with layer opacity (0..255)
// Pixels in format: 0xAARRGGBB
uint32_t blendPixel(BlendMode mode, uint32_t dstPixel, uint32_t srcPixel, uint8_t layerOpacity = 255);

// Composite src image over dst image using blend mode and opacity
void blendImages(uint32_t* dst, const uint32_t* src, int count, BlendMode mode, uint8_t layerOpacity);

} // namespace pdn
