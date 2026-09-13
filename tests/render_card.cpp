
#include <QCoreApplication>
#include "../src/io/PdnFormat.h"
#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QString err;
    auto doc = pdn::PdnFormat::load("Card.pdn", &err);
    if (!doc) return 1;
    QImage comp = doc->composite();
    comp.save("/tmp/card_rendered_by_cpp.png");
    std::cout << "SUCCESS: Saved /tmp/card_rendered_by_cpp.png (" << comp.width() << "x" << comp.height() << ")" << std::endl;
    return 0;
}
