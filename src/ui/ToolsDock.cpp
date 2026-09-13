#include "ToolsDock.h"
#include <QGridLayout>
#include <QWidget>

namespace pdn {

struct ToolLayoutItem {
    ToolType type;
    int row;
    int col;
    QString iconText;
};

static const ToolLayoutItem s_toolGrid[] = {
    { ToolType::RectangleSelect,    0, 0, "[ ]" },
    { ToolType::MoveSelectedPixels, 0, 1, "<+>" },
    { ToolType::LassoSelect,        1, 0, " Lasso " },
    { ToolType::MoveSelection,      1, 1, " MoveS " },
    { ToolType::EllipseSelect,      2, 0, "( O )" },
    { ToolType::Zoom,               2, 1, " Zoom " },
    { ToolType::MagicWand,          3, 0, " Wand " },
    { ToolType::Pan,                3, 1, " Pan " },
    { ToolType::PaintBucket,        4, 0, " Bucket" },
    { ToolType::Paintbrush,         4, 1, " Brush " },
    { ToolType::Eraser,             5, 0, " Eraser" },
    { ToolType::Pencil,             5, 1, " Pencil" },
    { ToolType::ColorPicker,        6, 0, " Picker" },
    { ToolType::CloneStamp,         6, 1, " Stamp " },
    { ToolType::Recolor,            7, 0, " Recolor" },
    { ToolType::Gradient,           7, 1, " Grad  " },
    { ToolType::Text,               8, 0, " Text  " },
    { ToolType::LineCurve,          8, 1, " Line  " },
    { ToolType::Shapes,             9, 0, " Shapes" }
};

ToolsDock::ToolsDock(ToolManager* toolMgr, QWidget* parent)
    : QDockWidget("Tools", parent), m_toolMgr(toolMgr) {
    setObjectName("ToolsDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupUI();

    connect(m_toolMgr, &ToolManager::activeToolChanged, this, &ToolsDock::onActiveToolChanged);
    onActiveToolChanged(m_toolMgr->activeToolType());
}

void ToolsDock::setupUI() {
    QWidget* container = new QWidget(this);
    QGridLayout* layout = new QGridLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    for (const auto& item : s_toolGrid) {
        auto toolObj = m_toolMgr->tool(item.type);
        if (!toolObj) continue;

        QToolButton* btn = new QToolButton(container);
        btn->setText(item.iconText.trimmed());
        btn->setToolTip(QString("%1 (%2)").arg(toolObj->name()).arg(toolObj->shortcut()));
        btn->setCheckable(true);
        btn->setFixedSize(48, 28);

        layout->addWidget(btn, item.row, item.col);
        m_buttonGroup->addButton(btn, static_cast<int>(item.type));
        m_buttons[item.type] = btn;
    }

    connect(m_buttonGroup, &QButtonGroup::idClicked, this, &ToolsDock::onToolButtonClicked);

    layout->setRowStretch(10, 1);
    container->setLayout(layout);
    setWidget(container);
    setFixedWidth(112);
}

void ToolsDock::onToolButtonClicked(int id) {
    m_toolMgr->setActiveTool(static_cast<ToolType>(id));
}

void ToolsDock::onActiveToolChanged(ToolType type) {
    if (m_buttons.contains(type)) {
        m_buttons[type]->setChecked(true);
    }
}

} // namespace pdn
