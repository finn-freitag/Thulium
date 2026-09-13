#pragma once

#include <QString>
#include <QImage>
#include <QWidget>
#include "../core/Document.h"

namespace pdn {

class IEffect {
public:
    virtual ~IEffect() = default;

    virtual QString name() const = 0;
    virtual QString category() const = 0; // "Adjustments", "Blurs", "Photo", etc.

    virtual bool apply(QImage& image, const Selection& selection) = 0;
    virtual bool showDialog(QWidget* parent, Document* doc) = 0;
};

} // namespace pdn
