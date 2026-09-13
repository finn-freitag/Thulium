#pragma once

#include <QObject>
#include <QList>
#include <memory>
#include "IEffect.h"

namespace pdn {

class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override = default;

    void registerEffect(std::shared_ptr<IEffect> effect);
    const QList<std::shared_ptr<IEffect>>& effects() const { return m_effects; }

    QList<std::shared_ptr<IEffect>> effectsForCategory(const QString& category) const;
    QStringList categories() const;

    // Scan directory for future third-party dynamic plugins (.so on Linux, .dll on Windows)
    void scanPluginDirectory(const QString& dirPath);

private:
    QList<std::shared_ptr<IEffect>> m_effects;
};

} // namespace pdn
