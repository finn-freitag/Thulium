#include <iostream>
#include <cassert>
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include "../src/core/Document.h"
#include "../src/tools/ToolManager.h"
#include "../src/tools/SelectionTools.h"
#include "../src/tools/BrushTools.h"
#include "../src/tools/TextTool.h"
#include "../src/tools/ShapeTools.h"
#include "../src/ui/ColorsDock.h"
#include "../src/ui/CanvasView.h"

int main(int argc, char* argv[]) {
    int fakeArgc = 1;
    char* fakeArgv[] = { argv[0], nullptr };
    QApplication app(fakeArgc, fakeArgv);

    std::cout << "=== Running Fixes Test Suite ===" << std::endl;

    // Test 1: ColorWheelWidget image rendering
    {
        std::cout << "Test 1: ColorWheelWidget rendering..." << std::endl;
        pdn::ColorWheelWidget wheel;
        wheel.setColor(Qt::black); // Default color

        // Force paint event to trigger renderWheelImage
        QPixmap pixmap(160, 140);
        wheel.render(&pixmap);

        // Sample center of wheel (radius 60, center at 70, 70 in widget space)
        QImage img = pixmap.toImage();
        QColor centerColor = img.pixelColor(70, 70);

        // Center should be white (saturation 0, value 255)
        assert(centerColor.red() >= 250);
        assert(centerColor.green() >= 250);
        assert(centerColor.blue() >= 250);

        // Top of wheel (angle 90 deg -> Hue ~90 deg or green/cyan) should be fully saturated and bright
        QColor topColor = img.pixelColor(70, 30);
        assert(topColor.value() >= 250);
        assert(topColor.saturation() >= 100);

        std::cout << "  Passed: Color wheel renders properly with full brightness and correct saturation!" << std::endl;
    }

    // Test 2: Magic Wand selection
    {
        std::cout << "Test 2: Magic Wand selection..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(100, 100);
        auto layer = doc->activeLayer();

        // Fill background with white
        layer->fill(Qt::white);

        // Draw a solid red square in the middle: (30,30) to (70,70)
        for (int y = 30; y < 70; ++y) {
            for (int x = 30; x < 70; ++x) {
                layer->scanLine(y)[x] = 0xFFFF0000; // Red ARGB
            }
        }

        pdn::MagicWandTool wand;
        pdn::ToolContext ctx;
        ctx.tolerance = 10;
        ctx.selectionCombineMode = pdn::SelectionCombineMode::Replace;

        // Click inside the red square
        wand.mousePress(nullptr, doc.get(), QPointF(50, 50), ctx);

        assert(!doc->selection().isEmpty());
        QRectF bounds = doc->selection().boundingRect();
        assert(bounds.x() == 30);
        assert(bounds.y() == 30);
        assert(bounds.width() == 40);
        assert(bounds.height() == 40);

        // Inside should be selected
        assert(doc->selection().contains(QPointF(50, 50)));
        // Outside should NOT be selected
        assert(!doc->selection().contains(QPointF(10, 10)));
        assert(!doc->selection().contains(QPointF(80, 80)));

        std::cout << "  Passed: Magic Wand flood selection is accurate and bounds match perfectly!" << std::endl;
    }

    // Test 3: Selection tools deselect on click without move/hold
    {
        std::cout << "Test 3: Selection click-to-deselect..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(100, 100);
        doc->selection().selectAll(100, 100);
        assert(!doc->selection().isEmpty());

        pdn::ToolContext ctx;
        ctx.selectionCombineMode = pdn::SelectionCombineMode::Replace;

        pdn::RectangleSelectTool rectTool;
        // Press at (20, 20) and release at (20, 20)
        rectTool.mousePress(nullptr, doc.get(), QPointF(20, 20), ctx);
        rectTool.mouseRelease(nullptr, doc.get(), QPointF(20, 20), ctx);

        assert(doc->selection().isEmpty());

        // Now test EllipseSelectTool
        doc->selection().selectAll(100, 100);
        assert(!doc->selection().isEmpty());

        pdn::EllipseSelectTool ellipseTool;
        ellipseTool.mousePress(nullptr, doc.get(), QPointF(20, 20), ctx);
        ellipseTool.mouseRelease(nullptr, doc.get(), QPointF(20, 20), ctx);

        assert(doc->selection().isEmpty());

        // Now test LassoSelectTool
        doc->selection().selectAll(100, 100);
        assert(!doc->selection().isEmpty());

        pdn::LassoSelectTool lassoTool;
        lassoTool.mousePress(nullptr, doc.get(), QPointF(20, 20), ctx);
        lassoTool.mouseRelease(nullptr, doc.get(), QPointF(20, 20), ctx);

        assert(doc->selection().isEmpty());

        std::cout << "  Passed: Clicking without dragging releases the selection across all selection tools!" << std::endl;
    }

    // Test 4: ToolManager document tracking and TextTool persistence on tool switch
    {
        std::cout << "Test 4: TextTool persistence on tool switch..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(200, 100);
        auto layer = doc->activeLayer();
        layer->fill(Qt::white);

        pdn::ToolManager toolMgr;
        toolMgr.setDocument(doc.get());

        toolMgr.setActiveTool(pdn::ToolType::Text);
        auto textTool = std::dynamic_pointer_cast<pdn::TextTool>(toolMgr.activeTool());
        assert(textTool != nullptr);

        toolMgr.context().primaryColor = Qt::black;
        toolMgr.context().font = QFont("Arial", 16);

        // Click to place text
        textTool->mousePress(nullptr, doc.get(), QPointF(20, 20), toolMgr.context());

        // Type "Hello"
        QKeyEvent keyH(QEvent::KeyPress, Qt::Key_H, Qt::NoModifier, "H");
        textTool->keyPress(&keyH, doc.get(), toolMgr.context());
        QKeyEvent keyE(QEvent::KeyPress, Qt::Key_E, Qt::NoModifier, "i");
        textTool->keyPress(&keyE, doc.get(), toolMgr.context());

        // Now switch tools WITHOUT manually pressing enter (e.g. user clicked Brush tool)
        toolMgr.setActiveTool(pdn::ToolType::Paintbrush);

        // Verify text was committed to layer->image() instead of disappearing
        bool foundNonWhite = false;
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 200; ++x) {
                if (layer->scanLine(y)[x] != 0xFFFFFFFF) {
                    foundNonWhite = true;
                    break;
                }
            }
            if (foundNonWhite) break;
        }
        assert(foundNonWhite);
        std::cout << "  Passed: Text commits to layer when switching tools!" << std::endl;
    }

    // Test 5: LineCurveTool persistence on tool switch and Return key
    {
        std::cout << "Test 5: LineCurveTool persistence..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(200, 100);
        auto layer = doc->activeLayer();
        layer->fill(Qt::white);

        pdn::ToolManager toolMgr;
        toolMgr.setDocument(doc.get());

        toolMgr.setActiveTool(pdn::ToolType::LineCurve);
        auto lineTool = std::dynamic_pointer_cast<pdn::LineCurveTool>(toolMgr.activeTool());
        assert(lineTool != nullptr);

        toolMgr.context().primaryColor = Qt::red;
        toolMgr.context().brushWidth = 4;

        // Draw line from (10, 50) to (190, 50)
        lineTool->mousePress(nullptr, doc.get(), QPointF(10, 50), toolMgr.context());
        lineTool->mouseMove(nullptr, doc.get(), QPointF(190, 50), toolMgr.context());
        lineTool->mouseRelease(nullptr, doc.get(), QPointF(190, 50), toolMgr.context());

        // Curve handle adjustment: drag near middle
        lineTool->mousePress(nullptr, doc.get(), QPointF(70, 50), toolMgr.context());
        lineTool->mouseMove(nullptr, doc.get(), QPointF(70, 20), toolMgr.context());
        lineTool->mouseRelease(nullptr, doc.get(), QPointF(70, 20), toolMgr.context());

        // Switch tools to commit
        toolMgr.setActiveTool(pdn::ToolType::Paintbrush);

        // Verify curvy line was committed to layer
        bool foundRed = false;
        for (int y = 0; y < 100; ++y) {
            for (int x = 0; x < 200; ++x) {
                uint32_t px = layer->scanLine(y)[x];
                if (((px >> 16) & 0xFF) > 200 && ((px >> 8) & 0xFF) < 50) {
                    foundRed = true;
                    break;
                }
            }
            if (foundRed) break;
        }
        assert(foundRed);
        std::cout << "  Passed: Curvy line commits to layer when switching tools!" << std::endl;
    }

    // Test 6: ColorPickerTool updates context and triggers contextChanged signal
    {
        std::cout << "Test 6: ColorPickerTool UI sync..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(100, 100);
        auto layer = doc->activeLayer();
        layer->fill(QColor(255, 0, 0)); // pure red
        // set (10, 10) to blue
        layer->scanLine(10)[10] = 0xFF0000FF; // blue ARGB

        pdn::ToolManager toolMgr;
        toolMgr.setDocument(doc.get());

        bool contextChangedFired = false;
        QObject::connect(&toolMgr, &pdn::ToolManager::contextChanged, [&]() {
            contextChangedFired = true;
        });

        pdn::ColorPickerTool picker;

        // Left click at (10, 10) -> Primary color becomes blue
        QMouseEvent leftClick(QEvent::MouseButtonPress, QPointF(10, 10), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        picker.mousePress(&leftClick, doc.get(), QPointF(10, 10), toolMgr.context());

        assert(contextChangedFired);
        assert(toolMgr.context().primaryColor == QColor(0, 0, 255));

        // Right click at (50, 50) -> Secondary color becomes red
        contextChangedFired = false;
        QMouseEvent rightClick(QEvent::MouseButtonPress, QPointF(50, 50), Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        picker.mousePress(&rightClick, doc.get(), QPointF(50, 50), toolMgr.context());

        assert(contextChangedFired);
        assert(toolMgr.context().secondaryColor == QColor(255, 0, 0));

        std::cout << "  Passed: ColorPickerTool updates context and fires contextChanged for ColorsDock!" << std::endl;
    }

    // Test 7: Shift + Scroll horizontal scrolling in CanvasView
    {
        std::cout << "Test 7: Shift + Scroll horizontal scroll..." << std::endl;
        pdn::ToolManager toolMgr;
        pdn::CanvasView canvas(&toolMgr);
        auto doc = std::make_shared<pdn::Document>(800, 600);
        canvas.setDocument(doc);

        QPointF initialPan = canvas.panOffset();

        // Send wheel event with ShiftModifier and vertical wheel delta 120 (standard mouse wheel notch)
        QWheelEvent wheelEvent(
            QPointF(100, 100),
            QPointF(100, 100),
            QPoint(0, 0),
            QPoint(0, 120),
            Qt::NoButton,
            Qt::ShiftModifier,
            Qt::NoScrollPhase,
            false
        );
        QApplication::sendEvent(&canvas, &wheelEvent);

        QPointF newPan = canvas.panOffset();
        // X pan should have shifted by 120 / 2.0 = 60.0, Y should remain unchanged
        assert(newPan.x() == initialPan.x() + 60.0);
        assert(newPan.y() == initialPan.y());

        std::cout << "  Passed: Shift + Scroll successfully pans the canvas horizontally!" << std::endl;
    }

    // Test 8: Magic Wand selection creates clean outline without interior scanline edges
    {
        std::cout << "Test 8: Magic Wand selection outline simplification..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(100, 100);
        auto layer = doc->activeLayer();
        layer->fill(Qt::white);

        // Solid 40x40 square: (30,30) to (70,70)
        for (int y = 30; y < 70; ++y) {
            for (int x = 30; x < 70; ++x) {
                layer->scanLine(y)[x] = 0xFFFF0000;
            }
        }

        pdn::MagicWandTool wand;
        pdn::ToolContext ctx;
        ctx.tolerance = 10;
        ctx.selectionCombineMode = pdn::SelectionCombineMode::Replace;

        wand.mousePress(nullptr, doc.get(), QPointF(50, 50), ctx);

        QPainterPath path = doc->selection().path();
        // Without simplified(), elementCount would be 40 scanline rects * 5 elements = 200 elements.
        // With simplified(), elementCount is exactly 5 elements (MoveTo + 4 LineTo).
        assert(path.elementCount() == 5);

        // Donut / ring test: shape with an actual hole
        layer->fill(Qt::white);
        // Outer square 20..80 (60x60)
        for (int y = 20; y < 80; ++y) {
            for (int x = 20; x < 80; ++x) {
                layer->scanLine(y)[x] = 0xFF00FF00; // green
            }
        }
        // Inner hole 40..60 (20x20) filled back to white
        for (int y = 40; y < 60; ++y) {
            for (int x = 40; x < 60; ++x) {
                layer->scanLine(y)[x] = 0xFFFFFFFF; // white
            }
        }

        // Click on green ring at (30, 30)
        wand.mousePress(nullptr, doc.get(), QPointF(30, 30), ctx);

        QPainterPath donutPath = doc->selection().path();
        // Ring should contain green pixels but NOT inner hole pixels
        assert(doc->selection().containsPixel(30, 30));
        assert(doc->selection().containsPixel(70, 70));
        assert(!doc->selection().containsPixel(50, 50)); // inside the hole
        assert(!doc->selection().containsPixel(10, 10)); // outside

        // A ring has an outer polygon and an inner hole polygon: exactly 2 subpaths, 10 elements
        assert(donutPath.elementCount() == 10);

        std::cout << "  Passed: Magic Wand creates outline-only paths (no internal scanlines) and preserves hole boundaries!" << std::endl;
    }

    std::cout << "=== All Tests Passed Successfully! ===" << std::endl;
    return 0;
}
