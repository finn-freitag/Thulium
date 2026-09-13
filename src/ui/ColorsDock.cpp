#include "ColorsDock.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QPainter>
#include <QMouseEvent>
#include <cmath>
#include <algorithm>

namespace pdn {

// 32 standard Paint.NET default swatches
static const QColor s_pdnSwatches[32] = {
    QColor(0, 0, 0),       QColor(64, 64, 64),    QColor(255, 0, 0),     QColor(255, 106, 0),
    QColor(255, 216, 0),   QColor(0, 255, 33),    QColor(0, 38, 255),    QColor(178, 0, 255),
    QColor(255, 0, 110),   QColor(255, 255, 255), QColor(128, 128, 128), QColor(127, 0, 0),
    QColor(127, 51, 0),    QColor(127, 106, 0),   QColor(0, 127, 14),    QColor(0, 19, 127),
    QColor(76, 0, 115),    QColor(127, 0, 55),    QColor(160, 160, 160), QColor(48, 48, 48),
    QColor(255, 127, 127), QColor(255, 178, 127), QColor(255, 233, 127), QColor(182, 255, 127),
    QColor(127, 201, 255), QColor(178, 127, 255), QColor(255, 127, 237), QColor(0, 162, 232),
    QColor(112, 146, 190), QColor(200, 191, 231), QColor(185, 122, 87), QColor(128, 0, 64)
};

// --- ColorWheelWidget ---
ColorWheelWidget::ColorWheelWidget(QWidget* parent) : QWidget(parent) {
    setFixedSize(160, 140);
    setColor(Qt::black);
}

void ColorWheelWidget::setColor(const QColor& c) {
    m_color = c;
    m_hue = c.hsvHue() >= 0 ? c.hsvHue() : 0;
    m_saturation = c.hsvSaturation();
    m_value = c.value();
    update();
}

void ColorWheelWidget::updateColorFromPos(const QPoint& pos) {
    int wheelDiameter = 120;
    int wheelRadius = wheelDiameter / 2;
    QPoint wheelCenter(wheelRadius + 10, wheelRadius + 10);

    int barLeft = 140;
    int barWidth = 14;
    int barTop = 10;
    int barHeight = 120;

    if (m_trackingWheel) {
        int dx = pos.x() - wheelCenter.x();
        int dy = pos.y() - wheelCenter.y();
        double dist = std::sqrt(dx * dx + dy * dy);

        double angle = std::atan2(-dy, dx) * 180.0 / M_PI;
        if (angle < 0) angle += 360.0;
        m_hue = static_cast<int>(angle) % 360;

        double satRatio = std::clamp(dist / wheelRadius, 0.0, 1.0);
        m_saturation = static_cast<int>(satRatio * 255.0);

        if (m_value == 0) {
            m_value = 255;
        }

        m_color.setHsv(m_hue, m_saturation, m_value, m_color.alpha());
        emit colorChanged(m_color);
        update();
    } else if (m_trackingValueBar) {
        int y = std::clamp(pos.y(), barTop, barTop + barHeight);
        double ratio = 1.0 - static_cast<double>(y - barTop) / barHeight;
        m_value = static_cast<int>(ratio * 255.0);

        m_color.setHsv(m_hue, m_saturation, m_value, m_color.alpha());
        emit colorChanged(m_color);
        update();
    }
}

void ColorWheelWidget::mousePressEvent(QMouseEvent* event) {
    int barLeft = 140;
    int barWidth = 14;
    int barTop = 10;
    int barHeight = 120;

    QRect barRect(barLeft - 2, barTop, barWidth + 4, barHeight);
    if (barRect.contains(event->pos())) {
        m_trackingValueBar = true;
        m_trackingWheel = false;
    } else {
        m_trackingWheel = true;
        m_trackingValueBar = false;
    }
    updateColorFromPos(event->pos());
}

void ColorWheelWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_trackingWheel || m_trackingValueBar) {
        updateColorFromPos(event->pos());
    }
}

void ColorWheelWidget::mouseReleaseEvent(QMouseEvent* /*event*/) {
    m_trackingWheel = false;
    m_trackingValueBar = false;
}

void ColorWheelWidget::renderWheelImage() {
    int wheelRadius = 60;
    int wheelDiameter = wheelRadius * 2;
    m_cachedWheelImage = QImage(wheelDiameter, wheelDiameter, QImage::Format_ARGB32_Premultiplied);
    m_cachedWheelImage.fill(Qt::transparent);

    QPoint center(wheelRadius, wheelRadius);
    for (int y = 0; y < wheelDiameter; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(m_cachedWheelImage.scanLine(y));
        for (int x = 0; x < wheelDiameter; ++x) {
            double dx = x - center.x();
            double dy = y - center.y();
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= wheelRadius) {
                double angle = std::atan2(-dy, dx) * 180.0 / M_PI;
                if (angle < 0) angle += 360.0;
                int hue = static_cast<int>(angle) % 360;
                int sat = std::clamp(static_cast<int>((dist / wheelRadius) * 255.0), 0, 255);

                double alpha = 1.0;
                if (dist > wheelRadius - 1.0) {
                    alpha = std::clamp(wheelRadius - dist, 0.0, 1.0);
                }

                QColor col;
                col.setHsv(hue, sat, 255, static_cast<int>(alpha * 255.0));
                line[x] = col.rgba();
            } else {
                line[x] = 0;
            }
        }
    }
}

void ColorWheelWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int wheelRadius = 60;
    QPoint center(wheelRadius + 10, wheelRadius + 10);

    if (m_cachedWheelImage.isNull()) {
        renderWheelImage();
    }
    p.drawImage(center.x() - wheelRadius, center.y() - wheelRadius, m_cachedWheelImage);

    // Wheel border
    p.setPen(QColor(120, 120, 120));
    p.drawEllipse(center, wheelRadius, wheelRadius);

    // Current Hue/Sat handle
    double handleAngle = m_hue * M_PI / 180.0;
    double handleDist = (m_saturation / 255.0) * wheelRadius;
    QPoint handlePos(center.x() + static_cast<int>(handleDist * std::cos(handleAngle)),
                     center.y() - static_cast<int>(handleDist * std::sin(handleAngle)));
    p.setPen(Qt::black);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(handlePos, 4, 4);
    p.setPen(Qt::white);
    p.drawEllipse(handlePos, 3, 3);

    // Value bar (vertical slider)
    int barLeft = 140;
    int barTop = 10;
    int barWidth = 14;
    int barHeight = 120;

    QLinearGradient barGrad(0, barTop, 0, barTop + barHeight);
    QColor topCol;
    topCol.setHsv(m_hue, m_saturation, 255);
    barGrad.setColorAt(0.0, topCol);
    barGrad.setColorAt(1.0, Qt::black);

    p.fillRect(barLeft, barTop, barWidth, barHeight, barGrad);
    p.setPen(QColor(120, 120, 120));
    p.drawRect(barLeft, barTop, barWidth, barHeight);

    // Value bar handle
    int handleY = barTop + static_cast<int>((1.0 - m_value / 255.0) * barHeight);
    p.setPen(Qt::white);
    p.drawLine(barLeft - 2, handleY, barLeft + barWidth + 2, handleY);
    p.setPen(Qt::black);
    p.drawLine(barLeft - 2, handleY + 1, barLeft + barWidth + 2, handleY + 1);
}

// --- ColorsDock ---
ColorsDock::ColorsDock(ToolManager* toolMgr, QWidget* parent)
    : QDockWidget("Colors", parent), m_toolMgr(toolMgr) {
    setObjectName("ColorsDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupUI();
    updateUIFromActiveColor();
    connect(m_toolMgr, &ToolManager::contextChanged, this, &ColorsDock::updateUIFromActiveColor);
}

void ColorsDock::setupUI() {
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    // Top: Primary & Secondary color swatches + Swap & Default buttons
    QHBoxLayout* topLayout = new QHBoxLayout();
    m_primaryBox = new QPushButton(container);
    m_primaryBox->setFixedSize(36, 36);
    m_primaryBox->setToolTip("Primary Color (Click to select for editing)");
    connect(m_primaryBox, &QPushButton::clicked, this, [this]() {
        m_editingPrimary = true;
        updateUIFromActiveColor();
    });

    m_secondaryBox = new QPushButton(container);
    m_secondaryBox->setFixedSize(36, 36);
    m_secondaryBox->setToolTip("Secondary Color (Click to select for editing)");
    connect(m_secondaryBox, &QPushButton::clicked, this, [this]() {
        m_editingPrimary = false;
        updateUIFromActiveColor();
    });

    m_swapBtn = new QPushButton("<->", container);
    m_swapBtn->setFixedSize(28, 28);
    m_swapBtn->setToolTip("Swap Primary and Secondary colors");
    connect(m_swapBtn, &QPushButton::clicked, this, &ColorsDock::onSwapColors);

    m_defaultBtn = new QPushButton("B/W", container);
    m_defaultBtn->setFixedSize(28, 28);
    m_defaultBtn->setToolTip("Reset to Black and White");
    connect(m_defaultBtn, &QPushButton::clicked, this, &ColorsDock::onDefaultColors);

    topLayout->addWidget(m_primaryBox);
    topLayout->addWidget(m_secondaryBox);
    topLayout->addWidget(m_swapBtn);
    topLayout->addWidget(m_defaultBtn);
    m_primaryBox->setFocusPolicy(Qt::NoFocus);
    m_secondaryBox->setFocusPolicy(Qt::NoFocus);
    m_swapBtn->setFocusPolicy(Qt::NoFocus);
    m_defaultBtn->setFocusPolicy(Qt::NoFocus);
    topLayout->addStretch();
    layout->addLayout(topLayout);

    // Color Wheel
    m_wheel = new ColorWheelWidget(container);
    connect(m_wheel, &ColorWheelWidget::colorChanged, this, &ColorsDock::onWheelColorChanged);
    layout->addWidget(m_wheel, 0, Qt::AlignHCenter);

    // Swatches grid (32 colors in 4 rows x 8 columns)
    QGridLayout* swatchesLayout = new QGridLayout();
    swatchesLayout->setSpacing(2);
    for (int i = 0; i < 32; ++i) {
        QPushButton* sbtn = new QPushButton(container);
        sbtn->setFocusPolicy(Qt::NoFocus);
        sbtn->setFixedSize(16, 16);
        QColor col = s_pdnSwatches[i];
        sbtn->setStyleSheet(QString("background-color: %1; border: 1px solid #888;").arg(col.name()));
        connect(sbtn, &QPushButton::clicked, this, [this, col]() {
            onSwatchClicked(col);
        });
        swatchesLayout->addWidget(sbtn, i / 8, i % 8);
    }
    layout->addLayout(swatchesLayout);

    // More >> / << Less button
    m_moreLessBtn = new QPushButton("More >>", container);
    m_moreLessBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_moreLessBtn, &QPushButton::clicked, this, &ColorsDock::onToggleMoreLess);
    layout->addWidget(m_moreLessBtn);

    // More Container (RGB, HSV, Alpha, Hex)
    m_moreContainer = new QWidget(container);
    QVBoxLayout* moreLayout = new QVBoxLayout(m_moreContainer);
    moreLayout->setContentsMargins(0, 2, 0, 0);
    moreLayout->setSpacing(2);

    auto addSliderRow = [this, moreLayout](const QString& label, QSlider*& slider, QSpinBox*& spin, int min, int max) {
        QHBoxLayout* row = new QHBoxLayout();
        QLabel* lbl = new QLabel(label, m_moreContainer);
        lbl->setFixedWidth(16);
        slider = new QSlider(Qt::Horizontal, m_moreContainer);
        slider->setRange(min, max);
        spin = new QSpinBox(m_moreContainer);
        spin->setRange(min, max);
        spin->setFixedWidth(50);
        connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);
        row->addWidget(lbl);
        row->addWidget(slider);
        row->addWidget(spin);
        moreLayout->addLayout(row);
    };

    addSliderRow("R:", m_rSlider, m_rSpin, 0, 255);
    addSliderRow("G:", m_gSlider, m_gSpin, 0, 255);
    addSliderRow("B:", m_bSlider, m_bSpin, 0, 255);

    addSliderRow("H:", m_hSlider, m_hSpin, 0, 360);
    addSliderRow("S:", m_sSlider, m_sSpin, 0, 100);
    addSliderRow("V:", m_vSlider, m_vSpin, 0, 100);

    addSliderRow("A:", m_aSlider, m_aSpin, 0, 255);

    QHBoxLayout* hexLayout = new QHBoxLayout();
    QLabel* hexLbl = new QLabel("Hex: #", m_moreContainer);
    m_hexEdit = new QLineEdit(m_moreContainer);
    m_hexEdit->setMaxLength(8);
    connect(m_hexEdit, &QLineEdit::editingFinished, this, &ColorsDock::onHexChanged);
    hexLayout->addWidget(hexLbl);
    hexLayout->addWidget(m_hexEdit);
    moreLayout->addLayout(hexLayout);

    connect(m_rSlider, &QSlider::valueChanged, this, &ColorsDock::onRgbChanged);
    connect(m_gSlider, &QSlider::valueChanged, this, &ColorsDock::onRgbChanged);
    connect(m_bSlider, &QSlider::valueChanged, this, &ColorsDock::onRgbChanged);

    connect(m_hSlider, &QSlider::valueChanged, this, &ColorsDock::onHsvChanged);
    connect(m_sSlider, &QSlider::valueChanged, this, &ColorsDock::onHsvChanged);
    connect(m_vSlider, &QSlider::valueChanged, this, &ColorsDock::onHsvChanged);

    connect(m_aSlider, &QSlider::valueChanged, this, &ColorsDock::onAlphaChanged);

    m_moreContainer->setVisible(false);
    layout->addWidget(m_moreContainer);

    container->setLayout(layout);
    setWidget(container);
    setMinimumWidth(180);
}

QColor& ColorsDock::activeTargetColor() {
    return m_editingPrimary ? m_toolMgr->context().primaryColor : m_toolMgr->context().secondaryColor;
}

void ColorsDock::updateUIFromActiveColor() {
    m_updating = true;
    QColor pri = m_toolMgr->context().primaryColor;
    QColor sec = m_toolMgr->context().secondaryColor;

    m_primaryBox->setStyleSheet(QString("background-color: %1; border: %2 solid #0078d7;")
                                    .arg(pri.name()).arg(m_editingPrimary ? "3px" : "1px"));
    m_secondaryBox->setStyleSheet(QString("background-color: %1; border: %2 solid #0078d7;")
                                      .arg(sec.name()).arg(!m_editingPrimary ? "3px" : "1px"));

    QColor active = activeTargetColor();
    m_wheel->setColor(active);

    m_rSlider->setValue(active.red());
    m_gSlider->setValue(active.green());
    m_bSlider->setValue(active.blue());

    m_hSlider->setValue(active.hsvHue() >= 0 ? active.hsvHue() : 0);
    m_sSlider->setValue(static_cast<int>((active.hsvSaturationF()) * 100.0));
    m_vSlider->setValue(static_cast<int>((active.valueF()) * 100.0));

    m_aSlider->setValue(active.alpha());
    m_hexEdit->setText(QString("%1%2%3")
                           .arg(active.red(), 2, 16, QChar('0'))
                           .arg(active.green(), 2, 16, QChar('0'))
                           .arg(active.blue(), 2, 16, QChar('0')).toUpper());
    m_updating = false;
}

void ColorsDock::onWheelColorChanged(const QColor& c) {
    if (m_updating) return;
    int a = activeTargetColor().alpha();
    activeTargetColor() = QColor(c.red(), c.green(), c.blue(), a);
    updateUIFromActiveColor();
    emit m_toolMgr->contextChanged();
}

void ColorsDock::onRgbChanged() {
    if (m_updating) return;
    int a = m_aSlider->value();
    activeTargetColor() = QColor(m_rSlider->value(), m_gSlider->value(), m_bSlider->value(), a);
    updateUIFromActiveColor();
    emit m_toolMgr->contextChanged();
}

void ColorsDock::onHsvChanged() {
    if (m_updating) return;
    int a = m_aSlider->value();
    QColor col;
    col.setHsvF(m_hSlider->value() / 360.0, m_sSlider->value() / 100.0, m_vSlider->value() / 100.0, a / 255.0);
    activeTargetColor() = col;
    updateUIFromActiveColor();
    emit m_toolMgr->contextChanged();
}

void ColorsDock::onAlphaChanged(int val) {
    if (m_updating) return;
    activeTargetColor().setAlpha(val);
    emit m_toolMgr->contextChanged();
}

void ColorsDock::onHexChanged() {
    if (m_updating) return;
    QString hex = m_hexEdit->text().trimmed();
    if (!hex.startsWith('#')) hex.prepend('#');
    QColor c(hex);
    if (c.isValid()) {
        activeTargetColor() = c;
        updateUIFromActiveColor();
        emit m_toolMgr->contextChanged();
    }
}

void ColorsDock::onSwapColors() {
    std::swap(m_toolMgr->context().primaryColor, m_toolMgr->context().secondaryColor);
    updateUIFromActiveColor();
    emit m_toolMgr->contextChanged();
}

void ColorsDock::onDefaultColors() {
    m_toolMgr->context().primaryColor = Qt::black;
    m_toolMgr->context().secondaryColor = Qt::white;
    updateUIFromActiveColor();
    emit m_toolMgr->contextChanged();
}

void ColorsDock::onToggleMoreLess() {
    m_moreExpanded = !m_moreExpanded;
    m_moreContainer->setVisible(m_moreExpanded);
    m_moreLessBtn->setText(m_moreExpanded ? "<< Less" : "More >>");
}

void ColorsDock::onSwatchClicked(const QColor& c) {
    activeTargetColor() = c;
    updateUIFromActiveColor();
    emit m_toolMgr->contextChanged();
}

} // namespace pdn
