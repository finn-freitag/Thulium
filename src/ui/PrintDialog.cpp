#include "PrintDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <cmath>
#include <algorithm>

namespace pdn {

// ============================================================================
// PrintPreviewWidget
// ============================================================================

PrintPreviewWidget::PrintPreviewWidget(QWidget* parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(320, 360);
    setMouseTracking(true);
}

void PrintPreviewWidget::setImage(const QImage& image) {
    m_image = image;
    update();
}

void PrintPreviewWidget::setPageDimensions(double widthMm, double heightMm) {
    m_pageWidthMm = std::max(10.0, widthMm);
    m_pageHeightMm = std::max(10.0, heightMm);
    update();
}

void PrintPreviewWidget::setImagePlacement(double leftMm, double topMm, double widthMm, double heightMm) {
    m_leftMm = leftMm;
    m_topMm = topMm;
    m_widthMm = widthMm;
    m_heightMm = heightMm;
    update();
}

void PrintPreviewWidget::setPrintMargins(double leftMm, double topMm, double rightMm, double bottomMm) {
    m_marginGuideLeftMm = leftMm;
    m_marginGuideTopMm = topMm;
    m_marginGuideRightMm = rightMm;
    m_marginGuideBottomMm = bottomMm;
    update();
}

QRectF PrintPreviewWidget::calculatePaperRect() const {
    const double padding = 24.0;
    QRectF avail(padding, padding, std::max(10.0, width() - 2.0 * padding), std::max(10.0, height() - 2.0 * padding));
    double pageAspect = m_pageWidthMm / m_pageHeightMm;

    double paperW = 0.0;
    double paperH = 0.0;
    if (avail.width() / avail.height() > pageAspect) {
        paperH = avail.height();
        paperW = paperH * pageAspect;
    } else {
        paperW = avail.width();
        paperH = paperW / pageAspect;
    }

    double paperX = avail.x() + (avail.width() - paperW) / 2.0;
    double paperY = avail.y() + (avail.height() - paperH) / 2.0;
    return QRectF(paperX, paperY, paperW, paperH);
}

QRectF PrintPreviewWidget::calculateImageRectOnWidget() const {
    QRectF paper = calculatePaperRect();
    double scale = paper.width() / m_pageWidthMm;
    return QRectF(paper.left() + m_leftMm * scale,
                  paper.top() + m_topMm * scale,
                  m_widthMm * scale,
                  m_heightMm * scale);
}

void PrintPreviewWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Canvas background
    painter.fillRect(rect(), QColor(48, 50, 54));

    QRectF paper = calculatePaperRect();
    double scale = paper.width() / m_pageWidthMm;

    // Drop shadow
    painter.fillRect(paper.translated(5, 5), QColor(20, 20, 20, 160));

    // Paper sheet
    painter.fillRect(paper, Qt::white);
    painter.setPen(QPen(QColor(180, 180, 180), 1));
    painter.drawRect(paper);

    // Printable / margin guide (dashed)
    QRectF guideRect(paper.left() + m_marginGuideLeftMm * scale,
                     paper.top() + m_marginGuideTopMm * scale,
                     std::max(0.0, (m_pageWidthMm - m_marginGuideLeftMm - m_marginGuideRightMm) * scale),
                     std::max(0.0, (m_pageHeightMm - m_marginGuideTopMm - m_marginGuideBottomMm) * scale));
    if (guideRect.width() > 0 && guideRect.height() > 0) {
        painter.setPen(QPen(QColor(190, 190, 190), 1, Qt::DashLine));
        painter.drawRect(guideRect);
    }

    // Target image rectangle on paper
    QRectF imgRect = calculateImageRectOnWidget();

    // Draw the image clipped to paper boundary
    if (!m_image.isNull() && imgRect.width() > 0 && imgRect.height() > 0) {
        painter.save();
        painter.setClipRect(paper);
        painter.drawImage(imgRect, m_image);
        painter.restore();

        // If part of the image overflows the paper, show red dashed warning boundary
        if (!paper.contains(imgRect)) {
            painter.setPen(QPen(QColor(230, 45, 45), 1.5, Qt::DashLine));
            painter.drawRect(imgRect);

            // Shading over overflowed areas outside paper
            QPainterPath overflowPath;
            overflowPath.addRect(imgRect);
            QPainterPath paperPath;
            paperPath.addRect(paper);
            QPainterPath clippedOverflow = overflowPath.subtracted(paperPath);
            painter.fillPath(clippedOverflow, QColor(230, 45, 45, 50));
        } else {
            // Subtle boundary around the image
            painter.setPen(QPen(QColor(70, 130, 180, 160), 1));
            painter.drawRect(imgRect);
        }
    }

    // Info overlay text at bottom
    QString infoText = QString("Page: %1 x %2 mm | Image: %3 x %4 mm")
        .arg(QString::number(m_pageWidthMm, 'f', 1))
        .arg(QString::number(m_pageHeightMm, 'f', 1))
        .arg(QString::number(m_widthMm, 'f', 1))
        .arg(QString::number(m_heightMm, 'f', 1));

    painter.setFont(QFont("sans-serif", 9));
    painter.setPen(QColor(220, 220, 220));
    painter.drawText(rect().adjusted(8, 0, -8, -6), Qt::AlignBottom | Qt::AlignLeft, infoText);
}

void PrintPreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QRectF imgRect = calculateImageRectOnWidget();
        if (imgRect.contains(event->position())) {
            m_isDragging = true;
            m_dragStartMousePos = event->position();
            m_dragStartImageMm = QPointF(m_leftMm, m_topMm);
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void PrintPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        QRectF paper = calculatePaperRect();
        double scale = paper.width() / m_pageWidthMm;
        if (scale > 0) {
            QPointF delta = event->position() - m_dragStartMousePos;
            double newLeft = m_dragStartImageMm.x() + (delta.x() / scale);
            double newTop = m_dragStartImageMm.y() + (delta.y() / scale);
            emit placementChanged(newLeft, newTop);
        }
        event->accept();
        return;
    }

    QRectF imgRect = calculateImageRectOnWidget();
    if (imgRect.contains(event->position())) {
        setCursor(Qt::OpenHandCursor);
    } else {
        unsetCursor();
    }
    QWidget::mouseMoveEvent(event);
}

void PrintPreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isDragging && event->button() == Qt::LeftButton) {
        m_isDragging = false;
        QRectF imgRect = calculateImageRectOnWidget();
        if (imgRect.contains(event->position())) {
            setCursor(Qt::OpenHandCursor);
        } else {
            unsetCursor();
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

// ============================================================================
// PrintDialog
// ============================================================================

PrintDialog::PrintDialog(Document* doc, QWidget* parent)
    : QDialog(parent), m_doc(doc) {
    setWindowTitle("Print");
    resize(920, 620);
    setMinimumSize(780, 540);

    if (m_doc) {
        m_compositeImage = m_doc->composite();
    }
    if (m_compositeImage.isNull()) {
        m_compositeImage = QImage(800, 600, QImage::Format_ARGB32_Premultiplied);
        m_compositeImage.fill(Qt::white);
    }

    m_docAspectRatio = (m_compositeImage.height() > 0)
        ? static_cast<double>(m_compositeImage.width()) / m_compositeImage.height()
        : 1.0;

    // Initialize dimensions from document DPI if available
    double dpi = (m_doc && m_doc->dpi() > 0.0) ? m_doc->dpi() : 96.0;
    double rawWidthMm = (m_compositeImage.width() / dpi) * 25.4;
    double rawHeightMm = (m_compositeImage.height() / dpi) * 25.4;

    // Default base page size: A4 (210 x 297 mm)
    m_basePageWidthMm = 210.0;
    m_basePageHeightMm = 297.0;
    recalculateEffectivePageSize();

    // Scale initial image size nicely to fit page comfortably if it is overly huge or tiny
    if (rawWidthMm > m_effectivePageWidthMm - 20.0 || rawHeightMm > m_effectivePageHeightMm - 20.0) {
        double maxW = std::max(20.0, m_effectivePageWidthMm - 40.0);
        double maxH = std::max(20.0, m_effectivePageHeightMm - 40.0);
        if (maxW / maxH > m_docAspectRatio) {
            m_imageHeightMm = maxH;
            m_imageWidthMm = m_imageHeightMm * m_docAspectRatio;
        } else {
            m_imageWidthMm = maxW;
            m_imageHeightMm = m_imageWidthMm / m_docAspectRatio;
        }
    } else if (rawWidthMm < 20.0 || rawHeightMm < 20.0) {
        m_imageWidthMm = std::min(m_effectivePageWidthMm - 40.0, 150.0);
        m_imageHeightMm = m_imageWidthMm / m_docAspectRatio;
    } else {
        m_imageWidthMm = rawWidthMm;
        m_imageHeightMm = rawHeightMm;
    }

    m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
    m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;

    setupUi();
    populatePrinters();
    syncUiFromValues();
    updatePreview();
}

double PrintDialog::toMm(double val, PrintUnit unit) {
    switch (unit) {
        case PrintUnit::Millimeters: return val;
        case PrintUnit::Centimeters: return val * 10.0;
        case PrintUnit::Inches: return val * 25.4;
    }
    return val;
}

double PrintDialog::fromMm(double mm, PrintUnit unit) {
    switch (unit) {
        case PrintUnit::Millimeters: return mm;
        case PrintUnit::Centimeters: return mm / 10.0;
        case PrintUnit::Inches: return mm / 25.4;
    }
    return mm;
}

QString PrintDialog::unitSuffix(PrintUnit unit) {
    switch (unit) {
        case PrintUnit::Millimeters: return " mm";
        case PrintUnit::Centimeters: return " cm";
        case PrintUnit::Inches: return " in";
    }
    return "";
}

int PrintDialog::unitDecimals(PrintUnit unit) {
    switch (unit) {
        case PrintUnit::Millimeters: return 1;
        case PrintUnit::Centimeters: return 2;
        case PrintUnit::Inches: return 2;
    }
    return 2;
}

double PrintDialog::unitSingleStep(PrintUnit unit) {
    switch (unit) {
        case PrintUnit::Millimeters: return 1.0;
        case PrintUnit::Centimeters: return 0.1;
        case PrintUnit::Inches: return 0.05;
    }
    return 0.1;
}

void PrintDialog::setUnit(PrintUnit unit) {
    if (m_unit == unit) return;
    m_unit = unit;
    if (m_unitCombo) {
        m_unitCombo->blockSignals(true);
        m_unitCombo->setCurrentIndex(static_cast<int>(m_unit));
        m_unitCombo->blockSignals(false);
    }
    bool wasUpdating = m_updating;
    m_updating = true;
    updateSpinboxBoundsAndDecimals();
    m_updating = wasUpdating;
    syncUiFromValues();
}

void PrintDialog::setImageDimensionsMm(double w, double h) {
    m_imageWidthMm = std::max(1.0, w);
    m_imageHeightMm = std::max(1.0, h);
    if (m_centerOnPage) {
        m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
        m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
    }
    syncUiFromValues();
    updatePreview();
}

void PrintDialog::setMarginsMm(double left, double top) {
    m_marginLeftMm = left;
    m_marginTopMm = top;
    m_centerOnPage = false;
    if (m_centerCheck) m_centerCheck->setChecked(false);
    syncUiFromValues();
    updatePreview();
}

void PrintDialog::setCenterOnPage(bool center) {
    m_centerOnPage = center;
    if (m_centerCheck) m_centerCheck->setChecked(center);
    if (m_centerOnPage) {
        m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
        m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
    }
    syncUiFromValues();
    updatePreview();
}

void PrintDialog::setMaintainAspectRatio(bool maintain) {
    m_maintainAspect = maintain;
    if (m_maintainAspectCheck) m_maintainAspectCheck->setChecked(maintain);
}

QPageLayout::Orientation PrintDialog::pageOrientation() const {
    return (m_landscapeRadio && m_landscapeRadio->isChecked())
        ? QPageLayout::Landscape
        : QPageLayout::Portrait;
}

void PrintDialog::setPageOrientation(QPageLayout::Orientation orient) {
    if (orient == QPageLayout::Landscape) {
        if (m_landscapeRadio) m_landscapeRadio->setChecked(true);
    } else {
        if (m_portraitRadio) m_portraitRadio->setChecked(true);
    }
    onOrientationChanged();
}

QPageSize PrintDialog::currentPageSize() const {
    if (!m_pageSizeCombo) return QPageSize(QPageSize::A4);

    int customIndex = m_pageSizeCombo->findData(QVariant::fromValue(static_cast<int>(QPageSize::Custom)));
    if (m_pageSizeCombo->currentIndex() == customIndex) {
        return QPageSize(QSizeF(m_basePageWidthMm, m_basePageHeightMm), QPageSize::Millimeter, "Custom");
    }

    QVariant idVar = m_pageSizeCombo->currentData();
    if (idVar.isValid()) {
        auto pid = static_cast<QPageSize::PageSizeId>(idVar.toInt());
        return QPageSize(pid);
    }
    return QPageSize(QPageSize::A4);
}

void PrintDialog::setupUi() {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    // --- Left side: Preview & Quick presets ---
    QVBoxLayout* previewCol = new QVBoxLayout();
    m_previewWidget = new PrintPreviewWidget(this);
    m_previewWidget->setImage(m_compositeImage);
    connect(m_previewWidget, &PrintPreviewWidget::placementChanged,
            this, &PrintDialog::onPreviewPlacementChanged);
    previewCol->addWidget(m_previewWidget, 1);

    QHBoxLayout* previewBtnLayout = new QHBoxLayout();
    m_fitPageBtn = new QPushButton("Fit to Page", this);
    m_origSizeBtn = new QPushButton("Original Size", this);
    m_fillPageBtn = new QPushButton("Fill Page", this);

    connect(m_fitPageBtn, &QPushButton::clicked, this, &PrintDialog::onFitToPage);
    connect(m_origSizeBtn, &QPushButton::clicked, this, &PrintDialog::onOriginalSize);
    connect(m_fillPageBtn, &QPushButton::clicked, this, &PrintDialog::onFillPage);

    previewBtnLayout->addWidget(m_fitPageBtn);
    previewBtnLayout->addWidget(m_origSizeBtn);
    previewBtnLayout->addWidget(m_fillPageBtn);
    previewCol->addLayout(previewBtnLayout);

    mainLayout->addLayout(previewCol, 3);

    // --- Right side: Controls ---
    QVBoxLayout* controlsCol = new QVBoxLayout();
    controlsCol->setSpacing(10);

    // 1. Printer & Paper Settings
    QGroupBox* paperGroup = new QGroupBox("Printer & Paper", this);
    QVBoxLayout* paperLayout = new QVBoxLayout(paperGroup);
    QFormLayout* paperForm = new QFormLayout();

    m_printerCombo = new QComboBox(this);
    connect(m_printerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintDialog::onPrinterChanged);
    paperForm->addRow("Printer:", m_printerCombo);

    m_pageSizeCombo = new QComboBox(this);
    connect(m_pageSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintDialog::onPageSizeChanged);
    paperForm->addRow("Page Size:", m_pageSizeCombo);
    paperLayout->addLayout(paperForm);

    // Custom page size widgets (hidden unless Custom selected)
    m_customPageWidget = new QWidget(this);
    QHBoxLayout* customSizeLayout = new QHBoxLayout(m_customPageWidget);
    customSizeLayout->setContentsMargins(0, 0, 0, 0);
    customSizeLayout->addWidget(new QLabel("W:", m_customPageWidget));
    m_customPageWidthSpin = new QDoubleSpinBox(m_customPageWidget);
    m_customPageWidthSpin->setRange(10.0, 5000.0);
    m_customPageWidthSpin->setValue(m_basePageWidthMm);
    connect(m_customPageWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PrintDialog::onCustomPageSizeEdited);
    customSizeLayout->addWidget(m_customPageWidthSpin);

    customSizeLayout->addWidget(new QLabel("H:", m_customPageWidget));
    m_customPageHeightSpin = new QDoubleSpinBox(m_customPageWidget);
    m_customPageHeightSpin->setRange(10.0, 5000.0);
    m_customPageHeightSpin->setValue(m_basePageHeightMm);
    connect(m_customPageHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PrintDialog::onCustomPageSizeEdited);
    customSizeLayout->addWidget(m_customPageHeightSpin);
    m_customPageWidget->setVisible(false);
    paperLayout->addWidget(m_customPageWidget);

    // Orientation
    QHBoxLayout* orientLayout = new QHBoxLayout();
    orientLayout->addWidget(new QLabel("Orientation:", this));
    m_portraitRadio = new QRadioButton("Portrait", this);
    m_landscapeRadio = new QRadioButton("Landscape", this);
    m_portraitRadio->setChecked(true);
    connect(m_portraitRadio, &QRadioButton::toggled, this, &PrintDialog::onOrientationChanged);
    connect(m_landscapeRadio, &QRadioButton::toggled, this, &PrintDialog::onOrientationChanged);
    orientLayout->addWidget(m_portraitRadio);
    orientLayout->addWidget(m_landscapeRadio);
    orientLayout->addStretch();
    paperLayout->addLayout(orientLayout);

    controlsCol->addWidget(paperGroup);

    // 2. Image Size Settings
    QGroupBox* sizeGroup = new QGroupBox("Image Size", this);
    QFormLayout* sizeForm = new QFormLayout(sizeGroup);

    m_unitCombo = new QComboBox(this);
    m_unitCombo->addItem("Centimeters (cm)", static_cast<int>(PrintUnit::Centimeters));
    m_unitCombo->addItem("Millimeters (mm)", static_cast<int>(PrintUnit::Millimeters));
    m_unitCombo->addItem("Inches (in)", static_cast<int>(PrintUnit::Inches));
    m_unitCombo->setCurrentIndex(0);
    connect(m_unitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintDialog::onUnitChanged);
    sizeForm->addRow("Units:", m_unitCombo);

    m_widthSpin = new QDoubleSpinBox(this);
    m_widthSpin->setRange(0.1, 10000.0);
    connect(m_widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PrintDialog::onWidthEdited);
    sizeForm->addRow("Width:", m_widthSpin);

    m_heightSpin = new QDoubleSpinBox(this);
    m_heightSpin->setRange(0.1, 10000.0);
    connect(m_heightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PrintDialog::onHeightEdited);
    sizeForm->addRow("Height:", m_heightSpin);

    m_maintainAspectCheck = new QCheckBox("Maintain aspect ratio", this);
    m_maintainAspectCheck->setChecked(true);
    connect(m_maintainAspectCheck, &QCheckBox::toggled,
            this, &PrintDialog::onMaintainAspectToggled);
    sizeForm->addRow("", m_maintainAspectCheck);

    controlsCol->addWidget(sizeGroup);

    // 3. Placement & Margins Settings
    QGroupBox* posGroup = new QGroupBox("Placement & Margins", this);
    QVBoxLayout* posLayout = new QVBoxLayout(posGroup);

    m_centerCheck = new QCheckBox("Center on page", this);
    m_centerCheck->setChecked(true);
    connect(m_centerCheck, &QCheckBox::toggled, this, &PrintDialog::onCenterToggled);
    posLayout->addWidget(m_centerCheck);

    // Alignment button grid (3x3)
    QHBoxLayout* alignContainer = new QHBoxLayout();
    alignContainer->addWidget(new QLabel("Align:", this));

    QWidget* gridWidget = new QWidget(this);
    QGridLayout* grid = new QGridLayout(gridWidget);
    grid->setSpacing(2);
    grid->setContentsMargins(0, 0, 0, 0);

    m_alignButtonGroup = new QButtonGroup(this);
    const char* labels[3][3] = {
        {"↖", "↑", "↗"},
        {"←", "•", "→"},
        {"↙", "↓", "↘"}
    };
    int idCounter = 0;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            QPushButton* btn = new QPushButton(labels[r][c], gridWidget);
            btn->setFixedSize(26, 26);
            grid->addWidget(btn, r, c);
            m_alignButtonGroup->addButton(btn, idCounter++);
        }
    }
    connect(m_alignButtonGroup, &QButtonGroup::idClicked,
            this, &PrintDialog::onAlignmentButtonClicked);

    alignContainer->addWidget(gridWidget);
    alignContainer->addStretch();
    posLayout->addLayout(alignContainer);

    // Margin Spinboxes
    QGridLayout* marginGrid = new QGridLayout();
    marginGrid->addWidget(new QLabel("Left:"), 0, 0);
    m_marginLeftSpin = new QDoubleSpinBox(this);
    m_marginLeftSpin->setRange(-1000.0, 5000.0);
    connect(m_marginLeftSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PrintDialog::onMarginLeftEdited);
    marginGrid->addWidget(m_marginLeftSpin, 0, 1);

    marginGrid->addWidget(new QLabel("Top:"), 0, 2);
    m_marginTopSpin = new QDoubleSpinBox(this);
    m_marginTopSpin->setRange(-1000.0, 5000.0);
    connect(m_marginTopSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PrintDialog::onMarginTopEdited);
    marginGrid->addWidget(m_marginTopSpin, 0, 3);

    marginGrid->addWidget(new QLabel("Right:"), 1, 0);
    m_marginRightSpin = new QDoubleSpinBox(this);
    m_marginRightSpin->setReadOnly(true);
    m_marginRightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_marginRightSpin->setRange(-10000.0, 10000.0);
    marginGrid->addWidget(m_marginRightSpin, 1, 1);

    marginGrid->addWidget(new QLabel("Bottom:"), 1, 2);
    m_marginBottomSpin = new QDoubleSpinBox(this);
    m_marginBottomSpin->setReadOnly(true);
    m_marginBottomSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_marginBottomSpin->setRange(-10000.0, 10000.0);
    marginGrid->addWidget(m_marginBottomSpin, 1, 3);

    posLayout->addLayout(marginGrid);

    // Warning label for clipping
    m_warningLabel = new QLabel(this);
    m_warningLabel->setStyleSheet("color: #d9383a; font-weight: bold; font-size: 11px;");
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setVisible(false);
    posLayout->addWidget(m_warningLabel);

    controlsCol->addWidget(posGroup);

    controlsCol->addStretch();

    // Dialog buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_printBtn = new QPushButton("Print...", this);
    m_printBtn->setDefault(true);
    connect(m_printBtn, &QPushButton::clicked, this, &PrintDialog::onPrintClicked);
    btnLayout->addWidget(m_printBtn);

    m_cancelBtn = new QPushButton("Cancel", this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);

    controlsCol->addLayout(btnLayout);

    mainLayout->addLayout(controlsCol, 2);

    updateSpinboxBoundsAndDecimals();
}

void PrintDialog::populatePrinters() {
    m_printerCombo->clear();

    QStringList names = QPrinterInfo::availablePrinterNames();
    QString defaultName = QPrinterInfo::defaultPrinterName();

    for (const QString& name : names) {
        QString label = (name == defaultName) ? QString("%1 (Default)").arg(name) : name;
        m_printerCombo->addItem(label, name);
    }

    // Always provide Print to PDF option
    m_printerCombo->addItem("[Print to PDF]", "PDF");

    // Select default printer if available, otherwise PDF
    int defIdx = m_printerCombo->findData(defaultName);
    if (defIdx >= 0) {
        m_printerCombo->setCurrentIndex(defIdx);
    } else {
        m_printerCombo->setCurrentIndex(m_printerCombo->count() - 1);
    }
}

void PrintDialog::populatePageSizesForPrinter(const QPrinterInfo& info) {
    m_pageSizeCombo->blockSignals(true);
    m_pageSizeCombo->clear();

    QList<QPageSize> sizes;
    if (!info.isNull()) {
        sizes = info.supportedPageSizes();
    }

    // Fallback standard page sizes if printer has no list or PDF is selected
    if (sizes.isEmpty()) {
        sizes = {
            QPageSize(QPageSize::A4),
            QPageSize(QPageSize::Letter),
            QPageSize(QPageSize::Legal),
            QPageSize(QPageSize::A3),
            QPageSize(QPageSize::A5),
            QPageSize(QPageSize::B5),
            QPageSize(QPageSize::Tabloid)
        };
    }

    int selectedIdx = -1;
    QPageSize defPage = (!info.isNull()) ? info.defaultPageSize() : QPageSize(QPageSize::A4);

    for (int i = 0; i < sizes.size(); ++i) {
        const auto& s = sizes[i];
        QSizeF mmSize = s.size(QPageSize::Millimeter);
        QString text = QString("%1 (%2 x %3 mm)")
            .arg(s.name())
            .arg(QString::number(mmSize.width(), 'f', 0))
            .arg(QString::number(mmSize.height(), 'f', 0));
        m_pageSizeCombo->addItem(text, static_cast<int>(s.id()));

        if (defPage.isValid() && s.isEquivalentTo(defPage)) {
            selectedIdx = i;
        }
    }

    // Add Custom page size option
    m_pageSizeCombo->addItem("Custom...", static_cast<int>(QPageSize::Custom));

    if (selectedIdx >= 0) {
        m_pageSizeCombo->setCurrentIndex(selectedIdx);
    } else {
        m_pageSizeCombo->setCurrentIndex(0);
    }

    m_pageSizeCombo->blockSignals(false);
}

void PrintDialog::recalculateEffectivePageSize() {
    if (pageOrientation() == QPageLayout::Landscape) {
        m_effectivePageWidthMm = std::max(m_basePageWidthMm, m_basePageHeightMm);
        m_effectivePageHeightMm = std::min(m_basePageWidthMm, m_basePageHeightMm);
    } else {
        m_effectivePageWidthMm = std::min(m_basePageWidthMm, m_basePageHeightMm);
        m_effectivePageHeightMm = std::max(m_basePageWidthMm, m_basePageHeightMm);
    }
}

void PrintDialog::onPrinterChanged(int /*index*/) {
    if (m_updating) return;
    QString pName = m_printerCombo->currentData().toString();
    if (pName == "PDF") {
        populatePageSizesForPrinter(QPrinterInfo());
    } else {
        QPrinterInfo info = QPrinterInfo::printerInfo(pName);
        populatePageSizesForPrinter(info);
    }
    onPageSizeChanged(m_pageSizeCombo->currentIndex());
}

void PrintDialog::onPageSizeChanged(int index) {
    if (m_updating || index < 0) return;

    int customId = static_cast<int>(QPageSize::Custom);
    if (m_pageSizeCombo->itemData(index).toInt() == customId) {
        m_customPageWidget->setVisible(true);
        m_basePageWidthMm = toMm(m_customPageWidthSpin->value(), m_unit);
        m_basePageHeightMm = toMm(m_customPageHeightSpin->value(), m_unit);
    } else {
        m_customPageWidget->setVisible(false);
        auto pid = static_cast<QPageSize::PageSizeId>(m_pageSizeCombo->itemData(index).toInt());
        QPageSize ps(pid);
        QSizeF sz = ps.size(QPageSize::Millimeter);
        m_basePageWidthMm = sz.width();
        m_basePageHeightMm = sz.height();
    }

    recalculateEffectivePageSize();

    if (m_centerOnPage) {
        m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
        m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
    }

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onCustomPageSizeEdited() {
    if (m_updating) return;
    m_basePageWidthMm = toMm(m_customPageWidthSpin->value(), m_unit);
    m_basePageHeightMm = toMm(m_customPageHeightSpin->value(), m_unit);
    recalculateEffectivePageSize();

    if (m_centerOnPage) {
        m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
        m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
    }

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onOrientationChanged() {
    if (m_updating) return;
    recalculateEffectivePageSize();

    if (m_centerOnPage) {
        m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
        m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
    }

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onUnitChanged(int index) {
    if (index < 0) return;
    m_unit = static_cast<PrintUnit>(m_unitCombo->itemData(index).toInt());
    bool wasUpdating = m_updating;
    m_updating = true;
    updateSpinboxBoundsAndDecimals();
    m_updating = wasUpdating;
    syncUiFromValues();
}

void PrintDialog::updateSpinboxBoundsAndDecimals() {
    int dec = unitDecimals(m_unit);
    double step = unitSingleStep(m_unit);
    QString sfx = unitSuffix(m_unit);

    auto configureSpinbox = [dec, step, sfx](QDoubleSpinBox* sb) {
        if (!sb) return;
        const bool blocked = sb->blockSignals(true);
        sb->setDecimals(dec);
        sb->setSingleStep(step);
        sb->setSuffix(sfx);
        sb->blockSignals(blocked);
    };

    configureSpinbox(m_widthSpin);
    configureSpinbox(m_heightSpin);
    configureSpinbox(m_marginLeftSpin);
    configureSpinbox(m_marginTopSpin);
    configureSpinbox(m_marginRightSpin);
    configureSpinbox(m_marginBottomSpin);
    configureSpinbox(m_customPageWidthSpin);
    configureSpinbox(m_customPageHeightSpin);
}

void PrintDialog::syncUiFromValues() {
    if (m_updating) return;
    m_updating = true;

    if (m_widthSpin) m_widthSpin->setValue(fromMm(m_imageWidthMm, m_unit));
    if (m_heightSpin) m_heightSpin->setValue(fromMm(m_imageHeightMm, m_unit));

    if (m_marginLeftSpin) m_marginLeftSpin->setValue(fromMm(m_marginLeftMm, m_unit));
    if (m_marginTopSpin) m_marginTopSpin->setValue(fromMm(m_marginTopMm, m_unit));

    double rightMm = m_effectivePageWidthMm - m_imageWidthMm - m_marginLeftMm;
    double bottomMm = m_effectivePageHeightMm - m_imageHeightMm - m_marginTopMm;

    if (m_marginRightSpin) m_marginRightSpin->setValue(fromMm(rightMm, m_unit));
    if (m_marginBottomSpin) m_marginBottomSpin->setValue(fromMm(bottomMm, m_unit));

    if (m_customPageWidthSpin) m_customPageWidthSpin->setValue(fromMm(m_basePageWidthMm, m_unit));
    if (m_customPageHeightSpin) m_customPageHeightSpin->setValue(fromMm(m_basePageHeightMm, m_unit));

    if (m_centerCheck) m_centerCheck->setChecked(m_centerOnPage);
    if (m_maintainAspectCheck) m_maintainAspectCheck->setChecked(m_maintainAspect);

    m_updating = false;

    checkBoundariesAndWarn();
}

void PrintDialog::updatePreview() {
    if (!m_previewWidget) return;
    m_previewWidget->setPageDimensions(m_effectivePageWidthMm, m_effectivePageHeightMm);
    m_previewWidget->setImagePlacement(m_marginLeftMm, m_marginTopMm, m_imageWidthMm, m_imageHeightMm);
    m_previewWidget->setPrintMargins(5.0, 5.0, 5.0, 5.0);
}

void PrintDialog::checkBoundariesAndWarn() {
    bool overflowX = (m_marginLeftMm < 0.0) || (m_marginLeftMm + m_imageWidthMm > m_effectivePageWidthMm + 0.01);
    bool overflowY = (m_marginTopMm < 0.0) || (m_marginTopMm + m_imageHeightMm > m_effectivePageHeightMm + 0.01);

    if (overflowX || overflowY) {
        if (m_warningLabel) {
            m_warningLabel->setText("⚠ Warning: Image extends beyond the page edge and will be clipped.");
            m_warningLabel->setVisible(true);
        }
    } else {
        if (m_warningLabel) {
            m_warningLabel->setVisible(false);
        }
    }
}

void PrintDialog::onWidthEdited(double val) {
    if (m_updating) return;
    double newWMm = toMm(val, m_unit);
    if (newWMm <= 0.0) return;

    m_imageWidthMm = newWMm;
    if (m_maintainAspect && m_docAspectRatio > 0.0) {
        m_imageHeightMm = m_imageWidthMm / m_docAspectRatio;
    }

    if (m_centerOnPage) {
        m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
        m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
    }

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onHeightEdited(double val) {
    if (m_updating) return;
    double newHMm = toMm(val, m_unit);
    if (newHMm <= 0.0) return;

    m_imageHeightMm = newHMm;
    if (m_maintainAspect && m_docAspectRatio > 0.0) {
        m_imageWidthMm = m_imageHeightMm * m_docAspectRatio;
    }

    if (m_centerOnPage) {
        m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
        m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
    }

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onMaintainAspectToggled(bool checked) {
    m_maintainAspect = checked;
    if (m_maintainAspect && m_docAspectRatio > 0.0) {
        m_imageHeightMm = m_imageWidthMm / m_docAspectRatio;
        if (m_centerOnPage) {
            m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
        }
        syncUiFromValues();
        updatePreview();
    }
}

void PrintDialog::onMarginLeftEdited(double val) {
    if (m_updating) return;
    m_marginLeftMm = toMm(val, m_unit);
    m_centerOnPage = false;
    if (m_centerCheck) m_centerCheck->setChecked(false);

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onMarginTopEdited(double val) {
    if (m_updating) return;
    m_marginTopMm = toMm(val, m_unit);
    m_centerOnPage = false;
    if (m_centerCheck) m_centerCheck->setChecked(false);

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onCenterToggled(bool checked) {
    m_centerOnPage = checked;
    if (m_centerOnPage) {
        m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
        m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
    }
    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onFitToPage() {
    double marginMm = 10.0;
    double maxW = std::max(10.0, m_effectivePageWidthMm - 2.0 * marginMm);
    double maxH = std::max(10.0, m_effectivePageHeightMm - 2.0 * marginMm);

    if (maxW / maxH > m_docAspectRatio) {
        m_imageHeightMm = maxH;
        m_imageWidthMm = m_imageHeightMm * m_docAspectRatio;
    } else {
        m_imageWidthMm = maxW;
        m_imageHeightMm = m_imageWidthMm / m_docAspectRatio;
    }

    m_centerOnPage = true;
    m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
    m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onOriginalSize() {
    double dpi = (m_doc && m_doc->dpi() > 0.0) ? m_doc->dpi() : 96.0;
    m_imageWidthMm = (m_compositeImage.width() / dpi) * 25.4;
    m_imageHeightMm = (m_compositeImage.height() / dpi) * 25.4;

    if (m_centerOnPage) {
        m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
        m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
    }

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onFillPage() {
    m_maintainAspect = false;
    if (m_maintainAspectCheck) m_maintainAspectCheck->setChecked(false);

    m_imageWidthMm = m_effectivePageWidthMm;
    m_imageHeightMm = m_effectivePageHeightMm;
    m_marginLeftMm = 0.0;
    m_marginTopMm = 0.0;
    m_centerOnPage = true;

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onAlignmentButtonClicked(int id) {
    // id mapping:
    // 0: TL, 1: TC, 2: TR
    // 3: ML, 4: MC, 5: MR
    // 6: BL, 7: BC, 8: BR
    int r = id / 3;
    int c = id % 3;

    double marginMm = 10.0;

    // Horizontal
    if (c == 0) {
        m_marginLeftMm = marginMm;
    } else if (c == 1) {
        m_marginLeftMm = (m_effectivePageWidthMm - m_imageWidthMm) / 2.0;
    } else {
        m_marginLeftMm = m_effectivePageWidthMm - m_imageWidthMm - marginMm;
    }

    // Vertical
    if (r == 0) {
        m_marginTopMm = marginMm;
    } else if (r == 1) {
        m_marginTopMm = (m_effectivePageHeightMm - m_imageHeightMm) / 2.0;
    } else {
        m_marginTopMm = m_effectivePageHeightMm - m_imageHeightMm - marginMm;
    }

    m_centerOnPage = (r == 1 && c == 1);
    if (m_centerCheck) m_centerCheck->setChecked(m_centerOnPage);

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onPreviewPlacementChanged(double leftMm, double topMm) {
    m_marginLeftMm = leftMm;
    m_marginTopMm = topMm;
    m_centerOnPage = false;
    if (m_centerCheck) m_centerCheck->setChecked(false);

    syncUiFromValues();
    updatePreview();
}

void PrintDialog::onPrintClicked() {
    QString pData = m_printerCombo->currentData().toString();
    if (pData == "PDF") {
        QString defaultFile = (m_doc && !m_doc->fileName().isEmpty())
            ? m_doc->fileName() + ".pdf"
            : "PrintOutput.pdf";

        QString savePath = QFileDialog::getSaveFileName(
            this, "Print to PDF", defaultFile, "PDF Files (*.pdf)");
        if (savePath.isEmpty()) return;

        if (printToPdf(savePath)) {
            accept();
        } else {
            QMessageBox::critical(this, "Print Error", "Failed to export PDF file.");
        }
    } else {
        QPrinterInfo pInfo = QPrinterInfo::printerInfo(pData);
        QPrinter printer(pInfo, QPrinter::HighResolution);
        if (executePrint(printer)) {
            accept();
        } else {
            QMessageBox::critical(this, "Print Error", "Failed to print document.");
        }
    }
}

bool PrintDialog::printToPdf(const QString& filePath) {
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    return executePrint(printer);
}

bool PrintDialog::executePrint(QPrinter& printer) {
    QPageLayout layout;
    layout.setPageSize(currentPageSize());
    layout.setOrientation(pageOrientation());
    layout.setUnits(QPageLayout::Millimeter);
    layout.setMargins(QMarginsF(0, 0, 0, 0));
    printer.setPageLayout(layout);
    printer.setFullPage(true);

    QPainter painter(&printer);
    if (!painter.isActive()) {
        return false;
    }

    double dpi = printer.resolution();
    double dotsPerMm = dpi / 25.4;

    QRectF targetRect(
        m_marginLeftMm * dotsPerMm,
        m_marginTopMm * dotsPerMm,
        m_imageWidthMm * dotsPerMm,
        m_imageHeightMm * dotsPerMm
    );

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(targetRect, m_compositeImage);
    painter.end();

    return true;
}

} // namespace pdn
