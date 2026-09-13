#include <iostream>
#include <cassert>
#include <QImage>
#include <QRect>
#include "../src/core/Selection.h"
#include "../src/effects/BrightnessContrast.h"
#include "../src/effects/GaussianBlur.h"
#include "../src/effects/Adjustments.h"
#include "../src/effects/BlurEffects.h"
#include "../src/effects/DistortEffects.h"
#include "../src/effects/NoiseEffects.h"
#include "../src/effects/PhotoEffects.h"
#include "../src/effects/StylizeEffects.h"

int main() {
    std::cout << "=== Comprehensive Effects & Adjustments Test Suite ===" << std::endl;

    pdn::Selection emptySel(64, 64);

    // 1. Test Brightness / Contrast
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(100, 100, 100, 255));
        pdn::BrightnessContrastEffect::process(img, emptySel, 20, 0);
        QColor c = img.pixelColor(10, 10);
        std::cout << "[1] Brightness +20: R=" << c.red() << " (expected ~120)" << std::endl;
        assert(c.red() > 110 && c.red() < 130);
    }

    // 2. Test Gaussian Blur
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(Qt::black);
        img.setPixelColor(32, 32, Qt::white);
        pdn::GaussianBlurEffect::process(img, emptySel, 3);
        QColor neighbor = img.pixelColor(32, 33);
        std::cout << "[2] Gaussian Blur neighbor: R=" << neighbor.red() << " (expected > 0)" << std::endl;
        assert(neighbor.red() > 0);
    }

    // 3. Test Black and White
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(255, 0, 0, 255)); // Pure red
        pdn::BlackAndWhiteEffect::process(img, emptySel);
        QColor c = img.pixelColor(10, 10);
        std::cout << "[3] Black and White from Red: R=" << c.red() << ", G=" << c.green() << ", B=" << c.blue() << std::endl;
        assert(c.red() == c.green() && c.green() == c.blue());
        assert(c.red() >= 70 && c.red() <= 80); // 0.299 * 255 ~ 76
    }

    // 4. Test Invert Colors
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(20, 50, 100, 255));
        pdn::InvertColorsEffect::process(img, emptySel);
        QColor c = img.pixelColor(10, 10);
        std::cout << "[4] Invert Colors: R=" << c.red() << ", G=" << c.green() << ", B=" << c.blue() << std::endl;
        assert(c.red() == 235 && c.green() == 205 && c.blue() == 155);
        assert(c.alpha() == 255);
    }

    // 5. Test Invert Alpha
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(100, 150, 200, 80));
        pdn::InvertAlphaEffect::process(img, emptySel);
        QColor c = img.pixelColor(10, 10);
        std::cout << "[5] Invert Alpha: Alpha=" << c.alpha() << " (expected 175)" << std::endl;
        assert(c.alpha() == 175);
        assert(c.red() == 100 && c.green() == 150 && c.blue() == 200);
    }

    // 6. Test Auto-Level
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        // Low contrast range [50, 100]
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                int val = 50 + (x * 50) / 63;
                img.setPixelColor(x, y, QColor(val, val, val, 255));
            }
        }
        pdn::AutoLevelEffect::process(img, emptySel);
        QColor left = img.pixelColor(0, 32);
        QColor right = img.pixelColor(63, 32);
        std::cout << "[6] Auto-Level: Left R=" << left.red() << " (stretched down), Right R=" << right.red() << " (stretched up)" << std::endl;
        assert(left.red() <= 10);
        assert(right.red() >= 245);
    }

    // 7. Test Sepia
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(128, 128, 128, 255));
        pdn::SepiaEffect::process(img, emptySel);
        QColor c = img.pixelColor(10, 10);
        std::cout << "[7] Sepia: R=" << c.red() << ", G=" << c.green() << ", B=" << c.blue() << std::endl;
        assert(c.red() > c.green() && c.green() > c.blue()); // Warm amber signature
    }

    // 8. Test Posterize
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(120, 120, 120, 255));
        pdn::PosterizeEffect::process(img, emptySel, 4); // 4 levels: 0, 85, 170, 255
        QColor c = img.pixelColor(10, 10);
        std::cout << "[8] Posterize 4 levels: R=" << c.red() << std::endl;
        assert(c.red() == 85 || c.red() == 170);
    }

    // 9. Test Hue / Saturation
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(200, 50, 50, 255));
        // Desaturate to 0% -> must be gray
        pdn::HueSaturationEffect::process(img, emptySel, 0, 0, 0);
        QColor c = img.pixelColor(10, 10);
        std::cout << "[9] Hue/Saturation (Sat=0%): R=" << c.red() << ", G=" << c.green() << ", B=" << c.blue() << std::endl;
        assert(c.red() == c.green() && c.green() == c.blue());
    }

    // 10. Test Temperature / Tint
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(128, 128, 128, 255));
        pdn::TemperatureTintEffect::process(img, emptySel, 50, 0); // Warm
        QColor c = img.pixelColor(10, 10);
        std::cout << "[10] Temperature (Warm +50): R=" << c.red() << ", B=" << c.blue() << std::endl;
        assert(c.red() > 128 && c.blue() < 128);
    }

    // 11. Test Pixelate
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        // Fill with gradient
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                img.setPixelColor(x, y, QColor(x * 4, y * 4, 100, 255));
            }
        }
        pdn::PixelateEffect::process(img, emptySel, 8);
        // All pixels within block [0..7, 0..7] must be equal
        QRgb blockColor = img.pixel(0, 0);
        for (int dy = 0; dy < 8; ++dy) {
            for (int dx = 0; dx < 8; ++dx) {
                assert(img.pixel(dx, dy) == blockColor);
            }
        }
        std::cout << "[11] Pixelate: 8x8 cell uniform test passed." << std::endl;
    }

    // 12. Test Add Noise
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(128, 128, 128, 255));
        pdn::AddNoiseEffect::process(img, emptySel, 50, 0, 100);
        bool hasVariation = false;
        QRgb base = qRgba(128, 128, 128, 255);
        for (int y = 0; y < 64 && !hasVariation; ++y) {
            for (int x = 0; x < 64; ++x) {
                if (img.pixel(x, y) != base) {
                    hasVariation = true;
                    break;
                }
            }
        }
        std::cout << "[12] Add Noise: variation introduced=" << (hasVariation ? "yes" : "no") << std::endl;
        assert(hasVariation);
    }

    // 13. Test Median
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(50, 50, 50, 255));
        // Single outlier salt noise
        img.setPixelColor(32, 32, Qt::white);
        pdn::MedianEffect::process(img, emptySel, 2, 50);
        QColor cleaned = img.pixelColor(32, 32);
        std::cout << "[13] Median noise cleanup: R=" << cleaned.red() << " (expected 50)" << std::endl;
        assert(cleaned.red() == 50);
    }

    // 14. Test Sharpen
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(100, 100, 100, 255));
        img.setPixelColor(32, 32, QColor(200, 200, 200, 255));
        pdn::SharpenEffect::process(img, emptySel, 10);
        QColor center = img.pixelColor(32, 32);
        std::cout << "[14] Sharpen: Center R=" << center.red() << " (enhanced > 200)" << std::endl;
        assert(center.red() > 200);
    }

    // 15. Test Glow
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(50, 50, 50, 255));
        img.setPixelColor(32, 32, Qt::white);
        pdn::GlowEffect::process(img, emptySel, 5, 20, 20);
        QColor glowNeighbor = img.pixelColor(32, 33);
        std::cout << "[15] Glow bloom neighbor: R=" << glowNeighbor.red() << " (expected > 50)" << std::endl;
        assert(glowNeighbor.red() > 50);
    }

    // 16. Test Vignette
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(200, 200, 200, 255));
        pdn::VignetteEffect::process(img, emptySel, 40, 80);
        QColor center = img.pixelColor(32, 32);
        QColor corner = img.pixelColor(0, 0);
        std::cout << "[16] Vignette: Center R=" << center.red() << ", Corner R=" << corner.red() << std::endl;
        assert(center.red() == 200);
        assert(corner.red() < 100);
    }

    // 17. Test Edge Detect
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(Qt::black);
        for (int y = 0; y < 64; ++y) {
            for (int x = 32; x < 64; ++x) {
                img.setPixelColor(x, y, Qt::white);
            }
        }
        pdn::EdgeDetectEffect::process(img, emptySel, 5);
        QColor edge = img.pixelColor(31, 32);
        QColor uniform = img.pixelColor(10, 32);
        std::cout << "[17] Edge Detect: Edge R=" << edge.red() << ", Uniform R=" << uniform.red() << std::endl;
        assert(edge.red() > 100);
        assert(uniform.red() == 0);
    }

    // 18. Test Emboss
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(100, 100, 100, 255));
        pdn::EmbossEffect::process(img, emptySel, 45);
        QColor c = img.pixelColor(32, 32);
        std::cout << "[18] Emboss neutral plane: R=" << c.red() << " (expected 128)" << std::endl;
        assert(c.red() == 128);
    }

    // 19. Test Oil Painting
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(100, 150, 200, 255));
        pdn::OilPaintingEffect::process(img, emptySel, 2, 20);
        QColor c = img.pixelColor(32, 32);
        std::cout << "[19] Oil Painting: R=" << c.red() << ", G=" << c.green() << ", B=" << c.blue() << std::endl;
        assert(c.red() > 90 && c.blue() > 190);
    }

    // 20. Selection Clipping Test
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(100, 100, 100, 255));

        pdn::Selection sel(64, 64);
        sel.addRect(QRectF(10, 10, 20, 20)); // Only [10..29, 10..29]

        pdn::InvertColorsEffect::process(img, sel);

        QColor inside = img.pixelColor(15, 15);
        QColor outside = img.pixelColor(5, 5);
        std::cout << "[20] Selection Clipping: Inside R=" << inside.red() << " (inverted to 155), Outside R=" << outside.red() << " (kept at 100)" << std::endl;
        assert(inside.red() == 155);
        assert(outside.red() == 100);
    }

    // 21. Test Custom Center Point in Vignette
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(QColor(200, 200, 200, 255));
        // Put center at top-left corner (0, 0)
        pdn::VignetteEffect::process(img, emptySel, 40, 80, QPointF(0, 0));
        QColor atCenter = img.pixelColor(0, 0);
        QColor atFarCorner = img.pixelColor(63, 63);
        std::cout << "[21] Vignette with Center (0,0): Center R=" << atCenter.red() << ", Far Corner R=" << atFarCorner.red() << std::endl;
        assert(atCenter.red() == 200);
        assert(atFarCorner.red() < 100);
    }

    // 22. Test Custom Center Point in Twist
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(Qt::black);
        // Draw cross lines through (16, 16)
        for (int i = 0; i < 64; ++i) {
            img.setPixelColor(16, i, Qt::white);
            img.setPixelColor(i, 16, Qt::white);
        }
        pdn::TwistEffect::process(img, emptySel, 60, 40, QPointF(16, 16));
        // The center itself at (16, 16) stays white
        QColor centerPx = img.pixelColor(16, 16);
        std::cout << "[22] Twist with Center (16,16): Center R=" << centerPx.red() << " (expected 255)" << std::endl;
        assert(centerPx.red() == 255);
    }

    // 23. Test ICanvasInteractionBridge
    {
        class MockReceiver : public pdn::ICanvasPointReceiver {
        public:
            QPointF pickedPoint;
            bool called = false;
            void onCanvasPointPicked(const QPointF& docPos) override {
                pickedPoint = docPos;
                called = true;
            }
        };

        class MockBridge : public pdn::ICanvasInteractionBridge {
        public:
            pdn::ICanvasPointReceiver* receiver = nullptr;
            pdn::ICanvasOverlayProvider* provider = nullptr;
            bool repainted = false;

            void setPointReceiver(pdn::ICanvasPointReceiver* r) override { receiver = r; }
            pdn::ICanvasPointReceiver* pointReceiver() const override { return receiver; }

            void setOverlayProvider(pdn::ICanvasOverlayProvider* p) override { provider = p; }
            pdn::ICanvasOverlayProvider* overlayProvider() const override { return provider; }

            void requestCanvasRepaint() override { repainted = true; }
        };

        MockBridge bridge;
        MockReceiver receiver;
        bridge.setPointReceiver(&receiver);
        assert(bridge.pointReceiver() == &receiver);

        bridge.pointReceiver()->onCanvasPointPicked(QPointF(42.5, 99.0));
        assert(receiver.called);
        assert(receiver.pickedPoint == QPointF(42.5, 99.0));

        bridge.requestCanvasRepaint();
        assert(bridge.repainted);
        std::cout << "[23] ICanvasInteractionBridge coordinate picking test passed." << std::endl;
    }

    std::cout << "\n>>> ALL 23 Effects, Adjustments, Center Picking & Bridge tests PASSED successfully! <<<\n" << std::endl;
    return 0;
}
