#include "ImageIO.h"
#include "PdnFormat.h"
#include "StandardFormats.h"
#include <QFileInfo>

namespace pdn {

std::shared_ptr<Document> ImageIO::openDocument(const QString& filePath, QString* errorMsg) {
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "pdn") {
        return PdnFormat::load(filePath, errorMsg);
    } else {
        return StandardFormats::load(filePath, errorMsg);
    }
}

bool ImageIO::saveDocument(const Document& doc, const QString& filePath, QString* errorMsg) {
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "pdn") {
        return PdnFormat::save(doc, filePath, errorMsg);
    } else {
        return StandardFormats::save(doc, filePath, QString(), -1, errorMsg);
    }
}

QString ImageIO::openFileFilter() {
    return "Supported Images (*.pdn *.png *.jpg *.jpeg *.bmp *.gif *.webp);;"
           "Paint.NET (*.pdn);;"
           "PNG (*.png);;"
           "JPEG (*.jpg *.jpeg);;"
           "Bitmap (*.bmp);;"
           "GIF (*.gif);;"
           "WebP (*.webp);;"
           "All Files (*.*)";
}

QString ImageIO::saveFileFilter() {
    return "Paint.NET (*.pdn);;"
           "PNG (*.png);;"
           "JPEG (*.jpg *.jpeg);;"
           "Bitmap (*.bmp);;"
           "GIF (*.gif);;"
           "WebP (*.webp);;"
           "All Files (*.*)";
}

} // namespace pdn
