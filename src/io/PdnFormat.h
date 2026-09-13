#pragma once

#include <QString>
#include <memory>
#include "../core/Document.h"

namespace pdn {

class PdnFormat {
public:
    static std::shared_ptr<Document> load(const QString& filePath, QString* errorMsg = nullptr);
    static bool save(const Document& doc, const QString& filePath, QString* errorMsg = nullptr);
};

} // namespace pdn
