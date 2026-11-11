// Regression tests for the cached compositing, patch-based undo, region selection model,
// flood fill, lazy layer edits and the data-driven effects.
#include <QApplication>
#include <QMouseEvent>
#include <cstdlib>
#include <iostream>
#include "core/Document.h"
#include "core/FloodFill.h"
#include "core/History.h"
#include "effects/GaussianBlur.h"
#include "effects/PluginManager.h"
#include "tools/LayerEdit.h"
#include "tools/ToolManager.h"
#include "ui/CanvasView.h"
#include "ui/MainWindow.h"
#include <QMenuBar>
#include <QSettings>
#include <QTemporaryDir>

using namespace pdn;

// assert() disappears in release builds; these checks must always run.
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::cerr << "FAILED: " #cond " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            std::exit(1);                                                                   \
        }                                                                                   \
    } while (0)

static QImage noiseImage(int w, int h, bool withAlpha) {
    QImage img(w, h, QImage::Format_ARGB32);
    uint32_t state = 12345;
    for (int y = 0; y < h; ++y) {
        auto* line = reinterpret_cast<uint32_t*>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            state = state * 1664525u + 1013904223u;
            line[x] = withAlpha ? state : (state | 0xFF000000u);
        }
    }
    return img;
}

static void mouse(ITool* tool, QEvent::Type type, Document* doc, ToolContext& ctx, const QPointF& pos,
                  Qt::MouseButton button = Qt::LeftButton) {
    QMouseEvent ev(type, pos, pos, type == QEvent::MouseMove ? Qt::NoButton : button,
                   type == QEvent::MouseButtonRelease ? Qt::NoButton : button, Qt::NoModifier);
    if (type == QEvent::MouseButtonPress) tool->mousePress(&ev, doc, pos, ctx);
    else if (type == QEvent::MouseMove) tool->mouseMove(&ev, doc, pos, ctx);
    else tool->mouseRelease(&ev, doc, pos, ctx);
}

static void dragTool(ITool* tool, Document* doc, ToolContext& ctx, const QPointF& from, const QPointF& to,
                     Qt::MouseButton button = Qt::LeftButton) {
    mouse(tool, QEvent::MouseButtonPress, doc, ctx, from, button);
    mouse(tool, QEvent::MouseMove, doc, ctx, (from + to) / 2, button);
    mouse(tool, QEvent::MouseMove, doc, ctx, to, button);
    mouse(tool, QEvent::MouseButtonRelease, doc, ctx, to, button);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    std::cout << "=== Running Core Rework Test Suite ===" << std::endl;

    {
        std::cout << "Test 1: Selection outline, mask lookup and shape regions..." << std::endl;
        Selection sel(100, 80);
        sel.addRect(QRectF(10, 20, 30, 40));
        CHECK(sel.outlineSegments().size() == 4);
        int perimeter = 0;
        for (const QLine& s : sel.outlineSegments()) perimeter += std::abs(s.dx()) + std::abs(s.dy());
        CHECK(perimeter == 2 * (30 + 40));

        // An L shape has 6 edges; shared interior edges must not appear
        sel.addRect(QRectF(40, 20, 20, 10), SelectionCombineMode::Union);
        perimeter = 0;
        for (const QLine& s : sel.outlineSegments()) perimeter += std::abs(s.dx()) + std::abs(s.dy());
        CHECK(perimeter == 50 + 10 + 20 + 30 + 30 + 40);

        CHECK(sel.containsPixel(10, 20) && sel.containsPixel(59, 29) && !sel.containsPixel(59, 30));
        CHECK(!sel.containsPixel(-1, 20) && !sel.containsPixel(100, 20));
        const quint64 before = sel.revision();
        sel.clear();
        CHECK(sel.revision() != before);
        CHECK(sel.containsPixel(5, 5)); // nothing selected = everything editable

        const QRegion ellipse = Selection::regionFromEllipse(QRectF(0, 0, 21, 11));
        CHECK(ellipse.boundingRect() == QRect(0, 0, 21, 11));
        CHECK(ellipse.contains(QPoint(10, 5)) && !ellipse.contains(QPoint(0, 0)));
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 2: Region compositing matches the full composite..." << std::endl;
        Document doc(noiseImage(97, 61, false));
        doc.addLayer("Top", false);
        doc.activeLayer()->setImage(noiseImage(97, 61, true));
        doc.activeLayer()->setOpacity(180);
        doc.activeLayer()->setBlendMode(BlendMode::Multiply);

        const QImage full = doc.composite();
        const QRect rect(13, 7, 40, 30);
        QImage part(rect.size(), QImage::Format_ARGB32);
        doc.compositeRect(rect, reinterpret_cast<uint32_t*>(part.bits()), part.bytesPerLine());
        CHECK(part == full.copy(rect));
        CHECK(doc.compositePixel(50, 33) == full.pixel(50, 33));
        CHECK(doc.compositePixel(-1, 0) == 0);

        const QImage thumb = doc.thumbnail(QSize(32, 32));
        CHECK(!thumb.isNull() && thumb.width() <= 32 && thumb.height() <= 32);
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 3: Brush stroke repaints and stores only what it touched..." << std::endl;
        auto doc = std::make_shared<Document>(noiseImage(400, 300, false));
        const QImage original = doc->activeLayer()->image().copy();
        ToolManager tools;
        tools.setDocument(doc.get());
        tools.setActiveTool(ToolType::Paintbrush);
        tools.context().brushWidth = 10;
        tools.context().primaryColor = Qt::red;

        QRect changed;
        int fullChanges = 0;
        QObject::connect(doc.get(), &Document::pixelsChanged, [&](const QRect& r) { changed = changed.united(r); });
        QObject::connect(doc.get(), &Document::documentChanged, [&]() { ++fullChanges; });

        dragTool(tools.activeTool().get(), doc.get(), tools.context(), QPointF(100, 100), QPointF(160, 120));
        CHECK(fullChanges == 0);                                     // never a whole-document refresh
        CHECK(!changed.isEmpty() && changed.width() < 120 && changed.height() < 80);
        CHECK(doc->activeLayer()->image().pixel(130, 110) == qRgb(255, 0, 0));
        CHECK(doc->activeLayer()->image().pixel(300, 250) == original.pixel(300, 250));
        CHECK(doc->undoStack()->count() == 1);

        doc->undoStack()->undo();
        CHECK(doc->activeLayer()->image() == original);
        doc->undoStack()->redo();
        CHECK(doc->activeLayer()->image().pixel(130, 110) == qRgb(255, 0, 0));
        doc->undoStack()->undo();
        CHECK(doc->activeLayer()->image() == original);
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 4: LayerEdit cancel restores, commit without changes records nothing..." << std::endl;
        Document doc(noiseImage(600, 500, true));
        const QImage original = doc.activeLayer()->image().copy();
        LayerEdit edit;
        CHECK(edit.begin(&doc));
        const QRect a(10, 10, 50, 50), b(500, 400, 80, 80); // far apart: different tiles
        for (const QRect& r : {a, b}) {
            edit.prepare(r);
            for (int y = r.top(); y <= r.bottom(); ++y)
                std::fill_n(doc.activeLayer()->scanLine(y) + r.left(), r.width(), 0xFF00FF00u);
            edit.markDirty(r);
        }
        CHECK(doc.activeLayer()->image() != original);
        edit.cancel();
        CHECK(doc.activeLayer()->image() == original);
        CHECK(doc.undoStack()->count() == 0);

        CHECK(edit.begin(&doc));
        edit.commit("Nothing");
        CHECK(doc.undoStack()->count() == 0);
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 5: Flood fill, contiguous vs global, tolerance and selection mask..." << std::endl;
        QImage img(40, 20, QImage::Format_ARGB32);
        img.fill(Qt::white);
        for (int y = 0; y < 20; ++y) img.setPixel(20, y, qRgb(0, 0, 0)); // wall splits the image
        const FloodMask left = floodFill(img, QPoint(5, 5), 0, true);
        CHECK(left.bounds == QRect(0, 0, 20, 20));
        CHECK(left.toRegion() == QRegion(0, 0, 20, 20));
        const FloodMask both = floodFill(img, QPoint(5, 5), 0, false);
        CHECK(both.toRegion() == QRegion(0, 0, 20, 20).united(QRegion(21, 0, 19, 20)));
        CHECK(floodFill(img, QPoint(5, 5), 100, true).toRegion() == QRegion(0, 0, 40, 20));
        CHECK(floodFill(img, QPoint(50, 5), 0, true).isEmpty());

        Selection sel(40, 20);
        sel.addRect(QRectF(0, 0, 10, 10));
        const FloodMask masked = floodFill(img, QPoint(5, 5), 0, true, &sel.mask(40, 20));
        CHECK(masked.toRegion() == QRegion(0, 0, 10, 10));
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 6: Flips and rotations undo without snapshots; selection follows the pixels..." << std::endl;
        Document doc(noiseImage(60, 40, true));
        const QImage original = doc.activeLayer()->image().copy();
        doc.selection().addRect(QRectF(5, 10, 20, 8));

        doc.flipHorizontal();
        CHECK(doc.selection().region().boundingRect() == QRect(60 - 25, 10, 20, 8));
        doc.rotate90CW();
        CHECK(doc.size() == QSize(40, 60));
        CHECK(doc.selection().region().boundingRect().size() == QSize(8, 20));
        doc.rotate180();
        doc.flipVertical();
        doc.rotate90CCW();
        while (doc.undoStack()->canUndo()) doc.undoStack()->undo();
        CHECK(doc.size() == QSize(60, 40));
        CHECK(doc.activeLayer()->image() == original);
        CHECK(doc.selection().region() == QRegion(5, 10, 20, 8));
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 7: Crop to a non-rectangular selection clears outside the shape..." << std::endl;
        Document doc(100, 100);
        doc.activeLayer()->fill(Qt::red);
        doc.selection().addEllipse(QRectF(20, 20, 40, 40));
        doc.cropToSelection();
        CHECK(doc.size() == QSize(40, 40));
        CHECK(qAlpha(doc.activeLayer()->image().pixel(0, 0)) == 0);       // corner: outside the ellipse
        CHECK(doc.activeLayer()->image().pixel(20, 20) == qRgb(255, 0, 0)); // centre: kept
        doc.undoStack()->undo();
        CHECK(doc.size() == QSize(100, 100));
        CHECK(doc.activeLayer()->image().pixel(0, 0) == qRgb(255, 0, 0));
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 8: Selection tools: live preview, right-drag subtracts, click deselects..." << std::endl;
        auto doc = std::make_shared<Document>(200, 200);
        ToolManager tools;
        tools.setDocument(doc.get());
        tools.setActiveTool(ToolType::RectangleSelect);
        ITool* rect = tools.activeTool().get();
        ToolContext& ctx = tools.context();

        mouse(rect, QEvent::MouseButtonPress, doc.get(), ctx, QPointF(10, 10));
        mouse(rect, QEvent::MouseMove, doc.get(), ctx, QPointF(60, 50));
        CHECK(doc->selection().region() == QRegion(10, 10, 51, 41)); // visible while still dragging
        CHECK(doc->undoStack()->count() == 0);                        // but not recorded yet
        mouse(rect, QEvent::MouseButtonRelease, doc.get(), ctx, QPointF(109, 109));
        CHECK(doc->selection().region() == QRegion(10, 10, 100, 100));
        CHECK(doc->undoStack()->count() == 1);

        dragTool(rect, doc.get(), ctx, QPointF(60, 60), QPointF(150, 150), Qt::RightButton);
        CHECK(doc->selection().region() == QRegion(10, 10, 100, 100).subtracted(QRegion(60, 60, 91, 91)));
        doc->undoStack()->undo();
        CHECK(doc->selection().region() == QRegion(10, 10, 100, 100));

        mouse(rect, QEvent::MouseButtonPress, doc.get(), ctx, QPointF(150, 150));
        mouse(rect, QEvent::MouseButtonRelease, doc.get(), ctx, QPointF(150, 150));
        CHECK(doc->selection().isEmpty());
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 9: Gaussian blur is safe for any radius and respects alpha and selection..." << std::endl;
        QImage tiny(5, 3, QImage::Format_ARGB32);
        tiny.fill(qRgb(40, 80, 120));
        GaussianBlurEffect::process(tiny, Selection(), 100); // radius far larger than the image
        CHECK(tiny.pixel(2, 1) == qRgb(40, 80, 120));        // a flat colour stays flat

        // An opaque red dot on a transparent *green* background must not pick up any green
        QImage dot(31, 31, QImage::Format_ARGB32);
        dot.fill(qRgba(0, 255, 0, 0));
        dot.setPixel(15, 15, qRgba(255, 0, 0, 255));
        GaussianBlurEffect::process(dot, Selection(), 4);
        const QRgb halo = dot.pixel(17, 15);
        CHECK(qAlpha(halo) > 0 && qAlpha(halo) < 255 && qGreen(halo) < 8 && qRed(halo) > 240);

        QImage half = noiseImage(40, 40, false);
        const QImage before = half.copy();
        Selection sel(40, 40);
        sel.addRect(QRectF(0, 0, 20, 40));
        GaussianBlurEffect::process(half, sel, 3);
        CHECK(half.copy(20, 0, 20, 40) == before.copy(20, 0, 20, 40));
        CHECK(half.copy(0, 0, 20, 40) != before.copy(0, 0, 20, 40));
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 10: Every registered effect runs, is deterministic and stays inside the selection..." << std::endl;
        PluginManager plugins;
        CHECK(plugins.effects().size() >= 40);
        const QImage source = noiseImage(64, 48, true);
        Selection sel(64, 48);
        sel.addEllipse(QRectF(8, 8, 30, 30));
        const QImage& mask = sel.mask(64, 48);
        for (const auto& effect : plugins.effects()) {
            QImage a = source.copy(), b = source.copy();
            CHECK(effect->apply(a, sel));
            CHECK(effect->apply(b, sel));
            if (a != b) std::cerr << "not deterministic: " << qPrintable(effect->name()) << std::endl;
            CHECK(a == b);
            CHECK(a.size() == source.size() && a.format() == QImage::Format_ARGB32);
            for (int y = 0; y < 48; ++y) {
                for (int x = 0; x < 64; ++x) {
                    if (!mask.constScanLine(y)[x] && a.pixel(x, y) != source.pixel(x, y)) {
                        std::cerr << "changed outside selection: " << qPrintable(effect->name()) << std::endl;
                        CHECK(false);
                    }
                }
            }
        }
        std::cout << "  Passed (" << plugins.effects().size() << " effects)" << std::endl;
    }

    {
        std::cout << "Test 11: Canvas repaint after a local edit shows the new pixels..." << std::endl;
        auto doc = std::make_shared<Document>(300, 200);
        ToolManager tools;
        CanvasView view(&tools);
        view.resize(500, 400);
        view.setDocument(doc);
        view.toggleRulers(false);
        view.setZoom(1.0);
        view.setPanOffset(QPointF(20, 30));

        QImage frame(view.size(), QImage::Format_RGB32);
        view.render(&frame);
        CHECK(frame.pixel(20 + 150, 30 + 100) == qRgb(255, 255, 255));

        // Modify the layer directly and report only the changed rectangle
        for (int y = 90; y < 110; ++y) std::fill_n(doc->activeLayer()->scanLine(y) + 140, 20, 0xFF0000FFu);
        doc->notifyPixelsChanged(QRect(140, 90, 20, 20));
        view.render(&frame);
        CHECK(frame.pixel(20 + 150, 30 + 100) == qRgb(0, 0, 255));
        CHECK(frame.pixel(20 + 100, 30 + 100) == qRgb(255, 255, 255));

        // Zoomed far out the view reads from a reduced mip level; it must be refreshed too
        view.setZoom(0.25);
        const QPoint docCenter = view.docToViewport(QPointF(150, 100)).toPoint();
        view.render(&frame);
        CHECK(frame.pixel(docCenter) == qRgb(0, 0, 255));
        doc->activeLayer()->fill(Qt::black);
        doc->notifyPixelsChanged(QRect(0, 0, 300, 200));
        view.render(&frame);
        CHECK(frame.pixel(docCenter) == qRgb(0, 0, 0));
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 12: Brush hardness fades the edge; blend mode changes how paint combines..." << std::endl;
        auto doc = std::make_shared<Document>(200, 200);
        doc->activeLayer()->fill(QColor(200, 100, 50));
        ToolManager tools;
        tools.setDocument(doc.get());
        tools.setActiveTool(ToolType::Paintbrush);
        ToolContext& ctx = tools.context();
        ITool* brush = tools.activeTool().get();
        ctx.brushWidth = 40;
        ctx.primaryColor = Qt::black;

        ctx.brushHardness = 100;
        mouse(brush, QEvent::MouseButtonPress, doc.get(), ctx, QPointF(50, 50));
        mouse(brush, QEvent::MouseButtonRelease, doc.get(), ctx, QPointF(50, 50));
        CHECK(doc->activeLayer()->image().pixel(50 + 15, 50) == qRgb(0, 0, 0)); // hard: solid near the edge

        ctx.brushHardness = 0;
        mouse(brush, QEvent::MouseButtonPress, doc.get(), ctx, QPointF(150, 50));
        mouse(brush, QEvent::MouseButtonRelease, doc.get(), ctx, QPointF(150, 50));
        const QImage& img = doc->activeLayer()->image();
        CHECK(qRed(img.pixel(150, 50)) < 5);                                       // centre fully painted
        const int mid = qRed(img.pixel(150 + 10, 50)), outer = qRed(img.pixel(150 + 17, 50));
        CHECK(mid > 20 && mid < 180 && outer > mid && outer <= 200);               // fades towards the edge
        CHECK(img.pixel(150 + 22, 50) == qRgb(200, 100, 50));                      // nothing outside the radius

        // Multiply with mid grey halves every channel; Normal would have painted grey
        ctx.brushHardness = 100;
        ctx.brushBlendMode = BlendMode::Multiply;
        ctx.primaryColor = QColor(128, 128, 128);
        mouse(brush, QEvent::MouseButtonPress, doc.get(), ctx, QPointF(50, 150));
        mouse(brush, QEvent::MouseButtonRelease, doc.get(), ctx, QPointF(50, 150));
        CHECK(doc->activeLayer()->image().pixel(50, 150) == qRgb(100, 50, 25));

        tools.setActiveTool(ToolType::PaintBucket);
        ctx.brushBlendMode = BlendMode::Difference;
        ctx.primaryColor = Qt::white;
        mouse(tools.activeTool().get(), QEvent::MouseButtonPress, doc.get(), ctx, QPointF(190, 190));
        CHECK(doc->activeLayer()->image().pixel(190, 190) == qRgb(55, 155, 205));
        std::cout << "  Passed" << std::endl;
    }

    {
        std::cout << "Test 13: Open Recent remembers files, most recent first, without duplicates..." << std::endl;
        QTemporaryDir dir;
        CHECK(dir.isValid());
        QCoreApplication::setOrganizationName("ThuliumTest");
        QCoreApplication::setApplicationName("ThuliumTest");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());

        const QString a = dir.filePath("a.png"), b = dir.filePath("b & c.png");
        QImage img(8, 8, QImage::Format_ARGB32);
        img.fill(Qt::red);
        CHECK(img.save(a) && img.save(b));

        auto recentMenu = [](MainWindow& win) {
            for (QAction* action : win.menuBar()->actions().first()->menu()->actions()) {
                if (action->menu() && action->text().contains("Recent")) return action->menu();
            }
            return static_cast<QMenu*>(nullptr);
        };
        {
            MainWindow win;
            CHECK(recentMenu(win) && !recentMenu(win)->isEnabled()); // empty list
            CHECK(win.openFile(a));
            CHECK(win.openFile(b));
            CHECK(win.openFile(a)); // already open: just activates it, order unchanged
            const QStringList stored = QSettings().value("recentFiles").toStringList();
            CHECK(stored == QStringList({b, a}));
        }
        {
            MainWindow win; // a new session reads the list back
            QMenu* menu = recentMenu(win);
            CHECK(menu && menu->isEnabled());
            CHECK(menu->actions().size() == 4); // two files, separator, "Clear List"
            CHECK(menu->actions().at(0)->text().endsWith("b && c.png"));
            menu->actions().at(1)->trigger();
            CHECK(win.activeDocument()->filePath() == a);
            CHECK(QSettings().value("recentFiles").toStringList() == QStringList({a, b}));
            menu->actions().last()->trigger();
            CHECK(QSettings().value("recentFiles").toStringList().isEmpty());
            CHECK(!recentMenu(win)->isEnabled());
        }
        QCoreApplication::setOrganizationName(QString());
        std::cout << "  Passed" << std::endl;
    }

    std::cout << "=== All Core Rework Tests Passed ===" << std::endl;
    return 0;
}
