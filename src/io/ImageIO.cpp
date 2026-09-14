#include "ImageIO.h"
#include "PdnFormat.h"
#include "StandardFormats.h"
#include <QFileInfo>
#include <QImageReader>

namespace pdn {

std::shared_ptr<Document> ImageIO::openDocument(const QString& filePath, QString* errorMsg) {
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "pdn") {
        return PdnFormat::load(filePath, errorMsg);
    } else {
        return StandardFormats::load(filePath, errorMsg);
    }
}

QImage ImageIO::loadImage(const QString& filePath, QString* errorMsg) {
    auto doc = openDocument(filePath, errorMsg);
    if (doc) {
        QImage comp = doc->composite();
        if (comp.format() != QImage::Format_ARGB32) {
            comp = comp.convertToFormat(QImage::Format_ARGB32);
        }
        return comp;
    }

    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (!img.isNull()) {
        if (img.format() != QImage::Format_ARGB32) {
            img = img.convertToFormat(QImage::Format_ARGB32);
        }
        return img;
    }

    if (errorMsg && errorMsg->isEmpty()) {
        *errorMsg = reader.errorString();
    }
    return QImage();
}

bool ImageIO::isImageFile(const QString& filePath) {
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) return false;
    QString ext = fi.suffix().toLower();
    if (ext == "pdn") return true;
    const auto supported = QImageReader::supportedImageFormats();
    for (const auto& fmt : supported) {
        if (ext == QString::fromUtf8(fmt).toLower()) return true;
    }
    return false;
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
           "Thulium / Paint.NET (*.pdn);;"
           "PNG (*.png);;"
           "JPEG (*.jpg *.jpeg);;"
           "Bitmap (*.bmp);;"
           "GIF (*.gif);;"
           "WebP (*.webp);;"
           "All Files (*.*)";
}

QString ImageIO::saveFileFilter() {
    return "Thulium / Paint.NET (*.pdn);;"
           "PNG (*.png);;"
           "JPEG (*.jpg *.jpeg);;"
           "Bitmap (*.bmp);;"
           "GIF (*.gif);;"
           "WebP (*.webp);;"
           "All Files (*.*)";
}

} // namespace pdn
