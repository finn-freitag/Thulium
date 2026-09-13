#include <iostream>
#include <cassert>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QPainter>
#include "../src/core/Document.h"
#include "../src/ui/MainWindow.h"
#include "../src/io/ImageIO.h"

int main(int argc, char* argv[]) {
    int fakeArgc = 1;
    char* fakeArgv[] = { argv[0], nullptr };
    QApplication app(fakeArgc, fakeArgv);

    std::cout << "=== Running Clipboard & Drop Image Test Suite ===" << std::endl;

    QString srcDir = ".";
    if (argc > 1) {
        srcDir = argv[1];
    } else if (QFileInfo::exists("../Card.png")) {
        srcDir = "..";
    }
    QString cardPng = srcDir + "/Card.png";
    QString cardPdn = srcDir + "/Card.pdn";
    QString cmakeFile = srcDir + "/CMakeLists.txt";

    // Test 1: ImageIO::isImageFile and ImageIO::loadImage
    {
        std::cout << "Test 1: ImageIO file inspection and loading..." << std::endl;
        assert(pdn::ImageIO::isImageFile(cardPng));
        assert(pdn::ImageIO::isImageFile(cardPdn));
        assert(!pdn::ImageIO::isImageFile(cmakeFile));
        assert(!pdn::ImageIO::isImageFile("non_existent_file.png"));

        QString err;
        QImage cardImg = pdn::ImageIO::loadImage(cardPng, &err);
        assert(!cardImg.isNull());
        assert(cardImg.format() == QImage::Format_ARGB32);
        assert(cardImg.width() > 0 && cardImg.height() > 0);

        QImage pdnImg = pdn::ImageIO::loadImage(cardPdn, &err);
        assert(!pdnImg.isNull());
        assert(pdnImg.format() == QImage::Format_ARGB32);
        assert(pdnImg.width() > 0 && pdnImg.height() > 0);

        std::cout << "  Passed: ImageIO::isImageFile and ImageIO::loadImage correctly recognize and load formats." << std::endl;
    }

    // Test 2: Clipboard raw image detection and paste within bounds
    {
        std::cout << "Test 2: Raw clipboard image paste without bounds exceeded..." << std::endl;
        pdn::MainWindow win;
        auto doc = win.document();
        assert(doc != nullptr);
        assert(doc->width() == 800 && doc->height() == 600);
        int initialLayers = doc->layerCount();

        // Put a 200x150 image into the clipboard
        QImage clipImg(200, 150, QImage::Format_ARGB32);
        clipImg.fill(Qt::red);
        QGuiApplication::clipboard()->setImage(clipImg);

        QString sourceName;
        QImage retrieved = win.getClipboardImage(&sourceName);
        assert(!retrieved.isNull());
        assert(retrieved.size() == QSize(200, 150));

        // Paste image: fits within 800x600, no expansion needed
        bool pasted = win.pasteImage(retrieved, pdn::CanvasExpandChoice::Prompt);
        assert(pasted);
        assert(doc->width() == 800 && doc->height() == 600);
        assert(doc->hasFloatingSelection());
        assert(doc->floatingImage().size() == QSize(200, 150));
        assert(doc->selection().boundingRect() == QRectF(0, 0, 200, 150));
        // Requirement check: Normal paste DOES NOT create a new layer in layer list
        assert(doc->layerCount() == initialLayers);

        std::cout << "  Passed: Raw image pasted directly into active layer via floating selection." << std::endl;
    }

    // Test 3: Clipboard file URL detection (e.g. copied from file explorer)
    {
        std::cout << "Test 3: Clipboard file URL detection..." << std::endl;
        pdn::MainWindow win;

        // Set QMimeData with file URL in clipboard
        QMimeData* mimeData = new QMimeData();
        QFileInfo fi(cardPng);
        mimeData->setUrls({ QUrl::fromLocalFile(fi.absoluteFilePath()) });
        QGuiApplication::clipboard()->setMimeData(mimeData);

        QString sourceName;
        QImage retrieved = win.getClipboardImage(&sourceName);
        assert(!retrieved.isNull());
        assert(sourceName == "Card");

        std::cout << "  Passed: Image file in clipboard correctly extracted as image with source name." << std::endl;
    }

    // Test 4: Pasting image that exceeds canvas bounds - KeepCanvasSize
    {
        std::cout << "Test 4: Paste exceeding bounds with KeepCanvasSize..." << std::endl;
        pdn::MainWindow win;
        // Document starts at 800x600
        auto doc = win.document();
        assert(doc->width() == 800 && doc->height() == 600);
        int initialLayers = doc->layerCount();

        // Image of 1200x900 exceeds 800x600
        QImage bigImg(1200, 900, QImage::Format_ARGB32);
        bigImg.fill(Qt::blue);

        bool pasted = win.pasteImage(bigImg, pdn::CanvasExpandChoice::KeepCanvasSize);
        assert(pasted);
        // Canvas size remains 800x600!
        assert(doc->width() == 800 && doc->height() == 600);
        // No new visible layer created
        assert(doc->layerCount() == initialLayers);
        // Floating selection holds the full 1200x900 image and bounds
        assert(doc->hasFloatingSelection());
        assert(doc->floatingImage().size() == QSize(1200, 900));
        assert(doc->selection().boundingRect() == QRectF(0, 0, 1200, 900));

        std::cout << "  Passed: KeepCanvasSize preserves canvas dimensions and creates floating selection exceeding bounds." << std::endl;
    }

    // Test 5: Pasting image that exceeds canvas bounds - ExpandCanvas
    {
        std::cout << "Test 5: Paste exceeding bounds with ExpandCanvas..." << std::endl;
        pdn::MainWindow win;
        auto doc = win.document();
        assert(doc->width() == 800 && doc->height() == 600);
        int initialLayers = doc->layerCount();

        // Image of 1200x900 exceeds 800x600
        QImage bigImg(1200, 900, QImage::Format_ARGB32);
        bigImg.fill(Qt::yellow);

        bool pasted = win.pasteImage(bigImg, pdn::CanvasExpandChoice::ExpandCanvas);
        assert(pasted);
        // Canvas is enlarged to max(800, 1200) = 1200 and max(600, 900) = 900
        assert(doc->width() == 1200 && doc->height() == 900);
        // No new layer created
        assert(doc->layerCount() == initialLayers);
        assert(doc->hasFloatingSelection());
        assert(doc->floatingImage().size() == QSize(1200, 900));
        assert(doc->selection().boundingRect() == QRectF(0, 0, 1200, 900));

        std::cout << "  Passed: ExpandCanvas enlarged canvas bounds to fit both previous and new image." << std::endl;
    }

    // Test 6: Pasting image that exceeds only one dimension
    {
        std::cout << "Test 6: Paste exceeding one dimension with ExpandCanvas..." << std::endl;
        pdn::MainWindow win;
        auto doc = win.document();
        assert(doc->width() == 800 && doc->height() == 600);

        // Image width exceeds (1000 > 800), but height is smaller (400 < 600)
        QImage wideImg(1000, 400, QImage::Format_ARGB32);
        wideImg.fill(Qt::green);

        bool pasted = win.pasteImage(wideImg, pdn::CanvasExpandChoice::ExpandCanvas);
        assert(pasted);
        assert(doc->width() == 1000);
        assert(doc->height() == 600); // Kept 600 since max(600, 400) == 600!

        std::cout << "  Passed: ExpandCanvas correctly took max along each dimension independently." << std::endl;
    }

    // Test 7: Pasting image that exceeds canvas bounds - Cancel
    {
        std::cout << "Test 7: Paste exceeding bounds with Cancel..." << std::endl;
        pdn::MainWindow win;
        auto doc = win.document();
        assert(doc->width() == 800 && doc->height() == 600);

        QImage bigImg(1200, 900, QImage::Format_ARGB32);
        bigImg.fill(Qt::cyan);

        bool pasted = win.pasteImage(bigImg, pdn::CanvasExpandChoice::Cancel);
        assert(!pasted);
        assert(doc->width() == 800 && doc->height() == 600);
        assert(!doc->hasFloatingSelection());

        std::cout << "  Passed: Cancel aborts the paste operation without altering the canvas." << std::endl;
    }

    // Test 8: Paste into New Layer
    {
        std::cout << "Test 8: Paste into New Layer with bounds check..." << std::endl;
        pdn::MainWindow win;
        auto doc = win.document();
        int initialLayers = doc->layerCount();

        QImage bigImg(1100, 700, QImage::Format_ARGB32);
        bigImg.fill(Qt::magenta);

        bool pasted = win.pasteImageIntoNewLayer(bigImg, pdn::CanvasExpandChoice::ExpandCanvas, "Custom Layer");
        assert(pasted);
        assert(doc->width() == 1100 && doc->height() == 700);
        // In this case, a new layer IS created!
        assert(doc->layerCount() == initialLayers + 1);
        assert(doc->activeLayer()->name() == "Custom Layer");
        assert(doc->hasFloatingSelection());

        std::cout << "  Passed: Paste into New Layer created new layer and placed floating selection on it." << std::endl;
    }

    // Test 9: Add Image as Layer (from dropped image file)
    {
        std::cout << "Test 9: Add Image as Layer from file with ExpandCanvas..." << std::endl;
        pdn::MainWindow win;
        auto doc = win.document();
        int initialLayers = doc->layerCount();

        QImage droppedImg(1500, 1000, QImage::Format_ARGB32);
        droppedImg.fill(Qt::darkBlue);

        bool added = win.addImageAsLayer(droppedImg, "Photo", pdn::CanvasExpandChoice::ExpandCanvas);
        assert(added);
        assert(doc->width() == 1500 && doc->height() == 1000);
        assert(doc->layerCount() == initialLayers + 1);
        assert(doc->activeLayer()->name() == "Photo");
        assert(doc->hasFloatingSelection());

        std::cout << "  Passed: addImageAsLayer added new layer with floating selection and enlarged canvas." << std::endl;
    }

    // Test 10: Add Image as Layer with KeepCanvasSize
    {
        std::cout << "Test 10: Add Image as Layer with KeepCanvasSize..." << std::endl;
        pdn::MainWindow win;
        auto doc = win.document();
        assert(doc->width() == 800 && doc->height() == 600);
        int initialLayers = doc->layerCount();

        QImage droppedImg(1500, 1000, QImage::Format_ARGB32);
        droppedImg.fill(Qt::darkCyan);

        bool added = win.addImageAsLayer(droppedImg, "Dropped Photo", pdn::CanvasExpandChoice::KeepCanvasSize);
        assert(added);
        assert(doc->width() == 800 && doc->height() == 600); // Kept!
        assert(doc->layerCount() == initialLayers + 1);
        assert(doc->activeLayer()->name() == "Dropped Photo");
        assert(doc->hasFloatingSelection());
        assert(doc->floatingImage().size() == QSize(1500, 1000));
        assert(doc->selection().boundingRect() == QRectF(0, 0, 1500, 1000));

        std::cout << "  Passed: addImageAsLayer with KeepCanvasSize created new layer and floating selection exceeding bounds." << std::endl;
    }

    // Test 11: Document replacement and multi-document addLayer verification
    {
        std::cout << "Test 11: Document open vs add layer with Card.png..." << std::endl;
        pdn::MainWindow win;
        // Starts with pristine 800x600 Untitled document
        assert(win.documentCount() == 1);
        assert(win.document()->fileName() == "Untitled");

        // When opened as a new document, replaces pristine document
        bool opened = win.openFile(cardPng);
        assert(opened);
        assert(win.documentCount() == 1);
        assert(win.document()->fileName().contains("Card.png"));
        int cardW = win.document()->width();
        int cardH = win.document()->height();

        // Now add a layer using an image from another file
        QImage anotherImg(cardW + 100, cardH + 50, QImage::Format_ARGB32);
        anotherImg.fill(Qt::yellow);
        bool layerAdded = win.addImageAsLayer(anotherImg, "NewLayer", pdn::CanvasExpandChoice::ExpandCanvas);
        assert(layerAdded);
        assert(win.document()->layerCount() == 2);
        assert(win.document()->width() == cardW + 100);
        assert(win.document()->height() == cardH + 50);

        std::cout << "  Passed: Document open and add layer verified with real image data." << std::endl;
    }

    std::cout << "\nALL CLIPBOARD & DROP IMAGE TESTS PASSED!" << std::endl;
    return 0;
}
