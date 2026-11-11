#include "ToolOptionsBar.h"

namespace pdn {

ToolOptionsBar::ToolOptionsBar(ToolManager* toolMgr, QWidget* parent)
    : QToolBar("Tool Options", parent), m_toolMgr(toolMgr) {
    setMovable(false);
    setMinimumHeight(36); // keep the bar's height when a tool has no options
    setupWidgets();
    updateForTool(toolMgr->activeToolType());

    connect(m_toolMgr, &ToolManager::activeToolChanged, this, &ToolOptionsBar::updateForTool);
}

void ToolOptionsBar::setupWidgets() {
    // 1. Brush Width
    m_brushWidthLabel = new QLabel(" Width: ", this);
    m_brushWidthSpin = new QSpinBox(this);
    m_brushWidthSpin->setRange(1, 500);
    m_brushWidthSpin->setValue(m_toolMgr->context().brushWidth);
    connect(m_brushWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_toolMgr->context().brushWidth = val;
    });
    connect(m_brushWidthSpin, &QSpinBox::editingFinished, this, [this]() {
        m_brushWidthSpin->clearFocus();
    });

    m_hardnessLabel = new QLabel(" Hardness: ", this);
    m_hardnessSpin = new QSpinBox(this);
    m_hardnessSpin->setRange(0, 100);
    m_hardnessSpin->setSuffix("%");
    m_hardnessSpin->setSingleStep(5);
    m_hardnessSpin->setToolTip("Brush hardness: 100% paints a crisp edge, lower values fade out towards the edge");
    m_hardnessSpin->setValue(m_toolMgr->context().brushHardness);
    connect(m_hardnessSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_toolMgr->context().brushHardness = val;
    });
    connect(m_hardnessSpin, &QSpinBox::editingFinished, this, [this]() { m_hardnessSpin->clearFocus(); });

    m_blendModeLabel = new QLabel(" Blend: ", this);
    m_blendModeCombo = new QComboBox(this);
    m_blendModeCombo->setFocusPolicy(Qt::NoFocus);
    m_blendModeCombo->setToolTip("How the painted colour is combined with the pixels already on the layer");
    for (const BlendModeInfo& info : getAvailableBlendModes()) {
        m_blendModeCombo->addItem(info.name, static_cast<int>(info.mode));
    }
    connect(m_blendModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_toolMgr->context().brushBlendMode = static_cast<BlendMode>(m_blendModeCombo->itemData(idx).toInt());
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

    // 2b. Composition mode (Draw Over vs Overwrite)
    m_compositionModeBtn = new QToolButton(this);
    m_compositionModeBtn->setFocusPolicy(Qt::NoFocus);
    bool isOverwrite = (m_toolMgr->context().compositionMode == ColorCompositionMode::Overwrite);
    m_compositionModeBtn->setText(isOverwrite ? "Overwrite" : "Draw Over");
    m_compositionModeBtn->setCheckable(true);
    m_compositionModeBtn->setChecked(isOverwrite);
    m_compositionModeBtn->setToolTip(isOverwrite ? "Color Composition: Overwrite (replace colors below)"
                                                 : "Color Composition: Draw Over (draw on top of colors below)");
    connect(m_compositionModeBtn, &QToolButton::toggled, this, [this](bool checked) {
        m_toolMgr->context().compositionMode = checked ? ColorCompositionMode::Overwrite : ColorCompositionMode::DrawOver;
        m_compositionModeBtn->setText(checked ? "Overwrite" : "Draw Over");
        m_compositionModeBtn->setToolTip(checked ? "Color Composition: Overwrite (replace colors below)"
                                                 : "Color Composition: Draw Over (draw on top of colors below)");
        m_toolMgr->context().notifyChanged();
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

    m_floodModeCombo = new QComboBox(this);
    m_floodModeCombo->setFocusPolicy(Qt::NoFocus);
    m_floodModeCombo->setToolTip("Flood mode: only the connected area, or every similar pixel on the layer");
    m_floodModeCombo->addItem("Contiguous", static_cast<int>(FloodMode::Contiguous));
    m_floodModeCombo->addItem("Global", static_cast<int>(FloodMode::Global));
    connect(m_floodModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_toolMgr->context().floodMode = static_cast<FloodMode>(m_floodModeCombo->itemData(idx).toInt());
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
    m_shapeTypeCombo->addItem("Pentagon", static_cast<int>(ShapeType::Pentagon));
    m_shapeTypeCombo->addItem("Hexagon", static_cast<int>(ShapeType::Hexagon));
    m_shapeTypeCombo->addItem("Arrow", static_cast<int>(ShapeType::Arrow));
    m_shapeTypeCombo->addItem("Heart", static_cast<int>(ShapeType::Heart));
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
    m_gradientModeCombo->addItem("Linear (Reflected)", static_cast<int>(GradientMode::LinearReflected));
    m_gradientModeCombo->addItem("Diamond", static_cast<int>(GradientMode::Diamond));
    m_gradientModeCombo->addItem("Radial", static_cast<int>(GradientMode::Radial));
    m_gradientModeCombo->addItem("Conical", static_cast<int>(GradientMode::Conical));
    m_gradientModeCombo->addItem("Spiral", static_cast<int>(GradientMode::Spiral));
    connect(m_gradientModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_toolMgr->context().gradientMode = static_cast<GradientMode>(m_gradientModeCombo->itemData(idx).toInt());
    });

    // 7. Text Options
    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setCurrentFont(m_toolMgr->context().font);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        m_toolMgr->context().font = f;
    });

    m_fontSizeCombo = new QComboBox(this);
    m_fontSizeCombo->setEditable(true);
    const int sizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 72};
    for (int s : sizes) m_fontSizeCombo->addItem(QString::number(s), s);
    m_fontSizeCombo->setCurrentText("12");
    auto updateFontSize = [this](const QString& text) {
        bool ok = false;
        int sz = text.toInt(&ok);
        if (ok && sz > 0 && sz <= 1000) {
            m_toolMgr->context().fontSize = sz;
            m_toolMgr->context().font.setPointSize(sz);
        }
    };
    connect(m_fontSizeCombo, &QComboBox::currentTextChanged, this, updateFontSize);

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
    addOption(m_brushWidthLabel);
    addOption(m_brushWidthSpin);
    addOption(m_hardnessLabel);
    addOption(m_hardnessSpin);
    addOption(m_antialiasBtn);
    addOption(m_compositionModeBtn);
    addOption(m_blendModeLabel);
    addOption(m_blendModeCombo);
    addOption(m_selectionModeLabel);
    addOption(m_selectionModeCombo);
    addOption(m_toleranceLabel);
    addOption(m_toleranceSlider);
    addOption(m_toleranceValLabel);
    addOption(m_floodModeCombo);
    addOption(m_shapeTypeLabel);
    addOption(m_shapeTypeCombo);
    addOption(m_fillModeLabel);
    addOption(m_fillModeCombo);
    addOption(m_gradientModeLabel);
    addOption(m_gradientModeCombo);
    addOption(m_fontCombo);
    addOption(m_fontSizeCombo);
    addOption(m_boldBtn);
    addOption(m_italicBtn);
    addOption(m_underlineBtn);
}

void ToolOptionsBar::addOption(QWidget* widget) {
    m_optionActions.insert(widget, addWidget(widget));
}

void ToolOptionsBar::setOptionVisible(QWidget* widget, bool visible) {
    // QToolBar owns the visibility of embedded widgets: QWidget::setVisible() has no lasting
    // effect, the action that wraps the widget has to be toggled instead.
    if (QAction* action = m_optionActions.value(widget)) {
        action->setVisible(visible);
    }
}

void ToolOptionsBar::updateForTool(ToolType type) {
    const bool hasWidth = (type == ToolType::Paintbrush || type == ToolType::Eraser ||
                           type == ToolType::CloneStamp || type == ToolType::Recolor ||
                           type == ToolType::LineCurve || type == ToolType::Shapes);
    setOptionVisible(m_brushWidthLabel, hasWidth);
    setOptionVisible(m_brushWidthSpin, hasWidth);
    setOptionVisible(m_antialiasBtn, hasWidth || type == ToolType::Text);
    // Only these tools honour the draw-over / overwrite choice
    const bool hasHardness = (type == ToolType::Paintbrush || type == ToolType::Eraser || type == ToolType::CloneStamp);
    setOptionVisible(m_hardnessLabel, hasHardness);
    setOptionVisible(m_hardnessSpin, hasHardness);
    const bool blendsColor = (type == ToolType::Paintbrush || type == ToolType::PaintBucket || type == ToolType::Gradient);
    setOptionVisible(m_compositionModeBtn, blendsColor);
    setOptionVisible(m_blendModeLabel, blendsColor);
    setOptionVisible(m_blendModeCombo, blendsColor);

    bool isSelection = (type == ToolType::RectangleSelect || type == ToolType::EllipseSelect ||
                        type == ToolType::LassoSelect || type == ToolType::MagicWand);
    setOptionVisible(m_selectionModeLabel, isSelection);
    setOptionVisible(m_selectionModeCombo, isSelection);

    bool isTolerance = (type == ToolType::MagicWand || type == ToolType::PaintBucket || type == ToolType::Recolor);
    setOptionVisible(m_toleranceLabel, isTolerance);
    setOptionVisible(m_toleranceSlider, isTolerance);
    setOptionVisible(m_toleranceValLabel, isTolerance);
    setOptionVisible(m_floodModeCombo, type == ToolType::MagicWand || type == ToolType::PaintBucket);

    bool isShapes = (type == ToolType::Shapes);
    setOptionVisible(m_shapeTypeLabel, isShapes);
    setOptionVisible(m_shapeTypeCombo, isShapes);
    setOptionVisible(m_fillModeLabel, isShapes);
    setOptionVisible(m_fillModeCombo, isShapes);

    bool isGradient = (type == ToolType::Gradient);
    setOptionVisible(m_gradientModeLabel, isGradient);
    setOptionVisible(m_gradientModeCombo, isGradient);

    bool isText = (type == ToolType::Text);
    setOptionVisible(m_fontCombo, isText);
    setOptionVisible(m_fontSizeCombo, isText);
    setOptionVisible(m_boldBtn, isText);
    setOptionVisible(m_italicBtn, isText);
    setOptionVisible(m_underlineBtn, isText);
}

} // namespace pdn
