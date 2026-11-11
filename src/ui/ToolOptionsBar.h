#pragma once

#include <QToolBar>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QToolButton>
#include <QFontComboBox>
#include <QLabel>
#include <QHash>
#include "../tools/ToolManager.h"

namespace pdn {

class ToolOptionsBar : public QToolBar {
    Q_OBJECT
public:
    explicit ToolOptionsBar(ToolManager* toolMgr, QWidget* parent = nullptr);

public slots:
    void updateForTool(ToolType type);

    QSpinBox* brushWidthSpin() const { return m_brushWidthSpin; }
    QToolButton* antialiasButton() const { return m_antialiasBtn; }
    QToolButton* compositionModeButton() const { return m_compositionModeBtn; }

private:
    void setupWidgets();
    void addOption(QWidget* widget);
    void setOptionVisible(QWidget* widget, bool visible);

    QHash<QWidget*, QAction*> m_optionActions;

    ToolManager* m_toolMgr;

    // Brush width
    QLabel* m_brushWidthLabel;
    QSpinBox* m_brushWidthSpin;

    // Hardness and blend mode of painting tools
    QLabel* m_hardnessLabel;
    QSpinBox* m_hardnessSpin;
    QLabel* m_blendModeLabel;
    QComboBox* m_blendModeCombo;

    // Antialias
    QToolButton* m_antialiasBtn;

    // Composition mode (Draw Over vs Overwrite)
    QToolButton* m_compositionModeBtn;

    // Selection mode
    QLabel* m_selectionModeLabel;
    QComboBox* m_selectionModeCombo;

    // Tolerance
    QLabel* m_toleranceLabel;
    QSlider* m_toleranceSlider;
    QLabel* m_toleranceValLabel;
    QComboBox* m_floodModeCombo;

    // Fill mode & Shape type
    QLabel* m_shapeTypeLabel;
    QComboBox* m_shapeTypeCombo;
    QLabel* m_fillModeLabel;
    QComboBox* m_fillModeCombo;

    // Gradient mode
    QLabel* m_gradientModeLabel;
    QComboBox* m_gradientModeCombo;

    // Text tool options
    QFontComboBox* m_fontCombo;
    QComboBox* m_fontSizeCombo;
    QToolButton* m_boldBtn;
    QToolButton* m_italicBtn;
    QToolButton* m_underlineBtn;
};

} // namespace pdn
