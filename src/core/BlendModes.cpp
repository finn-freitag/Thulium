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

namespace {

// One functor per blend mode so the per-pixel loops below are instantiated with the channel
// formula inlined, instead of dispatching through a switch for every channel of every pixel.
// a = dst (bottom), b = src (top).
struct NormalOp     { static uint8_t apply(uint8_t, uint8_t b) { return b; } };
struct MultiplyOp   { static uint8_t apply(uint8_t a, uint8_t b) { return static_cast<uint8_t>((static_cast<uint32_t>(a) * b) / 255); } };
struct AddOp        { static uint8_t apply(uint8_t a, uint8_t b) { return static_cast<uint8_t>(std::min(255, a + b)); } };
struct ColorBurnOp  { static uint8_t apply(uint8_t a, uint8_t b) { return b == 0 ? 0 : static_cast<uint8_t>(std::max(0, 255 - ((255 - a) * 255) / b)); } };
struct ColorDodgeOp { static uint8_t apply(uint8_t a, uint8_t b) { return b == 255 ? 255 : static_cast<uint8_t>(std::min(255, (a * 255) / (255 - b))); } };
struct ReflectOp    { static uint8_t apply(uint8_t a, uint8_t b) { return b == 255 ? 255 : static_cast<uint8_t>(std::min(255, (a * a) / (255 - b))); } };
struct GlowOp       { static uint8_t apply(uint8_t a, uint8_t b) { return a == 255 ? 255 : static_cast<uint8_t>(std::min(255, (b * b) / (255 - a))); } };
struct OverlayOp    {
    static uint8_t apply(uint8_t a, uint8_t b) {
        return a < 128 ? static_cast<uint8_t>((2 * a * b) / 255)
                       : static_cast<uint8_t>(255 - (2 * (255 - a) * (255 - b)) / 255);
    }
};
struct DifferenceOp { static uint8_t apply(uint8_t a, uint8_t b) { return static_cast<uint8_t>(std::abs(a - b)); } };
struct NegationOp   { static uint8_t apply(uint8_t a, uint8_t b) { return static_cast<uint8_t>(255 - std::abs(255 - a - b)); } };
struct LightenOp    { static uint8_t apply(uint8_t a, uint8_t b) { return std::max(a, b); } };
struct DarkenOp     { static uint8_t apply(uint8_t a, uint8_t b) { return std::min(a, b); } };
struct ScreenOp     { static uint8_t apply(uint8_t a, uint8_t b) { return static_cast<uint8_t>(255 - ((255 - a) * (255 - b)) / 255); } };
struct XorOp        { static uint8_t apply(uint8_t a, uint8_t b) { return static_cast<uint8_t>(a ^ b); } };

// Porter-Duff "over" of a straight-alpha source pixel onto a straight-alpha destination pixel,
// with the colour term replaced by the blend-mode result.
template <typename Op>
inline uint32_t blendPixelT(uint32_t dstPixel, uint32_t srcPixel, uint32_t layerOpacity) {
    uint32_t srcA = srcPixel >> 24;
    if (srcA == 0) return dstPixel;
    srcA = (srcA * layerOpacity) / 255;
    if (srcA == 0) return dstPixel;

    const uint32_t dstA = dstPixel >> 24;
    if (dstA == 0) {
        // Nothing underneath: the result is the source with its (opacity-scaled) alpha.
        return (srcA << 24) | (srcPixel & 0x00FFFFFF);
    }

    const uint8_t srcR = (srcPixel >> 16) & 0xFF, srcG = (srcPixel >> 8) & 0xFF, srcB = srcPixel & 0xFF;
    const uint8_t dstR = (dstPixel >> 16) & 0xFF, dstG = (dstPixel >> 8) & 0xFF, dstB = dstPixel & 0xFF;
    const uint32_t blendedR = Op::apply(dstR, srcR);
    const uint32_t blendedG = Op::apply(dstG, srcG);
    const uint32_t blendedB = Op::apply(dstB, srcB);

    if (dstA == 255) {
        // Opaque destination (the common case for photos): the result stays opaque and the
        // general formula below reduces to a lerp with a constant divisor.
        const uint32_t inv = 255 - srcA;
        return 0xFF000000u |
               (((blendedR * srcA + dstR * inv) / 255) << 16) |
               (((blendedG * srcA + dstG * inv) / 255) << 8) |
               ((blendedB * srcA + dstB * inv) / 255);
    }

    const uint32_t dstWeight = (dstA * (255 - srcA)) / 255;
    const uint32_t outA = srcA + dstWeight;
    const uint32_t outR = (blendedR * srcA + dstR * dstWeight) / outA;
    const uint32_t outG = (blendedG * srcA + dstG * dstWeight) / outA;
    const uint32_t outB = (blendedB * srcA + dstB * dstWeight) / outA;
    return (outA << 24) | ((outR & 0xFF) << 16) | ((outG & 0xFF) << 8) | (outB & 0xFF);
}

template <typename Op>
void blendSpanT(uint32_t* dst, const uint32_t* src, int count, uint32_t layerOpacity) {
    for (int i = 0; i < count; ++i) {
        dst[i] = blendPixelT<Op>(dst[i], src[i], layerOpacity);
    }
}

// Normal mode gets its own loop: fully opaque source pixels are plain copies.
void blendSpanNormal(uint32_t* dst, const uint32_t* src, int count, uint32_t layerOpacity) {
    if (layerOpacity == 255) {
        for (int i = 0; i < count; ++i) {
            const uint32_t s = src[i];
            if (s >= 0xFF000000u) dst[i] = s;
            else if (s >> 24) dst[i] = blendPixelT<NormalOp>(dst[i], s, 255);
        }
    } else {
        blendSpanT<NormalOp>(dst, src, count, layerOpacity);
    }
}

// Invokes fn with the functor type matching the runtime blend mode.
template <typename Fn>
auto dispatchBlendMode(BlendMode mode, Fn&& fn) {
    switch (mode) {
        case BlendMode::Multiply:   return fn(MultiplyOp{});
        case BlendMode::Add:        return fn(AddOp{});
        case BlendMode::ColorBurn:  return fn(ColorBurnOp{});
        case BlendMode::ColorDodge: return fn(ColorDodgeOp{});
        case BlendMode::Reflect:    return fn(ReflectOp{});
        case BlendMode::Glow:       return fn(GlowOp{});
        case BlendMode::Overlay:    return fn(OverlayOp{});
        case BlendMode::Difference: return fn(DifferenceOp{});
        case BlendMode::Negation:   return fn(NegationOp{});
        case BlendMode::Lighten:    return fn(LightenOp{});
        case BlendMode::Darken:     return fn(DarkenOp{});
        case BlendMode::Screen:     return fn(ScreenOp{});
        case BlendMode::Xor:        return fn(XorOp{});
        case BlendMode::Normal:     break;
    }
    return fn(NormalOp{});
}

} // namespace

uint8_t blendChannel(BlendMode mode, uint8_t dst, uint8_t src) {
    return dispatchBlendMode(mode, [dst, src](auto op) { return decltype(op)::apply(dst, src); });
}

uint32_t blendPixel(BlendMode mode, uint32_t dstPixel, uint32_t srcPixel, uint8_t layerOpacity) {
    return dispatchBlendMode(mode, [=](auto op) {
        return blendPixelT<decltype(op)>(dstPixel, srcPixel, layerOpacity);
    });
}

void blendImages(uint32_t* dst, const uint32_t* src, int count, BlendMode mode, uint8_t layerOpacity) {
    if (layerOpacity == 0 || count <= 0) return;
    if (mode == BlendMode::Normal) {
        blendSpanNormal(dst, src, count, layerOpacity);
        return;
    }
    dispatchBlendMode(mode, [=](auto op) {
        blendSpanT<decltype(op)>(dst, src, count, layerOpacity);
    });
}

} // namespace pdn
