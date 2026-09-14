#include "ToolOptionsBar.h"

namespace pdn {

ToolOptionsBar::ToolOptionsBar(ToolManager* toolMgr, QWidget* parent)
    : QToolBar("Tool Options", parent), m_toolMgr(toolMgr) {
    setMovable(false);
    setupWidgets();
    updateForTool(toolMgr->activeToolType());

    connect(m_toolMgr, &ToolManager::activeToolChanged, this, &ToolOptionsBar::updateForTool);
}

void ToolOptionsBar::setupWidgets() {
    // 1. Brush Width
    m_brushWidthLabel = new QLabel(" Width: ", this);
    m_brushWidthSpin = new QSpinBox(this);
    m_brushWidthSpin->setFocusPolicy(Qt::NoFocus);
    m_brushWidthSpin->setRange(1, 500);
    m_brushWidthSpin->setValue(m_toolMgr->context().brushWidth);
    connect(m_brushWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_toolMgr->context().brushWidth = val;
    });

    // 2. Antialias
    m_antialiasBtn = new QToolButton(this);
    m_antialiasBtn->setFocusPolicy(Qt::NoFocus);
    m_antialiasBtn->setText("Smooth");
    m_antialiasBtn->setCheckable(true);
    m_antialiasBtn->setChecked(m_toolMgr->context().antiAliasing);
    connect(m_antialiasBtn, &QToolButton::toggled, this, [this](bool checked) {
        m_toolMgr->context().antiAliasing = checked;
        m_antialiasBtn->setText(checked ? "Smooth" : "Pixelated");
    });

    // 3. Selection mode
    m_selectionModeLabel = new QLabel(" Mode: ", this);
    m_selectionModeCombo = new QComboBox(this);
    m_selectionModeCombo->setFocusPolicy(Qt::NoFocus);
    m_selectionModeCombo->addItem("Replace", static_cast<int>(SelectionCombineMode::Replace));
    m_selectionModeCombo->addItem("Add (Union)", static_cast<int>(SelectionCombineMode::Union));
    m_selectionModeCombo->addItem("Subtract", static_cast<int>(SelectionCombineMode::Exclude));
    m_selectionModeCombo->addItem("Intersect", static_cast<int>(SelectionCombineMode::Intersect));
    m_selectionModeCombo->addItem("Invert", static_cast<int>(SelectionCombineMode::Invert));
    connect(m_selectionModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_toolMgr->context().selectionCombineMode = static_cast<SelectionCombineMode>(m_selectionModeCombo->itemData(idx).toInt());
    });

    // 4. Tolerance
    m_toleranceLabel = new QLabel(" Tolerance: ", this);
    m_toleranceSlider = new QSlider(Qt::Horizontal, this);
    m_toleranceSlider->setFocusPolicy(Qt::NoFocus);
    m_toleranceSlider->setRange(0, 100);
    m_toleranceSlider->setValue(m_toolMgr->context().tolerance);
    m_toleranceSlider->setFixedWidth(100);
    m_toleranceValLabel = new QLabel(QString("%1%").arg(m_toolMgr->context().tolerance), this);
    connect(m_toleranceSlider, &QSlider::valueChanged, this, [this](int val) {
        m_toolMgr->context().tolerance = val;
        m_toleranceValLabel->setText(QString("%1%").arg(val));
    });

    // 5. Shapes & Fill Mode
    m_shapeTypeLabel = new QLabel(" Shape: ", this);
    m_shapeTypeCombo = new QComboBox(this);
    m_shapeTypeCombo->setFocusPolicy(Qt::NoFocus);
    m_shapeTypeCombo->addItem("Rectangle", static_cast<int>(ShapeType::Rectangle));
    m_shapeTypeCombo->addItem("Rounded Rect", static_cast<int>(ShapeType::RoundedRectangle));
    m_shapeTypeCombo->addItem("Ellipse", static_cast<int>(ShapeType::Ellipse));
    m_shapeTypeCombo->addItem("Diamond", static_cast<int>(ShapeType::Diamond));
    m_shapeTypeCombo->addItem("Triangle", static_cast<int>(ShapeType::Triangle));
    m_shapeTypeCombo->addItem("Star", static_cast<int>(ShapeType::Star));
    connect(m_shapeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_toolMgr->context().shapeType = static_cast<ShapeType>(m_shapeTypeCombo->itemData(idx).toInt());
    });

    m_fillModeLabel = new QLabel(" Style: ", this);
    m_fillModeCombo = new QComboBox(this);
    m_fillModeCombo->setFocusPolicy(Qt::NoFocus);
    m_fillModeCombo->addItem("Outline", static_cast<int>(FillMode::OutlineOnly));
    m_fillModeCombo->addItem("Fill", static_cast<int>(FillMode::FillOnly));
    m_fillModeCombo->addItem("Outline & Fill", static_cast<int>(FillMode::OutlineAndFill));
    connect(m_fillModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_toolMgr->context().fillMode = static_cast<FillMode>(m_fillModeCombo->itemData(idx).toInt());
    });

    // 6. Gradient mode
    m_gradientModeLabel = new QLabel(" Gradient: ", this);
    m_gradientModeCombo = new QComboBox(this);
    m_gradientModeCombo->setFocusPolicy(Qt::NoFocus);
    m_gradientModeCombo->addItem("Linear", static_cast<int>(GradientMode::Linear));
    m_gradientModeCombo->addItem("Radial", static_cast<int>(GradientMode::Radial));
    m_gradientModeCombo->addItem("Conical", static_cast<int>(GradientMode::Conical));
    m_gradientModeCombo->addItem("Diamond", static_cast<int>(GradientMode::Diamond));
    connect(m_gradientModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_toolMgr->context().gradientMode = static_cast<GradientMode>(m_gradientModeCombo->itemData(idx).toInt());
    });

    // 7. Text Options
    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setFocusPolicy(Qt::NoFocus);
    m_fontCombo->setCurrentFont(m_toolMgr->context().font);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        m_toolMgr->context().font = f;
    });

    m_fontSizeCombo = new QComboBox(this);
    m_fontSizeCombo->setFocusPolicy(Qt::NoFocus);
    const int sizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 72};
    for (int s : sizes) m_fontSizeCombo->addItem(QString::number(s), s);
    m_fontSizeCombo->setCurrentText("12");
    connect(m_fontSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        int sz = m_fontSizeCombo->itemData(idx).toInt();
        m_toolMgr->context().fontSize = sz;
        m_toolMgr->context().font.setPointSize(sz);
    });

    m_boldBtn = new QToolButton(this);
    m_boldBtn->setFocusPolicy(Qt::NoFocus);
    m_boldBtn->setText("B");
    m_boldBtn->setCheckable(true);
    QFont bf = m_boldBtn->font();
    bf.setBold(true);
    m_boldBtn->setFont(bf);
    connect(m_boldBtn, &QToolButton::toggled, this, [this](bool checked) {
        m_toolMgr->context().font.setBold(checked);
    });

    m_italicBtn = new QToolButton(this);
    m_italicBtn->setFocusPolicy(Qt::NoFocus);
    m_italicBtn->setText("I");
    m_italicBtn->setCheckable(true);
    QFont ift = m_italicBtn->font();
    ift.setItalic(true);
    m_italicBtn->setFont(ift);
    connect(m_italicBtn, &QToolButton::toggled, this, [this](bool checked) {
        m_toolMgr->context().font.setItalic(checked);
    });

    m_underlineBtn = new QToolButton(this);
    m_underlineBtn->setFocusPolicy(Qt::NoFocus);
    m_underlineBtn->setText("U");
    m_underlineBtn->setCheckable(true);
    QFont uf = m_underlineBtn->font();
    uf.setUnderline(true);
    m_underlineBtn->setFont(uf);
    connect(m_underlineBtn, &QToolButton::toggled, this, [this](bool checked) {
        m_toolMgr->context().font.setUnderline(checked);
    });

    // Add all to toolbar
    addWidget(m_brushWidthLabel);
    addWidget(m_brushWidthSpin);
    addWidget(m_antialiasBtn);
    addSeparator();
    addWidget(m_selectionModeLabel);
    addWidget(m_selectionModeCombo);
    addSeparator();
    addWidget(m_toleranceLabel);
    addWidget(m_toleranceSlider);
    addWidget(m_toleranceValLabel);
    addSeparator();
    addWidget(m_shapeTypeLabel);
    addWidget(m_shapeTypeCombo);
    addWidget(m_fillModeLabel);
    addWidget(m_fillModeCombo);
    addSeparator();
    addWidget(m_gradientModeLabel);
    addWidget(m_gradientModeCombo);
    addSeparator();
    addWidget(m_fontCombo);
    addWidget(m_fontSizeCombo);
    addWidget(m_boldBtn);
    addWidget(m_italicBtn);
    addWidget(m_underlineBtn);
}

void ToolOptionsBar::updateForTool(ToolType type) {
    bool isBrush = (type == ToolType::Paintbrush || type == ToolType::Eraser ||
                    type == ToolType::CloneStamp || type == ToolType::Recolor ||
                    type == ToolType::LineCurve || type == ToolType::Shapes);
    m_brushWidthLabel->setVisible(isBrush);
    m_brushWidthSpin->setVisible(isBrush);
    m_antialiasBtn->setVisible(isBrush);

    bool isSelection = (type == ToolType::RectangleSelect || type == ToolType::EllipseSelect ||
                        type == ToolType::LassoSelect || type == ToolType::MagicWand);
    m_selectionModeLabel->setVisible(isSelection);
    m_selectionModeCombo->setVisible(isSelection);

    bool isTolerance = (type == ToolType::MagicWand || type == ToolType::PaintBucket || type == ToolType::Recolor);
    m_toleranceLabel->setVisible(isTolerance);
    m_toleranceSlider->setVisible(isTolerance);
    m_toleranceValLabel->setVisible(isTolerance);

    bool isShapes = (type == ToolType::Shapes);
    m_shapeTypeLabel->setVisible(isShapes);
    m_shapeTypeCombo->setVisible(isShapes);
    m_fillModeLabel->setVisible(isShapes);
    m_fillModeCombo->setVisible(isShapes);

    bool isGradient = (type == ToolType::Gradient);
    m_gradientModeLabel->setVisible(isGradient);
    m_gradientModeCombo->setVisible(isGradient);

    bool isText = (type == ToolType::Text);
    m_fontCombo->setVisible(isText);
    m_fontSizeCombo->setVisible(isText);
    m_boldBtn->setVisible(isText);
    m_italicBtn->setVisible(isText);
    m_underlineBtn->setVisible(isText);
}

} // namespace pdn
