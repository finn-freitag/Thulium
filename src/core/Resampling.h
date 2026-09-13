#pragma once

#include <QImage>
#include <QString>

namespace pdn {

enum class ResampleAlgorithm {
    NearestNeighbor = 0,
    Bilinear,
    Bicubic,
    Lanczos,
    SuperSampling
};

class Resampling {
public:
    typedef float (*FilterKernelFunc)(float x);
    static QImage resample(const QImage& src, int dstWidth, int dstHeight, ResampleAlgorithm algo);
    static QString algorithmName(ResampleAlgorithm algo);

private:
    static QImage resampleNearestNeighbor(const QImage& src, int dstWidth, int dstHeight);
    static QImage resampleBilinear(const QImage& src, int dstWidth, int dstHeight);
    static QImage resampleBicubic(const QImage& src, int dstWidth, int dstHeight);
    static QImage resampleLanczos(const QImage& src, int dstWidth, int dstHeight);
    static QImage resampleSuperSampling(const QImage& src, int dstWidth, int dstHeight);

    static QImage resampleSeparable(const QImage& src, int dstWidth, int dstHeight,
                                    FilterKernelFunc kernel, float radius);
};

} // namespace pdn
