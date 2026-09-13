#include <iostream>
#include <cassert>
#include <QImage>
#include "../src/core/Selection.h"
#include "../src/effects/BrightnessContrast.h"
#include "../src/effects/GaussianBlur.h"

int main() {
    std::cout << "=== Effects & Adjustments Test ===" << std::endl;

    // Test Brightness / Contrast
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(QColor(100, 100, 100, 255));

    pdn::Selection sel(64, 64);
    pdn::BrightnessContrastEffect::process(img, sel, 20, 0);
    QColor c = img.pixelColor(10, 10);
    std::cout << "Brightness +20 test: result color R=" << c.red() << " (expected ~120)" << std::endl;
    assert(c.red() > 100);

    // Test Gaussian Blur
    img.fill(Qt::black);
    img.setPixelColor(32, 32, Qt::white);
    pdn::GaussianBlurEffect::process(img, sel, 3);
    QColor neighbor = img.pixelColor(32, 33);
    std::cout << "Gaussian Blur test: blurred neighbor color R=" << neighbor.red() << " (expected > 0)" << std::endl;
    assert(neighbor.red() > 0);

    std::cout << "All Effects tests passed!" << std::endl;
    return 0;
}
