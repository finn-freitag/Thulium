#include <iostream>
#include <cassert>
#include <QApplication>
#include "../src/core/Document.h"
#include "../src/core/Resampling.h"
#include "../src/ui/Dialogs.h"

int main(int argc, char* argv[]) {
    int fakeArgc = 1;
    char* fakeArgv[] = { argv[0], nullptr };
    QApplication app(fakeArgc, fakeArgv);

    std::cout << "=== Running Image Operations & Resampling Test Suite ===" << std::endl;

    // --- Part 1: Resampling Algorithms Tests ---
    std::cout << "Test 1: Nearest Neighbor Pixel Art Scaling..." << std::endl;
    {
        // 2x2 image with 4 distinct colors:
        // (0,0) = Red, (1,0) = Green
        // (0,1) = Blue, (1,1) = Yellow
        QImage img(2, 2, QImage::Format_ARGB32);
        img.setPixelColor(0, 0, QColor(255, 0, 0, 255));
        img.setPixelColor(1, 0, QColor(0, 255, 0, 255));
        img.setPixelColor(0, 1, QColor(0, 0, 255, 255));
        img.setPixelColor(1, 1, QColor(255, 255, 0, 255));

        // Scale by 4x to 8x8 using Nearest Neighbor
        QImage scaled = pdn::Resampling::resample(img, 8, 8, pdn::ResampleAlgorithm::NearestNeighbor);
        assert(scaled.width() == 8);
        assert(scaled.height() == 8);

        // Every pixel in top-left 4x4 quadrant must be EXACT Red (no blending)
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                assert(scaled.pixelColor(x, y) == QColor(255, 0, 0, 255));
            }
        }
        // Top-right 4x4 quadrant must be EXACT Green
        for (int y = 0; y < 4; ++y) {
            for (int x = 4; x < 8; ++x) {
                assert(scaled.pixelColor(x, y) == QColor(0, 255, 0, 255));
            }
        }
        // Bottom-left 4x4 quadrant must be EXACT Blue
        for (int y = 4; y < 8; ++y) {
            for (int x = 0; x < 4; ++x) {
                assert(scaled.pixelColor(x, y) == QColor(0, 0, 255, 255));
            }
        }
        // Bottom-right 4x4 quadrant must be EXACT Yellow
        for (int y = 4; y < 8; ++y) {
            for (int x = 4; x < 8; ++x) {
                assert(scaled.pixelColor(x, y) == QColor(255, 255, 0, 255));
            }
        }
        std::cout << "  Passed: Nearest neighbor preserves exact blocks of pixels with no color corruption!" << std::endl;
    }

    std::cout << "Test 2: Bilinear, Bicubic, Lanczos, and Super Sampling Resampling..." << std::endl;
    {
        QImage img(4, 4, QImage::Format_ARGB32);
        img.fill(Qt::black);
        img.setPixelColor(1, 1, Qt::white);
        img.setPixelColor(2, 2, Qt::white);

        QImage bilinear = pdn::Resampling::resample(img, 16, 16, pdn::ResampleAlgorithm::Bilinear);
        assert(bilinear.width() == 16 && bilinear.height() == 16);

        QImage bicubic = pdn::Resampling::resample(img, 16, 16, pdn::ResampleAlgorithm::Bicubic);
        assert(bicubic.width() == 16 && bicubic.height() == 16);

        QImage lanczos = pdn::Resampling::resample(img, 16, 16, pdn::ResampleAlgorithm::Lanczos);
        assert(lanczos.width() == 16 && lanczos.height() == 16);

        QImage supersample = pdn::Resampling::resample(img, 2, 2, pdn::ResampleAlgorithm::SuperSampling);
        assert(supersample.width() == 2 && supersample.height() == 2);

        // Algorithm names
        assert(pdn::Resampling::algorithmName(pdn::ResampleAlgorithm::NearestNeighbor) == "Nearest Neighbor");
        assert(pdn::Resampling::algorithmName(pdn::ResampleAlgorithm::Bicubic) == "Bicubic");
        assert(pdn::Resampling::algorithmName(pdn::ResampleAlgorithm::Lanczos) == "Lanczos");

        std::cout << "  Passed: All resampling algorithms executed successfully!" << std::endl;
    }

    std::cout << "Test 3: Resize Dialog UI Resampling Dropdown..." << std::endl;
    {
        pdn::ResizeImageDialog dlg(200, 100);
        assert(dlg.newWidth() == 200);
        assert(dlg.newHeight() == 100);
        // Default algorithm should be Bicubic
        assert(dlg.algorithm() == pdn::ResampleAlgorithm::Bicubic);
        std::cout << "  Passed: ResizeImageDialog has functional algorithm dropdown!" << std::endl;
    }

    // --- Part 2: Image Menu Operations Undo/Redo Tests ---

    std::cout << "Test 4: Undo/Redo 'Resize'..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(100, 100);
        doc->activeLayer()->fill(Qt::white);
        doc->activeLayer()->image().setPixelColor(10, 10, Qt::red);

        // Perform resize to 200x200 with Nearest Neighbor
        doc->resizeImage(200, 200, pdn::ResampleAlgorithm::NearestNeighbor);
        assert(doc->width() == 200);
        assert(doc->height() == 200);
        assert(doc->activeLayer()->width() == 200);
        assert(doc->activeLayer()->height() == 200);
        assert(doc->undoStack()->canUndo());
        assert(doc->undoStack()->undoText() == "Resize");

        // Undo
        doc->undoStack()->undo();
        assert(doc->width() == 100);
        assert(doc->height() == 100);
        assert(doc->activeLayer()->width() == 100);
        assert(doc->activeLayer()->height() == 100);
        assert(doc->activeLayer()->image().pixelColor(10, 10) == Qt::red);

        // Redo
        doc->undoStack()->redo();
        assert(doc->width() == 200);
        assert(doc->height() == 200);
        assert(doc->activeLayer()->width() == 200);
        assert(doc->activeLayer()->height() == 200);

        std::cout << "  Passed: Resize undo and redo restore dimensions and layers correctly!" << std::endl;
    }

    std::cout << "Test 5: Undo/Redo 'Canvas Size'..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(100, 100);
        doc->activeLayer()->fill(Qt::white);
        doc->activeLayer()->image().setPixelColor(50, 50, Qt::green);

        doc->resizeCanvas(150, 120, Qt::AlignCenter);
        assert(doc->width() == 150);
        assert(doc->height() == 120);
        assert(doc->undoStack()->canUndo());
        assert(doc->undoStack()->undoText() == "Canvas Size");

        // Undo
        doc->undoStack()->undo();
        assert(doc->width() == 100);
        assert(doc->height() == 100);
        assert(doc->activeLayer()->image().pixelColor(50, 50) == Qt::green);

        // Redo
        doc->undoStack()->redo();
        assert(doc->width() == 150);
        assert(doc->height() == 120);

        std::cout << "  Passed: Canvas Size undo and redo work correctly!" << std::endl;
    }

    std::cout << "Test 6: Undo/Redo 'Crop to Selection'..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(100, 100);
        doc->activeLayer()->fill(Qt::white);
        doc->activeLayer()->image().setPixelColor(25, 35, Qt::blue);

        // Create selection (20, 30, 40, 50)
        doc->selection().addRect(QRectF(20, 30, 40, 50));
        assert(!doc->selection().isEmpty());

        doc->crop(QRect(20, 30, 40, 50));
        assert(doc->width() == 40);
        assert(doc->height() == 50);
        assert(doc->selection().isEmpty());
        assert(doc->undoStack()->canUndo());
        assert(doc->undoStack()->undoText() == "Crop to Selection");
        // Pixel at (25, 35) in old space is now at (5, 5) in cropped space
        assert(doc->activeLayer()->image().pixelColor(5, 5) == Qt::blue);

        // Undo
        doc->undoStack()->undo();
        assert(doc->width() == 100);
        assert(doc->height() == 100);
        assert(!doc->selection().isEmpty());
        assert(doc->selection().boundingRect().toRect() == QRect(20, 30, 40, 50));
        assert(doc->activeLayer()->image().pixelColor(25, 35) == Qt::blue);

        // Redo
        doc->undoStack()->redo();
        assert(doc->width() == 40);
        assert(doc->height() == 50);
        assert(doc->selection().isEmpty());

        std::cout << "  Passed: Crop to Selection undo restores canvas, layer pixels, and selection path!" << std::endl;
    }

    std::cout << "Test 7: Undo/Redo 'Flip Horizontal' and 'Flip Vertical'..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(100, 100);
        doc->activeLayer()->fill(Qt::white);
        doc->activeLayer()->image().setPixelColor(10, 20, Qt::red);

        doc->flipHorizontal();
        assert(doc->activeLayer()->image().pixelColor(89, 20) == Qt::red);
        assert(doc->undoStack()->undoText() == "Flip Horizontal");

        doc->undoStack()->undo();
        assert(doc->activeLayer()->image().pixelColor(10, 20) == Qt::red);

        doc->undoStack()->redo();
        assert(doc->activeLayer()->image().pixelColor(89, 20) == Qt::red);

        // Flip Vertical
        doc->flipVertical();
        assert(doc->activeLayer()->image().pixelColor(89, 79) == Qt::red);
        assert(doc->undoStack()->undoText() == "Flip Vertical");

        doc->undoStack()->undo();
        assert(doc->activeLayer()->image().pixelColor(89, 20) == Qt::red);

        doc->undoStack()->redo();
        assert(doc->activeLayer()->image().pixelColor(89, 79) == Qt::red);

        std::cout << "  Passed: Flip Horizontal and Flip Vertical undo/redo work correctly!" << std::endl;
    }

    std::cout << "Test 8: Undo/Redo Rotations (90 CW, 90 CCW, 180)..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(100, 50);
        doc->activeLayer()->fill(Qt::white);
        doc->activeLayer()->image().setPixelColor(10, 20, Qt::magenta);

        // Rotate 90 CW: 100x50 -> 50x100
        doc->rotate90CW();
        assert(doc->width() == 50);
        assert(doc->height() == 100);
        assert(doc->undoStack()->undoText() == "Rotate 90° Clockwise");

        doc->undoStack()->undo();
        assert(doc->width() == 100);
        assert(doc->height() == 50);
        assert(doc->activeLayer()->image().pixelColor(10, 20) == Qt::magenta);

        doc->undoStack()->redo();
        assert(doc->width() == 50);
        assert(doc->height() == 100);

        // Rotate 90 CCW: 50x100 -> 100x50
        doc->rotate90CCW();
        assert(doc->width() == 100);
        assert(doc->height() == 50);
        assert(doc->undoStack()->undoText() == "Rotate 90° Counter-Clockwise");

        doc->undoStack()->undo();
        assert(doc->width() == 50);
        assert(doc->height() == 100);

        doc->undoStack()->redo();
        assert(doc->width() == 100);
        assert(doc->height() == 50);

        // Rotate 180: 100x50 -> 100x50
        doc->rotate180();
        assert(doc->undoStack()->undoText() == "Rotate 180°");

        doc->undoStack()->undo();
        assert(doc->activeLayer()->image().pixelColor(10, 20) == Qt::magenta);

        doc->undoStack()->redo();

        std::cout << "  Passed: All rotation operations have working undo/redo!" << std::endl;
    }

    std::cout << "Test 9: Undo/Redo 'Flatten Image'..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(100, 100);
        doc->activeLayer()->setName("Background");
        doc->activeLayer()->fill(Qt::white);

        auto layer2 = doc->addLayer("Overlay");
        layer2->fill(Qt::transparent);
        layer2->image().setPixelColor(30, 30, Qt::red);

        assert(doc->layerCount() == 2);
        assert(doc->activeLayerIndex() == 1);

        doc->flatten();
        assert(doc->layerCount() == 1);
        assert(doc->activeLayerIndex() == 0);
        assert(doc->activeLayer()->name() == "Background");
        assert(doc->undoStack()->undoText() == "Flatten Image");
        // Flattened composite has the red pixel over white
        assert(doc->activeLayer()->image().pixelColor(30, 30) == Qt::red);

        // Undo flatten
        doc->undoStack()->undo();
        assert(doc->layerCount() == 2);
        assert(doc->activeLayerIndex() == 1);
        assert(doc->layer(0)->name() == "Background");
        assert(doc->layer(1)->name() == "Overlay");

        // Redo flatten
        doc->undoStack()->redo();
        assert(doc->layerCount() == 1);
        assert(doc->activeLayerIndex() == 0);

        std::cout << "  Passed: Flatten Image properly restores all original layers on undo!" << std::endl;
    }

    std::cout << "\nALL IMAGE OPERATIONS AND RESAMPLING TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
