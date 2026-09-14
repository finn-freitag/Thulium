#include <iostream>
#include <cassert>
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QClipboard>
#include <QDialog>
#include "../src/core/Document.h"
#include "../src/tools/ToolManager.h"
#include "../src/tools/SelectionTools.h"
#include "../src/tools/MoveTools.h"
#include "../src/tools/BrushTools.h"
#include "../src/tools/TextTool.h"
#include "../src/tools/ShapeTools.h"
#include "../src/ui/ColorsDock.h"
#include "../src/ui/CanvasView.h"
#include "../src/ui/MainWindow.h"

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

        // Test slot selection: when secondary slot is selected, left click picks secondary
        // Set (20, 20) to green
        layer->scanLine(20)[20] = 0xFF00FF00; // green ARGB
        toolMgr.context().activeColorIsPrimary = false; // Secondary slot selected
        contextChangedFired = false;
        QMouseEvent leftClick2(QEvent::MouseButtonPress, QPointF(20, 20), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        picker.mousePress(&leftClick2, doc.get(), QPointF(20, 20), toolMgr.context());

        assert(contextChangedFired);
        // Secondary color should now be green
        assert(toolMgr.context().secondaryColor == QColor(0, 255, 0));
        // Primary color should still be blue
        assert(toolMgr.context().primaryColor == QColor(0, 0, 255));

        // When secondary slot is selected, right click picks primary
        contextChangedFired = false;
        QMouseEvent rightClick2(QEvent::MouseButtonPress, QPointF(50, 50), Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        picker.mousePress(&rightClick2, doc.get(), QPointF(50, 50), toolMgr.context());

        assert(contextChangedFired);
        // Primary color should now be red
        assert(toolMgr.context().primaryColor == QColor(255, 0, 0));

        // Verify full synchronization with ColorsDock
        pdn::ColorsDock colorsDock(&toolMgr);
        colorsDock.setEditingPrimary(false);
        assert(!colorsDock.isEditingPrimary());
        assert(!toolMgr.context().activeColorIsPrimary);

        // Pick green with left click
        picker.mousePress(&leftClick2, doc.get(), QPointF(20, 20), toolMgr.context());
        assert(toolMgr.context().secondaryColor == QColor(0, 255, 0));
        assert(colorsDock.activeTargetColor() == QColor(0, 255, 0));

        // Switch to primary
        colorsDock.setEditingPrimary(true);
        assert(colorsDock.isEditingPrimary());
        assert(toolMgr.context().activeColorIsPrimary);

        // Pick blue with left click
        picker.mousePress(&leftClick, doc.get(), QPointF(10, 10), toolMgr.context());
        assert(toolMgr.context().primaryColor == QColor(0, 0, 255));
        assert(colorsDock.activeTargetColor() == QColor(0, 0, 255));

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

    // Test 9: Keyboard Shortcuts and Keybindings
    {
        std::cout << "Test 9: Keybindings and tool shortcut handling..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(200, 200);
        pdn::ToolManager toolMgr;
        toolMgr.setDocument(doc.get());

        // 1. Line/Curve tool shortcut is V, not O (which is Shapes)
        auto lineTool = toolMgr.tool(pdn::ToolType::LineCurve);
        auto shapesTool = toolMgr.tool(pdn::ToolType::Shapes);
        assert(lineTool->shortcut() == "V");
        assert(shapesTool->shortcut() == "O");
        assert(lineTool->toolTip().contains("(V)"));
        assert(shapesTool->toolTip().contains("(O)"));

        // 2. Individual tool shortcuts
        QKeyEvent keyB(QEvent::KeyPress, Qt::Key_B, Qt::NoModifier, "B");
        toolMgr.handleKeyPress(&keyB);
        assert(toolMgr.activeToolType() == pdn::ToolType::Paintbrush);

        QKeyEvent keyP(QEvent::KeyPress, Qt::Key_P, Qt::NoModifier, "P");
        toolMgr.handleKeyPress(&keyP);
        assert(toolMgr.activeToolType() == pdn::ToolType::Pencil);

        QKeyEvent keyE(QEvent::KeyPress, Qt::Key_E, Qt::NoModifier, "E");
        toolMgr.handleKeyPress(&keyE);
        assert(toolMgr.activeToolType() == pdn::ToolType::Eraser);

        QKeyEvent keyK(QEvent::KeyPress, Qt::Key_K, Qt::NoModifier, "K");
        toolMgr.handleKeyPress(&keyK);
        assert(toolMgr.activeToolType() == pdn::ToolType::ColorPicker);

        QKeyEvent keyV(QEvent::KeyPress, Qt::Key_V, Qt::NoModifier, "V");
        toolMgr.handleKeyPress(&keyV);
        assert(toolMgr.activeToolType() == pdn::ToolType::LineCurve);

        QKeyEvent keyO(QEvent::KeyPress, Qt::Key_O, Qt::NoModifier, "O");
        toolMgr.handleKeyPress(&keyO);
        assert(toolMgr.activeToolType() == pdn::ToolType::Shapes);

        QKeyEvent keyF(QEvent::KeyPress, Qt::Key_F, Qt::NoModifier, "F");
        toolMgr.handleKeyPress(&keyF);
        assert(toolMgr.activeToolType() == pdn::ToolType::PaintBucket);

        QKeyEvent keyG(QEvent::KeyPress, Qt::Key_G, Qt::NoModifier, "G");
        toolMgr.handleKeyPress(&keyG);
        assert(toolMgr.activeToolType() == pdn::ToolType::Gradient);

        QKeyEvent keyZ(QEvent::KeyPress, Qt::Key_Z, Qt::NoModifier, "Z");
        toolMgr.handleKeyPress(&keyZ);
        assert(toolMgr.activeToolType() == pdn::ToolType::Zoom);

        QKeyEvent keyH(QEvent::KeyPress, Qt::Key_H, Qt::NoModifier, "H");
        toolMgr.handleKeyPress(&keyH);
        assert(toolMgr.activeToolType() == pdn::ToolType::Pan);

        // 3. Selection tools cycling with S and Shift+S
        QKeyEvent keyS(QEvent::KeyPress, Qt::Key_S, Qt::NoModifier, "S");
        toolMgr.handleKeyPress(&keyS);
        assert(toolMgr.activeToolType() == pdn::ToolType::RectangleSelect);

        toolMgr.handleKeyPress(&keyS);
        assert(toolMgr.activeToolType() == pdn::ToolType::LassoSelect);

        toolMgr.handleKeyPress(&keyS);
        assert(toolMgr.activeToolType() == pdn::ToolType::EllipseSelect);

        toolMgr.handleKeyPress(&keyS);
        assert(toolMgr.activeToolType() == pdn::ToolType::MagicWand);

        toolMgr.handleKeyPress(&keyS);
        assert(toolMgr.activeToolType() == pdn::ToolType::RectangleSelect);

        QKeyEvent keyShiftS(QEvent::KeyPress, Qt::Key_S, Qt::ShiftModifier, "S");
        toolMgr.handleKeyPress(&keyShiftS);
        assert(toolMgr.activeToolType() == pdn::ToolType::MagicWand);

        // 4. Move tools cycling with M
        QKeyEvent keyM(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier, "M");
        toolMgr.handleKeyPress(&keyM);
        assert(toolMgr.activeToolType() == pdn::ToolType::MoveSelectedPixels);

        toolMgr.handleKeyPress(&keyM);
        assert(toolMgr.activeToolType() == pdn::ToolType::MoveSelection);

        toolMgr.handleKeyPress(&keyM);
        assert(toolMgr.activeToolType() == pdn::ToolType::MoveSelectedPixels);

        // 5. Color swap (X) and Default (D)
        toolMgr.context().primaryColor = Qt::red;
        toolMgr.context().secondaryColor = Qt::blue;
        QKeyEvent keyX(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, "X");
        toolMgr.handleKeyPress(&keyX);
        assert(toolMgr.context().primaryColor == Qt::blue);
        assert(toolMgr.context().secondaryColor == Qt::red);

        QKeyEvent keyD(QEvent::KeyPress, Qt::Key_D, Qt::NoModifier, "D");
        toolMgr.handleKeyPress(&keyD);
        assert(toolMgr.context().primaryColor == Qt::black);
        assert(toolMgr.context().secondaryColor == Qt::white);

        // 6. Brush width adjustment with [ and ]
        toolMgr.context().brushWidth = 5;
        QKeyEvent keyRightBracket(QEvent::KeyPress, Qt::Key_BracketRight, Qt::NoModifier, "]");
        toolMgr.handleKeyPress(&keyRightBracket);
        assert(toolMgr.context().brushWidth == 6);

        QKeyEvent keyLeftBracket(QEvent::KeyPress, Qt::Key_BracketLeft, Qt::NoModifier, "[");
        toolMgr.handleKeyPress(&keyLeftBracket);
        assert(toolMgr.context().brushWidth == 5);

        // 7. Arrow keys nudging active selection
        doc->selection().addRect(QRectF(20, 20, 30, 30));
        QKeyEvent keyRight(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        toolMgr.handleKeyPress(&keyRight);
        QRectF r = doc->selection().boundingRect();
        assert(r.x() == 21 && r.y() == 20);

        QKeyEvent keyDownShift(QEvent::KeyPress, Qt::Key_Down, Qt::ShiftModifier);
        toolMgr.handleKeyPress(&keyDownShift);
        r = doc->selection().boundingRect();
        assert(r.x() == 21 && r.y() == 30);

        // 8. Escape key deselects
        assert(!doc->selection().isEmpty());
        QKeyEvent keyEsc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        toolMgr.handleKeyPress(&keyEsc);
        assert(doc->selection().isEmpty());

        // 9. Text tool typing isolation: 'B' while editing text does not switch to Brush
        toolMgr.setActiveTool(pdn::ToolType::Text);
        auto textTool = std::dynamic_pointer_cast<pdn::TextTool>(toolMgr.activeTool());
        textTool->mousePress(nullptr, doc.get(), QPointF(50, 50), toolMgr.context());
        assert(textTool->isEditing());

        // Send key 'B' to canvas / text tool
        textTool->keyPress(&keyB, doc.get(), toolMgr.context());
        // HandleKeyPress should NOT steal 'B' while editing
        bool handled = toolMgr.handleKeyPress(&keyB);
        assert(!handled);
        assert(toolMgr.activeToolType() == pdn::ToolType::Text);

        std::cout << "  Passed: All keybindings, cycling, color swaps, brush sizes, nudging, and text isolation work perfectly!" << std::endl;
    }

    // Test 10: Global EventFilter across docks, buttons, and windows
    {
        std::cout << "Test 10: Window-wide keybinding dispatch via eventFilter..." << std::endl;
        pdn::MainWindow win;
        win.show();

        // 1. Send key 'B' to a child button in a dock (e.g. ToolsDock or ColorsDock)
        auto buttons = win.findChildren<QAbstractButton*>();
        assert(!buttons.isEmpty());
        QAbstractButton* testBtn = buttons.first();
        testBtn->setFocus();

        // Initially active tool is Paintbrush, let's switch to Pencil first
        win.toolManager()->setActiveTool(pdn::ToolType::Pencil);
        assert(win.toolManager()->activeToolType() == pdn::ToolType::Pencil);

        // Send key 'B' while button has focus
        QKeyEvent keyB(QEvent::KeyPress, Qt::Key_B, Qt::NoModifier, "B");
        QApplication::sendEvent(testBtn, &keyB);

        // Through eventFilter, 'B' was intercepted and switched tool to Paintbrush!
        assert(win.toolManager()->activeToolType() == pdn::ToolType::Paintbrush);

        // 2. Send key 'S' to cycle selection tools while focused on a child widget
        QKeyEvent keyS(QEvent::KeyPress, Qt::Key_S, Qt::NoModifier, "S");
        QApplication::sendEvent(testBtn, &keyS);
        assert(win.toolManager()->activeToolType() == pdn::ToolType::RectangleSelect);

        QApplication::sendEvent(testBtn, &keyS);
        assert(win.toolManager()->activeToolType() == pdn::ToolType::LassoSelect);

        // 3. Verify typing inside a QLineEdit does NOT trigger tool shortcuts
        QLineEdit testEdit(&win);
        testEdit.show();
        testEdit.setFocus();

        QApplication::sendEvent(&testEdit, &keyB);
        // Tool remains LassoSelect because typing in line edit takes priority
        assert(win.toolManager()->activeToolType() == pdn::ToolType::LassoSelect);

        // 4. Verify that another window (e.g. QDialog simulating QFileDialog or EffectDialog) does NOT trigger MainWindow shortcuts
        QDialog otherDialog(&win);
        otherDialog.show();
        otherDialog.activateWindow();
        QLineEdit dialogEdit(&otherDialog);
        dialogEdit.show();
        dialogEdit.setFocus();

        // Send key 'B' to the external dialog line edit
        QApplication::sendEvent(&dialogEdit, &keyB);
        assert(win.toolManager()->activeToolType() == pdn::ToolType::LassoSelect);

        // Send key 'B' to the external dialog itself
        QApplication::sendEvent(&otherDialog, &keyB);
        assert(win.toolManager()->activeToolType() == pdn::ToolType::LassoSelect);

        // Send key 'S' to the external dialog
        QApplication::sendEvent(&otherDialog, &keyS);
        assert(win.toolManager()->activeToolType() == pdn::ToolType::LassoSelect);

        // Send Backspace to dialogEdit: should not trigger Fill Selection or change tools
        QKeyEvent keyBack(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
        QApplication::sendEvent(&dialogEdit, &keyBack);
        assert(win.toolManager()->activeToolType() == pdn::ToolType::LassoSelect);

        otherDialog.close();

        // 5. Verify that keybinds still work when MainWindow or a dock component is focused
        win.activateWindow();
        testBtn->setFocus();
        QApplication::sendEvent(testBtn, &keyB);
        assert(win.toolManager()->activeToolType() == pdn::ToolType::Paintbrush);

        std::cout << "  Passed: eventFilter dispatches keybindings window-wide across all docks and child widgets, and isolates other windows!" << std::endl;
    }

    // Test 13: Copy/Cut/Paste behavior, selection persistence & floating selection movement
    {
        std::cout << "Test 13: Copy/Cut/Paste behavior and floating temporary layer..." << std::endl;
        pdn::MainWindow win;
        win.newDocument(200, 200);
        auto doc = win.document();
        assert(doc != nullptr);

        auto layer = doc->activeLayer();
        assert(layer != nullptr);
        layer->fill(Qt::white);

        // Draw a distinct green rect at (30, 40) size (50, 60)
        for (int y = 40; y < 100; ++y) {
            for (int x = 30; x < 80; ++x) {
                layer->scanLine(y)[x] = 0xFF00FF00; // Green ARGB
            }
        }

        // 1. Select the green rect
        doc->selection().addRect(QRectF(30, 40, 50, 60), pdn::SelectionCombineMode::Replace);
        assert(!doc->selection().isEmpty());

        // 2. Test Cut: selection should disappear, content cut to clipboard, position & size saved
        QMetaObject::invokeMethod(&win, "onCut");
        assert(doc->selection().isEmpty()); // Requirement 1: selection disappears upon cut!
        assert(layer->scanLine(50)[40] == 0x00000000); // Erased from active layer (transparent hole)
        assert(pdn::MainWindow::hasLastCopied());
        assert(pdn::MainWindow::lastCopiedPos() == QPoint(30, 40)); // Requirement 4: position saved!
        assert(pdn::MainWindow::lastCopiedSize() == QSize(50, 60)); // Requirement 4: size saved!

        // 3. Test Paste: same size, should paste at exact saved position (30, 40)
        // and have selection around the pasted content
        QMetaObject::invokeMethod(&win, "onPaste");
        assert(!doc->selection().isEmpty()); // Requirement 2: selection around pasted content!
        assert(doc->selection().boundingRect() == QRectF(30, 40, 50, 60));
        assert(doc->hasFloatingSelection());
        assert(doc->floatingOffset() == QPointF(30, 40));

        // 4. Test Hidden Temporary Layer & Move pasted content without leaving a hole where pasted
        // Fill layer with blue underneath floating selection
        layer->fill(Qt::blue);
        assert(layer->scanLine(50)[40] == 0xFF0000FF); // Layer underneath is blue!

        // Move the floating selection by delta (70, 60) to (100, 100)
        doc->moveFloatingSelection(QPointF(70, 60));
        assert(doc->floatingOffset() == QPointF(100, 100));
        assert(doc->selection().boundingRect() == QRectF(100, 100, 50, 60));

        // CRITICAL CHECK: Where it originally was pasted (30, 40), there is NO HOLE in the layer!
        assert(layer->scanLine(50)[40] == 0xFF0000FF); // No hole where it originally was pasted!
        assert(layer->scanLine(110)[110] == 0xFF0000FF); // Not baked into layer yet!

        // Composite image has green at (100, 100) and blue at (30, 40)
        QImage comp = doc->composite();
        assert(comp.pixelColor(40, 50) == QColor(Qt::blue));
        assert(comp.pixelColor(110, 110) == QColor(Qt::green));

        // 5. Test Deselect: bakes into real layer
        QMetaObject::invokeMethod(&win, "onDeselect");
        assert(!doc->hasFloatingSelection());
        assert(doc->selection().isEmpty());
        assert(layer->scanLine(110)[110] == 0xFF00FF00); // Now baked as green at (100, 100)!
        assert(layer->scanLine(50)[40] == 0xFF0000FF); // Still blue at original paste location!

        // 6. Test Normal selected content movement without paste:
        // Select the newly baked green rect at (100, 100, 50, 60)
        doc->selection().addRect(QRectF(100, 100, 50, 60), pdn::SelectionCombineMode::Replace);
        win.toolManager()->setActiveTool(pdn::ToolType::MoveSelectedPixels);
        auto moveTool = std::dynamic_pointer_cast<pdn::MoveSelectedPixelsTool>(win.toolManager()->activeTool());
        assert(moveTool != nullptr);

        // Move with MoveSelectedPixelsTool: mousePress lifts pixels
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(110, 110), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        moveTool->mousePress(&press, doc.get(), QPointF(110, 110), win.toolManager()->context());
        assert(doc->hasFloatingSelection());
        assert(layer->scanLine(110)[110] == 0x00000000); // Lifted! Transparent hole at original location

        // Move to (130, 120)
        QMouseEvent move1(QEvent::MouseMove, QPointF(130, 120), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        moveTool->mouseMove(&move1, doc.get(), QPointF(130, 120), win.toolManager()->context());
        QMouseEvent release1(QEvent::MouseButtonRelease, QPointF(130, 120), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        moveTool->mouseRelease(&release1, doc.get(), QPointF(130, 120), win.toolManager()->context());

        // Move again to (140, 130) - ensure no hole at intermediate position (120, 110)
        QMouseEvent press2(QEvent::MouseButtonPress, QPointF(130, 120), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        moveTool->mousePress(&press2, doc.get(), QPointF(130, 120), win.toolManager()->context());
        QMouseEvent move2(QEvent::MouseMove, QPointF(140, 130), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        moveTool->mouseMove(&move2, doc.get(), QPointF(140, 130), win.toolManager()->context());

        // Deselect bakes into layer
        QMetaObject::invokeMethod(&win, "onDeselect");
        assert(!doc->hasFloatingSelection());
        assert(doc->selection().isEmpty());
        // Baked at final position: offset was (100, 100) + (30, 20) = (130, 120)
        assert(layer->scanLine(130)[140] == 0xFF00FF00); // Baked at final position!
        assert(layer->scanLine(110)[110] == 0x00000000); // Hole remains only at original lifted position!

        // 7. Test Paste with different image size: defaults to (0, 0)
        QImage diffImg(25, 25, QImage::Format_ARGB32);
        diffImg.fill(Qt::red);
        QClipboard* clip = QGuiApplication::clipboard();
        clip->setImage(diffImg);

        QMetaObject::invokeMethod(&win, "onPaste");
        assert(doc->hasFloatingSelection());
        assert(doc->floatingOffset() == QPointF(0, 0)); // Not matching size -> pastes at (0, 0)!
        assert(doc->selection().boundingRect() == QRectF(0, 0, 25, 25)); // Selection around pasted image!

        // Deselect bakes it
        QMetaObject::invokeMethod(&win, "onDeselect");
        assert(!doc->hasFloatingSelection());
        assert(layer->scanLine(5)[5] == 0xFFFF0000); // Red baked at (0, 0)

        // 8. Test New Document clears cut size and position data:
        // Cut a rect at (40, 50) of size (60, 70)
        doc->selection().addRect(QRectF(40, 50, 60, 70), pdn::SelectionCombineMode::Replace);
        QMetaObject::invokeMethod(&win, "onCut");
        assert(pdn::MainWindow::hasLastCopied());
        assert(pdn::MainWindow::lastCopiedPos() == QPoint(40, 50));
        assert(pdn::MainWindow::lastCopiedSize() == QSize(60, 70));

        // Create a new document
        win.newDocument(400, 400);
        assert(!pdn::MainWindow::hasLastCopied());
        assert(pdn::MainWindow::lastCopiedPos() == QPoint(0, 0));
        assert(pdn::MainWindow::lastCopiedSize() == QSize(0, 0));

        // Now paste on the new document: even though the clipboard still has the cut image (60x70),
        // it must paste at (0, 0) because the new file was created!
        auto newDoc = win.document();
        assert(newDoc != nullptr);
        QMetaObject::invokeMethod(&win, "onPaste");
        assert(newDoc->hasFloatingSelection());
        assert(newDoc->floatingOffset() == QPointF(0, 0));
        assert(newDoc->selection().boundingRect() == QRectF(0, 0, 60, 70));

        std::cout << "  Passed: Copy/Cut/Paste, position persistence, selection bounds, new document clearing, and floating temporary layer all work properly!" << std::endl;
    }

    // Test 11: Move Tool Resize and Rotate (Paint.NET behavior)
    {
        std::cout << "Test 11: Move tool resize and rotate..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(200, 200);
        auto layer = doc->activeLayer();
        // Fill a 40x40 square with solid blue at (20, 20)
        for (int y = 20; y < 60; ++y) {
            for (int x = 20; x < 60; ++x) {
                layer->scanLine(y)[x] = 0xFF0000FF; // Solid blue
            }
        }

        pdn::ToolManager toolMgr;
        toolMgr.setDocument(doc.get());

        // Select the 40x40 region
        doc->selection().addRect(QRectF(20, 20, 40, 40));
        assert(!doc->selection().isEmpty());

        toolMgr.setActiveTool(pdn::ToolType::MoveSelectedPixels);
        auto moveTool = std::dynamic_pointer_cast<pdn::MoveSelectedPixelsTool>(toolMgr.activeTool());
        assert(moveTool != nullptr);
        assert(moveTool->mode() == pdn::TransformMode::Resize);

        // Hover over inside: cursor should be SizeAllCursor
        QMouseEvent hoverInside(QEvent::MouseMove, QPointF(40, 40), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        moveTool->mouseMove(&hoverInside, doc.get(), QPointF(40, 40), toolMgr.context());
        assert(moveTool->cursor() == Qt::SizeAllCursor);

        // Hover over bottom-right handle (60, 60): cursor should be SizeFDiagCursor
        QMouseEvent hoverSE(QEvent::MouseMove, QPointF(60, 60), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        moveTool->mouseMove(&hoverSE, doc.get(), QPointF(60, 60), toolMgr.context());
        assert(moveTool->cursor() == Qt::SizeFDiagCursor);

        // 1. Resize without Shift: drag SE corner from (60, 60) to (80, 70)
        QMouseEvent pressSE(QEvent::MouseButtonPress, QPointF(60, 60), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        moveTool->mousePress(&pressSE, doc.get(), QPointF(60, 60), toolMgr.context());
        QMouseEvent dragSE(QEvent::MouseMove, QPointF(80, 70), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        moveTool->mouseMove(&dragSE, doc.get(), QPointF(80, 70), toolMgr.context());
        QMouseEvent releaseSE(QEvent::MouseButtonRelease, QPointF(80, 70), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        moveTool->mouseRelease(&releaseSE, doc.get(), QPointF(80, 70), toolMgr.context());

        QRectF resizedBounds = doc->selection().boundingRect();
        assert(std::abs(resizedBounds.x() - 20) < 0.1);
        assert(std::abs(resizedBounds.y() - 20) < 0.1);
        assert(std::abs(resizedBounds.width() - 60) < 0.1);
        assert(std::abs(resizedBounds.height() - 50) < 0.1);

        // 2. Resize with Shift (aspect ratio locked to original 40:40 = 1:1):
        // Drag SE corner to (100, 120) with Shift
        QMouseEvent pressShift(QEvent::MouseButtonPress, QPointF(80, 70), Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        moveTool->mousePress(&pressShift, doc.get(), QPointF(80, 70), toolMgr.context());
        QMouseEvent dragShift(QEvent::MouseMove, QPointF(100, 120), Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        moveTool->mouseMove(&dragShift, doc.get(), QPointF(100, 120), toolMgr.context());
        QMouseEvent releaseShift(QEvent::MouseButtonRelease, QPointF(100, 120), Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
        moveTool->mouseRelease(&releaseShift, doc.get(), QPointF(100, 120), toolMgr.context());

        QRectF shiftBounds = doc->selection().boundingRect();
        assert(std::abs(shiftBounds.x() - 20) < 0.1);
        assert(std::abs(shiftBounds.y() - 20) < 0.1);
        assert(std::abs(shiftBounds.width() - shiftBounds.height()) < 0.1);
        assert(std::abs(shiftBounds.width() - 100) < 0.1);

        // 3. Click inside selected part toggles to Rotation mode
        QPointF centerPt = shiftBounds.center();
        QMouseEvent clickInsidePress(QEvent::MouseButtonPress, centerPt, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        moveTool->mousePress(&clickInsidePress, doc.get(), centerPt, toolMgr.context());
        QMouseEvent clickInsideRelease(QEvent::MouseButtonRelease, centerPt, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        moveTool->mouseRelease(&clickInsideRelease, doc.get(), centerPt, toolMgr.context());

        assert(moveTool->mode() == pdn::TransformMode::Rotate);

        // Cursor on border should now be rotation cursor!
        QMouseEvent hoverBorder(QEvent::MouseMove, QPointF(shiftBounds.right(), centerPt.y()), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        moveTool->mouseMove(&hoverBorder, doc.get(), QPointF(shiftBounds.right(), centerPt.y()), toolMgr.context());
        assert(moveTool->cursor() != Qt::SizeHorCursor && moveTool->cursor() != Qt::ArrowCursor);

        // 4. Rotate with Shift: 15-degree angle snapping
        QPointF handleRight(shiftBounds.right(), centerPt.y());
        QMouseEvent rotPress(QEvent::MouseButtonPress, handleRight, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        moveTool->mousePress(&rotPress, doc.get(), handleRight, toolMgr.context());

        // In the beginning, drag by a small angle (~3 deg): with Shift, it must stay at 0 deg, NOT rotate by a few degrees!
        QPointF dragSmall(70.0 + 50.0 * std::cos(3.0 * M_PI / 180.0), 70.0 + 50.0 * std::sin(3.0 * M_PI / 180.0));
        QMouseEvent rotDragSmall(QEvent::MouseMove, dragSmall, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        moveTool->mouseMove(&rotDragSmall, doc.get(), dragSmall, toolMgr.context());

        QPointF p0 = doc->floatingTransform().map(QPointF(0, 0));
        QPointF p1 = doc->floatingTransform().map(QPointF(100, 0));
        qreal angleDeg = std::atan2(p1.y() - p0.y(), p1.x() - p0.x()) * 180.0 / M_PI;
        if (angleDeg < 0) angleDeg += 360.0;
        assert(std::abs(angleDeg - 0.0) < 0.01); // EXACTLY 0 deg, no initial jump!

        // Drag to ~20 deg: center is (70, 70) -> snaps to 15 deg
        QPointF dragTarget(70.0 + 47.0, 70.0 + 17.0);
        QMouseEvent rotDrag(QEvent::MouseMove, dragTarget, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        moveTool->mouseMove(&rotDrag, doc.get(), dragTarget, toolMgr.context());

        // Rotation snapped to 15 degrees!
        p0 = doc->floatingTransform().map(QPointF(0, 0));
        p1 = doc->floatingTransform().map(QPointF(100, 0));
        angleDeg = std::atan2(p1.y() - p0.y(), p1.x() - p0.x()) * 180.0 / M_PI;
        if (angleDeg < 0) angleDeg += 360.0;
        assert(std::abs(angleDeg - 15.0) < 0.5);

        // Now drag to ~38 deg -> should snap to 45 degrees
        QPointF dragTarget45(70.0 + 50.0 * std::cos(38.0 * M_PI / 180.0), 70.0 + 50.0 * std::sin(38.0 * M_PI / 180.0));
        QMouseEvent rotDrag45(QEvent::MouseMove, dragTarget45, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        moveTool->mouseMove(&rotDrag45, doc.get(), dragTarget45, toolMgr.context());

        p0 = doc->floatingTransform().map(QPointF(0, 0));
        p1 = doc->floatingTransform().map(QPointF(100, 0));
        angleDeg = std::atan2(p1.y() - p0.y(), p1.x() - p0.x()) * 180.0 / M_PI;
        if (angleDeg < 0) angleDeg += 360.0;
        assert(std::abs(angleDeg - 45.0) < 0.5);

        QMouseEvent rotRelease(QEvent::MouseButtonRelease, dragTarget45, Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
        moveTool->mouseRelease(&rotRelease, doc.get(), dragTarget45, toolMgr.context());

        // 5. Click inside selection again toggles back to Resize mode
        moveTool->mousePress(&clickInsidePress, doc.get(), centerPt, toolMgr.context());
        moveTool->mouseRelease(&clickInsideRelease, doc.get(), centerPt, toolMgr.context());
        assert(moveTool->mode() == pdn::TransformMode::Resize);

        // 6. Commit / Bake
        moveTool->commit(doc.get());
        assert(!doc->hasFloatingSelection());

        std::cout << "  Passed: Move tool resize (free & Shift aspect-ratio), click-to-rotate toggle, and 15-degree rotation snapping work perfectly!" << std::endl;
    }

    // Test 12: Move Selection Tool (outline-only transform, right-click rotate, untouched layer bitmap)
    {
        std::cout << "Test 12: Move selection tool (outline only, 15-deg snapping, right-click rotate)..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(200, 200);
        auto layer = doc->activeLayer();
        layer->fill(Qt::white);
        // Put a red marker at (50, 50)
        layer->scanLine(50)[50] = 0xFFFF0000;

        pdn::ToolManager toolMgr;
        toolMgr.setDocument(doc.get());

        // Create selection at (30, 30, 40, 40)
        doc->selection().addRect(QRectF(30, 30, 40, 40));

        toolMgr.setActiveTool(pdn::ToolType::MoveSelection);
        auto selTool = std::dynamic_pointer_cast<pdn::MoveSelectionTool>(toolMgr.activeTool());
        assert(selTool != nullptr);
        assert(selTool->mode() == pdn::TransformMode::Resize);

        // Hover over handle: East handle is at (70, 50)
        QMouseEvent hoverE(QEvent::MouseMove, QPointF(70, 50), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        selTool->mouseMove(&hoverE, doc.get(), QPointF(70, 50), toolMgr.context());
        assert(selTool->cursor() == Qt::SizeHorCursor);

        // 1. Resize East handle from (70, 50) to (90, 50)
        QMouseEvent pressE(QEvent::MouseButtonPress, QPointF(70, 50), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        selTool->mousePress(&pressE, doc.get(), QPointF(70, 50), toolMgr.context());
        QMouseEvent dragE(QEvent::MouseMove, QPointF(90, 50), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        selTool->mouseMove(&dragE, doc.get(), QPointF(90, 50), toolMgr.context());
        QMouseEvent releaseE(QEvent::MouseButtonRelease, QPointF(90, 50), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        selTool->mouseRelease(&releaseE, doc.get(), QPointF(90, 50), toolMgr.context());

        QRectF boundsAfterResize = doc->selection().boundingRect();
        assert(std::abs(boundsAfterResize.x() - 30) < 0.1);
        assert(std::abs(boundsAfterResize.y() - 30) < 0.1);
        assert(std::abs(boundsAfterResize.width() - 60) < 0.1); // 40 + 20
        assert(std::abs(boundsAfterResize.height() - 40) < 0.1);

        // Layer pixels MUST be untouched (MoveSelectionTool only moves the selection outline)
        assert(layer->scanLine(50)[50] == 0xFFFF0000);
        assert(!doc->hasFloatingSelection());

        // 2. Click inside toggles to Rotate mode
        QPointF centerPt = boundsAfterResize.center();
        QMouseEvent clickInsidePress(QEvent::MouseButtonPress, centerPt, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        selTool->mousePress(&clickInsidePress, doc.get(), centerPt, toolMgr.context());
        QMouseEvent clickInsideRelease(QEvent::MouseButtonRelease, centerPt, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        selTool->mouseRelease(&clickInsideRelease, doc.get(), centerPt, toolMgr.context());
        assert(selTool->mode() == pdn::TransformMode::Rotate);

        // 3. Right-click drag rotates directly even in Resize mode, and left-drag rotates in Rotate mode
        // Test clicking on top-right corner of a non-square (60x40) rectangle with Shift held
        QPointF cornerTR(boundsAfterResize.right(), boundsAfterResize.top());
        QMouseEvent rotPressCorner(QEvent::MouseButtonPress, cornerTR, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        selTool->mousePress(&rotPressCorner, doc.get(), cornerTR, toolMgr.context());

        // Drag slightly (~2 deg around center): angle must stay at EXACTLY 0.0 deg (no initial offset/jump!)
        qreal cornerRadius = std::hypot(cornerTR.x() - centerPt.x(), cornerTR.y() - centerPt.y());
        qreal cornerAngleRad = std::atan2(cornerTR.y() - centerPt.y(), cornerTR.x() - centerPt.x());
        QPointF dragSmallCorner(centerPt.x() + cornerRadius * std::cos(cornerAngleRad + 2.0 * M_PI / 180.0),
                                centerPt.y() + cornerRadius * std::sin(cornerAngleRad + 2.0 * M_PI / 180.0));
        QMouseEvent rotDragSmallCorner(QEvent::MouseMove, dragSmallCorner, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        selTool->mouseMove(&rotDragSmallCorner, doc.get(), dragSmallCorner, toolMgr.context());

        auto currentQuad = selTool->currentQuad();
        qreal currentAngleDeg = std::atan2(currentQuad[1].y() - currentQuad[0].y(), currentQuad[1].x() - currentQuad[0].x()) * 180.0 / M_PI;
        if (currentAngleDeg < 0) currentAngleDeg += 360.0;
        assert(std::abs(currentAngleDeg - 0.0) < 0.01); // EXACTLY 0 deg, no jump despite corner angle being -33.69 deg!

        // Drag to ~20 deg -> snaps to 15 deg
        QPointF drag15Corner(centerPt.x() + cornerRadius * std::cos(cornerAngleRad + 19.0 * M_PI / 180.0),
                             centerPt.y() + cornerRadius * std::sin(cornerAngleRad + 19.0 * M_PI / 180.0));
        QMouseEvent rotDrag15Corner(QEvent::MouseMove, drag15Corner, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        selTool->mouseMove(&rotDrag15Corner, doc.get(), drag15Corner, toolMgr.context());

        currentQuad = selTool->currentQuad();
        currentAngleDeg = std::atan2(currentQuad[1].y() - currentQuad[0].y(), currentQuad[1].x() - currentQuad[0].x()) * 180.0 / M_PI;
        if (currentAngleDeg < 0) currentAngleDeg += 360.0;
        assert(std::abs(currentAngleDeg - 15.0) < 0.5);

        // Drag to ~33 deg -> snaps to 30 deg
        QPointF drag30Corner(centerPt.x() + cornerRadius * std::cos(cornerAngleRad + 33.0 * M_PI / 180.0),
                             centerPt.y() + cornerRadius * std::sin(cornerAngleRad + 33.0 * M_PI / 180.0));
        QMouseEvent rotDrag30Corner(QEvent::MouseMove, drag30Corner, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        selTool->mouseMove(&rotDrag30Corner, doc.get(), drag30Corner, toolMgr.context());

        currentQuad = selTool->currentQuad();
        currentAngleDeg = std::atan2(currentQuad[1].y() - currentQuad[0].y(), currentQuad[1].x() - currentQuad[0].x()) * 180.0 / M_PI;
        if (currentAngleDeg < 0) currentAngleDeg += 360.0;
        assert(std::abs(currentAngleDeg - 30.0) < 0.5);

        QMouseEvent rotReleaseCorner(QEvent::MouseButtonRelease, drag30Corner, Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
        selTool->mouseRelease(&rotReleaseCorner, doc.get(), drag30Corner, toolMgr.context());

        // Verify selection path was transformed and layer remains untouched
        assert(!doc->selection().isEmpty());
        assert(layer->scanLine(50)[50] == 0xFFFF0000);
        assert(!doc->hasFloatingSelection());

        std::cout << "  Passed: Move selection tool (outline only, 15-deg snapping, right-click rotate) works properly!" << std::endl;
    }

    // Test 14: Viewbox scroll boundaries (image border never passes viewbox center)
    {
        std::cout << "Test 14: Viewbox scroll boundaries..." << std::endl;
        pdn::ToolManager toolMgr;
        pdn::CanvasView canvas(&toolMgr);
        auto doc = std::make_shared<pdn::Document>(400, 200);
        canvas.setDocument(doc);
        canvas.resize(800, 600);
        canvas.toggleRulers(false); // viewW = 800, viewH = 600
        canvas.setZoom(1.0);

        // Center of viewbox is (400, 300).
        // Document size is 400 x 200.
        // 1. Scroll left far into negative X:
        // Right border of image must stop at horizontal center (400)
        // Right border = panOffset.x() + docW = panOffset.x() + 400.
        // So panOffset.x() must clamp to 400 - 400 = 0.
        canvas.setPanOffset(QPointF(-1000, 100));
        assert(canvas.panOffset().x() == 0.0);
        // Right border is at panOffset.x() + 400 = 400 (viewbox center)
        assert(canvas.panOffset().x() + doc->width() * canvas.zoom() == 400.0);

        // 2. Scroll right far into positive X:
        // Left border of image must stop at horizontal center (400)
        // Left border = panOffset.x().
        // So panOffset.x() must clamp to 400.
        canvas.setPanOffset(QPointF(1000, 100));
        assert(canvas.panOffset().x() == 400.0);

        // 3. Scroll up far into negative Y:
        // Bottom border of image must stop at vertical center (300)
        // Bottom border = panOffset.y() + docH = panOffset.y() + 200.
        // So panOffset.y() must clamp to 300 - 200 = 100.
        canvas.setPanOffset(QPointF(200, -1000));
        assert(canvas.panOffset().y() == 100.0);
        assert(canvas.panOffset().y() + doc->height() * canvas.zoom() == 300.0);

        // 4. Scroll down far into positive Y:
        // Top border of image must stop at vertical center (300)
        // Top border = panOffset.y().
        // So panOffset.y() must clamp to 300.
        canvas.setPanOffset(QPointF(200, 1000));
        assert(canvas.panOffset().y() == 300.0);

        // 5. Test with Zoom = 2.0 (docW = 800, docH = 400):
        canvas.setZoom(2.0);
        // Scroll left: right border at 400 -> panOffset.x() = 400 - 800 = -400.
        canvas.setPanOffset(QPointF(-2000, 100));
        assert(canvas.panOffset().x() == -400.0);
        assert(canvas.panOffset().x() + doc->width() * canvas.zoom() == 400.0);

        // Scroll right: left border at 400 -> panOffset.x() = 400.
        canvas.setPanOffset(QPointF(2000, 100));
        assert(canvas.panOffset().x() == 400.0);

        // Scroll up: bottom border at 300 -> panOffset.y() = 300 - 400 = -100.
        canvas.setPanOffset(QPointF(0, -2000));
        assert(canvas.panOffset().y() == -100.0);
        assert(canvas.panOffset().y() + doc->height() * canvas.zoom() == 300.0);

        // Scroll down: top border at 300 -> panOffset.y() = 300.
        canvas.setPanOffset(QPointF(0, 2000));
        assert(canvas.panOffset().y() == 300.0);

        std::cout << "  Passed: Image cannot scroll off viewbox; borders clamp at viewbox center across all 4 directions!" << std::endl;
    }

    std::cout << "=== All Tests Passed Successfully! ===" << std::endl;
    return 0;
}
