#include <iostream>
#include <cassert>
#include <cmath>
#include <QApplication>
#include <QFileInfo>
#include "../src/core/Document.h"
#include "../src/ui/MainWindow.h"
#include "../src/ui/CanvasView.h"
#include "../src/ui/DocumentStrip.h"
#include "../src/core/History.h"

int main(int argc, char* argv[]) {
    int fakeArgc = 1;
    char* fakeArgv[] = { argv[0], nullptr };
    QApplication app(fakeArgc, fakeArgv);

    std::cout << "=== Running Multi-Document Test Suite ===" << std::endl;

    // Test 1: Initial window document and creating new document
    {
        std::cout << "Test 1: Initial window document and creating new document..." << std::endl;
        pdn::MainWindow win;
        assert(win.documentCount() == 1);
        assert(win.activeDocumentIndex() == 0);
        assert(win.document() != nullptr);
        assert(win.document()->width() == 800 && win.document()->height() == 600);
        assert(win.document()->fileName() == "Untitled");
        assert(!win.document()->isModified());

        // Calling newDocument adds a new document tab
        win.newDocument(400, 300);
        assert(win.documentCount() == 2);
        assert(win.activeDocumentIndex() == 1);
        assert(win.document()->width() == 400 && win.document()->height() == 300);
        assert(win.document()->fileName() == "Untitled 2");

        std::cout << "  Passed: Initial startup document and new document tab creation verified." << std::endl;
    }

    // Test 2: Multiple documents creation, numbering, and document strip sync
    {
        std::cout << "Test 2: Multiple documents creation and tab numbering..." << std::endl;
        pdn::MainWindow win;
        // win starts with Untitled 800x600

        // Now creating a new document adds a second tab
        win.newDocument(200, 200);
        assert(win.documentCount() == 2);
        assert(win.activeDocumentIndex() == 1);
        assert(win.document()->width() == 200 && win.document()->height() == 200);
        assert(win.document()->fileName() == "Untitled 2");

        // Create a third document
        win.newDocument(100, 100);
        assert(win.documentCount() == 3);
        assert(win.activeDocumentIndex() == 2);
        assert(win.document()->width() == 100 && win.document()->height() == 100);
        assert(win.document()->fileName() == "Untitled 3");

        // Verify document strip reflects 3 tabs
        assert(win.documentStrip()->count() == 3);
        assert(win.documentStrip()->currentIndex() == 2);

        std::cout << "  Passed: Multiple documents created with automatic title numbering ('Untitled', 'Untitled 2', 'Untitled 3')." << std::endl;
    }

    // Test 3: Document switching and navigation (next/previous)
    {
        std::cout << "Test 3: Document switching and keyboard navigation..." << std::endl;
        pdn::MainWindow win;
        win.newDocument(100, 100);
        win.newDocument(200, 200);

        assert(win.documentCount() == 3);
        assert(win.activeDocumentIndex() == 2);

        // Switch to index 0
        win.setActiveDocumentIndex(0);
        assert(win.activeDocumentIndex() == 0);
        assert(win.document()->width() == 800);
        assert(win.documentStrip()->currentIndex() == 0);

        // Next document: 0 -> 1
        win.nextDocument();
        assert(win.activeDocumentIndex() == 1);
        assert(win.document()->width() == 100);

        // Next document: 1 -> 2
        win.nextDocument();
        assert(win.activeDocumentIndex() == 2);
        assert(win.document()->width() == 200);

        // Next document: 2 -> 0 (wrap around)
        win.nextDocument();
        assert(win.activeDocumentIndex() == 0);
        assert(win.document()->width() == 800);

        // Previous document: 0 -> 2 (wrap around)
        win.previousDocument();
        assert(win.activeDocumentIndex() == 2);
        assert(win.document()->width() == 200);

        // Previous document: 2 -> 1
        win.previousDocument();
        assert(win.activeDocumentIndex() == 1);
        assert(win.document()->width() == 100);

        std::cout << "  Passed: Document switching and forward/backward wrap navigation work correctly." << std::endl;
    }

    // Test 4: Per-document View State isolation (Zoom and Pan)
    {
        std::cout << "Test 4: Per-document view state preservation (zoom & pan)..." << std::endl;
        pdn::MainWindow win;
        auto docA = win.document();
        docA->undoStack()->push(new pdn::LayerBitmapUndoCommand(docA.get(), 0, docA->activeLayer()->image(), "M"));

        win.newDocument(500, 500);

        // Document 1 (index 1) is currently active. Set zoom to 0.75
        win.canvasView()->setZoom(0.75);
        assert(std::abs(win.canvasView()->zoom() - 0.75) < 0.001);

        // Switch to Document 0 (index 0). Set zoom to 3.0
        win.setActiveDocumentIndex(0);
        win.canvasView()->setZoom(3.0);
        assert(std::abs(win.canvasView()->zoom() - 3.0) < 0.001);

        // Switch back to Document 1: zoom must be restored to 0.75!
        win.setActiveDocumentIndex(1);
        assert(std::abs(win.canvasView()->zoom() - 0.75) < 0.001);

        // Switch back to Document 0: zoom must be restored to 3.0!
        win.setActiveDocumentIndex(0);
        assert(std::abs(win.canvasView()->zoom() - 3.0) < 0.001);

        std::cout << "  Passed: Each document preserves its own zoom level across tab switches." << std::endl;
    }

    // Test 5: Per-document History & Undo stack isolation
    {
        std::cout << "Test 5: Per-document history & undo stack isolation..." << std::endl;
        pdn::MainWindow win;
        auto docA = win.document();

        // Push 2 commands on Doc A
        docA->undoStack()->push(new pdn::LayerBitmapUndoCommand(docA.get(), 0, docA->activeLayer()->image(), "DocA Action 1"));
        docA->undoStack()->push(new pdn::LayerBitmapUndoCommand(docA.get(), 0, docA->activeLayer()->image(), "DocA Action 2"));
        assert(docA->undoStack()->count() == 2);

        // Create Doc B
        win.newDocument(200, 200);
        auto docB = win.document();
        assert(docB != docA);
        // Doc B history must be empty!
        assert(docB->undoStack()->count() == 0);

        // Push 1 command on Doc B
        docB->undoStack()->push(new pdn::LayerBitmapUndoCommand(docB.get(), 0, docB->activeLayer()->image(), "DocB Action 1"));
        assert(docB->undoStack()->count() == 1);
        assert(docA->undoStack()->count() == 2); // Doc A untouched!

        // Undo on Doc B
        docB->undoStack()->undo();
        assert(docB->undoStack()->count() == 1);
        assert(docB->undoStack()->canUndo() == false);
        assert(docA->undoStack()->canUndo() == true); // Doc A still has 2 undoable items!

        std::cout << "  Passed: Each document has an isolated undo stack without cross-contamination." << std::endl;
    }

    // Test 6: Per-document Layers and Selection isolation
    {
        std::cout << "Test 6: Per-document layers and selection isolation..." << std::endl;
        pdn::MainWindow win;
        auto docA = win.document();
        docA->undoStack()->push(new pdn::LayerBitmapUndoCommand(docA.get(), 0, docA->activeLayer()->image(), "Init"));

        // Add 2 extra layers to Doc A (total 3)
        docA->addLayer("DocA_Layer2");
        docA->addLayer("DocA_Layer3");
        assert(docA->layerCount() == 3);

        // Set selection in Doc A
        docA->selection().addRect(QRectF(10, 10, 30, 30), pdn::SelectionCombineMode::Replace);
        assert(!docA->selection().isEmpty());

        // Create Doc B
        win.newDocument(200, 200);
        auto docB = win.document();
        assert(docB->layerCount() == 1); // Doc B has only default background layer
        assert(docB->selection().isEmpty()); // Doc B has no selection

        // Switch to Doc A
        win.setActiveDocumentIndex(0);
        assert(win.document()->layerCount() == 3);
        assert(!win.document()->selection().isEmpty());

        std::cout << "  Passed: Layers and selection states are completely isolated per document." << std::endl;
    }

    // Test 7: Closing documents and fallback to default document
    {
        std::cout << "Test 7: Closing documents and empty workspace handling..." << std::endl;
        pdn::MainWindow win;
        win.newDocument(100, 100);
        win.newDocument(200, 200);
        assert(win.documentCount() == 3);

        // Close middle document (index 1)
        bool closed = win.closeDocument(1);
        assert(closed);
        assert(win.documentCount() == 2);
        assert(win.documentStrip()->count() == 2);

        // Close index 1 (was index 2)
        closed = win.closeDocument(1);
        assert(closed);
        assert(win.documentCount() == 1);

        // Close the last remaining document:
        // Must automatically recreate a new default 800x600 document so canvas is never broken!
        closed = win.closeDocument(0);
        assert(closed);
        assert(win.documentCount() == 1);
        assert(win.document() != nullptr);
        assert(win.document()->width() == 800 && win.document()->height() == 600);
        assert(win.document()->fileName() == "Untitled");

        std::cout << "  Passed: Document closure updates indices and recreating fallback default document on empty workspace." << std::endl;
    }

    // Test 8: Cross-document Cut/Copy & Paste
    {
        std::cout << "Test 8: Cross-document copy & paste..." << std::endl;
        pdn::MainWindow win;
        auto docA = win.document();
        win.newDocument(300, 300);
        auto docB = win.document();
        assert(docB != docA);

        // Switch to Doc A
        win.setActiveDocumentIndex(0);

        // Fill background of Doc A with green in a 40x40 area
        auto layerA = docA->activeLayer();
        for (int y = 20; y < 60; ++y) {
            for (int x = 20; x < 60; ++x) {
                layerA->scanLine(y)[x] = 0xFF00FF00;
            }
        }

        // Select the green box in Doc A and Copy
        docA->selection().addRect(QRectF(20, 20, 40, 40), pdn::SelectionCombineMode::Replace);
        QMetaObject::invokeMethod(&win, "onCopy");
        assert(pdn::MainWindow::hasLastCopied());
        assert(pdn::MainWindow::lastCopiedPos() == QPoint(20, 20));
        assert(pdn::MainWindow::lastCopiedSize() == QSize(40, 40));

        // Switch to Doc B and Paste into it
        win.setActiveDocumentIndex(1);
        assert(win.document() == docB);

        QMetaObject::invokeMethod(&win, "onPaste");
        assert(docB->hasFloatingSelection());
        assert(docB->floatingOffset() == QPointF(20, 20));
        assert(!docB->selection().isEmpty());
        assert(docB->selection().boundingRect() == QRectF(20, 20, 40, 40));

        // Commit floating selection
        docB->bakeFloatingSelection();
        assert(!docB->hasFloatingSelection());

        // Verify pixel in Doc B at (30, 30) is green!
        assert(docB->activeLayer()->scanLine(30)[30] == 0xFF00FF00);

        std::cout << "  Passed: Copied selection from Document A successfully pasted into Document B." << std::endl;
    }

    // Test 9: Open file and duplicate open avoidance
    {
        std::cout << "Test 9: Open file and duplicate check..." << std::endl;
        pdn::MainWindow win;

        // Card.pdn exists in the current directory or parent
        QString cardPath = "Card.pdn";
        if (!QFileInfo::exists(cardPath)) {
            cardPath = "../Card.pdn";
        }

        if (QFileInfo::exists(cardPath)) {
            bool opened = win.openFile(cardPath);
            assert(opened);
            int firstIdx = win.activeDocumentIndex();
            int countBefore = win.documentCount();

            // Try opening the same file again
            bool openedAgain = win.openFile(cardPath);
            assert(openedAgain);
            // Must NOT have opened a second tab! It should just activate the existing tab
            assert(win.documentCount() == countBefore);
            assert(win.activeDocumentIndex() == firstIdx);

            std::cout << "  Passed: Opening existing file switches to open tab instead of duplicating." << std::endl;
        } else {
            std::cout << "  Skipped Card.pdn lookup (not found in current/parent dir)." << std::endl;
        }
    }

    std::cout << "=== All Multi-Document Tests Passed Successfully! ===" << std::endl;
    return 0;
}
