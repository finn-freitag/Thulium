#pragma once

#include <QDockWidget>
#include <QButtonGroup>
#include <QToolButton>
#include <QMap>
#include "../tools/ToolManager.h"

namespace pdn {

class ToolsDock : public QDockWidget {
    Q_OBJECT
public:
    explicit ToolsDock(ToolManager* toolMgr, QWidget* parent = nullptr);

private slots:
    void onToolButtonClicked(int id);
    void onActiveToolChanged(ToolType type);

private:
    void setupUI();

    ToolManager* m_toolMgr;
    QButtonGroup* m_buttonGroup;
    QMap<ToolType, QToolButton*> m_buttons;
};

} // namespace pdn
