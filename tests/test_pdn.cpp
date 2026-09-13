#include <QCoreApplication>
#include <iostream>
#include <cassert>
#include "../src/io/PdnFormat.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    std::string pdnPath = "Card.pdn";
    if (argc > 1) {
        pdnPath = argv[1];
    }

    std::cout << "=== PDN Format Verification Test ===" << std::endl;
    std::cout << "Loading: " << pdnPath << std::endl;

    QString err;
    auto doc = pdn::PdnFormat::load(QString::fromStdString(pdnPath), &err);
    if (!doc) {
        std::cerr << "FAIL: Failed to load PDN: " << err.toStdString() << std::endl;
        return 1;
    }

    std::cout << "SUCCESS: PDN Loaded!" << std::endl;
    std::cout << "Dimensions: " << doc->width() << " x " << doc->height() << std::endl;
    std::cout << "Layer count: " << doc->layerCount() << std::endl;

    if (doc->width() != 870 || doc->height() != 560) {
        std::cerr << "FAIL: Dimensions mismatch (expected 870x560, got " << doc->width() << "x" << doc->height() << ")" << std::endl;
        return 1;
    }
    if (doc->layerCount() != 5) {
        std::cerr << "FAIL: Layer count mismatch (expected 5, got " << doc->layerCount() << ")" << std::endl;
        return 1;
    }

    for (int i = 0; i < doc->layerCount(); ++i) {
        auto l = doc->layer(i);
        std::cout << "  Layer " << i << ": '" << l->name().toStdString()
                  << "' (Visible: " << (l->isVisible() ? "true" : "false")
                  << ", Opacity: " << static_cast<int>(l->opacity())
                  << ", BlendMode: " << pdn::getBlendModeName(l->blendMode()).toStdString()
                  << ", Size: " << l->width() << "x" << l->height() << ")" << std::endl;
    }

    // Roundtrip Save & Reload Test
    QString tempSave = "/tmp/test_pdn_roundtrip.pdn";
    std::cout << "Testing save to: " << tempSave.toStdString() << std::endl;
    if (!pdn::PdnFormat::save(*doc, tempSave, &err)) {
        std::cerr << "FAIL: Failed to save PDN: " << err.toStdString() << std::endl;
        return 1;
    }

    std::cout << "Reloading saved PDN..." << std::endl;
    auto reloadedDoc = pdn::PdnFormat::load(tempSave, &err);
    if (!reloadedDoc) {
        std::cerr << "FAIL: Failed to reload saved PDN: " << err.toStdString() << std::endl;
        return 1;
    }

    if (reloadedDoc->width() != doc->width() || reloadedDoc->height() != doc->height()) {
        std::cerr << "FAIL: Reloaded dimensions do not match original!" << std::endl;
        return 1;
    }
    if (reloadedDoc->layerCount() != doc->layerCount()) {
        std::cerr << "FAIL: Reloaded layer count does not match original!" << std::endl;
        return 1;
    }

    // Compare pixels
    int totalPixels = doc->width() * doc->height();
    for (int i = 0; i < doc->layerCount(); ++i) {
        auto origL = doc->layer(i);
        auto reloadL = reloadedDoc->layer(i);
        const uint32_t* origPx = origL->bits();
        const uint32_t* reloadPx = reloadL->bits();
        int diffCount = 0;
        for (int p = 0; p < totalPixels; ++p) {
            if (origPx[p] != reloadPx[p]) {
                diffCount++;
            }
        }
        std::cout << "  Layer " << i << " pixel differences: " << diffCount << std::endl;
        if (diffCount > 0) {
            std::cerr << "FAIL: Layer " << i << " has pixel mismatch after roundtrip!" << std::endl;
            return 1;
        }
    }

    std::cout << "ALL PDN TESTS PASSED WITH 100% EXACTNESS!" << std::endl;
    return 0;
}
