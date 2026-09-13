#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Paint.NET Clone");
    app.setOrganizationName("PaintNetClone");
    app.setApplicationVersion("1.0.0");

    pdn::MainWindow window;
    window.show();

    // If image / pdn files were provided as command line arguments, open them!
    for (int i = 1; i < argc; ++i) {
        QString filePath = QString::fromLocal8Bit(argv[i]);
        window.openFile(filePath);
    }

    return app.exec();
}
