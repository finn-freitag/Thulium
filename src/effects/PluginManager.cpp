#include "PluginManager.h"
#include "BrightnessContrast.h"
#include "GaussianBlur.h"
#include "Adjustments.h"
#include "BlurEffects.h"
#include "DistortEffects.h"
#include "NoiseEffects.h"
#include "PhotoEffects.h"
#include "StylizeEffects.h"
#include <QDir>
#include <QSet>

namespace pdn {

PluginManager::PluginManager(QObject* parent) : QObject(parent) {
    // Adjustments
    registerEffect(std::make_shared<AutoLevelEffect>());
    registerEffect(std::make_shared<BlackAndWhiteEffect>());
    registerEffect(std::make_shared<BrightnessContrastEffect>());
    registerEffect(std::make_shared<HueSaturationEffect>());
    registerEffect(std::make_shared<InvertColorsEffect>());
    registerEffect(std::make_shared<InvertAlphaEffect>());
    registerEffect(std::make_shared<PosterizeEffect>());
    registerEffect(std::make_shared<SepiaEffect>());
    registerEffect(std::make_shared<TemperatureTintEffect>());

    // Artistic
    registerEffect(std::make_shared<OilPaintingEffect>());

    // Blurs
    registerEffect(std::make_shared<GaussianBlurEffect>());
    registerEffect(std::make_shared<MotionBlurEffect>());
    registerEffect(std::make_shared<RadialBlurEffect>());

    // Distort
    registerEffect(std::make_shared<PixelateEffect>());
    registerEffect(std::make_shared<TwistEffect>());

    // Noise
    registerEffect(std::make_shared<AddNoiseEffect>());
    registerEffect(std::make_shared<MedianEffect>());

    // Photo
    registerEffect(std::make_shared<SharpenEffect>());
    registerEffect(std::make_shared<GlowEffect>());
    registerEffect(std::make_shared<VignetteEffect>());

    // Stylize
    registerEffect(std::make_shared<EdgeDetectEffect>());
    registerEffect(std::make_shared<EmbossEffect>());
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
