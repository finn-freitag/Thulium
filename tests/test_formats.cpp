#include <QCoreApplication>
#include <iostream>
#include <cassert>
#include "../src/io/StandardFormats.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "=== Image Format Import / Export Test ===" << std::endl;

    QImage testImg(100, 100, QImage::Format_ARGB32);
    testImg.fill(QColor(255, 120, 50, 255));
    pdn::Document doc(testImg);

    const char* formats[] = {"png", "bmp", "jpg", "webp", "gif"};
    for (const char* fmt : formats) {
        QString outPath = QString("/tmp/test_out.%1").arg(fmt);
        QString err;
        bool ok = pdn::StandardFormats::save(doc, outPath, fmt, 90, &err);
        std::cout << "Saving " << fmt << " to " << outPath.toStdString() << ": " << (ok ? "SUCCESS" : "FAIL") << std::endl;
        assert(ok);

        auto loaded = pdn::StandardFormats::load(outPath, &err);
        std::cout << "Loading " << fmt << " from " << outPath.toStdString() << ": " << (loaded ? "SUCCESS" : "FAIL") << std::endl;
        assert(loaded != nullptr);
        assert(loaded->width() == 100 && loaded->height() == 100);
    }

    std::cout << "All 5 standard image formats verified successfully!" << std::endl;
    return 0;
}
