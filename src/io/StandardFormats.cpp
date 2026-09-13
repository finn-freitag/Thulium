#include "StandardFormats.h"
#include <QImageReader>
#include <QImageWriter>
#include <QFileInfo>
#include <gif_lib.h>
#include <vector>

namespace pdn {

// Forward declarations of EXIF helpers
static void readExifFromJpegFile(const QString& filePath, Metadata& meta);
static void insertExifIntoJpegFile(const QString& filePath, const Metadata& meta);

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

    // Read metadata if available
    Metadata meta;
    auto readKey = [&](const QString& key) -> QString {
        return img.text(key);
    };

    meta.title = readKey("Title");
    meta.author = readKey("Author");
    if (meta.author.isEmpty()) meta.author = readKey("Artist");
    meta.description = readKey("Description");
    if (meta.description.isEmpty()) meta.description = readKey("Comment");
    meta.copyright = readKey("Copyright");
    meta.creationDate = readKey("Creation Time");
    if (meta.creationDate.isEmpty()) meta.creationDate = readKey("Date");
    meta.software = readKey("Software");

    // Read EXIF if it's a JPEG
    readExifFromJpegFile(filePath, meta);

    if (!meta.isEmpty() || !meta.title.isEmpty() || !meta.author.isEmpty() || !meta.description.isEmpty()) {
        doc->setMetadata(meta, false);
    }

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

struct ExifTagEntry {
    uint16_t tag;
    uint16_t type; // 1=BYTE, 2=ASCII
    uint32_t count;
    QByteArray data;
};

static QByteArray createExifApp1(const Metadata& meta) {
    std::vector<ExifTagEntry> entries;

    auto addAscii = [&](uint16_t tag, const QString& val) {
        if (!val.isEmpty()) {
            QByteArray bytes = val.toUtf8();
            bytes.append('\0');
            entries.push_back({tag, 2, static_cast<uint32_t>(bytes.size()), bytes});
        }
    };

    auto addUnicodeLE = [&](uint16_t tag, const QString& val) {
        if (!val.isEmpty()) {
            QByteArray bytes;
            for (QChar c : val) {
                uint16_t u = c.unicode();
                bytes.append(static_cast<char>(u & 0xFF));
                bytes.append(static_cast<char>((u >> 8) & 0xFF));
            }
            bytes.append('\0');
            bytes.append('\0');
            entries.push_back({tag, 1, static_cast<uint32_t>(bytes.size()), bytes});
        }
    };

    addAscii(0x010E, meta.description); // ImageDescription
    addAscii(0x0131, meta.software);    // Software
    if (!meta.creationDate.isEmpty()) {
        QDateTime dt = QDateTime::fromString(meta.creationDate, Qt::ISODate);
        if (dt.isValid()) {
            addAscii(0x0132, dt.toString("yyyy:MM:dd hh:mm:ss"));
        } else {
            addAscii(0x0132, meta.creationDate);
        }
    }
    addAscii(0x013B, meta.author);    // Artist
    addAscii(0x8298, meta.copyright); // Copyright

    addUnicodeLE(0x9C9B, meta.title);       // XPTitle
    addUnicodeLE(0x9C9C, meta.description); // XPComment
    addUnicodeLE(0x9C9D, meta.author);      // XPAuthor

    if (entries.empty()) return QByteArray();

    std::sort(entries.begin(), entries.end(), [](const ExifTagEntry& a, const ExifTagEntry& b) {
        return a.tag < b.tag;
    });

    uint32_t ifd0Offset = 8;
    uint16_t numEntries = static_cast<uint16_t>(entries.size());
    uint32_t dataOffset = ifd0Offset + 2 + numEntries * 12 + 4;

    QByteArray tiff;
    tiff.append("II\x2A\x00\x08\x00\x00\x00", 8);
    tiff.append(reinterpret_cast<const char*>(&numEntries), 2);

    QByteArray extraData;
    for (const auto& entry : entries) {
        tiff.append(reinterpret_cast<const char*>(&entry.tag), 2);
        tiff.append(reinterpret_cast<const char*>(&entry.type), 2);
        tiff.append(reinterpret_cast<const char*>(&entry.count), 4);

        if (entry.data.size() <= 4) {
            char valBuf[4] = {0, 0, 0, 0};
            std::memcpy(valBuf, entry.data.constData(), entry.data.size());
            tiff.append(valBuf, 4);
        } else {
            uint32_t curOffset = dataOffset + static_cast<uint32_t>(extraData.size());
            tiff.append(reinterpret_cast<const char*>(&curOffset), 4);
            extraData.append(entry.data);
            if (extraData.size() % 2 != 0) {
                extraData.append('\0');
            }
        }
    }

    uint32_t nextIfd = 0;
    tiff.append(reinterpret_cast<const char*>(&nextIfd), 4);
    tiff.append(extraData);

    uint32_t app1PayloadLen = 6 + static_cast<uint32_t>(tiff.size());
    if (app1PayloadLen + 2 > 65535) return QByteArray();

    uint16_t totalMarkerLen = static_cast<uint16_t>(app1PayloadLen + 2);
    QByteArray app1;
    app1.append(static_cast<char>(0xFF));
    app1.append(static_cast<char>(0xE1));
    app1.append(static_cast<char>((totalMarkerLen >> 8) & 0xFF));
    app1.append(static_cast<char>(totalMarkerLen & 0xFF));
    app1.append("Exif\0\0", 6);
    app1.append(tiff);

    return app1;
}

static void insertExifIntoJpegFile(const QString& filePath, const Metadata& meta) {
    if (meta.isEmpty() && meta.title.isEmpty() && meta.author.isEmpty() && meta.description.isEmpty()) return;
    QByteArray app1 = createExifApp1(meta);
    if (app1.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QByteArray data = file.readAll();
    file.close();

    if (data.size() < 4 || static_cast<uint8_t>(data[0]) != 0xFF || static_cast<uint8_t>(data[1]) != 0xD8) {
        return;
    }

    int insertPos = 2;
    if (data.size() >= 4 && static_cast<uint8_t>(data[2]) == 0xFF && static_cast<uint8_t>(data[3]) == 0xE0) {
        if (data.size() >= 6) {
            int app0Len = (static_cast<uint8_t>(data[4]) << 8) | static_cast<uint8_t>(data[5]);
            insertPos = 4 + app0Len;
            if (insertPos > data.size()) insertPos = 2;
        }
    }

    if (insertPos + 4 <= data.size() && static_cast<uint8_t>(data[insertPos]) == 0xFF && static_cast<uint8_t>(data[insertPos + 1]) == 0xE1) {
        int oldLen = (static_cast<uint8_t>(data[insertPos + 2]) << 8) | static_cast<uint8_t>(data[insertPos + 3]);
        data.remove(insertPos, oldLen + 2);
    }

    data.insert(insertPos, app1);

    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
    }
}

static void readExifFromJpegFile(const QString& filePath, Metadata& meta) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QByteArray data = file.read(131072);
    file.close();

    if (data.size() < 4 || static_cast<uint8_t>(data[0]) != 0xFF || static_cast<uint8_t>(data[1]) != 0xD8) {
        return;
    }

    int pos = 2;
    while (pos + 4 <= data.size()) {
        if (static_cast<uint8_t>(data[pos]) != 0xFF) break;
        uint8_t marker = static_cast<uint8_t>(data[pos + 1]);
        if (marker == 0xDA || marker == 0xD9) break;
        int segLen = (static_cast<uint8_t>(data[pos + 2]) << 8) | static_cast<uint8_t>(data[pos + 3]);
        if (segLen < 2) break;

        if (marker == 0xE1 && pos + 10 <= data.size() && std::memcmp(data.constData() + pos + 4, "Exif\0\0", 6) == 0) {
            const uint8_t* tiff = reinterpret_cast<const uint8_t*>(data.constData() + pos + 10);
            int tiffLen = segLen - 8;
            if (tiffLen >= 14 && pos + 10 + tiffLen <= data.size()) {
                bool littleEndian = (tiff[0] == 'I' && tiff[1] == 'I');
                auto read16 = [&](int offset) -> uint16_t {
                    if (offset + 2 > tiffLen) return 0;
                    return littleEndian ? (tiff[offset] | (tiff[offset + 1] << 8))
                                        : ((tiff[offset] << 8) | tiff[offset + 1]);
                };
                auto read32 = [&](int offset) -> uint32_t {
                    if (offset + 4 > tiffLen) return 0;
                    return littleEndian ? (tiff[offset] | (tiff[offset + 1] << 8) | (tiff[offset + 2] << 16) | (tiff[offset + 3] << 24))
                                        : ((tiff[offset] << 24) | (tiff[offset + 1] << 16) | (tiff[offset + 2] << 8) | tiff[offset + 3]);
                };

                uint32_t ifdOffset = read32(4);
                if (ifdOffset + 2 <= static_cast<uint32_t>(tiffLen)) {
                    uint16_t numEntries = read16(ifdOffset);
                    int entryPos = ifdOffset + 2;
                    for (int e = 0; e < numEntries && entryPos + 12 <= tiffLen; ++e, entryPos += 12) {
                        uint16_t tag = read16(entryPos);
                        uint16_t type = read16(entryPos + 2);
                        uint32_t count = read32(entryPos + 4);
                        uint32_t valOffset = (count <= 4 && type != 3) ? (entryPos + 8) : read32(entryPos + 8);

                        auto readString = [&]() -> QString {
                            if (valOffset + count <= static_cast<uint32_t>(tiffLen)) {
                                uint32_t strLen = (count > 0 && tiff[valOffset + count - 1] == 0) ? count - 1 : count;
                                return QString::fromUtf8(reinterpret_cast<const char*>(tiff + valOffset), strLen);
                            }
                            return QString();
                        };

                        auto readUnicodeLE = [&]() -> QString {
                            if (valOffset + count <= static_cast<uint32_t>(tiffLen)) {
                                QString res;
                                for (uint32_t i = 0; i + 1 < count; i += 2) {
                                    uint16_t u = tiff[valOffset + i] | (tiff[valOffset + i + 1] << 8);
                                    if (u == 0) break;
                                    res.append(QChar(u));
                                }
                                return res;
                            }
                            return QString();
                        };

                        if (tag == 0x010E && meta.description.isEmpty()) meta.description = readString();
                        else if (tag == 0x0131 && meta.software.isEmpty()) meta.software = readString();
                        else if (tag == 0x0132 && meta.creationDate.isEmpty()) meta.creationDate = readString();
                        else if (tag == 0x013B && meta.author.isEmpty()) meta.author = readString();
                        else if (tag == 0x8298 && meta.copyright.isEmpty()) meta.copyright = readString();
                        else if (tag == 0x9C9B && meta.title.isEmpty()) meta.title = readUnicodeLE();
                        else if (tag == 0x9C9C && meta.description.isEmpty()) meta.description = readUnicodeLE();
                        else if (tag == 0x9C9D && meta.author.isEmpty()) meta.author = readUnicodeLE();
                    }
                }
            }
            break;
        }
        pos += 2 + segLen;
    }
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

    const auto& meta = doc.metadata();
    if (!meta.isEmpty() || !meta.title.isEmpty() || !meta.author.isEmpty() || !meta.description.isEmpty()) {
        auto setMeta = [&](const QString& key, const QString& val) {
            if (!val.isEmpty()) {
                composite.setText(key, val);
            }
        };
        setMeta("Title", meta.title);
        setMeta("Author", meta.author);
        setMeta("Description", meta.description);
        setMeta("Copyright", meta.copyright);
        setMeta("Creation Time", meta.creationDate);
        setMeta("Software", meta.software);
    }

    if (!writer.write(composite)) {
        if (errorMsg) *errorMsg = QString("Failed to save image: %1").arg(writer.errorString());
        return false;
    }

    // For JPEG, embed EXIF APP1
    if (targetFormat.compare("jpg", Qt::CaseInsensitive) == 0 || targetFormat.compare("jpeg", Qt::CaseInsensitive) == 0) {
        insertExifIntoJpegFile(filePath, meta);
    }

    return true;
}

} // namespace pdn
