#include "ToolManager.h"
#include "BrushTools.h"
#include "SelectionTools.h"
#include "MoveTools.h"
#include "FillTools.h"
#include "ShapeTools.h"
#include "TextTool.h"
#include "ViewTools.h"

namespace pdn {

ToolManager::ToolManager(QObject* parent) : QObject(parent) {
    // Register all standard Paint.NET tools
    registerTool(std::make_shared<RectangleSelectTool>());
    registerTool(std::make_shared<MoveSelectedPixelsTool>());
    registerTool(std::make_shared<LassoSelectTool>());
    registerTool(std::make_shared<MoveSelectionTool>());
    registerTool(std::make_shared<EllipseSelectTool>());
    registerTool(std::make_shared<ZoomTool>());
    registerTool(std::make_shared<MagicWandTool>());
    registerTool(std::make_shared<PanTool>());
    registerTool(std::make_shared<PaintBucketTool>());
    registerTool(std::make_shared<PaintbrushTool>());
    registerTool(std::make_shared<EraserTool>());
    registerTool(std::make_shared<PencilTool>());
    registerTool(std::make_shared<ColorPickerTool>());
    registerTool(std::make_shared<CloneStampTool>());
    registerTool(std::make_shared<RecolorTool>());
    registerTool(std::make_shared<GradientTool>());
    registerTool(std::make_shared<TextTool>());
    registerTool(std::make_shared<LineCurveTool>());
    registerTool(std::make_shared<ShapesTool>());

    m_activeToolType = ToolType::Paintbrush;
}

void ToolManager::registerTool(std::shared_ptr<ITool> tool) {
    if (tool) {
        m_tools[tool->type()] = tool;
    }
}

std::shared_ptr<ITool> ToolManager::tool(ToolType type) const {
    return m_tools.value(type, nullptr);
}

std::shared_ptr<ITool> ToolManager::activeTool() const {
    return tool(m_activeToolType);
}

void ToolManager::setActiveTool(ToolType type, Document* doc) {
    if (m_activeToolType != type && m_tools.contains(type)) {
        auto prevTool = activeTool();
        if (prevTool) {
            prevTool->deactivate(doc, m_context);
        }
        m_activeToolType = type;
        auto nextTool = activeTool();
        if (nextTool) {
            nextTool->activate(doc, m_context);
        }
        emit activeToolChanged(m_activeToolType);
    }
}

} // namespace pdn
