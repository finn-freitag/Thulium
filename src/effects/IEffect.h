#pragma once

#include <QString>
#include <QImage>
#include <QWidget>
#include <functional>
#include "../core/Document.h"

namespace pdn {

class EffectDialog;

// Applies an effect with a fixed set of parameters to an image, restricted to a selection.
using EffectApplier = std::function<void(QImage&, const Selection&)>;

class IEffect {
public:
    virtual ~IEffect() = default;

    virtual QString name() const = 0;
    virtual QString category() const = 0; // "Adjustments", "Blurs", "Photo", etc.

    // Applies the effect without user interaction, using the settings last confirmed in its
    // dialog (or its defaults if the dialog was never accepted). Used by "Repeat Effect".
    virtual bool apply(QImage& image, const Selection& selection) = 0;
    virtual bool showDialog(QWidget* parent, Document* doc) = 0;

protected:
    // Runs the dialog; when accepted, remembers its settings for later apply() calls.
    bool runDialog(EffectDialog& dialog);
    // Applies the remembered settings, if there are any.
    bool applyRemembered(QImage& image, const Selection& selection) const {
        if (!m_remembered) return false;
        m_remembered(image, selection);
        return true;
    }

private:
    EffectApplier m_remembered;
};

} // namespace pdn
