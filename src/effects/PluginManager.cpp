#include "PluginManager.h"
#include "BrightnessContrast.h"
#include "GaussianBlur.h"
#include <QDir>
#include <QSet>

namespace pdn {

PluginManager::PluginManager(QObject* parent) : QObject(parent) {
    // Register built-in Adjustments & Effects
    registerEffect(std::make_shared<BrightnessContrastEffect>());
    registerEffect(std::make_shared<GaussianBlurEffect>());
}

void PluginManager::registerEffect(std::shared_ptr<IEffect> effect) {
    if (effect) {
        m_effects.append(effect);
    }
}

QList<std::shared_ptr<IEffect>> PluginManager::effectsForCategory(const QString& category) const {
    QList<std::shared_ptr<IEffect>> list;
    for (const auto& fx : m_effects) {
        if (fx->category().compare(category, Qt::CaseInsensitive) == 0) {
            list.append(fx);
        }
    }
    return list;
}

QStringList PluginManager::categories() const {
    QSet<QString> cats;
    for (const auto& fx : m_effects) {
        cats.insert(fx->category());
    }
    return cats.values();
}

void PluginManager::scanPluginDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) return;
    // Architecture ready for QPluginLoader / dynamic loading
}

} // namespace pdn
