#include "StandardFormats.h"
#include <QImageReader>
#include <QImageWriter>
#include <QFileInfo>
#include <gif_lib.h>
#include <vector>

namespace pdn {

std::shared_ptr<Document> StandardFormats::load(const QString& filePath, QString* errorMsg) {
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        if (errorMsg) *errorMsg = QString("Failed to load image: %1").arg(reader.errorString());
        return nullptr;
    }

    auto doc = std::make_shared<Document>(img);
    doc->setFilePath(filePath);
    return doc;
}

static bool saveGifFile(const QImage& srcImage, const QString& filePath, QString* errorMsg) {
    QImage indexed = srcImage;
    if (indexed.format() != QImage::Format_Indexed8) {
        indexed = indexed.convertToFormat(QImage::Format_Indexed8);
    }

    int width = indexed.width();
    int height = indexed.height();
    QList<QRgb> colorTable = indexed.colorTable();
    int numColors = static_cast<int>(colorTable.size());
    if (numColors == 0) numColors = 256;

    // Nearest power of 2
    int mapSize = 1;
    while (mapSize < numColors && mapSize < 256) {
        mapSize <<= 1;
    }

    ColorMapObject* cmap = GifMakeMapObject(mapSize, nullptr);
    if (!cmap) {
        if (errorMsg) *errorMsg = "Failed to allocate GIF color map.";
        return false;
    }

    for (int i = 0; i < mapSize; ++i) {
        if (i < colorTable.size()) {
            QRgb rgb = colorTable[i];
            cmap->Colors[i].Red = qRed(rgb);
            cmap->Colors[i].Green = qGreen(rgb);
            cmap->Colors[i].Blue = qBlue(rgb);
        } else {
            cmap->Colors[i].Red = 0;
            cmap->Colors[i].Green = 0;
            cmap->Colors[i].Blue = 0;
        }
    }

    int error = 0;
    GifFileType* gif = EGifOpenFileName(filePath.toLocal8Bit().constData(), false, &error);
    if (!gif) {
        GifFreeMapObject(cmap);
        if (errorMsg) *errorMsg = QString("Failed to create GIF file: error code %1").arg(error);
        return false;
    }

    if (EGifPutScreenDesc(gif, width, height, 8, 0, cmap) == GIF_ERROR ||
        EGifPutImageDesc(gif, 0, 0, width, height, false, nullptr) == GIF_ERROR) {
        GifFreeMapObject(cmap);
        EGifCloseFile(gif, &error);
        if (errorMsg) *errorMsg = "Failed writing GIF descriptors.";
        return false;
    }

    std::vector<GifByteType> rowBuffer(width);
    for (int y = 0; y < height; ++y) {
        const uint8_t* scan = indexed.constScanLine(y);
        for (int x = 0; x < width; ++x) {
            rowBuffer[x] = static_cast<GifByteType>(scan[x]);
        }
        if (EGifPutLine(gif, rowBuffer.data(), width) == GIF_ERROR) {
            GifFreeMapObject(cmap);
            EGifCloseFile(gif, &error);
            if (errorMsg) *errorMsg = "Failed writing GIF pixel line.";
            return false;
        }
    }

    GifFreeMapObject(cmap);
    EGifCloseFile(gif, &error);
    return true;
}

bool StandardFormats::save(const Document& doc, const QString& filePath, const QString& format, int quality, QString* errorMsg) {
    QImage composite = doc.composite();

    QString targetFormat = format;
    if (targetFormat.isEmpty()) {
        targetFormat = QFileInfo(filePath).suffix().toLower();
    }

    if (targetFormat.compare("gif", Qt::CaseInsensitive) == 0) {
        return saveGifFile(composite, filePath, errorMsg);
    }

    QImageWriter writer(filePath, targetFormat.toUtf8());
    if (quality >= 0) {
        writer.setQuality(quality);
    }

    if (!writer.write(composite)) {
        if (errorMsg) *errorMsg = QString("Failed to save image: %1").arg(writer.errorString());
        return false;
    }
    return true;
}

} // namespace pdn
