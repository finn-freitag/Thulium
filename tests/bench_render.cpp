// Performance benchmark for the hot interactive paths on a large (6K) document.
// Not part of ctest; run manually:  QT_QPA_PLATFORM=offscreen ./bench_render [width height]
#include <QApplication>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPainter>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include "core/Document.h"
#include "tools/ToolManager.h"
#include "ui/CanvasView.h"

using namespace pdn;

static double timeMs(int iterations, const std::function<void()>& fn) {
    QElapsedTimer t;
    t.start();
    for (int i = 0; i < iterations; ++i) fn();
    return t.nsecsElapsed() / 1e6 / iterations;
}

static void sendMouse(ITool* tool, QEvent::Type type, Document* doc, ToolContext& ctx, const QPointF& docPos) {
    Qt::MouseButton button = (type == QEvent::MouseMove) ? Qt::NoButton : Qt::LeftButton;
    Qt::MouseButtons buttons = (type == QEvent::MouseButtonRelease) ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent ev(type, docPos, docPos, button, buttons, Qt::NoModifier);
    if (type == QEvent::MouseButtonPress) tool->mousePress(&ev, doc, docPos, ctx);
    else if (type == QEvent::MouseMove) tool->mouseMove(&ev, doc, docPos, ctx);
    else tool->mouseRelease(&ev, doc, docPos, ctx);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const int w = argc > 2 ? std::atoi(argv[1]) : 6000;
    const int h = argc > 2 ? std::atoi(argv[2]) : 4000;

    // A photo-like opaque background plus one partially transparent layer.
    QImage photo(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y) {
        auto* line = reinterpret_cast<uint32_t*>(photo.scanLine(y));
        for (int x = 0; x < w; ++x) {
            line[x] = 0xFF000000u | ((x * 255 / w) << 16) | ((y * 255 / h) << 8) | ((x ^ y) & 0xFF);
        }
    }
    auto doc = std::make_shared<Document>(photo);
    doc->addLayer("Overlay", false);
    {
        QPainter p(&doc->activeLayer()->image());
        p.fillRect(w / 4, h / 4, w / 2, h / 2, QColor(255, 0, 0, 120));
    }

    ToolManager toolMgr;
    CanvasView view(&toolMgr);
    view.resize(1920, 1080);
    view.setDocument(doc);

    QImage target(view.size(), QImage::Format_RGB32);
    auto paintView = [&]() { view.render(&target); };

    std::printf("Document %dx%d, %d layers, viewport %dx%d\n", w, h, doc->layerCount(), view.width(), view.height());
    std::printf("%-44s %9.2f ms\n", "Document::composite() full", timeMs(3, [&]() { doc->composite(); }));

    paintView(); // warm caches
    std::printf("%-44s %9.2f ms\n", "Repaint, fit-to-window (idle frame)", timeMs(10, paintView));

    view.setZoom(1.0);
    paintView();
    std::printf("%-44s %9.2f ms\n", "Repaint, 100% zoom (idle frame)", timeMs(10, paintView));

    int panStep = 0;
    std::printf("%-44s %9.2f ms\n", "Pan + repaint, 100% zoom", timeMs(20, [&]() {
        view.setPanOffset(view.panOffset() + QPointF(-7, -5 + (panStep++ % 2)));
        paintView();
    }));

    view.zoomToWindow();
    doc->selection().addEllipse(QRectF(w * 0.2, h * 0.2, w * 0.6, h * 0.6));
    emit doc->selectionChanged();
    paintView();
    std::printf("%-44s %9.2f ms\n", "Repaint with ellipse selection (ants frame)", timeMs(10, paintView));
    doc->selection().clear();
    emit doc->selectionChanged();

    // Brush stroke: press, 60 moves with a repaint after each, release.
    toolMgr.setActiveTool(ToolType::Paintbrush, doc.get());
    toolMgr.context().brushWidth = 40;
    ITool* brush = toolMgr.activeTool().get();
    ToolContext& ctx = toolMgr.context();
    QElapsedTimer t;
    t.start();
    sendMouse(brush, QEvent::MouseButtonPress, doc.get(), ctx, QPointF(w * 0.3, h * 0.3));
    const double pressMs = t.nsecsElapsed() / 1e6;
    t.restart();
    const int moves = 60;
    for (int i = 1; i <= moves; ++i) {
        sendMouse(brush, QEvent::MouseMove, doc.get(), ctx, QPointF(w * 0.3 + i * 25, h * 0.3 + i * 12));
        paintView();
    }
    const double moveMs = t.nsecsElapsed() / 1e6 / moves;
    t.restart();
    sendMouse(brush, QEvent::MouseButtonRelease, doc.get(), ctx, QPointF(w * 0.3 + moves * 25, h * 0.3 + moves * 12));
    const double releaseMs = t.nsecsElapsed() / 1e6;
    std::printf("%-44s %9.2f ms\n", "Brush stroke: press", pressMs);
    std::printf("%-44s %9.2f ms\n", "Brush stroke: move + repaint (avg)", moveMs);
    std::printf("%-44s %9.2f ms\n", "Brush stroke: release (undo capture)", releaseMs);

    std::printf("%-44s %9.2f ms\n", "Undo + repaint", timeMs(1, [&]() { doc->undoStack()->undo(); paintView(); }));
    std::printf("%-44s %9.2f ms\n", "Redo + repaint", timeMs(1, [&]() { doc->undoStack()->redo(); paintView(); }));

    // Magic wand on a flat region of the overlay layer.
    toolMgr.setActiveTool(ToolType::MagicWand, doc.get());
    ctx.tolerance = 30;
    t.restart();
    sendMouse(toolMgr.activeTool().get(), QEvent::MouseButtonPress, doc.get(), ctx, QPointF(w / 2.0, h / 2.0));
    paintView();
    std::printf("%-44s %9.2f ms\n", "Magic wand (flat region) + repaint", t.nsecsElapsed() / 1e6);
    return 0;
}
