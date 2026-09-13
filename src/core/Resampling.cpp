#include "Resampling.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <QColor>

namespace pdn {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

struct SampleWeight {
    int index;
    float weight;
};

struct Contribution {
    std::vector<SampleWeight> samples;
};

float bilinearKernel(float x) {
    x = std::abs(x);
    return (x < 1.0f) ? (1.0f - x) : 0.0f;
}

float bicubicKernel(float x) {
    x = std::abs(x);
    if (x < 1.0f) {
        return (1.5f * x - 2.5f) * x * x + 1.0f; // Catmull-Rom
    } else if (x < 2.0f) {
        return ((-0.5f * x + 2.5f) * x - 4.0f) * x + 2.0f;
    }
    return 0.0f;
}

float sinc(float x) {
    if (std::abs(x) < 1e-5f) return 1.0f;
    float px = static_cast<float>(M_PI) * x;
    return std::sin(px) / px;
}

float lanczosKernel(float x) {
    x = std::abs(x);
    if (x < 3.0f) {
        return sinc(x) * sinc(x / 3.0f);
    }
    return 0.0f;
}

float boxKernel(float x) {
    x = std::abs(x);
    return (x <= 0.5f) ? 1.0f : 0.0f;
}

std::vector<Contribution> computeContributions(int srcLength, int dstLength,
                                              Resampling::FilterKernelFunc kernel, float radius) {
    std::vector<Contribution> contribs(dstLength);
    float scale = static_cast<float>(dstLength) / static_cast<float>(srcLength);
    float filterRadius = radius;
    float filterScale = 1.0f;

    if (scale < 1.0f) {
        // Downscaling: stretch kernel to filter out high frequencies (anti-aliasing)
        filterRadius = radius / scale;
        filterScale = scale;
    }

    for (int i = 0; i < dstLength; ++i) {
        float center = (i + 0.5f) / scale - 0.5f;
        int left = static_cast<int>(std::floor(center - filterRadius));
        int right = static_cast<int>(std::ceil(center + filterRadius));

        float totalWeight = 0.0f;
        for (int j = left; j <= right; ++j) {
            float dist = (j - center) * filterScale;
            float w = kernel(dist) * filterScale;
            if (std::abs(w) > 1e-6f) {
                int clampedIdx = std::clamp(j, 0, srcLength - 1);
                contribs[i].samples.push_back({clampedIdx, w});
                totalWeight += w;
            }
        }

        if (std::abs(totalWeight) > 1e-6f) {
            for (auto& s : contribs[i].samples) {
                s.weight /= totalWeight;
            }
        }
    }
    return contribs;
}

} // anonymous namespace

QString Resampling::algorithmName(ResampleAlgorithm algo) {
    switch (algo) {
        case ResampleAlgorithm::NearestNeighbor: return "Nearest Neighbor";
        case ResampleAlgorithm::Bilinear:        return "Bilinear";
        case ResampleAlgorithm::Bicubic:         return "Bicubic";
        case ResampleAlgorithm::Lanczos:         return "Lanczos";
        case ResampleAlgorithm::SuperSampling:   return "Super Sampling";
        default:                                 return "Bicubic";
    }
}

QImage Resampling::resample(const QImage& src, int dstWidth, int dstHeight, ResampleAlgorithm algo) {
    if (dstWidth <= 0 || dstHeight <= 0) return QImage();
    if (src.isNull()) return QImage(dstWidth, dstHeight, QImage::Format_ARGB32);
    if (src.width() == dstWidth && src.height() == dstHeight) {
        return src.copy();
    }

    switch (algo) {
        case ResampleAlgorithm::NearestNeighbor:
            return resampleNearestNeighbor(src, dstWidth, dstHeight);
        case ResampleAlgorithm::Bilinear:
            return resampleBilinear(src, dstWidth, dstHeight);
        case ResampleAlgorithm::Bicubic:
            return resampleBicubic(src, dstWidth, dstHeight);
        case ResampleAlgorithm::Lanczos:
            return resampleLanczos(src, dstWidth, dstHeight);
        case ResampleAlgorithm::SuperSampling:
            return resampleSuperSampling(src, dstWidth, dstHeight);
        default:
            return resampleBicubic(src, dstWidth, dstHeight);
    }
}

QImage Resampling::resampleNearestNeighbor(const QImage& src, int dstWidth, int dstHeight) {
    QImage srcImg = (src.format() == QImage::Format_ARGB32) ? src : src.convertToFormat(QImage::Format_ARGB32);
    QImage dst(dstWidth, dstHeight, QImage::Format_ARGB32);

    int srcW = srcImg.width();
    int srcH = srcImg.height();

    std::vector<int> mapX(dstWidth);
    for (int x = 0; x < dstWidth; ++x) {
        mapX[x] = std::clamp(static_cast<int>(x * static_cast<double>(srcW) / dstWidth), 0, srcW - 1);
    }

    for (int y = 0; y < dstHeight; ++y) {
        int sy = std::clamp(static_cast<int>(y * static_cast<double>(srcH) / dstHeight), 0, srcH - 1);
        const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(srcImg.constScanLine(sy));
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(dst.scanLine(y));

        for (int x = 0; x < dstWidth; ++x) {
            dstRow[x] = srcRow[mapX[x]];
        }
    }

    return dst;
}

QImage Resampling::resampleSeparable(const QImage& src, int dstWidth, int dstHeight,
                                    FilterKernelFunc kernel, float radius) {
    QImage srcPremul = (src.format() == QImage::Format_ARGB32_Premultiplied)
                        ? src
                        : src.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    int srcW = srcPremul.width();
    int srcH = srcPremul.height();

    // Pass 1: Horizontal scale (srcW x srcH -> dstWidth x srcH)
    auto contribsX = computeContributions(srcW, dstWidth, kernel, radius);
    QImage intermediate(dstWidth, srcH, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < srcH; ++y) {
        const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(srcPremul.constScanLine(y));
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(intermediate.scanLine(y));

        for (int x = 0; x < dstWidth; ++x) {
            float a = 0.0f, r = 0.0f, g = 0.0f, b = 0.0f;
            for (const auto& sample : contribsX[x].samples) {
                uint32_t pixel = srcRow[sample.index];
                float w = sample.weight;
                a += qAlpha(pixel) * w;
                r += qRed(pixel) * w;
                g += qGreen(pixel) * w;
                b += qBlue(pixel) * w;
            }
            int ia = std::clamp(static_cast<int>(std::round(a)), 0, 255);
            int ir = std::clamp(static_cast<int>(std::round(r)), 0, ia);
            int ig = std::clamp(static_cast<int>(std::round(g)), 0, ia);
            int ib = std::clamp(static_cast<int>(std::round(b)), 0, ia);
            dstRow[x] = qRgba(ir, ig, ib, ia);
        }
    }

    // Pass 2: Vertical scale (dstWidth x srcH -> dstWidth x dstHeight)
    auto contribsY = computeContributions(srcH, dstHeight, kernel, radius);
    QImage result(dstWidth, dstHeight, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < dstHeight; ++y) {
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(result.scanLine(y));
        const auto& cY = contribsY[y];

        for (int x = 0; x < dstWidth; ++x) {
            float a = 0.0f, r = 0.0f, g = 0.0f, b = 0.0f;
            for (const auto& sample : cY.samples) {
                const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(intermediate.constScanLine(sample.index));
                uint32_t pixel = srcRow[x];
                float w = sample.weight;
                a += qAlpha(pixel) * w;
                r += qRed(pixel) * w;
                g += qGreen(pixel) * w;
                b += qBlue(pixel) * w;
            }
            int ia = std::clamp(static_cast<int>(std::round(a)), 0, 255);
            int ir = std::clamp(static_cast<int>(std::round(r)), 0, ia);
            int ig = std::clamp(static_cast<int>(std::round(g)), 0, ia);
            int ib = std::clamp(static_cast<int>(std::round(b)), 0, ia);
            dstRow[x] = qRgba(ir, ig, ib, ia);
        }
    }

    return result.convertToFormat(QImage::Format_ARGB32);
}

QImage Resampling::resampleBilinear(const QImage& src, int dstWidth, int dstHeight) {
    return resampleSeparable(src, dstWidth, dstHeight, bilinearKernel, 1.0f);
}

QImage Resampling::resampleBicubic(const QImage& src, int dstWidth, int dstHeight) {
    return resampleSeparable(src, dstWidth, dstHeight, bicubicKernel, 2.0f);
}

QImage Resampling::resampleLanczos(const QImage& src, int dstWidth, int dstHeight) {
    return resampleSeparable(src, dstWidth, dstHeight, lanczosKernel, 3.0f);
}

QImage Resampling::resampleSuperSampling(const QImage& src, int dstWidth, int dstHeight) {
    if (dstWidth >= src.width() && dstHeight >= src.height()) {
        // When upscaling, box filter is nearest neighbor
        return resampleNearestNeighbor(src, dstWidth, dstHeight);
    }
    // When downscaling, box filter computes area averaging
    return resampleSeparable(src, dstWidth, dstHeight, boxKernel, 0.5f);
}

} // namespace pdn
