#include "PdnFormat.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QtEndian>
#include <QBuffer>
#include <zlib.h>
#include <iostream>
#include <vector>

namespace pdn {

static QByteArray decompressGzipChunk(const uint8_t* data, size_t compressedSize, size_t expectedUncompressedSize) {
    QByteArray result;
    result.resize(static_cast<qsizetype>(expectedUncompressedSize));

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = static_cast<uInt>(compressedSize);
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
    strm.avail_out = static_cast<uInt>(expectedUncompressedSize);
    strm.next_out = reinterpret_cast<Bytef*>(result.data());

    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
        return QByteArray();
    }

    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END && ret != Z_OK) {
        return QByteArray();
    }

    return result;
}

static QByteArray compressGzipChunk(const uint8_t* data, size_t uncompressedSize) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return QByteArray();
    }

    strm.avail_in = static_cast<uInt>(uncompressedSize);
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));

    uLong bound = deflateBound(&strm, static_cast<uLong>(uncompressedSize));
    QByteArray result;
    result.resize(static_cast<qsizetype>(bound));

    strm.avail_out = static_cast<uInt>(bound);
    strm.next_out = reinterpret_cast<Bytef*>(result.data());

    int ret = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        return QByteArray();
    }

    result.resize(static_cast<qsizetype>(strm.total_out));
    return result;
}

std::shared_ptr<Document> PdnFormat::load(const QString& filePath, QString* errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMsg) *errorMsg = QString("Could not open file: %1").arg(file.errorString());
        return nullptr;
    }

    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.size() < 7) {
        if (errorMsg) *errorMsg = "File is too small to be a valid PDN file.";
        return nullptr;
    }

    // 1. Magic check "PDN3"
    if (std::memcmp(fileData.constData(), "PDN3", 4) != 0) {
        if (errorMsg) *errorMsg = "Invalid file signature; expected 'PDN3'.";
        return nullptr;
    }

    // 2. 3-byte little endian XML length
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(fileData.constData());
    uint32_t xmlLen = ptr[4] | (ptr[5] << 8) | (ptr[6] << 16);

    if (7 + xmlLen > static_cast<size_t>(fileData.size())) {
        if (errorMsg) *errorMsg = "Truncated PDN header.";
        return nullptr;
    }

    // 3. Parse XML metadata
    QByteArray xmlBytes = fileData.mid(7, xmlLen);
    QXmlStreamReader xml(xmlBytes);
    int width = 0;
    int height = 0;
    int layerCount = 0;
    Metadata meta;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            if (name == "pdnImage") {
                width = xml.attributes().value("width").toInt();
                height = xml.attributes().value("height").toInt();
                layerCount = xml.attributes().value("layers").toInt();
            } else if (name == "metadata") {
                auto attrs = xml.attributes();
                if (attrs.hasAttribute("title")) meta.title = attrs.value("title").toString();
                if (attrs.hasAttribute("author")) meta.author = attrs.value("author").toString();
                if (attrs.hasAttribute("copyright")) meta.copyright = attrs.value("copyright").toString();
                if (attrs.hasAttribute("description")) meta.description = attrs.value("description").toString();
                if (attrs.hasAttribute("creationDate")) meta.creationDate = attrs.value("creationDate").toString();
                if (attrs.hasAttribute("creationTime")) meta.creationDate = attrs.value("creationTime").toString();
                if (attrs.hasAttribute("software")) meta.software = attrs.value("software").toString();
            }
        }
    }

    if (width <= 0 || height <= 0 || layerCount <= 0) {
        if (errorMsg) *errorMsg = QString("Invalid dimensions or layer count in PDN XML header: %1x%2, layers=%3")
                                      .arg(width).arg(height).arg(layerCount);
        return nullptr;
    }

    // 4. Find the start of pixel data (layer 0 header: 0x00 0x00 0x04 0x00 0x00 0x00 0x00 0x00 0x00)
    size_t nrbfStart = 7 + xmlLen;
    const char layerHdrPattern[] = "\x00\x00\x04\x00\x00\x00\x00\x00\x00";
    qsizetype layer0HdrPos = fileData.indexOf(QByteArray(layerHdrPattern, 9), static_cast<qsizetype>(nrbfStart));

    if (layer0HdrPos == -1) {
        if (errorMsg) *errorMsg = "Could not find layer data chunks in PDN file.";
        return nullptr;
    }

    // 5. Try extracting layer metadata from NRBF portion
    QByteArray nrbfData = fileData.mid(static_cast<qsizetype>(nrbfStart), layer0HdrPos - static_cast<qsizetype>(nrbfStart));
    struct LayerMeta {
        QString name;
        bool visible = true;
        bool isBackground = false;
        uint8_t opacity = 255;
        BlendMode blendMode = BlendMode::Normal;
    };
    std::vector<LayerMeta> metaList;

    // Search for BinaryObjectString instances (0x06 id len name) in NRBF
    // Pattern: 0x06, followed by 4-byte object ID, then 7-bit length, then string
    for (int i = 0; i < nrbfData.size() - 8; ++i) {
        if (static_cast<uint8_t>(nrbfData[i]) == 0x06) {
            int len = static_cast<uint8_t>(nrbfData[i + 5]);
            if (len > 0 && len < 64 && i + 6 + len <= nrbfData.size()) {
                QString potentialName = QString::fromUtf8(nrbfData.mid(i + 6, len));
                // Layer properties follow: 0x09 (MemberRef), 0x01 (visible), 0x01/0x00 (isBg), opacity, blendMode
                int afterName = i + 6 + len;
                if (afterName + 7 <= nrbfData.size() && static_cast<uint8_t>(nrbfData[afterName]) == 0x09) {
                    LayerMeta m;
                    m.name = potentialName;
                    m.visible = (nrbfData[afterName + 5] != 0);
                    m.isBackground = (nrbfData[afterName + 6] != 0);
                    m.opacity = static_cast<uint8_t>(nrbfData[afterName + 7]);
                    metaList.push_back(m);
                }
            }
        }
    }

    // Create Document
    auto doc = std::make_shared<Document>(width, height);
    // Remove the default background layer; we will add the PDN layers
    doc->clearLayers();

    size_t layerBytes = static_cast<size_t>(width) * height * 4;
    const size_t chunkSize = 262144; // 64K pixels * 4 bytes
    size_t chunksPerLayer = (layerBytes + chunkSize - 1) / chunkSize;

    size_t offset = static_cast<size_t>(layer0HdrPos);

    for (int l = 0; l < layerCount; ++l) {
        if (offset + 13 > static_cast<size_t>(fileData.size())) {
            if (errorMsg) *errorMsg = QString("Unexpected EOF reading layer %1 header").arg(l);
            return nullptr;
        }

        // Layer chunk 0 header: 9 bytes pattern + 4 bytes big endian chunk size
        uint32_t chunk0Size = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(fileData.constData() + offset + 9));
        offset += 13;

        QByteArray uncompressedLayerData;
        uncompressedLayerData.reserve(static_cast<qsizetype>(layerBytes));

        for (size_t c = 0; c < chunksPerLayer; ++c) {
            uint32_t cSize = chunk0Size;
            if (c > 0) {
                if (offset + 8 > static_cast<size_t>(fileData.size())) {
                    if (errorMsg) *errorMsg = QString("Unexpected EOF reading chunk %1 header in layer %2").arg(c).arg(l);
                    return nullptr;
                }
                uint32_t chunkIdx = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(fileData.constData() + offset));
                cSize = qFromBigEndian<uint32_t>(reinterpret_cast<const uchar*>(fileData.constData() + offset + 4));
                offset += 8;
                (void)chunkIdx;
            }

            if (offset + cSize > static_cast<size_t>(fileData.size())) {
                if (errorMsg) *errorMsg = QString("Unexpected EOF reading chunk %1 data in layer %2").arg(c).arg(l);
                return nullptr;
            }

            size_t expectedChunkUncompressed = std::min(chunkSize, layerBytes - c * chunkSize);
            QByteArray chunkData = decompressGzipChunk(reinterpret_cast<const uint8_t*>(fileData.constData() + offset),
                                                      cSize, expectedChunkUncompressed);
            if (chunkData.isEmpty()) {
                if (errorMsg) *errorMsg = QString("Failed to decompress gzip chunk %1 of layer %2").arg(c).arg(l);
                return nullptr;
            }

            uncompressedLayerData.append(chunkData);
            offset += cSize;
        }

        // Create QImage from BGRA uncompressed data
        // Paint.NET stores BGRA (B, G, R, A)
        QImage layerImg(width, height, QImage::Format_ARGB32);
        const uint8_t* srcBgra = reinterpret_cast<const uint8_t*>(uncompressedLayerData.constData());
        uint32_t* dstArgb = reinterpret_cast<uint32_t*>(layerImg.bits());
        int totalPixels = width * height;

        for (int p = 0; p < totalPixels; ++p) {
            uint8_t b = srcBgra[p * 4 + 0];
            uint8_t g = srcBgra[p * 4 + 1];
            uint8_t r = srcBgra[p * 4 + 2];
            uint8_t a = srcBgra[p * 4 + 3];
            dstArgb[p] = (static_cast<uint32_t>(a) << 24) |
                         (static_cast<uint32_t>(r) << 16) |
                         (static_cast<uint32_t>(g) << 8) |
                         static_cast<uint32_t>(b);
        }

        QString layerName = QString("Layer %1").arg(l + 1);
        bool isBg = (l == 0);
        bool visible = true;
        uint8_t opacity = 255;
        BlendMode bmode = BlendMode::Normal;

        if (l < static_cast<int>(metaList.size())) {
            layerName = metaList[l].name;
            isBg = metaList[l].isBackground;
            visible = metaList[l].visible;
            opacity = metaList[l].opacity;
            bmode = metaList[l].blendMode;
        }

        auto layerObj = std::make_shared<Layer>(layerImg, layerName, isBg);
        layerObj->setVisible(visible);
        layerObj->setOpacity(opacity);
        layerObj->setBlendMode(bmode);
        doc->insertLayer(doc->layerCount(), layerObj);
    }

    doc->setActiveLayerIndex(doc->layerCount() - 1);
    doc->setFilePath(filePath);
    doc->setMetadata(meta, false);
    return doc;
}

bool PdnFormat::save(const Document& doc, const QString& filePath, QString* errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMsg) *errorMsg = QString("Could not open file for writing: %1").arg(file.errorString());
        return false;
    }

    int width = doc.width();
    int height = doc.height();
    int layerCount = doc.layerCount();

    // 1. Generate XML header with thumbnail and metadata
    QImage thumb = doc.composite().scaled(160, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray thumbBytes;
    QBuffer thumbBuffer(&thumbBytes);
    thumbBuffer.open(QIODevice::WriteOnly);
    thumb.save(&thumbBuffer, "PNG");
    QString thumbBase64 = QString::fromLatin1(thumbBytes.toBase64());

    QString customContent = QString("<thumb png=\"%1\" />").arg(thumbBase64);
    const auto& meta = doc.metadata();
    if (!meta.isEmpty() || !meta.title.isEmpty() || !meta.author.isEmpty() ||
        !meta.copyright.isEmpty() || !meta.description.isEmpty() || !meta.creationDate.isEmpty()) {
        auto escapeXml = [](const QString& str) {
            QString s = str;
            s.replace("&", "&amp;");
            s.replace("\"", "&quot;");
            s.replace("<", "&lt;");
            s.replace(">", "&gt;");
            return s;
        };
        customContent += QString("<metadata title=\"%1\" author=\"%2\" copyright=\"%3\" description=\"%4\" creationDate=\"%5\" software=\"%6\" />")
                             .arg(escapeXml(meta.title))
                             .arg(escapeXml(meta.author))
                             .arg(escapeXml(meta.copyright))
                             .arg(escapeXml(meta.description))
                             .arg(escapeXml(meta.creationDate))
                             .arg(escapeXml(meta.software));
    }

    QString xmlStr = QString("<pdnImage width=\"%1\" height=\"%2\" layers=\"%3\" savedWithVersion=\"4.312.8267.29064\">"
                             "<custom>%4</custom></pdnImage>")
                         .arg(width).arg(height).arg(layerCount).arg(customContent);
    QByteArray xmlBytes = xmlStr.toUtf8();
    uint32_t xmlLen = static_cast<uint32_t>(xmlBytes.size());

    // 2. Write Magic "PDN3" and 3-byte XML length
    file.write("PDN3", 4);
    uint8_t lenBytes[3] = {
        static_cast<uint8_t>(xmlLen & 0xFF),
        static_cast<uint8_t>((xmlLen >> 8) & 0xFF),
        static_cast<uint8_t>((xmlLen >> 16) & 0xFF)
    };
    file.write(reinterpret_cast<const char*>(lenBytes), 3);
    file.write(xmlBytes);

    // 3. Write NRBF header template for PaintDotNet document and layers
    // We construct a standard NRBF header stream
    QByteArray nrbfStream;
    // RecordType: SerializedStreamHeader (0x00)
    nrbfStream.append('\x00');
    uint32_t rootId = 1, headerId = 1, major = 1, minor = 0;
    nrbfStream.append(reinterpret_cast<const char*>(&rootId), 4);
    nrbfStream.append(reinterpret_cast<const char*>(&headerId), 4);
    nrbfStream.append(reinterpret_cast<const char*>(&major), 4);
    nrbfStream.append(reinterpret_cast<const char*>(&minor), 4);

    // BinaryLibrary (0x0C) for PaintDotNet.Data
    nrbfStream.append('\x0c');
    uint32_t libId = 2;
    nrbfStream.append(reinterpret_cast<const char*>(&libId), 4);
    QByteArray libName = "PaintDotNet.Data, Version=4.312.8267.29064, Culture=neutral, PublicKeyToken=null";
    nrbfStream.append(static_cast<char>(libName.size()));
    nrbfStream.append(libName);

    // Layer metadata entries (BinaryObjectString records for each layer name & properties)
    for (int l = 0; l < layerCount; ++l) {
        auto layer = doc.layer(l);
        nrbfStream.append('\x06'); // BinaryObjectString
        uint32_t objId = 100 + l;
        nrbfStream.append(reinterpret_cast<const char*>(&objId), 4);
        QByteArray nameBytes = layer->name().toUtf8();
        nrbfStream.append(static_cast<char>(nameBytes.size()));
        nrbfStream.append(nameBytes);

        // Properties marker (0x09 MemberReference)
        nrbfStream.append('\x09');
        uint32_t refId = 200 + l;
        nrbfStream.append(reinterpret_cast<const char*>(&refId), 4);
        nrbfStream.append(layer->isVisible() ? '\x01' : '\x00');
        nrbfStream.append(layer->isBackground() ? '\x01' : '\x00');
        nrbfStream.append(static_cast<char>(layer->opacity()));
        uint32_t bmodeVal = static_cast<uint32_t>(layer->blendMode());
        nrbfStream.append(reinterpret_cast<const char*>(&bmodeVal), 4);
    }

    // End of NRBF stream: MessageEnd (0x0B)
    nrbfStream.append('\x0b');
    file.write(nrbfStream);

    // 4. Write Layer pixel chunks
    size_t layerBytes = static_cast<size_t>(width) * height * 4;
    const size_t chunkSize = 262144;
    size_t chunksPerLayer = (layerBytes + chunkSize - 1) / chunkSize;

    const char layerHdrPattern[] = "\x00\x00\x04\x00\x00\x00\x00\x00\x00";

    for (int l = 0; l < layerCount; ++l) {
        auto layer = doc.layer(l);
        const uint32_t* srcArgb = reinterpret_cast<const uint32_t*>(layer->bits());

        // Convert ARGB to BGRA byte array
        QByteArray bgraData;
        bgraData.resize(static_cast<qsizetype>(layerBytes));
        uint8_t* dstBgra = reinterpret_cast<uint8_t*>(bgraData.data());
        int totalPixels = width * height;

        for (int p = 0; p < totalPixels; ++p) {
            uint32_t px = srcArgb[p];
            dstBgra[p * 4 + 0] = px & 0xFF;         // B
            dstBgra[p * 4 + 1] = (px >> 8) & 0xFF;  // G
            dstBgra[p * 4 + 2] = (px >> 16) & 0xFF; // R
            dstBgra[p * 4 + 3] = (px >> 24) & 0xFF; // A
        }

        // Compress chunks
        std::vector<QByteArray> compressedChunks;
        compressedChunks.reserve(chunksPerLayer);

        for (size_t c = 0; c < chunksPerLayer; ++c) {
            size_t cOffset = c * chunkSize;
            size_t cLen = std::min(chunkSize, layerBytes - cOffset);
            QByteArray gz = compressGzipChunk(reinterpret_cast<const uint8_t*>(bgraData.constData() + cOffset), cLen);
            compressedChunks.push_back(gz);
        }

        // Write chunk 0 header (9 bytes pattern + 4 bytes big endian size)
        file.write(layerHdrPattern, 9);
        uint32_t c0Size = qToBigEndian<uint32_t>(static_cast<uint32_t>(compressedChunks[0].size()));
        file.write(reinterpret_cast<const char*>(&c0Size), 4);
        file.write(compressedChunks[0]);

        // Write subsequent chunks (4 bytes chunk index + 4 bytes size)
        for (size_t c = 1; c < chunksPerLayer; ++c) {
            uint32_t cIdx = qToBigEndian<uint32_t>(static_cast<uint32_t>(c));
            uint32_t cSz = qToBigEndian<uint32_t>(static_cast<uint32_t>(compressedChunks[c].size()));
            file.write(reinterpret_cast<const char*>(&cIdx), 4);
            file.write(reinterpret_cast<const char*>(&cSz), 4);
            file.write(compressedChunks[c]);
        }
    }

    file.close();
    return true;
}

} // namespace pdn
