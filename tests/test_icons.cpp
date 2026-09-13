#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QToolButton>
#include <iostream>
#include <cassert>
#include <vector>
#include "ui/ToolsDock.h"
#include "ui/LayersDock.h"
#include "tools/ToolManager.h"

int main(int argc, char* argv[]) {
    // Set offscreen platform for headless execution
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "=== Running Icons Verification Test Suite ===" << std::endl;

    std::vector<QString> iconPaths = {
        ":/icons/rectangle-select.svg",
        ":/icons/move-pixels.svg",
        ":/icons/lasso-select.svg",
        ":/icons/move-selection.svg",
        ":/icons/ellipse-select.svg",
        ":/icons/zoom.svg",
        ":/icons/magic-wand.svg",
        ":/icons/pan.svg",
        ":/icons/paint-bucket.svg",
        ":/icons/paint-brush.svg",
        ":/icons/eraser.svg",
        ":/icons/pencil.svg",
        ":/icons/color-picker.svg",
        ":/icons/clone-stamp.svg",
        ":/icons/recolor.svg",
        ":/icons/gradient.svg",
        ":/icons/text.svg",
        ":/icons/line-curve.svg",
        ":/icons/shapes.svg",
        ":/icons/layer-add.svg",
        ":/icons/layer-delete.svg",
        ":/icons/layer-duplicate.svg",
        ":/icons/layer-merge-down.svg",
        ":/icons/layer-move-up.svg",
        ":/icons/layer-move-down.svg",
        ":/icons/layer-properties.svg"
    };

    std::cout << "Test 1: Verifying all " << iconPaths.size() << " resource icons load and render..." << std::endl;
    for (const auto& path : iconPaths) {
        QIcon icon(path);
        if (icon.isNull()) {
            std::cerr << "FAILED: Icon is null: " << path.toStdString() << std::endl;
            return 1;
        }
        QPixmap pixmap = icon.pixmap(QSize(24, 24));
        if (pixmap.isNull() || pixmap.width() == 0 || pixmap.height() == 0) {
            std::cerr << "FAILED: Icon pixmap is null or empty: " << path.toStdString() << std::endl;
            return 1;
        }
        std::cout << "  [PASS] " << path.toStdString() << " (" << pixmap.width() << "x" << pixmap.height() << ")" << std::endl;
    }

    std::cout << "Test 2: Verifying ToolsDock buttons have non-null icons..." << std::endl;
    pdn::ToolManager toolMgr;
    pdn::ToolsDock toolsDock(&toolMgr);
    auto toolButtons = toolsDock.findChildren<QToolButton*>();
    assert(toolButtons.size() == 19);
    for (auto* btn : toolButtons) {
        assert(!btn->icon().isNull());
        QPixmap px = btn->icon().pixmap(btn->iconSize());
        assert(!px.isNull());
    }
    std::cout << "  [PASS] All 19 tool buttons in ToolsDock have valid icons." << std::endl;

    std::cout << "Test 3: Verifying LayersDock buttons have non-null icons..." << std::endl;
    pdn::LayersDock layersDock;
    auto layerButtons = layersDock.findChildren<QToolButton*>();
    assert(layerButtons.size() == 7);
    for (auto* btn : layerButtons) {
        assert(!btn->icon().isNull());
        QPixmap px = btn->icon().pixmap(btn->iconSize());
        assert(!px.isNull());
    }
    std::cout << "  [PASS] All 7 layer operation buttons in LayersDock have valid icons." << std::endl;

    std::cout << "=== All Icon Tests Passed Successfully! ===" << std::endl;
    return 0;
}
