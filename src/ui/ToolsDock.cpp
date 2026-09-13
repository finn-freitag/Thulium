#include "ToolsDock.h"
#include <QGridLayout>
#include <QWidget>

static void initResources() {
    static bool inited = false;
    if (!inited) {
        Q_INIT_RESOURCE(resources);
        inited = true;
    }
}

namespace pdn {

struct ToolLayoutItem {
    ToolType type;
    int row;
    int col;
    QString iconPath;
    QString iconText;
};

static const ToolLayoutItem s_toolGrid[] = {
    { ToolType::RectangleSelect,    0, 0, ":/icons/rectangle-select.svg", "[ ]" },
    { ToolType::MoveSelectedPixels, 0, 1, ":/icons/move-pixels.svg",       "<+>" },
    { ToolType::LassoSelect,        1, 0, ":/icons/lasso-select.svg",       "Lasso" },
    { ToolType::MoveSelection,      1, 1, ":/icons/move-selection.svg",    "MoveS" },
    { ToolType::EllipseSelect,      2, 0, ":/icons/ellipse-select.svg",    "( O )" },
    { ToolType::Zoom,               2, 1, ":/icons/zoom.svg",              "Zoom" },
    { ToolType::MagicWand,          3, 0, ":/icons/magic-wand.svg",        "Wand" },
    { ToolType::Pan,                3, 1, ":/icons/pan.svg",               "Pan" },
    { ToolType::PaintBucket,        4, 0, ":/icons/paint-bucket.svg",      "Bucket" },
    { ToolType::Paintbrush,         4, 1, ":/icons/paint-brush.svg",       "Brush" },
    { ToolType::Eraser,             5, 0, ":/icons/eraser.svg",            "Eraser" },
    { ToolType::Pencil,             5, 1, ":/icons/pencil.svg",            "Pencil" },
    { ToolType::ColorPicker,        6, 0, ":/icons/color-picker.svg",      "Picker" },
    { ToolType::CloneStamp,         6, 1, ":/icons/clone-stamp.svg",       "Stamp" },
    { ToolType::Recolor,            7, 0, ":/icons/recolor.svg",           "Recolor" },
    { ToolType::Gradient,           7, 1, ":/icons/gradient.svg",          "Grad" },
    { ToolType::Text,               8, 0, ":/icons/text.svg",              "Text" },
    { ToolType::LineCurve,          8, 1, ":/icons/line-curve.svg",        "Line" },
    { ToolType::Shapes,             9, 0, ":/icons/shapes.svg",            "Shapes" }
};

ToolsDock::ToolsDock(ToolManager* toolMgr, QWidget* parent)
    : QDockWidget("Tools", parent), m_toolMgr(toolMgr) {
    setObjectName("ToolsDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    initResources();
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
        btn->setFocusPolicy(Qt::NoFocus);
        QIcon icon(item.iconPath);
        if (!icon.isNull()) {
            btn->setIcon(icon);
            btn->setIconSize(QSize(20, 20));
        } else {
            btn->setText(item.iconText.trimmed());
        }
        btn->setToolTip(QString("%1 (%2)").arg(toolObj->name()).arg(toolObj->shortcut()));
        btn->setCheckable(true);
        btn->setFixedSize(36, 30);

        layout->addWidget(btn, item.row, item.col);
        m_buttonGroup->addButton(btn, static_cast<int>(item.type));
        m_buttons[item.type] = btn;
    }

    connect(m_buttonGroup, &QButtonGroup::idClicked, this, &ToolsDock::onToolButtonClicked);

    layout->setRowStretch(10, 1);
    container->setLayout(layout);
    setWidget(container);
    setFixedWidth(90);
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
