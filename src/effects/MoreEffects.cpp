// Adjustments and effects implemented as data-driven ParametricEffects.
#include "ParametricEffect.h"
#include "GaussianBlur.h"
#include "../core/Parallel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace pdn {

namespace {

inline int red(uint32_t px) { return (px >> 16) & 0xFF; }
inline int green(uint32_t px) { return (px >> 8) & 0xFF; }
inline int blue(uint32_t px) { return px & 0xFF; }
inline int luma(uint32_t px) { return (299 * red(px) + 587 * green(px) + 114 * blue(px) + 500) / 1000; }
inline int clampByte(double v) { return static_cast<int>(std::clamp(v, 0.0, 255.0) + 0.5); }
inline uint32_t withRgb(uint32_t px, int r, int g, int b) {
    return (px & 0xFF000000u) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

// Calls fn(dst, x, y) in parallel for every selected pixel; dst points at the pixel to write.
template <typename Fn>
void forEachSelectedPixel(QImage& image, const Selection& selection, Fn&& fn) {
    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) return;
    const QImage* mask = selection.isEmpty() ? nullptr : &selection.mask(w, h);
    image.detach(); // worker threads must not trigger a copy-on-write
    parallelRows(h, [&](int y) {
        auto* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        const uint8_t* maskRow = mask ? mask->constScanLine(y) : nullptr;
        for (int x = 0; x < w; ++x) {
            if (!maskRow || maskRow[x]) fn(line[x], x, y);
        }
    });
}

// Applies one lookup table to the red, green and blue channels.
void applyLut(QImage& image, const Selection& selection, const std::array<uint8_t, 256>& lut) {
    forEachSelectedPixel(image, selection, [&](uint32_t& px, int, int) {
        px = withRgb(px, lut[red(px)], lut[green(px)], lut[blue(px)]);
    });
}

// Bilinear sample of a straight-alpha image with clamped edges.
uint32_t sampleBilinear(const QImage& src, double fx, double fy) {
    const int w = src.width(), h = src.height();
    fx = std::clamp(fx, 0.0, w - 1.0);
    fy = std::clamp(fy, 0.0, h - 1.0);
    const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
    const double tx = fx - x0, ty = fy - y0;
    const auto* r0 = reinterpret_cast<const uint32_t*>(src.constScanLine(y0));
    const auto* r1 = reinterpret_cast<const uint32_t*>(src.constScanLine(y1));
    const uint32_t p[4] = {r0[x0], r0[x1], r1[x0], r1[x1]};
    const double wgt[4] = {(1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty};
    // Weight colours by alpha so transparent neighbours do not darken the result
    double a = 0, r = 0, g = 0, b = 0;
    for (int i = 0; i < 4; ++i) {
        const double pa = (p[i] >> 24) * wgt[i];
        a += pa;
        r += red(p[i]) * pa;
        g += green(p[i]) * pa;
        b += blue(p[i]) * pa;
    }
    if (a < 0.5) return 0;
    return (static_cast<uint32_t>(clampByte(a)) << 24) | (clampByte(r / a) << 16) | (clampByte(g / a) << 8) | clampByte(b / a);
}

// Geometric distortion: every selected pixel takes the colour found at mapFn(x, y) in the original.
template <typename MapFn>
void remap(QImage& image, const Selection& selection, MapFn&& mapFn) {
    const QImage src = image.copy();
    forEachSelectedPixel(image, selection, [&](uint32_t& px, int x, int y) {
        const QPointF from = mapFn(x, y);
        px = sampleBilinear(src, from.x(), from.y());
    });
}

// Averages `count` samples taken at sampleFn(x, y, i) from the original image.
template <typename SampleFn>
void multiSample(QImage& image, const Selection& selection, int count, SampleFn&& sampleFn) {
    const QImage src = image.copy();
    forEachSelectedPixel(image, selection, [&](uint32_t& px, int x, int y) {
        double a = 0, r = 0, g = 0, b = 0;
        for (int i = 0; i < count; ++i) {
            const QPointF at = sampleFn(x, y, i);
            const uint32_t s = sampleBilinear(src, at.x(), at.y());
            const double sa = s >> 24;
            a += sa;
            r += red(s) * sa;
            g += green(s) * sa;
            b += blue(s) * sa;
        }
        if (a < 0.5) {
            px = 0;
        } else {
            px = (static_cast<uint32_t>(clampByte(a / count)) << 24) | (clampByte(r / a) << 16) |
                 (clampByte(g / a) << 8) | clampByte(b / a);
        }
    });
}

// Cheap repeatable hash noise in [0, 1) for a lattice point.
inline double hashNoise(int x, int y, uint32_t seed) {
    uint32_t n = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + seed * 2246822519u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return ((n ^ (n >> 16)) & 0xFFFFFF) / static_cast<double>(0x1000000);
}

// Smoothly interpolated lattice noise.
double valueNoise(double x, double y, uint32_t seed) {
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    const double tx = x - xi, ty = y - yi;
    const double sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
    const double top = hashNoise(xi, yi, seed) + (hashNoise(xi + 1, yi, seed) - hashNoise(xi, yi, seed)) * sx;
    const double bottom = hashNoise(xi, yi + 1, seed) + (hashNoise(xi + 1, yi + 1, seed) - hashNoise(xi, yi + 1, seed)) * sx;
    return top + (bottom - top) * sy;
}

// Escape-time colouring shared by the two fractal renderers.
uint32_t fractalColor(int iterations, int maxIterations) {
    if (iterations >= maxIterations) return 0xFF000000u;
    const double t = static_cast<double>(iterations) / maxIterations;
    return 0xFF000000u | (clampByte(9 * (1 - t) * t * t * t * 255) << 16) |
           (clampByte(15 * (1 - t) * (1 - t) * t * t * 255) << 8) | clampByte(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255);
}

// ---------- Adjustments ----------

void levels(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const double inBlack = v[0], inWhite = std::max(v[1], v[0] + 1), gamma = v[2] / 100.0;
    const double outBlack = v[3], outWhite = v[4];
    std::array<uint8_t, 256> lut;
    for (int i = 0; i < 256; ++i) {
        const double t = std::clamp((i - inBlack) / (inWhite - inBlack), 0.0, 1.0);
        lut[i] = static_cast<uint8_t>(clampByte(outBlack + std::pow(t, 1.0 / gamma) * (outWhite - outBlack)));
    }
    applyLut(image, sel, lut);
}

void exposure(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const double gain = std::pow(2.0, v[0] / 100.0); // slider is in hundredths of a stop
    std::array<uint8_t, 256> lut;
    for (int i = 0; i < 256; ++i) {
        // Scale in linear light, then convert back to gamma space
        const double linear = std::pow(i / 255.0, 2.2) * gain;
        lut[i] = static_cast<uint8_t>(clampByte(std::pow(std::min(linear, 1.0), 1.0 / 2.2) * 255.0));
    }
    applyLut(image, sel, lut);
}

void vibrance(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const double amount = v[0] / 100.0;
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int, int) {
        const int r = red(px), g = green(px), b = blue(px);
        const int maxC = std::max({r, g, b}), minC = std::min({r, g, b});
        // Boost muted colours more than already saturated ones
        const double saturation = maxC == 0 ? 0.0 : static_cast<double>(maxC - minC) / maxC;
        const double scale = 1.0 + amount * (1.0 - saturation);
        const double grey = luma(px);
        px = withRgb(px, clampByte(grey + (r - grey) * scale), clampByte(grey + (g - grey) * scale),
                     clampByte(grey + (b - grey) * scale));
    });
}

// ---------- Artistic ----------

void pencilSketch(QImage& image, const Selection& sel, const EffectParamValues& v) {
    // Classic technique: colour-dodge the greyscale image with a blurred copy of its negative
    QImage inverted = image.copy();
    forEachSelectedPixel(inverted, Selection(), [](uint32_t& px, int, int) {
        const int l = 255 - luma(px);
        px = withRgb(px, l, l, l);
    });
    GaussianBlurEffect::process(inverted, Selection(), std::max(1, v[0]));
    const double range = v[1] / 100.0;
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int x, int y) {
        const int base = luma(px);
        const int blend = red(reinterpret_cast<const uint32_t*>(inverted.constScanLine(y))[x]);
        const double dodged = blend >= 255 ? 255.0 : std::min(255.0, base * 255.0 / (255 - blend));
        const int out = clampByte(255.0 - (255.0 - dodged) * (1.0 + range));
        px = withRgb(px, out, out, out);
    });
}

void inkSketch(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const QImage src = image.copy();
    const int w = image.width(), h = image.height();
    const double outline = v[0] / 25.0, coloring = v[1] / 100.0;
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int x, int y) {
        auto lumaAt = [&](int sx, int sy) {
            return luma(reinterpret_cast<const uint32_t*>(src.constScanLine(std::clamp(sy, 0, h - 1)))[std::clamp(sx, 0, w - 1)]);
        };
        const int gx = lumaAt(x + 1, y) - lumaAt(x - 1, y);
        const int gy = lumaAt(x, y + 1) - lumaAt(x, y - 1);
        const double ink = std::clamp(1.0 - std::sqrt(gx * gx + gy * gy) * outline / 255.0, 0.0, 1.0);
        // White paper with the original colours showing through, darkened where edges are
        auto channel = [&](int c) { return clampByte((255.0 + (c - 255.0) * coloring) * ink); };
        px = withRgb(px, channel(red(px)), channel(green(px)), channel(blue(px)));
    });
}

// ---------- Blurs ----------

void zoomBlur(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const double cx = image.width() / 2.0, cy = image.height() / 2.0;
    const double amount = v[0] / 400.0;
    const int samples = 24;
    multiSample(image, sel, samples, [&](int x, int y, int i) {
        const double scale = 1.0 - amount * i / (samples - 1);
        return QPointF(cx + (x - cx) * scale, cy + (y - cy) * scale);
    });
}

void fragment(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const int count = std::max(2, v[0]);
    const double distance = v[1], rotation = v[2] * M_PI / 180.0;
    multiSample(image, sel, count, [&](int x, int y, int i) {
        const double angle = rotation + 2.0 * M_PI * i / count;
        return QPointF(x + std::cos(angle) * distance, y + std::sin(angle) * distance);
    });
}

void surfaceBlur(QImage& image, const Selection& sel, const EffectParamValues& v) {
    // Bilateral filter: neighbours only contribute if their colour is close to the centre pixel
    const QImage src = image.copy();
    const int w = image.width(), h = image.height();
    const int radius = v[0];
    const double threshold = std::max(1, v[1]) * 2.55;
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int x, int y) {
        const int cr = red(px), cg = green(px), cb = blue(px);
        double sumW = 0, r = 0, g = 0, b = 0;
        for (int dy = -radius; dy <= radius; ++dy) {
            const auto* row = reinterpret_cast<const uint32_t*>(src.constScanLine(std::clamp(y + dy, 0, h - 1)));
            for (int dx = -radius; dx <= radius; ++dx) {
                const uint32_t s = row[std::clamp(x + dx, 0, w - 1)];
                const double diff = (std::abs(red(s) - cr) + std::abs(green(s) - cg) + std::abs(blue(s) - cb)) / 3.0;
                const double weight = std::max(0.0, 1.0 - diff / threshold);
                sumW += weight;
                r += red(s) * weight;
                g += green(s) * weight;
                b += blue(s) * weight;
            }
        }
        if (sumW > 0) px = withRgb(px, clampByte(r / sumW), clampByte(g / sumW), clampByte(b / sumW));
    });
}

// ---------- Distort ----------

void bulge(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const double cx = image.width() / 2.0, cy = image.height() / 2.0;
    const double radius = std::min(cx, cy);
    const double amount = v[0] / 100.0;
    remap(image, sel, [&](int x, int y) {
        const double dx = x - cx, dy = y - cy;
        const double d = std::sqrt(dx * dx + dy * dy) / radius;
        if (d >= 1.0) return QPointF(x, y);
        // Positive amounts magnify the centre, negative amounts pinch it
        const double scale = std::pow(std::max(d, 1e-6), amount);
        return QPointF(cx + dx * scale, cy + dy * scale);
    });
}

void frostedGlass(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const int radius = v[0];
    remap(image, sel, [&](int x, int y) {
        return QPointF(x + (hashNoise(x, y, 1) * 2 - 1) * radius, y + (hashNoise(x, y, 2) * 2 - 1) * radius);
    });
}

void polarInversion(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const double cx = image.width() / 2.0, cy = image.height() / 2.0;
    const double amount = v[0] / 100.0;
    const double radiusSquared = std::min(cx, cy) * std::min(cx, cy);
    const int w = image.width(), h = image.height();
    remap(image, sel, [&](int x, int y) {
        const double dx = x - cx, dy = y - cy;
        const double distSquared = std::max(dx * dx + dy * dy, 1e-6);
        // Blend between the identity and the inversion in the unit circle
        const double scale = 1.0 + amount * (radiusSquared / distSquared - 1.0);
        // Wrap around so the inverted image tiles
        const double sx = std::fmod(std::fmod(cx + dx * scale, w) + w, w);
        const double sy = std::fmod(std::fmod(cy + dy * scale, h) + h, h);
        return QPointF(sx, sy);
    });
}

void tileReflection(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const double angle = v[0] * M_PI / 180.0, tileSize = std::max(2, v[1]), curvature = v[2] / 10.0;
    const double cosA = std::cos(angle), sinA = std::sin(angle);
    const double cx = image.width() / 2.0, cy = image.height() / 2.0;
    remap(image, sel, [&](int x, int y) {
        // Position inside the (rotated) tile grid, then a lens-like offset per tile
        const double u = (x - cx) * cosA + (y - cy) * sinA;
        const double w = -(x - cx) * sinA + (y - cy) * cosA;
        const double du = std::tan(M_PI * (u / tileSize - std::floor(u / tileSize) - 0.5) * 0.5) * curvature;
        const double dw = std::tan(M_PI * (w / tileSize - std::floor(w / tileSize) - 0.5) * 0.5) * curvature;
        return QPointF(x + (du * cosA - dw * sinA) * tileSize / 4.0, y + (du * sinA + dw * cosA) * tileSize / 4.0);
    });
}

// ---------- Photo ----------

void redEyeRemoval(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const double tolerance = v[0] / 100.0, saturation = v[1] / 100.0;
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int, int) {
        const int r = red(px), g = green(px), b = blue(px);
        // "Red eye" pixels: red clearly dominates both other channels
        const double redness = r - std::max(g, b) * (1.0 + (1.0 - tolerance));
        if (redness <= 0) return;
        const int target = (g + b) / 2;
        px = withRgb(px, clampByte(target + (r - target) * saturation), g, b);
    });
}

void softenPortrait(QImage& image, const Selection& sel, const EffectParamValues& v) {
    QImage blurred = image.copy();
    GaussianBlurEffect::process(blurred, Selection(), std::max(1, v[0]));
    const double lighting = v[1] / 100.0, warmth = v[2] / 100.0;
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int x, int y) {
        const uint32_t soft = reinterpret_cast<const uint32_t*>(blurred.constScanLine(y))[x];
        // Half sharp, half blurred keeps detail while smoothing skin; then lift and warm the tones
        auto mix = [&](int sharp, int blur, double tint) {
            const double base = (sharp + blur) / 2.0;
            return clampByte(base + (255.0 - base) * lighting * 0.5 + tint);
        };
        px = withRgb(px, mix(red(px), red(soft), 20 * warmth), mix(green(px), green(soft), 5 * warmth),
                     mix(blue(px), blue(soft), -15 * warmth));
    });
}

// ---------- Render ----------

void clouds(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const double scale = std::max(1, v[0]), roughness = v[1] / 100.0;
    const uint32_t seed = static_cast<uint32_t>(v[2]);
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int x, int y) {
        // Fractal sum of noise octaves
        double value = 0, amplitude = 1, total = 0, frequency = 1.0 / scale;
        for (int octave = 0; octave < 7; ++octave) {
            value += valueNoise(x * frequency, y * frequency, seed + octave) * amplitude;
            total += amplitude;
            amplitude *= roughness;
            frequency *= 2;
        }
        const int grey = clampByte(value / total * 255.0);
        px = 0xFF000000u | (grey << 16) | (grey << 8) | grey;
    });
}

void mandelbrot(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const int w = image.width(), h = image.height();
    const double zoom = std::pow(1.5, v[0]), span = 3.0 / zoom / std::min(w, h);
    const int maxIter = v[1];
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int x, int y) {
        const double cr = (x - w / 2.0) * span - 0.6, ci = (y - h / 2.0) * span;
        double zr = 0, zi = 0;
        int i = 0;
        while (i < maxIter && zr * zr + zi * zi <= 4.0) {
            const double t = zr * zr - zi * zi + cr;
            zi = 2 * zr * zi + ci;
            zr = t;
            ++i;
        }
        px = fractalColor(i, maxIter);
    });
}

void julia(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const int w = image.width(), h = image.height();
    const double zoom = std::pow(1.5, v[0]), span = 3.2 / zoom / std::min(w, h);
    const int maxIter = v[1];
    const double angle = v[2] * M_PI / 180.0;
    const double cr = 0.7885 * std::cos(angle), ci = 0.7885 * std::sin(angle);
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int x, int y) {
        double zr = (x - w / 2.0) * span, zi = (y - h / 2.0) * span;
        int i = 0;
        while (i < maxIter && zr * zr + zi * zi <= 4.0) {
            const double t = zr * zr - zi * zi + cr;
            zi = 2 * zr * zi + ci;
            zr = t;
            ++i;
        }
        px = fractalColor(i, maxIter);
    });
}

// ---------- Stylize ----------

void outline(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const QImage src = image.copy();
    const int w = image.width(), h = image.height();
    const int thickness = v[0];
    const double intensity = v[1] / 50.0;
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int x, int y) {
        // Local contrast: difference between the brightest and darkest neighbour per channel
        int minC[3] = {255, 255, 255}, maxC[3] = {0, 0, 0};
        for (int dy = -thickness; dy <= thickness; ++dy) {
            const auto* row = reinterpret_cast<const uint32_t*>(src.constScanLine(std::clamp(y + dy, 0, h - 1)));
            for (int dx = -thickness; dx <= thickness; ++dx) {
                const uint32_t s = row[std::clamp(x + dx, 0, w - 1)];
                const int c[3] = {red(s), green(s), blue(s)};
                for (int k = 0; k < 3; ++k) {
                    minC[k] = std::min(minC[k], c[k]);
                    maxC[k] = std::max(maxC[k], c[k]);
                }
            }
        }
        px = withRgb(px, clampByte((maxC[0] - minC[0]) * intensity), clampByte((maxC[1] - minC[1]) * intensity),
                     clampByte((maxC[2] - minC[2]) * intensity));
    });
}

void relief(QImage& image, const Selection& sel, const EffectParamValues& v) {
    const QImage src = image.copy();
    const int w = image.width(), h = image.height();
    const double angle = v[0] * M_PI / 180.0;
    const double dx = std::cos(angle), dy = std::sin(angle);
    forEachSelectedPixel(image, sel, [&](uint32_t& px, int x, int y) {
        const uint32_t ahead = sampleBilinear(src, std::clamp(x + dx, 0.0, w - 1.0), std::clamp(y + dy, 0.0, h - 1.0));
        const uint32_t behind = sampleBilinear(src, std::clamp(x - dx, 0.0, w - 1.0), std::clamp(y - dy, 0.0, h - 1.0));
        // Unlike Emboss, the relief keeps the image's colours and adds the directional light on top
        const int light = (luma(ahead) - luma(behind)) * 2;
        px = withRgb(px, clampByte(red(px) + light), clampByte(green(px) + light), clampByte(blue(px) + light));
    });
}

} // namespace

QList<std::shared_ptr<IEffect>> createParametricEffects() {
    auto make = [](const char* name, const char* category, QVector<EffectParam> params, ParametricFn fn) {
        return std::static_pointer_cast<IEffect>(
            std::make_shared<ParametricEffect>(name, category, std::move(params), std::move(fn)));
    };
    return {
        make("Levels", "Adjustments",
             {{"Input black", 0, 254, 0}, {"Input white", 1, 255, 255}, {"Gamma (x100)", 10, 500, 100},
              {"Output black", 0, 255, 0}, {"Output white", 0, 255, 255}}, levels),
        make("Exposure", "Adjustments", {{"Exposure (1/100 stop)", -300, 300, 0}}, exposure),
        make("Vibrance", "Adjustments", {{"Vibrance", -100, 100, 30}}, vibrance),

        make("Ink Sketch", "Artistic", {{"Ink outline", 0, 99, 50}, {"Coloring", 0, 100, 50}}, inkSketch),
        make("Pencil Sketch", "Artistic", {{"Pencil tip size", 1, 20, 2}, {"Range", -20, 20, 0}}, pencilSketch),

        make("Fragment", "Blurs", {{"Fragment count", 2, 50, 4}, {"Distance", 0, 100, 8}, {"Rotation", 0, 360, 0}}, fragment),
        make("Surface Blur", "Blurs", {{"Radius", 1, 12, 4}, {"Threshold", 1, 100, 15}}, surfaceBlur),
        make("Zoom Blur", "Blurs", {{"Zoom amount", 0, 100, 10}}, zoomBlur),

        make("Bulge", "Distort", {{"Bulge", -200, 100, 45}}, bulge),
        make("Frosted Glass", "Distort", {{"Scatter radius", 0, 50, 3}}, frostedGlass),
        make("Polar Inversion", "Distort", {{"Amount", -100, 100, 100}}, polarInversion),
        make("Tile Reflection", "Distort", {{"Angle", -45, 45, 30}, {"Tile size", 2, 200, 40}, {"Curvature", -100, 100, 8}}, tileReflection),

        make("Red Eye Removal", "Photo", {{"Tolerance", 0, 100, 70}, {"Saturation", 0, 100, 10}}, redEyeRemoval),
        make("Soften Portrait", "Photo", {{"Softness", 1, 10, 5}, {"Lighting", -20, 20, 0}, {"Warmth", 0, 20, 10}}, softenPortrait),

        make("Clouds", "Render", {{"Scale", 2, 1000, 250}, {"Roughness", 0, 100, 50}, {"Random seed", 0, 255, 0}}, clouds),
        make("Julia Fractal", "Render", {{"Zoom", 0, 20, 0}, {"Quality (iterations)", 16, 1000, 150}, {"Angle", 0, 360, 115}}, julia),
        make("Mandelbrot Fractal", "Render", {{"Zoom", 0, 20, 0}, {"Quality (iterations)", 16, 1000, 150}}, mandelbrot),

        make("Outline", "Stylize", {{"Thickness", 1, 8, 2}, {"Intensity", 0, 100, 50}}, outline),
        make("Relief", "Stylize", {{"Angle", 0, 360, 45}}, relief),
    };
}

} // namespace pdn
