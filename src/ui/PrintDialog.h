#pragma once

#include <QDialog>
#include <QWidget>
#include <QImage>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QPageSize>
#include <QPageLayout>
#include <QPrinter>
#include <QPrinterInfo>
#include <memory>
#include "../core/Document.h"

namespace pdn {

enum class PrintUnit {
    Millimeters,
    Centimeters,
    Inches
};

class PrintPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit PrintPreviewWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setPageDimensions(double widthMm, double heightMm);
    void setImagePlacement(double leftMm, double topMm, double widthMm, double heightMm);
    void setPrintMargins(double leftMm, double topMm, double rightMm, double bottomMm);

signals:
    void placementChanged(double leftMm, double topMm);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRectF calculatePaperRect() const;
    QRectF calculateImageRectOnWidget() const;

    QImage m_image;
    double m_pageWidthMm = 210.0;
    double m_pageHeightMm = 297.0;
    double m_leftMm = 10.0;
    double m_topMm = 10.0;
    double m_widthMm = 190.0;
    double m_heightMm = 142.5;

    double m_marginGuideLeftMm = 5.0;
    double m_marginGuideTopMm = 5.0;
    double m_marginGuideRightMm = 5.0;
    double m_marginGuideBottomMm = 5.0;

    bool m_isDragging = false;
    QPointF m_dragStartMousePos;
    QPointF m_dragStartImageMm;
};

class PrintDialog : public QDialog {
    Q_OBJECT
public:
    explicit PrintDialog(Document* doc, QWidget* parent = nullptr);
    ~PrintDialog() override = default;

    static double toMm(double val, PrintUnit unit);
    static double fromMm(double mm, PrintUnit unit);
    static QString unitSuffix(PrintUnit unit);
    static int unitDecimals(PrintUnit unit);
    static double unitSingleStep(PrintUnit unit);

    // Public getters for automated testing
    PrintUnit currentUnit() const { return m_unit; }
    void setUnit(PrintUnit unit);

    double imageWidthMm() const { return m_imageWidthMm; }
    double imageHeightMm() const { return m_imageHeightMm; }
    void setImageDimensionsMm(double w, double h);

    double marginLeftMm() const { return m_marginLeftMm; }
    double marginTopMm() const { return m_marginTopMm; }
    void setMarginsMm(double left, double top);

    bool isCenterOnPage() const { return m_centerOnPage; }
    void setCenterOnPage(bool center);

    bool maintainAspectRatio() const { return m_maintainAspect; }
    void setMaintainAspectRatio(bool maintain);

    QPageLayout::Orientation pageOrientation() const;
    void setPageOrientation(QPageLayout::Orientation orient);

    QPageSize currentPageSize() const;
    double pageWidthMm() const { return m_effectivePageWidthMm; }
    double pageHeightMm() const { return m_effectivePageHeightMm; }

    bool printToPdf(const QString& filePath);
    bool executePrint(QPrinter& printer);

private slots:
    void onPrinterChanged(int index);
    void onPageSizeChanged(int index);
    void onCustomPageSizeEdited();
    void onOrientationChanged();
    void onUnitChanged(int index);

    void onWidthEdited(double val);
    void onHeightEdited(double val);
    void onMaintainAspectToggled(bool checked);

    void onMarginLeftEdited(double val);
    void onMarginTopEdited(double val);
    void onCenterToggled(bool checked);

    void onFitToPage();
    void onOriginalSize();
    void onFillPage();

    void onAlignmentButtonClicked(int id);
    void onPreviewPlacementChanged(double leftMm, double topMm);
    void onPrintClicked();

private:
    void setupUi();
    void populatePrinters();
    void populatePageSizesForPrinter(const QPrinterInfo& info);
    void recalculateEffectivePageSize();
    void updateSpinboxBoundsAndDecimals();
    void syncUiFromValues();
    void updatePreview();
    void checkBoundariesAndWarn();

    Document* m_doc;
    QImage m_compositeImage;
    double m_docAspectRatio = 1.0;

    PrintUnit m_unit = PrintUnit::Centimeters;
    bool m_updating = false;
    bool m_maintainAspect = true;
    bool m_centerOnPage = true;

    // Physical values in mm
    double m_basePageWidthMm = 210.0;
    double m_basePageHeightMm = 297.0;
    double m_effectivePageWidthMm = 210.0;
    double m_effectivePageHeightMm = 297.0;

    double m_imageWidthMm = 150.0;
    double m_imageHeightMm = 100.0;
    double m_marginLeftMm = 30.0;
    double m_marginTopMm = 98.5;

    // UI Widgets
    PrintPreviewWidget* m_previewWidget = nullptr;

    // Printer & Paper
    QComboBox* m_printerCombo = nullptr;
    QComboBox* m_pageSizeCombo = nullptr;
    QWidget* m_customPageWidget = nullptr;
    QDoubleSpinBox* m_customPageWidthSpin = nullptr;
    QDoubleSpinBox* m_customPageHeightSpin = nullptr;
    QRadioButton* m_portraitRadio = nullptr;
    QRadioButton* m_landscapeRadio = nullptr;

    // Image Size
    QComboBox* m_unitCombo = nullptr;
    QDoubleSpinBox* m_widthSpin = nullptr;
    QDoubleSpinBox* m_heightSpin = nullptr;
    QCheckBox* m_maintainAspectCheck = nullptr;
    QPushButton* m_fitPageBtn = nullptr;
    QPushButton* m_origSizeBtn = nullptr;
    QPushButton* m_fillPageBtn = nullptr;

    // Placement & Margins
    QCheckBox* m_centerCheck = nullptr;
    QDoubleSpinBox* m_marginLeftSpin = nullptr;
    QDoubleSpinBox* m_marginTopSpin = nullptr;
    QDoubleSpinBox* m_marginRightSpin = nullptr;
    QDoubleSpinBox* m_marginBottomSpin = nullptr;
    QButtonGroup* m_alignButtonGroup = nullptr;
    QLabel* m_warningLabel = nullptr;

    // Dialog buttons
    QPushButton* m_printBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};

} // namespace pdn
