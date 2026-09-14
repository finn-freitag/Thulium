#pragma once

#include <QDockWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include "../tools/ToolManager.h"

namespace pdn {

class ColorWheelWidget : public QWidget {
    Q_OBJECT
public:
    explicit ColorWheelWidget(QWidget* parent = nullptr);

    void setColor(const QColor& color);
    QColor color() const { return m_color; }

signals:
    void colorChanged(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void updateColorFromPos(const QPoint& pos);

    QColor m_color = Qt::black;
    int m_hue = 0;        // 0..359
    int m_saturation = 0; // 0..255
    int m_value = 0;      // 0..255
    bool m_trackingWheel = false;
    bool m_trackingValueBar = false;
    QImage m_cachedWheelImage;
    int m_cachedValue = -1;
    void renderWheelImage();
};

class ColorPreviewBox : public QPushButton {
    Q_OBJECT
public:
    explicit ColorPreviewBox(QWidget* parent = nullptr);

    void setColor(const QColor& color, bool isSelected);
    QColor color() const { return m_color; }
    bool isSelected() const { return m_isSelected; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_color = Qt::black;
    bool m_isSelected = false;
};

class ColorsDock : public QDockWidget {
    Q_OBJECT
public:
    explicit ColorsDock(ToolManager* toolMgr, QWidget* parent = nullptr);

    bool isEditingPrimary() const { return m_editingPrimary; }
    void setEditingPrimary(bool primary);
    QColor& activeTargetColor();
    QColor activeTargetColor() const;

    ColorPreviewBox* primaryBox() const { return m_primaryBox; }
    ColorPreviewBox* secondaryBox() const { return m_secondaryBox; }

private slots:
    void onWheelColorChanged(const QColor& c);
    void onRgbChanged();
    void onHsvChanged();
    void onAlphaChanged(int val);
    void onHexChanged();
    void onSwapColors();
    void onDefaultColors();
    void onToggleMoreLess();
    void onSwatchClicked(const QColor& c);

private:
    void setupUI();
    void updateUIFromActiveColor();

    ToolManager* m_toolMgr;
    bool m_editingPrimary = true; // true = Primary, false = Secondary
    bool m_moreExpanded = false;
    bool m_updating = false;

    // Color Swatches
    ColorPreviewBox* m_primaryBox;
    ColorPreviewBox* m_secondaryBox;
    QPushButton* m_swapBtn;
    QPushButton* m_defaultBtn;

    ColorWheelWidget* m_wheel;

    // Expand toggle
    QPushButton* m_moreLessBtn;
    QWidget* m_moreContainer;

    // RGB
    QSlider* m_rSlider; QSpinBox* m_rSpin;
    QSlider* m_gSlider; QSpinBox* m_gSpin;
    QSlider* m_bSlider; QSpinBox* m_bSpin;

    // HSV
    QSlider* m_hSlider; QSpinBox* m_hSpin;
    QSlider* m_sSlider; QSpinBox* m_sSpin;
    QSlider* m_vSlider; QSpinBox* m_vSpin;

    // Alpha
    QSlider* m_aSlider; QSpinBox* m_aSpin;

    // Hex
    QLineEdit* m_hexEdit;
};

} // namespace pdn
