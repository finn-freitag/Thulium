#include "BlendModes.h"
#include <algorithm>
#include <cmath>

namespace pdn {

static const QVector<BlendModeInfo> s_blendModes = {
    { BlendMode::Normal, "Normal" },
    { BlendMode::Multiply, "Multiply" },
    { BlendMode::Add, "Additive" },
    { BlendMode::ColorBurn, "Color Burn" },
    { BlendMode::ColorDodge, "Color Dodge" },
    { BlendMode::Reflect, "Reflect" },
    { BlendMode::Glow, "Glow" },
    { BlendMode::Overlay, "Overlay" },
    { BlendMode::Difference, "Difference" },
    { BlendMode::Negation, "Negation" },
    { BlendMode::Lighten, "Lighten" },
    { BlendMode::Darken, "Darken" },
    { BlendMode::Screen, "Screen" },
    { BlendMode::Xor, "XOR" }
};

const QVector<BlendModeInfo>& getAvailableBlendModes() {
    return s_blendModes;
}

QString getBlendModeName(BlendMode mode) {
    for (const auto& info : s_blendModes) {
        if (info.mode == mode) return info.name;
    }
    return "Normal";
}

inline uint8_t blendChannel(BlendMode mode, uint8_t a, uint8_t b) {
    // a = dst (bottom), b = src (top)
    switch (mode) {
        case BlendMode::Normal:
            return b;
        case BlendMode::Multiply:
            return static_cast<uint8_t>((static_cast<uint32_t>(a) * b) / 255);
        case BlendMode::Add:
            return static_cast<uint8_t>(std::min(255, a + b));
        case BlendMode::ColorBurn:
            if (b == 0) return 0;
            return static_cast<uint8_t>(std::max(0, 255 - ((255 - a) * 255) / b));
        case BlendMode::ColorDodge:
            if (b == 255) return 255;
            return static_cast<uint8_t>(std::min(255, (a * 255) / (255 - b)));
        case BlendMode::Reflect:
            if (b == 255) return 255;
            return static_cast<uint8_t>(std::min(255, (a * a) / (255 - b)));
        case BlendMode::Glow:
            if (a == 255) return 255;
            return static_cast<uint8_t>(std::min(255, (b * b) / (255 - a)));
        case BlendMode::Overlay:
            if (a < 128) {
                return static_cast<uint8_t>((2 * a * b) / 255);
            } else {
                return static_cast<uint8_t>(255 - (2 * (255 - a) * (255 - b)) / 255);
            }
        case BlendMode::Difference:
            return static_cast<uint8_t>(std::abs(a - b));
        case BlendMode::Negation:
            return static_cast<uint8_t>(255 - std::abs(255 - a - b));
        case BlendMode::Lighten:
            return std::max(a, b);
        case BlendMode::Darken:
            return std::min(a, b);
        case BlendMode::Screen:
            return static_cast<uint8_t>(255 - ((255 - a) * (255 - b)) / 255);
        case BlendMode::Xor:
            return static_cast<uint8_t>(a ^ b);
        default:
            return b;
    }
}

uint32_t blendPixel(BlendMode mode, uint32_t dstPixel, uint32_t srcPixel, uint8_t layerOpacity) {
    uint32_t srcA = (srcPixel >> 24) & 0xFF;
    if (srcA == 0 || layerOpacity == 0) {
        return dstPixel;
    }

    // Apply layer opacity to source alpha
    srcA = (srcA * layerOpacity) / 255;
    if (srcA == 0) {
        return dstPixel;
    }

    uint32_t dstA = (dstPixel >> 24) & 0xFF;
    if (dstA == 0) {
        // Destination is completely transparent, result is source with adjusted alpha
        return (srcA << 24) | (srcPixel & 0x00FFFFFF);
    }

    uint8_t srcR = (srcPixel >> 16) & 0xFF;
    uint8_t srcG = (srcPixel >> 8) & 0xFF;
    uint8_t srcB = srcPixel & 0xFF;

    uint8_t dstR = (dstPixel >> 16) & 0xFF;
    uint8_t dstG = (dstPixel >> 8) & 0xFF;
    uint8_t dstB = dstPixel & 0xFF;

    // Calculate blended color for each channel
    uint8_t blendedR = blendChannel(mode, dstR, srcR);
    uint8_t blendedG = blendChannel(mode, dstG, srcG);
    uint8_t blendedB = blendChannel(mode, dstB, srcB);

    // Standard Porter-Duff Over composite with blended color
    // outA = srcA + dstA * (255 - srcA) / 255
    uint32_t outA = srcA + (dstA * (255 - srcA)) / 255;
    if (outA == 0) return 0;

    // outRGB = (blendedRGB * srcA + dstRGB * dstA * (255 - srcA) / 255) / outA
    uint32_t dstWeight = (dstA * (255 - srcA)) / 255;
    uint32_t outR = (blendedR * srcA + dstR * dstWeight) / outA;
    uint32_t outG = (blendedG * srcA + dstG * dstWeight) / outA;
    uint32_t outB = (blendedB * srcA + dstB * dstWeight) / outA;

    return (outA << 24) | ((outR & 0xFF) << 16) | ((outG & 0xFF) << 8) | (outB & 0xFF);
}

void blendImages(uint32_t* dst, const uint32_t* src, int count, BlendMode mode, uint8_t layerOpacity) {
    if (layerOpacity == 0) return;
    for (int i = 0; i < count; ++i) {
        dst[i] = blendPixel(mode, dst[i], src[i], layerOpacity);
    }
}

} // namespace pdn
