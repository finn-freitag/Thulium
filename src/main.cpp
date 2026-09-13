#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Paint.NET Clone");
    app.setOrganizationName("PaintNetClone");
    app.setApplicationVersion("1.0.0");

    pdn::MainWindow window;
    window.show();

    // If an image / pdn file was provided as a command line argument, open it!
    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        window.openFile(filePath);
    }

    return app.exec();
}
