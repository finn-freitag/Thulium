// Renders the main window to PNG files after a few scripted interactions, for eyeballing the UI
// without a display. Not part of ctest; run manually:
//   QT_QPA_PLATFORM=offscreen ./ui_snapshot <image-or-pdn> <output-directory>
#include <QApplication>
#include <QDir>
#include <QMouseEvent>
#include <QTest>
#include <cstdio>
#include "ui/MainWindow.h"

using namespace pdn;

static void drag(CanvasView* view, const QPointF& fromDoc, const QPointF& toDoc, Qt::MouseButton button = Qt::LeftButton) {
    const QPoint from = view->docToViewport(fromDoc).toPoint();
    const QPoint to = view->docToViewport(toDoc).toPoint();
    QTest::mousePress(view, button, Qt::NoModifier, from);
    for (int i = 1; i <= 8; ++i) {
        const QPoint p = from + (to - from) * i / 8;
        QMouseEvent move(QEvent::MouseMove, p, view->mapToGlobal(p), Qt::NoButton, button, Qt::NoModifier);
        QApplication::sendEvent(view, &move);
    }
    QTest::mouseRelease(view, button, Qt::NoModifier, to);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <image> <output-directory>\n", argv[0]);
        return 2;
    }
    const QDir out(argv[2]);
    QDir().mkpath(out.absolutePath());

    MainWindow window;
    window.resize(1400, 900);
    window.show();
    if (!window.openFile(argv[1])) return 1;
    QApplication::processEvents();

    auto doc = window.activeDocument();
    CanvasView* view = window.canvasView();
    ToolManager* tools = window.toolManager();
    const double w = doc->width(), h = doc->height();
    int shot = 0;
    auto snap = [&](const char* name) {
        QApplication::processEvents();
        const QString path = out.filePath(QString("%1-%2.png").arg(++shot).arg(name));
        window.grab().save(path);
        std::printf("%s\n", qPrintable(path));
    };

    snap("opened");

    tools->setActiveTool(ToolType::EllipseSelect);
    drag(view, QPointF(w * 0.15, h * 0.2), QPointF(w * 0.55, h * 0.7));
    snap("ellipse-selection");

    tools->setActiveTool(ToolType::RectangleSelect);
    drag(view, QPointF(w * 0.45, h * 0.35), QPointF(w * 0.8, h * 0.6), Qt::RightButton); // subtract
    snap("selection-subtracted");

    tools->setActiveTool(ToolType::Paintbrush);
    tools->context().brushWidth = std::max(4, static_cast<int>(w / 60));
    tools->context().primaryColor = QColor(220, 30, 60, 200);
    drag(view, QPointF(w * 0.1, h * 0.5), QPointF(w * 0.9, h * 0.45)); // clipped to the selection
    snap("brush-clipped-to-selection");

    window.onDeselect();
    tools->setActiveTool(ToolType::Gradient);
    tools->context().gradientMode = GradientMode::Spiral;
    tools->context().primaryColor = QColor(255, 200, 0, 160);
    tools->context().secondaryColor = QColor(0, 80, 255, 0);
    drag(view, QPointF(w * 0.7, h * 0.3), QPointF(w * 0.85, h * 0.45));
    snap("spiral-gradient");

    view->setZoom(8.0, QPointF(w * 0.5, h * 0.5));
    snap("zoomed-in");
    return 0;
}
