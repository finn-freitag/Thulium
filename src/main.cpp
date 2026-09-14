#include <QApplication>
#include <QIcon>
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    Q_INIT_RESOURCE(resources);

    QApplication app(argc, argv);
    app.setApplicationName("Thulium");
    app.setOrganizationName("Thulium");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/icons/logo.png"));

    pdn::MainWindow window;
    window.show();

    // If image / pdn files were provided as command line arguments, open them!
    for (int i = 1; i < argc; ++i) {
        QString filePath = QString::fromLocal8Bit(argv[i]);
        window.openFile(filePath);
    }

    return app.exec();
}
