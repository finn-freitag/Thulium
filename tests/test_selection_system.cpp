#include <QApplication>
#include <cassert>
#include <iostream>
#include <cmath>
#include "../src/core/Document.h"
#include "../src/core/History.h"
#include "../src/core/Selection.h"
#include "../src/tools/SelectionTools.h"
#include "../src/tools/MoveTools.h"
#include "../src/ui/StatusWidget.h"
#include "../src/ui/MainWindow.h"

// Helper to create a dummy QMouseEvent
static QMouseEvent createMouseEvent(QEvent::Type type, const QPointF& pos, Qt::MouseButton button, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers) {
    return QMouseEvent(type, pos, pos, button, buttons, modifiers);
}

// Verify that all vertices in a QPainterPath have integer coordinates
// and that all line segments are either purely horizontal or purely vertical
static void verifyPixelSnappedPath(const QPainterPath& path) {
    for (int i = 0; i < path.elementCount(); ++i) {
        auto el = path.elementAt(i);
        // Check integer coordinates
        assert(std::abs(el.x - std::round(el.x)) < 1e-5);
        assert(std::abs(el.y - std::round(el.y)) < 1e-5);
        // Check that segments are horizontal or vertical
        if (el.type == QPainterPath::LineToElement && i > 0) {
            auto prev = path.elementAt(i - 1);
            bool isHorizontal = (std::abs(el.y - prev.y) < 1e-5);
            bool isVertical = (std::abs(el.x - prev.x) < 1e-5);
            assert(isHorizontal || isVertical);
        }
    }
}

// Verify that a selection mask has ONLY values 0 and 255
static void verifyBinaryMask(const QImage& mask) {
    for (int y = 0; y < mask.height(); ++y) {
        const uint8_t* line = mask.scanLine(y);
        for (int x = 0; x < mask.width(); ++x) {
            assert(line[x] == 0 || line[x] == 255);
        }
    }
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    std::cout << "Running Selection System Tests..." << std::endl;

    // Test 1: Pixel grid snapping for Ellipse Select (no partly-selected pixels)
    {
        std::cout << "Test 1: Ellipse select pixel-grid snapping..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(100, 100);
        pdn::EllipseSelectTool ellipseTool;
        pdn::ToolContext ctx;
        ctx.selectionCombineMode = pdn::SelectionCombineMode::Replace;

        // Select an ellipse from (20, 20) to (60, 50)
        auto press = createMouseEvent(QEvent::MouseButtonPress, QPointF(20.3, 20.7), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        ellipseTool.mousePress(&press, doc.get(), QPointF(20.3, 20.7), ctx);

        auto release = createMouseEvent(QEvent::MouseButtonRelease, QPointF(59.8, 49.2), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        ellipseTool.mouseRelease(&release, doc.get(), QPointF(59.8, 49.2), ctx);

        assert(!doc->selection().isEmpty());
        QRectF bounds = doc->selection().boundingRect();
        assert(bounds.x() == 20.0);
        assert(bounds.y() == 20.0);
        assert(bounds.width() == 40.0);
        assert(bounds.height() == 30.0);

        // Verify that path outline consists only of integer-border segments (staircased along pixel borders)
        verifyPixelSnappedPath(doc->selection().path());

        // Verify binary mask (0 or 255 only)
        QImage mask = doc->selection().mask(100, 100);
        verifyBinaryMask(mask);

        // Verify center pixel is selected, outside is not
        assert(doc->selection().containsPixel(40, 35));
        assert(!doc->selection().containsPixel(10, 10));
        // Corner of the bounding box (20, 20) should be outside ellipse
        assert(!doc->selection().containsPixel(20, 20));

        std::cout << "  Passed: Ellipse select snaps to pixel borders with zero partly-selected pixels!" << std::endl;
    }

    // Test 2: Pixel grid snapping for Lasso Select
    {
        std::cout << "Test 2: Lasso select pixel-grid snapping..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(100, 100);
        pdn::LassoSelectTool lassoTool;
        pdn::ToolContext ctx;
        ctx.selectionCombineMode = pdn::SelectionCombineMode::Replace;

        // Draw a triangle polygon
        auto press = createMouseEvent(QEvent::MouseButtonPress, QPointF(10.2, 10.4), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        lassoTool.mousePress(&press, doc.get(), QPointF(10.2, 10.4), ctx);

        auto move1 = createMouseEvent(QEvent::MouseMove, QPointF(40.6, 10.4), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        lassoTool.mouseMove(&move1, doc.get(), QPointF(40.6, 10.4), ctx);

        auto move2 = createMouseEvent(QEvent::MouseMove, QPointF(40.6, 40.8), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        lassoTool.mouseMove(&move2, doc.get(), QPointF(40.6, 40.8), ctx);

        auto release = createMouseEvent(QEvent::MouseButtonRelease, QPointF(10.2, 10.4), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        lassoTool.mouseRelease(&release, doc.get(), QPointF(10.2, 10.4), ctx);

        assert(!doc->selection().isEmpty());
        // Verify path has only integer coordinates along pixel borders
        verifyPixelSnappedPath(doc->selection().path());
        verifyBinaryMask(doc->selection().mask(100, 100));

        std::cout << "  Passed: Lasso select snaps to pixel borders!" << std::endl;
    }

    // Test 3: Shift modifier automatically uses "Add" (Union) mode, and releasing Shift switches back
    {
        std::cout << "Test 3: Shift modifier dynamically adds to existing selection..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(100, 100);
        pdn::RectangleSelectTool rectTool;
        pdn::ToolContext ctx;
        ctx.selectionCombineMode = pdn::SelectionCombineMode::Replace;

        // 1. Initial rectangle: (10, 10) to (30, 30) without Shift
        auto p1 = createMouseEvent(QEvent::MouseButtonPress, QPointF(10, 10), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        rectTool.mousePress(&p1, doc.get(), QPointF(10, 10), ctx);
        auto r1 = createMouseEvent(QEvent::MouseButtonRelease, QPointF(29, 29), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        rectTool.mouseRelease(&r1, doc.get(), QPointF(29, 29), ctx);

        assert(doc->selection().containsPixel(15, 15));
        assert(!doc->selection().containsPixel(50, 50));
        QRectF bounds1 = doc->selection().boundingRect();
        assert(bounds1 == QRectF(10, 10, 20, 20));

        // 2. Second rectangle: (40, 40) to (60, 60) WITH Shift held
        auto p2 = createMouseEvent(QEvent::MouseButtonPress, QPointF(40, 40), Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
        rectTool.mousePress(&p2, doc.get(), QPointF(40, 40), ctx);
        auto r2 = createMouseEvent(QEvent::MouseButtonRelease, QPointF(59, 59), Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
        rectTool.mouseRelease(&r2, doc.get(), QPointF(59, 59), ctx);

        // BOTH areas must now be selected (Union)!
        assert(doc->selection().containsPixel(15, 15));
        assert(doc->selection().containsPixel(50, 50));
        assert(!doc->selection().containsPixel(35, 35));

        // 3. Third rectangle WITHOUT Shift: should replace everything
        auto p3 = createMouseEvent(QEvent::MouseButtonPress, QPointF(70, 70), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        rectTool.mousePress(&p3, doc.get(), QPointF(70, 70), ctx);
        auto r3 = createMouseEvent(QEvent::MouseButtonRelease, QPointF(89, 89), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        rectTool.mouseRelease(&r3, doc.get(), QPointF(89, 89), ctx);

        assert(!doc->selection().containsPixel(15, 15));
        assert(!doc->selection().containsPixel(50, 50));
        assert(doc->selection().containsPixel(75, 75));

        std::cout << "  Passed: Shift modifier adds to selection, and releasing Shift reverts to default mode!" << std::endl;
    }

    // Test 4: Selection History (Undo & Redo)
    {
        std::cout << "Test 4: Selection history (undo & redo)..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(100, 100);
        pdn::RectangleSelectTool rectTool;
        pdn::ToolContext ctx;
        ctx.selectionCombineMode = pdn::SelectionCombineMode::Replace;

        int initialUndos = doc->undoStack()->count();

        // Make selection
        auto p1 = createMouseEvent(QEvent::MouseButtonPress, QPointF(10, 10), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        rectTool.mousePress(&p1, doc.get(), QPointF(10, 10), ctx);
        auto r1 = createMouseEvent(QEvent::MouseButtonRelease, QPointF(39, 39), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        rectTool.mouseRelease(&r1, doc.get(), QPointF(39, 39), ctx);

        assert(doc->undoStack()->count() == initialUndos + 1);
        assert(!doc->selection().isEmpty());

        // Undo -> selection should become empty
        doc->undoStack()->undo();
        assert(doc->selection().isEmpty());

        // Redo -> selection should be restored
        doc->undoStack()->redo();
        assert(!doc->selection().isEmpty());
        assert(doc->selection().boundingRect() == QRectF(10, 10, 30, 30));

        // Clear selection via click-to-deselect
        auto p2 = createMouseEvent(QEvent::MouseButtonPress, QPointF(5, 5), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        rectTool.mousePress(&p2, doc.get(), QPointF(5, 5), ctx);
        auto r2 = createMouseEvent(QEvent::MouseButtonRelease, QPointF(5, 5), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        rectTool.mouseRelease(&r2, doc.get(), QPointF(5, 5), ctx);

        assert(doc->selection().isEmpty());
        assert(doc->undoStack()->count() == initialUndos + 2);

        // Undo deselect -> selection restored!
        doc->undoStack()->undo();
        assert(!doc->selection().isEmpty());
        assert(doc->selection().boundingRect() == QRectF(10, 10, 30, 30));

        std::cout << "  Passed: Selections and deselections are recorded in history and undo/redo cleanly!" << std::endl;
    }

    // Test 5: Cut action restores both erased pixels and selection on Undo
    {
        std::cout << "Test 5: Cut undo restores cut pixels AND selection..." << std::endl;
        pdn::MainWindow win;
        win.newDocument(100, 100);
        auto doc = win.activeDocument();
        assert(doc != nullptr);

        auto layer = doc->activeLayer();
        layer->fill(QColor(255, 0, 0, 255)); // Red

        // Select (20, 20) to (50, 50)
        doc->selection().addRect(QRectF(20, 20, 30, 30), pdn::SelectionCombineMode::Replace);
        assert(!doc->selection().isEmpty());
        assert(layer->image().pixelColor(30, 30) == QColor(255, 0, 0, 255));

        int undoCountBefore = doc->undoStack()->count();

        // Perform Cut
        win.onCut();

        // After cut: pixels in selection erased (transparent), and selection cleared
        assert(layer->image().pixelColor(30, 30).alpha() == 0);
        assert(layer->image().pixelColor(10, 10) == QColor(255, 0, 0, 255)); // Uncut area preserved
        assert(doc->selection().isEmpty());
        assert(doc->undoStack()->count() == undoCountBefore + 1);

        // Perform Undo: BOTH cut pixels AND selection must reappear!
        doc->undoStack()->undo();

        assert(layer->image().pixelColor(30, 30) == QColor(255, 0, 0, 255));
        assert(!doc->selection().isEmpty());
        assert(doc->selection().boundingRect() == QRectF(20, 20, 30, 30));
        assert(doc->selection().containsPixel(30, 30));

        // Perform Redo: pixels erased and selection cleared again
        doc->undoStack()->redo();
        assert(layer->image().pixelColor(30, 30).alpha() == 0);
        assert(doc->selection().isEmpty());

        std::cout << "  Passed: Cut undo restores both image content and selection outline!" << std::endl;
    }

    // Test 6: Status bar displays selection position and size in pixels
    {
        std::cout << "Test 6: Status bar displays x, y, width, height in pixels..." << std::endl;
        pdn::StatusWidget statusWidget;

        // No selection
        statusWidget.setSelectionBounds(false, 0, 0, 0, 0);

        // Find labels
        auto labels = statusWidget.findChildren<QLabel*>();
        bool hasSelTextBefore = false;
        for (auto l : labels) {
            if (l->text().contains("Selection:")) hasSelTextBefore = true;
        }
        assert(!hasSelTextBefore);

        // Set active selection: X=15, Y=25, W=80, H=60
        statusWidget.setSelectionBounds(true, 15, 25, 80, 60);

        QString selText;
        for (auto l : labels) {
            if (l->text().contains("Selection:")) selText = l->text();
        }
        assert(!selText.isEmpty());
        assert(selText.contains("15"));
        assert(selText.contains("25"));
        assert(selText.contains("80"));
        assert(selText.contains("60"));
        assert(selText.contains("px"));

        // Clear selection
        statusWidget.setSelectionBounds(false, 0, 0, 0, 0);
        bool hasSelTextAfter = false;
        for (auto l : labels) {
            if (l->text().contains("Selection:")) hasSelTextAfter = true;
        }
        assert(!hasSelTextAfter);

        std::cout << "  Passed: Status bar correctly reflects selection position and size in pixels!" << std::endl;
    }

    // Test 7: Selection can NEVER exceed the image frame (Rectangle / Ellipse / Lasso)
    {
        std::cout << "Test 7: Selection never exceeds the image frame..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(100, 100);
        pdn::RectangleSelectTool rectTool;
        pdn::ToolContext ctx;
        ctx.selectionCombineMode = pdn::SelectionCombineMode::Replace;

        // Drag rectangle from (-20, -30) to (50, 60) across canvas borders
        auto p1 = createMouseEvent(QEvent::MouseButtonPress, QPointF(-20, -30), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        rectTool.mousePress(&p1, doc.get(), QPointF(-20, -30), ctx);
        auto r1 = createMouseEvent(QEvent::MouseButtonRelease, QPointF(49, 59), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        rectTool.mouseRelease(&r1, doc.get(), QPointF(49, 59), ctx);

        assert(!doc->selection().isEmpty());
        QRectF bounds = doc->selection().boundingRect();
        assert(bounds.x() == 0.0);
        assert(bounds.y() == 0.0);
        assert(bounds.width() == 50.0);
        assert(bounds.height() == 60.0);
        assert(!doc->selection().containsPixel(-5, 10));
        assert(!doc->selection().containsPixel(10, -5));
        assert(doc->selection().containsPixel(20, 20));
        verifyPixelSnappedPath(doc->selection().path());

        // Drag completely outside canvas (should yield empty selection)
        auto p2 = createMouseEvent(QEvent::MouseButtonPress, QPointF(-100, -100), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        rectTool.mousePress(&p2, doc.get(), QPointF(-100, -100), ctx);
        auto r2 = createMouseEvent(QEvent::MouseButtonRelease, QPointF(-10, -10), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        rectTool.mouseRelease(&r2, doc.get(), QPointF(-10, -10), ctx);

        assert(doc->selection().isEmpty());

        // Ellipse tool partially outside canvas: center at (0, 0), radius 30
        pdn::EllipseSelectTool ellipseTool;
        auto p3 = createMouseEvent(QEvent::MouseButtonPress, QPointF(-30, -30), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        ellipseTool.mousePress(&p3, doc.get(), QPointF(-30, -30), ctx);
        auto r3 = createMouseEvent(QEvent::MouseButtonRelease, QPointF(29, 29), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        ellipseTool.mouseRelease(&r3, doc.get(), QPointF(29, 29), ctx);

        assert(!doc->selection().isEmpty());
        QRectF ellBounds = doc->selection().boundingRect();
        assert(ellBounds.x() >= 0.0);
        assert(ellBounds.y() >= 0.0);
        assert(ellBounds.right() <= 100.0);
        assert(ellBounds.bottom() <= 100.0);
        assert(!doc->selection().containsPixel(-5, 5));
        assert(!doc->selection().containsPixel(5, -5));
        verifyPixelSnappedPath(doc->selection().path());

        std::cout << "  Passed: Selections crossing canvas boundaries are automatically clipped to the image frame!" << std::endl;
    }

    // Test 8: Moving selection truncates at the image frame
    {
        std::cout << "Test 8: Moving selection truncates at the image frame..." << std::endl;
        auto doc = std::make_shared<pdn::Document>(100, 100);
        doc->selection().addRect(QRectF(10, 10, 30, 30), pdn::SelectionCombineMode::Replace);
        assert(doc->selection().boundingRect() == QRectF(10, 10, 30, 30));

        // Nudge / translate by -25 on X: (10 - 25 = -15)
        doc->selection().translate(-25, 0);

        // Portion from -15 to -1 is outside and must be removed; remainder is x in [0, 14], width 15
        assert(!doc->selection().isEmpty());
        QRectF movedBounds = doc->selection().boundingRect();
        assert(movedBounds == QRectF(0, 10, 15, 30));
        assert(!doc->selection().containsPixel(-5, 15));
        assert(doc->selection().containsPixel(5, 15));

        // Move completely off-canvas: translate by another -20 on X
        doc->selection().translate(-20, 0);
        assert(doc->selection().isEmpty());

        std::cout << "  Passed: Moving selection past canvas edge automatically removes out-of-frame pixels!" << std::endl;
    }

    // Test 9: Ctrl+A selects all regardless of active SelectionCombineMode
    {
        std::cout << "Test 9: Ctrl+A selects all in every combine mode..." << std::endl;
        pdn::MainWindow win;
        win.newDocument(100, 100);
        auto doc = win.activeDocument();
        assert(doc != nullptr);

        std::vector<pdn::SelectionCombineMode> modes = {
            pdn::SelectionCombineMode::Replace,
            pdn::SelectionCombineMode::Union,
            pdn::SelectionCombineMode::Exclude,   // Subtract
            pdn::SelectionCombineMode::Intersect,
            pdn::SelectionCombineMode::Invert
        };

        for (auto mode : modes) {
            // Create a small initial selection
            doc->selection().addRect(QRectF(20, 20, 30, 30), pdn::SelectionCombineMode::Replace);
            assert(!doc->selection().isEmpty());
            assert(doc->selection().boundingRect() == QRectF(20, 20, 30, 30));

            // Set the combine mode on tool context
            win.toolManager()->context().selectionCombineMode = mode;
            doc->selection().setCombineMode(mode);

            // Execute Select All (Ctrl+A action)
            win.onSelectAll();

            // Must unconditionally select the ENTIRE document
            assert(!doc->selection().isEmpty());
            assert(doc->selection().region() == QRegion(0, 0, 100, 100));
            assert(doc->selection().boundingRect() == QRectF(0, 0, 100, 100));
            assert(doc->selection().containsPixel(0, 0));
            assert(doc->selection().containsPixel(99, 99));

            // Undo must restore the previous partial selection
            doc->undoStack()->undo();
            assert(doc->selection().boundingRect() == QRectF(20, 20, 30, 30));
        }

        std::cout << "  Passed: Ctrl+A selects all regardless of whether mode is Replace, Add, Subtract, Intersect, or Invert!" << std::endl;
    }

    std::cout << "All Selection System Tests Passed Successfully!" << std::endl;
    return 0;
}
