#pragma once

#include <QObject>
#include <QMap>
#include <memory>
#include "ITool.h"

namespace pdn {

class Document;

class ToolManager : public QObject {
    Q_OBJECT
public:
    explicit ToolManager(QObject* parent = nullptr);
    ~ToolManager() override = default;

    void registerTool(std::shared_ptr<ITool> tool);
    std::shared_ptr<ITool> tool(ToolType type) const;
    std::shared_ptr<ITool> activeTool() const;
    ToolType activeToolType() const { return m_activeToolType; }

    void setDocument(Document* doc) { m_document = doc; }
    Document* currentDocument() const { return m_document; }

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
    Document* m_document = nullptr;
};

} // namespace pdn
