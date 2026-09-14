#include "ToolManager.h"
#include "BrushTools.h"
#include "SelectionTools.h"
#include "MoveTools.h"
#include "FillTools.h"
#include "ShapeTools.h"
#include "TextTool.h"
#include "ViewTools.h"
#include "../core/Document.h"
#include "../core/History.h"
#include <QKeyEvent>

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
    m_context.onContextChanged = [this]() {
        emit contextChanged();
    };
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

void ToolManager::setDocument(Document* doc) {
    if (m_document == doc) return;
    auto tool = activeTool();
    if (tool && m_document) {
        tool->deactivate(m_document, m_context);
    }
    m_document = doc;
    if (tool && m_document) {
        tool->activate(m_document, m_context);
    }
}

void ToolManager::setActiveTool(ToolType type, Document* doc) {
    if (!doc) {
        doc = m_document;
    }
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

void ToolManager::cycleSelectionTool(bool reverse) {
    static const ToolType selTools[] = {
        ToolType::RectangleSelect,
        ToolType::LassoSelect,
        ToolType::EllipseSelect,
        ToolType::MagicWand
    };
    int count = 4;
    int curIdx = -1;
    for (int i = 0; i < count; ++i) {
        if (m_activeToolType == selTools[i]) {
            curIdx = i;
            break;
        }
    }

    if (curIdx == -1) {
        setActiveTool(ToolType::RectangleSelect);
    } else {
        int nextIdx = reverse ? (curIdx - 1 + count) % count : (curIdx + 1) % count;
        setActiveTool(selTools[nextIdx]);
    }
}

void ToolManager::cycleMoveTool() {
    if (m_activeToolType == ToolType::MoveSelectedPixels) {
        setActiveTool(ToolType::MoveSelection);
    } else {
        setActiveTool(ToolType::MoveSelectedPixels);
    }
}

bool ToolManager::handleKeyPress(QKeyEvent* event) {
    if (!event) return false;

    // Do not handle if Ctrl, Alt, or Meta is held (reserved for menu/app shortcuts)
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        return false;
    }

    // If TextTool is active and currently editing text, do not steal keys
    if (m_activeToolType == ToolType::Text) {
        auto textTool = std::dynamic_pointer_cast<TextTool>(activeTool());
        if (textTool && textTool->isEditing()) {
            return false;
        }
    }

    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    // If Control, Alt, or Meta is held, do not trigger single-key tool shortcuts
    if (mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        // Only allow arrow keys for nudging
        if (key != Qt::Key_Left && key != Qt::Key_Right && key != Qt::Key_Up && key != Qt::Key_Down) {
            return false;
        }
    }

    // Color swap: X
    if (key == Qt::Key_X) {
        std::swap(m_context.primaryColor, m_context.secondaryColor);
        m_context.notifyChanged();
        return true;
    }

    // Default colors (B/W): D (only without Shift/Ctrl/Alt)
    if (key == Qt::Key_D && event->modifiers() == Qt::NoModifier) {
        m_context.primaryColor = Qt::black;
        m_context.secondaryColor = Qt::white;
        m_context.notifyChanged();
        return true;
    }

    // Brush width decrease: [
    if (key == Qt::Key_BracketLeft) {
        m_context.brushWidth = std::max(1, m_context.brushWidth - 1);
        m_context.notifyChanged();
        return true;
    }

    // Brush width increase: ]
    if (key == Qt::Key_BracketRight) {
        m_context.brushWidth = std::min(500, m_context.brushWidth + 1);
        m_context.notifyChanged();
        return true;
    }

    // Selection tools cycle: S
    if (key == Qt::Key_S) {
        bool shift = (event->modifiers() & Qt::ShiftModifier);
        cycleSelectionTool(shift);
        return true;
    }

    // Move tools cycle: M
    if (key == Qt::Key_M) {
        cycleMoveTool();
        return true;
    }

    // Individual tools
    switch (key) {
        case Qt::Key_B: setActiveTool(ToolType::Paintbrush); return true;
        case Qt::Key_P: setActiveTool(ToolType::Pencil); return true;
        case Qt::Key_E: setActiveTool(ToolType::Eraser); return true;
        case Qt::Key_K: setActiveTool(ToolType::ColorPicker); return true;
        case Qt::Key_L: setActiveTool(ToolType::CloneStamp); return true;
        case Qt::Key_R: setActiveTool(ToolType::Recolor); return true;
        case Qt::Key_F: setActiveTool(ToolType::PaintBucket); return true;
        case Qt::Key_G: setActiveTool(ToolType::Gradient); return true;
        case Qt::Key_T: setActiveTool(ToolType::Text); return true;
        case Qt::Key_V: setActiveTool(ToolType::LineCurve); return true;
        case Qt::Key_O: setActiveTool(ToolType::Shapes); return true;
        case Qt::Key_Z: setActiveTool(ToolType::Zoom); return true;
        case Qt::Key_H: setActiveTool(ToolType::Pan); return true;
        default: break;
    }

    // Nudging with arrow keys if selection is active
    if (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down) {
        if (m_document && !m_document->selection().isEmpty()) {
            int step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
            qreal dx = 0, dy = 0;
            if (key == Qt::Key_Left) dx = -step;
            else if (key == Qt::Key_Right) dx = step;
            else if (key == Qt::Key_Up) dy = -step;
            else if (key == Qt::Key_Down) dy = step;

            if (m_activeToolType == ToolType::MoveSelectedPixels) {
                auto movePixTool = std::dynamic_pointer_cast<MoveSelectedPixelsTool>(activeTool());
                if (movePixTool) {
                    movePixTool->nudge(m_document, dx, dy);
                    return true;
                }
            } else if (m_activeToolType == ToolType::MoveSelection) {
                auto moveSelTool = std::dynamic_pointer_cast<MoveSelectionTool>(activeTool());
                if (moveSelTool) {
                    moveSelTool->nudge(m_document, dx, dy);
                    return true;
                }
            }
            QRegion oldRegion = m_document->selection().region();
            m_document->selection().translate(dx, dy);
            QRegion newRegion = m_document->selection().region();
            if (oldRegion != newRegion) {
                m_document->undoStack()->push(new SelectionUndoCommand(m_document, oldRegion, newRegion, "Move Selection"));
            }
            emit m_document->selectionChanged();
            emit m_document->documentChanged();
            return true;
        }
    }

    // Escape to deselect if selection is active
    if (key == Qt::Key_Escape) {
        if (m_document && (!m_document->selection().isEmpty() || m_document->hasFloatingSelection())) {
            if (m_document->hasFloatingSelection()) {
                m_document->clearSelection();
            } else {
                QRegion oldRegion = m_document->selection().region();
                m_document->clearSelection();
                m_document->undoStack()->push(new SelectionUndoCommand(m_document, oldRegion, QRegion(), "Deselect"));
            }
            return true;
        }
    }

    return false;
}

} // namespace pdn
