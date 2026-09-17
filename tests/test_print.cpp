#include <iostream>
#include <cassert>
#include <cmath>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QTemporaryFile>
#include "../src/core/Document.h"
#include "../src/ui/PrintDialog.h"
#include "../src/ui/MainWindow.h"
#include <QMenuBar>
#include <QAction>

int main(int argc, char* argv[]) {
    int fakeArgc = 1;
    char* fakeArgv[] = { argv[0], nullptr };
    QApplication app(fakeArgc, fakeArgv);

    std::cout << "=== Running Print Feature Test Suite ===" << std::endl;

    // ------------------------------------------------------------------------
    // Test 1: Unit conversion math
    // ------------------------------------------------------------------------
    std::cout << "Test 1: Unit conversions (mm, cm, inch)..." << std::endl;
    {
        // 1.0 inch -> 25.4 mm
        assert(std::abs(pdn::PrintDialog::toMm(1.0, pdn::PrintUnit::Inches) - 25.4) < 1e-6);
        // 2.54 cm -> 25.4 mm
        assert(std::abs(pdn::PrintDialog::toMm(2.54, pdn::PrintUnit::Centimeters) - 25.4) < 1e-6);
        // 25.4 mm -> 25.4 mm
        assert(std::abs(pdn::PrintDialog::toMm(25.4, pdn::PrintUnit::Millimeters) - 25.4) < 1e-6);

        // 25.4 mm -> 1.0 inch
        assert(std::abs(pdn::PrintDialog::fromMm(25.4, pdn::PrintUnit::Inches) - 1.0) < 1e-6);
        // 25.4 mm -> 2.54 cm
        assert(std::abs(pdn::PrintDialog::fromMm(25.4, pdn::PrintUnit::Centimeters) - 2.54) < 1e-6);
        // 25.4 mm -> 25.4 mm
        assert(std::abs(pdn::PrintDialog::fromMm(25.4, pdn::PrintUnit::Millimeters) - 25.4) < 1e-6);

        // Check unit suffixes
        assert(pdn::PrintDialog::unitSuffix(pdn::PrintUnit::Millimeters) == " mm");
        assert(pdn::PrintDialog::unitSuffix(pdn::PrintUnit::Centimeters) == " cm");
        assert(pdn::PrintDialog::unitSuffix(pdn::PrintUnit::Inches) == " in");

        std::cout << "  Passed: Unit conversions are mathematically exact!" << std::endl;
    }

    // ------------------------------------------------------------------------
    // Test 2: Dialog Initialization with Document
    // ------------------------------------------------------------------------
    std::cout << "Test 2: Dialog initialization and default dimensions..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(800, 600);
        doc->setDpi(96.0);

        pdn::PrintDialog dlg(doc.get());

        // Default unit is Centimeters
        assert(dlg.currentUnit() == pdn::PrintUnit::Centimeters);

        // Default orientation is Portrait
        assert(dlg.pageOrientation() == QPageLayout::Portrait);

        // Default aspect ratio maintained
        assert(dlg.maintainAspectRatio() == true);

        // Default centered on page
        assert(dlg.isCenterOnPage() == true);

        // Check image dimensions aspect ratio (800 / 600 = 4/3 = 1.333...)
        double aspect = dlg.imageWidthMm() / dlg.imageHeightMm();
        assert(std::abs(aspect - (4.0 / 3.0)) < 0.01);

        // Check centering
        double expectedLeft = (dlg.pageWidthMm() - dlg.imageWidthMm()) / 2.0;
        double expectedTop = (dlg.pageHeightMm() - dlg.imageHeightMm()) / 2.0;
        assert(std::abs(dlg.marginLeftMm() - expectedLeft) < 0.01);
        assert(std::abs(dlg.marginTopMm() - expectedTop) < 0.01);

        std::cout << "  Passed: Dialog correctly initialized with document aspect ratio and centering!" << std::endl;
    }

    // ------------------------------------------------------------------------
    // Test 3: Unit Switching
    // ------------------------------------------------------------------------
    std::cout << "Test 3: Unit switching preserves physical millimeters..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(800, 600);
        pdn::PrintDialog dlg(doc.get());

        double origW = dlg.imageWidthMm();
        double origH = dlg.imageHeightMm();
        double origL = dlg.marginLeftMm();
        double origT = dlg.marginTopMm();

        dlg.setUnit(pdn::PrintUnit::Inches);
        assert(dlg.currentUnit() == pdn::PrintUnit::Inches);
        assert(std::abs(dlg.imageWidthMm() - origW) < 1e-5);
        assert(std::abs(dlg.imageHeightMm() - origH) < 1e-5);
        assert(std::abs(dlg.marginLeftMm() - origL) < 1e-5);
        assert(std::abs(dlg.marginTopMm() - origT) < 1e-5);

        dlg.setUnit(pdn::PrintUnit::Millimeters);
        assert(dlg.currentUnit() == pdn::PrintUnit::Millimeters);
        assert(std::abs(dlg.imageWidthMm() - origW) < 1e-5);

        dlg.setUnit(pdn::PrintUnit::Centimeters);
        assert(dlg.currentUnit() == pdn::PrintUnit::Centimeters);
        assert(std::abs(dlg.imageWidthMm() - origW) < 1e-5);

        std::cout << "  Passed: Switching between cm, mm, and inches preserves exact physical dimensions!" << std::endl;
    }

    // ------------------------------------------------------------------------
    // Test 4: Aspect Ratio Maintenance and Resizing
    // ------------------------------------------------------------------------
    std::cout << "Test 4: Aspect ratio lock behavior..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(1000, 500); // 2:1 aspect ratio
        pdn::PrintDialog dlg(doc.get());

        dlg.setImageDimensionsMm(100.0, 50.0);
        assert(std::abs(dlg.imageWidthMm() - 100.0) < 1e-5);
        assert(std::abs(dlg.imageHeightMm() - 50.0) < 1e-5);

        // Aspect ratio is 2.0
        double aspect = dlg.imageWidthMm() / dlg.imageHeightMm();
        assert(std::abs(aspect - 2.0) < 1e-5);

        // Disable maintain aspect ratio
        dlg.setMaintainAspectRatio(false);
        assert(dlg.maintainAspectRatio() == false);

        dlg.setImageDimensionsMm(120.0, 80.0);
        assert(std::abs(dlg.imageWidthMm() - 120.0) < 1e-5);
        assert(std::abs(dlg.imageHeightMm() - 80.0) < 1e-5);

        std::cout << "  Passed: Aspect ratio lock and unlock work accurately!" << std::endl;
    }

    // ------------------------------------------------------------------------
    // Test 5: Placement, Margins, and Centering
    // ------------------------------------------------------------------------
    std::cout << "Test 5: Margins and custom placement..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(600, 600);
        pdn::PrintDialog dlg(doc.get());

        // Custom margins
        dlg.setMarginsMm(15.0, 25.0);
        assert(std::abs(dlg.marginLeftMm() - 15.0) < 1e-5);
        assert(std::abs(dlg.marginTopMm() - 25.0) < 1e-5);
        assert(dlg.isCenterOnPage() == false);

        // Re-enable centering
        dlg.setCenterOnPage(true);
        assert(dlg.isCenterOnPage() == true);
        double expectedLeft = (dlg.pageWidthMm() - dlg.imageWidthMm()) / 2.0;
        double expectedTop = (dlg.pageHeightMm() - dlg.imageHeightMm()) / 2.0;
        assert(std::abs(dlg.marginLeftMm() - expectedLeft) < 1e-5);
        assert(std::abs(dlg.marginTopMm() - expectedTop) < 1e-5);

        std::cout << "  Passed: Margins and centering logic function correctly!" << std::endl;
    }

    // ------------------------------------------------------------------------
    // Test 6: Page Orientation (Portrait <-> Landscape)
    // ------------------------------------------------------------------------
    std::cout << "Test 6: Page orientation switching..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(800, 600);
        pdn::PrintDialog dlg(doc.get());

        // Initial Portrait: width < height for A4
        assert(dlg.pageOrientation() == QPageLayout::Portrait);
        double portW = dlg.pageWidthMm();
        double portH = dlg.pageHeightMm();
        assert(portW < portH);

        // Switch to Landscape
        dlg.setPageOrientation(QPageLayout::Landscape);
        assert(dlg.pageOrientation() == QPageLayout::Landscape);
        double landW = dlg.pageWidthMm();
        double landH = dlg.pageHeightMm();
        assert(landW > landH);
        assert(std::abs(landW - portH) < 1e-5);
        assert(std::abs(landH - portW) < 1e-5);

        // Switch back to Portrait
        dlg.setPageOrientation(QPageLayout::Portrait);
        assert(dlg.pageOrientation() == QPageLayout::Portrait);
        assert(std::abs(dlg.pageWidthMm() - portW) < 1e-5);
        assert(std::abs(dlg.pageHeightMm() - portH) < 1e-5);

        std::cout << "  Passed: Page orientation swaps page dimensions properly!" << std::endl;
    }

    // ------------------------------------------------------------------------
    // Test 7: Print to PDF Generation
    // ------------------------------------------------------------------------
    std::cout << "Test 7: PDF export / print verification..." << std::endl;
    {
        auto doc = std::make_shared<pdn::Document>(400, 300);
        // Paint a pattern into the document layer
        auto layer = doc->activeLayer();
        QImage& img = layer->image();
        img.fill(QColor(100, 150, 200));

        pdn::PrintDialog dlg(doc.get());
        dlg.setImageDimensionsMm(140.0, 105.0);
        dlg.setCenterOnPage(true);

        QTemporaryFile tempPdf;
        tempPdf.setFileTemplate(QDir::tempPath() + "/pdn_test_print_XXXXXX.pdf");
        assert(tempPdf.open());
        QString tempPath = tempPdf.fileName();
        tempPdf.close();

        bool success = dlg.printToPdf(tempPath);
        assert(success == true);

        // Verify PDF file exists and is valid
        QFile pdfFile(tempPath);
        assert(pdfFile.exists());
        assert(pdfFile.size() > 500); // Valid PDF header and objects

        assert(pdfFile.open(QIODevice::ReadOnly));
        QByteArray header = pdfFile.read(5);
        assert(header.startsWith("%PDF-"));
        pdfFile.close();
        pdfFile.remove();

        std::cout << "  Passed: Successfully printed document to PDF with valid PDF output!" << std::endl;
    }

    // ------------------------------------------------------------------------
    // Test 8: MainWindow File Menu Print Action
    // ------------------------------------------------------------------------
    std::cout << "Test 8: MainWindow File menu Print action..." << std::endl;
    {
        pdn::MainWindow mw;
        QMenuBar* mb = mw.menuBar();
        assert(mb != nullptr);

        bool foundPrintAction = false;
        for (QAction* act : mb->actions()) {
            QMenu* menu = act->menu();
            if (menu && menu->title().contains("File")) {
                for (QAction* item : menu->actions()) {
                    if (item->text().contains("Print")) {
                        foundPrintAction = true;
                        assert(item->shortcut() == QKeySequence::Print);
                        break;
                    }
                }
            }
        }
        assert(foundPrintAction);
        std::cout << "  Passed: Print action found in File menu with Ctrl+P shortcut!" << std::endl;
    }

    std::cout << "=== All Print Feature Tests Passed Successfully! ===" << std::endl;
    return 0;
}
