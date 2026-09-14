#pragma once

#include <QToolBar>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QToolButton>
#include <QFontComboBox>
#include <QLabel>
#include "../tools/ToolManager.h"

namespace pdn {

class ToolOptionsBar : public QToolBar {
    Q_OBJECT
public:
    explicit ToolOptionsBar(ToolManager* toolMgr, QWidget* parent = nullptr);

public slots:
    void updateForTool(ToolType type);

    QSpinBox* brushWidthSpin() const { return m_brushWidthSpin; }

private:
    void setupWidgets();

    ToolManager* m_toolMgr;

    // Brush width
    QLabel* m_brushWidthLabel;
    QSpinBox* m_brushWidthSpin;

    // Antialias
    QToolButton* m_antialiasBtn;

    // Selection mode
    QLabel* m_selectionModeLabel;
    QComboBox* m_selectionModeCombo;

    // Tolerance
    QLabel* m_toleranceLabel;
    QSlider* m_toleranceSlider;
    QLabel* m_toleranceValLabel;

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
