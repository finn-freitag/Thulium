#pragma once

#include <QObject>
#include <QMap>
#include <memory>
#include "ITool.h"

namespace pdn {

class ToolManager : public QObject {
    Q_OBJECT
public:
    explicit ToolManager(QObject* parent = nullptr);
    ~ToolManager() override = default;

    void registerTool(std::shared_ptr<ITool> tool);
    std::shared_ptr<ITool> tool(ToolType type) const;
    std::shared_ptr<ITool> activeTool() const;
    ToolType activeToolType() const { return m_activeToolType; }

    void setActiveTool(ToolType type, Document* doc = nullptr);

    ToolContext& context() { return m_context; }
    const ToolContext& context() const { return m_context; }

signals:
    void activeToolChanged(ToolType type);
    void contextChanged();

private:
    QMap<ToolType, std::shared_ptr<ITool>> m_tools;
    ToolType m_activeToolType = ToolType::Paintbrush;
    ToolContext m_context;
};

} // namespace pdn
