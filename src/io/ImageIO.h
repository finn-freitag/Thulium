#pragma once

#include <QString>
#include <memory>
#include "../core/Document.h"

namespace pdn {

class ImageIO {
public:
    static std::shared_ptr<Document> openDocument(const QString& filePath, QString* errorMsg = nullptr);
    static bool saveDocument(const Document& doc, const QString& filePath, QString* errorMsg = nullptr);
    static QImage loadImage(const QString& filePath, QString* errorMsg = nullptr);
    static bool isImageFile(const QString& filePath);

    static QString openFileFilter();
    static QString saveFileFilter();
};

} // namespace pdn
