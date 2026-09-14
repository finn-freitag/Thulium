#include <QMimeData>
#include "Dialogs.h"
#include "../core/History.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QRadioButton>
#include <QGridLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QDateTime>

namespace pdn {

// --- LayerPropertiesDialog ---
LayerPropertiesDialog::LayerPropertiesDialog(Document* doc, int layerIndex, QWidget* parent)
    : QDialog(parent), m_doc(doc), m_layerIndex(layerIndex) {
    setWindowTitle("Layer Properties");
    setFixedSize(320, 240);

    auto layer = doc->layer(layerIndex);
    m_origName = layer ? layer->name() : "";
    m_origVisible = layer ? layer->isVisible() : true;
    m_origOpacity = layer ? layer->opacity() : 255;
    m_origBlendMode = layer ? layer->blendMode() : BlendMode::Normal;

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();

    m_nameEdit = new QLineEdit(m_origName, this);
    form->addRow("Name:", m_nameEdit);

    m_visibleCheck = new QCheckBox("Visible", this);
    m_visibleCheck->setChecked(m_origVisible);
    form->addRow("", m_visibleCheck);

    m_blendModeCombo = new QComboBox(this);
    for (const auto& info : getAvailableBlendModes()) {
        m_blendModeCombo->addItem(info.name, static_cast<int>(info.mode));
    }
    m_blendModeCombo->setCurrentIndex(m_blendModeCombo->findData(static_cast<int>(m_origBlendMode)));
    form->addRow("Blending:", m_blendModeCombo);

    QHBoxLayout* opLayout = new QHBoxLayout();
    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(0, 255);
    m_opacitySlider->setValue(m_origOpacity);
    m_opacitySpin = new QSpinBox(this);
    m_opacitySpin->setRange(0, 255);
    m_opacitySpin->setValue(m_origOpacity);
    connect(m_opacitySlider, &QSlider::valueChanged, m_opacitySpin, &QSpinBox::setValue);
    connect(m_opacitySpin, QOverload<int>::of(&QSpinBox::valueChanged), m_opacitySlider, &QSlider::setValue);
    opLayout->addWidget(m_opacitySlider);
    opLayout->addWidget(m_opacitySpin);
    form->addRow("Opacity:", opLayout);

    mainLayout->addLayout(form);

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bbox, &QDialogButtonBox::accepted, this, [this]() {
        applyChanges();
        accept();
    });
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(bbox);
}

QString LayerPropertiesDialog::layerName() const { return m_nameEdit->text(); }
bool LayerPropertiesDialog::isLayerVisible() const { return m_visibleCheck->isChecked(); }
uint8_t LayerPropertiesDialog::opacity() const { return static_cast<uint8_t>(m_opacitySlider->value()); }
BlendMode LayerPropertiesDialog::blendMode() const {
    return static_cast<BlendMode>(m_blendModeCombo->currentData().toInt());
}

void LayerPropertiesDialog::applyChanges() {
    auto layer = m_doc->layer(m_layerIndex);
    if (!layer) return;

    QString newN = layerName();
    bool newV = isLayerVisible();
    uint8_t newO = opacity();
    BlendMode newB = blendMode();

    m_doc->undoStack()->push(new LayerPropertyUndoCommand(m_doc, m_layerIndex,
        m_origName, newN, m_origOpacity, newO, m_origBlendMode, newB, m_origVisible, newV));
}

// --- ResizeImageDialog ---
ResizeImageDialog::ResizeImageDialog(int currentWidth, int currentHeight, QWidget* parent)
    : QDialog(parent), m_origWidth(currentWidth), m_origHeight(currentHeight) {
    setWindowTitle("Resize");
    setFixedSize(320, 210);
    m_aspectRatio = static_cast<double>(m_origWidth) / m_origHeight;

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(1, 32768);
    m_widthSpin->setValue(m_origWidth);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(1, 32768);
    m_heightSpin->setValue(m_origHeight);

    form->addRow("Width (pixels):", m_widthSpin);
    form->addRow("Height (pixels):", m_heightSpin);

    m_maintainAspectCheck = new QCheckBox("Maintain aspect ratio", this);
    m_maintainAspectCheck->setChecked(true);
    form->addRow("", m_maintainAspectCheck);

    m_resampleCombo = new QComboBox(this);
    m_resampleCombo->addItem("Nearest Neighbor", static_cast<int>(ResampleAlgorithm::NearestNeighbor));
    m_resampleCombo->addItem("Bilinear", static_cast<int>(ResampleAlgorithm::Bilinear));
    m_resampleCombo->addItem("Bicubic", static_cast<int>(ResampleAlgorithm::Bicubic));
    m_resampleCombo->addItem("Lanczos", static_cast<int>(ResampleAlgorithm::Lanczos));
    m_resampleCombo->addItem("Super Sampling (Box)", static_cast<int>(ResampleAlgorithm::SuperSampling));
    m_resampleCombo->setCurrentIndex(2); // Bicubic by default
    form->addRow("Resampling:", m_resampleCombo);

    connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResizeImageDialog::onWidthChanged);
    connect(m_heightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResizeImageDialog::onHeightChanged);

    mainLayout->addLayout(form);

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(bbox);
}

int ResizeImageDialog::newWidth() const { return m_widthSpin->value(); }
int ResizeImageDialog::newHeight() const { return m_heightSpin->value(); }
ResampleAlgorithm ResizeImageDialog::algorithm() const {
    return static_cast<ResampleAlgorithm>(m_resampleCombo->currentData().toInt());
}

void ResizeImageDialog::onWidthChanged(int w) {
    if (m_updating || !m_maintainAspectCheck->isChecked()) return;
    m_updating = true;
    m_heightSpin->setValue(std::max(1, static_cast<int>(w / m_aspectRatio + 0.5)));
    m_updating = false;
}

void ResizeImageDialog::onHeightChanged(int h) {
    if (m_updating || !m_maintainAspectCheck->isChecked()) return;
    m_updating = true;
    m_widthSpin->setValue(std::max(1, static_cast<int>(h * m_aspectRatio + 0.5)));
    m_updating = false;
}

// --- CanvasSizeDialog ---
CanvasSizeDialog::CanvasSizeDialog(int currentWidth, int currentHeight, QWidget* parent)
    : QDialog(parent), m_origWidth(currentWidth), m_origHeight(currentHeight) {
    setWindowTitle("Canvas Size");
    setFixedSize(320, 240);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(1, 32768);
    m_widthSpin->setValue(m_origWidth);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(1, 32768);
    m_heightSpin->setValue(m_origHeight);

    form->addRow("Width (pixels):", m_widthSpin);
    form->addRow("Height (pixels):", m_heightSpin);

    mainLayout->addLayout(form);

    // Anchor Grid (3x3)
    QLabel* anchorLabel = new QLabel("Anchor:", this);
    mainLayout->addWidget(anchorLabel);

    QWidget* gridWidget = new QWidget(this);
    QGridLayout* gridLayout = new QGridLayout(gridWidget);
    gridLayout->setSpacing(2);
    m_anchorGroup = new QButtonGroup(this);

    const Qt::Alignment anchors[3][3] = {
        { Qt::AlignTop | Qt::AlignLeft, Qt::AlignTop | Qt::AlignHCenter, Qt::AlignTop | Qt::AlignRight },
        { Qt::AlignVCenter | Qt::AlignLeft, Qt::AlignCenter, Qt::AlignVCenter | Qt::AlignRight },
        { Qt::AlignBottom | Qt::AlignLeft, Qt::AlignBottom | Qt::AlignHCenter, Qt::AlignBottom | Qt::AlignRight }
    };

    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            QRadioButton* rb = new QRadioButton(gridWidget);
            if (r == 1 && c == 1) rb->setChecked(true); // Center default
            gridLayout->addWidget(rb, r, c);
            m_anchorGroup->addButton(rb, static_cast<int>(anchors[r][c]));
        }
    }
    mainLayout->addWidget(gridWidget);

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(bbox);
}

int CanvasSizeDialog::newWidth() const { return m_widthSpin->value(); }
int CanvasSizeDialog::newHeight() const { return m_heightSpin->value(); }
Qt::Alignment CanvasSizeDialog::anchor() const {
    return static_cast<Qt::Alignment>(m_anchorGroup->checkedId());
}

// --- NewImageDialog ---
NewImageDialog::NewImageDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("New");
    setFixedSize(400, 220);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* form = new QFormLayout();

    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem("Custom");
    m_presetCombo->addItem("800 x 600 (SVGA)", QSize(800, 600));
    m_presetCombo->addItem("1024 x 768 (XGA)", QSize(1024, 768));
    m_presetCombo->addItem("1920 x 1080 (Full HD)", QSize(1920, 1080));
    m_presetCombo->addItem("2560 x 1440 (2K QHD)", QSize(2560, 1440));
    m_presetCombo->addItem("3840 x 2160 (4K UHD)", QSize(3840, 2160));
    m_presetCombo->addItem("A4 Portrait (Print 300 DPI: 2480 x 3508)", QSize(2480, 3508));
    m_presetCombo->addItem("A4 Landscape (Print 300 DPI: 3508 x 2480)", QSize(3508, 2480));
    m_presetCombo->addItem("A4 Portrait (Screen 96 DPI: 794 x 1123)", QSize(794, 1123));
    m_presetCombo->addItem("A4 Landscape (Screen 96 DPI: 1123 x 794)", QSize(1123, 794));
    m_presetCombo->addItem("Letter Portrait (Print 300 DPI: 2550 x 3300)", QSize(2550, 3300));
    m_presetCombo->addItem("Letter Landscape (Print 300 DPI: 3300 x 2550)", QSize(3300, 2550));
    m_presetCombo->addItem("Letter Portrait (Screen 96 DPI: 816 x 1056)", QSize(816, 1056));
    m_presetCombo->addItem("Letter Landscape (Screen 96 DPI: 1056 x 816)", QSize(1056, 816));

    int initW = 800;
    int initH = 600;
    int selectIndex = 1; // Default to first preset if no clipboard

    // Check clipboard for image
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        QImage clipImg = clipboard->image();
        if (clipImg.isNull() && clipboard->mimeData() && clipboard->mimeData()->hasImage()) {
            clipImg = qvariant_cast<QImage>(clipboard->mimeData()->imageData());
        }
        if (!clipImg.isNull() && clipImg.width() > 0 && clipImg.height() > 0) {
            initW = clipImg.width();
            initH = clipImg.height();
            m_presetCombo->insertItem(1, QString("Clipboard (%1 x %2)").arg(initW).arg(initH), clipImg.size());
            selectIndex = 1;
        }
    }

    form->addRow("Preset:", m_presetCombo);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(1, 32768);
    m_widthSpin->setValue(initW);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(1, 32768);
    m_heightSpin->setValue(initH);

    form->addRow("Width (pixels):", m_widthSpin);
    form->addRow("Height (pixels):", m_heightSpin);

    m_whiteRadio = new QRadioButton("White", this);
    m_transparentRadio = new QRadioButton("Transparent", this);
    m_whiteRadio->setChecked(true);
    QHBoxLayout* bgLayout = new QHBoxLayout();
    bgLayout->addWidget(m_whiteRadio);
    bgLayout->addWidget(m_transparentRadio);
    bgLayout->addStretch();
    form->addRow("Background:", bgLayout);

    m_presetCombo->setCurrentIndex(selectIndex);

    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewImageDialog::onPresetChanged);

    auto onSpinChanged = [this]() {
        QVariant data = m_presetCombo->itemData(m_presetCombo->currentIndex());
        if (data.isValid()) {
            QSize sz = data.toSize();
            if (m_widthSpin->value() != sz.width() || m_heightSpin->value() != sz.height()) {
                m_presetCombo->blockSignals(true);
                m_presetCombo->setCurrentIndex(0); // Custom
                m_presetCombo->blockSignals(false);
            }
        }
    };
    connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, onSpinChanged);
    connect(m_heightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, onSpinChanged);

    mainLayout->addLayout(form);

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(bbox);
}

int NewImageDialog::imageWidth() const { return m_widthSpin->value(); }
int NewImageDialog::imageHeight() const { return m_heightSpin->value(); }
bool NewImageDialog::isTransparentBackground() const {
    return m_transparentRadio && m_transparentRadio->isChecked();
}

void NewImageDialog::onPresetChanged(int index) {
    QVariant data = m_presetCombo->itemData(index);
    if (data.isValid()) {
        QSize sz = data.toSize();
        m_widthSpin->setValue(sz.width());
        m_heightSpin->setValue(sz.height());
    }
}

// --- MetadataDialog ---
MetadataDialog::MetadataDialog(const Document* doc, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Metadata");
    resize(520, 480);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    if (doc) {
        m_initialMetadata = doc->metadata();

        QGroupBox* infoGroup = new QGroupBox("Image Information", this);
        QFormLayout* infoLayout = new QFormLayout(infoGroup);
        infoLayout->addRow("File:", new QLabel(doc->fileName(), this));
        infoLayout->addRow("Dimensions:", new QLabel(QString("%1 x %2 pixels (%3 DPI)")
                                                          .arg(doc->width()).arg(doc->height()).arg(doc->dpi()), this));
        infoLayout->addRow("Layers:", new QLabel(QString::number(doc->layerCount()), this));
        mainLayout->addWidget(infoGroup);
    }

    QGroupBox* metaGroup = new QGroupBox("Metadata Properties", this);
    QFormLayout* form = new QFormLayout(metaGroup);

    m_titleEdit = new QLineEdit(m_initialMetadata.title, this);
    m_titleEdit->setPlaceholderText("Image title or heading");
    form->addRow("Title:", m_titleEdit);

    m_authorEdit = new QLineEdit(m_initialMetadata.author, this);
    m_authorEdit->setPlaceholderText("Creator or artist name");
    form->addRow("Author / Artist:", m_authorEdit);

    m_copyrightEdit = new QLineEdit(m_initialMetadata.copyright, this);
    m_copyrightEdit->setPlaceholderText("e.g. Copyright (C) 2026");
    form->addRow("Copyright:", m_copyrightEdit);

    m_descriptionEdit = new QTextEdit(this);
    m_descriptionEdit->setPlainText(m_initialMetadata.description);
    m_descriptionEdit->setPlaceholderText("Detailed description or comments...");
    m_descriptionEdit->setMaximumHeight(90);
    form->addRow("Description:", m_descriptionEdit);

    QHBoxLayout* dateLayout = new QHBoxLayout();
    m_creationDateEdit = new QLineEdit(m_initialMetadata.creationDate, this);
    m_creationDateEdit->setPlaceholderText("YYYY-MM-DDTHH:MM:SS");
    QPushButton* nowBtn = new QPushButton("Now", this);
    nowBtn->setToolTip("Set to current date and time");
    connect(nowBtn, &QPushButton::clicked, this, &MetadataDialog::onSetCurrentDateTime);
    dateLayout->addWidget(m_creationDateEdit);
    dateLayout->addWidget(nowBtn);
    form->addRow("Date / Time:", dateLayout);

    m_softwareEdit = new QLineEdit(m_initialMetadata.software, this);
    form->addRow("Software:", m_softwareEdit);

    mainLayout->addWidget(metaGroup);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* clearBtn = new QPushButton("Clear All Metadata", this);
    connect(clearBtn, &QPushButton::clicked, this, &MetadataDialog::onClearAll);
    btnLayout->addWidget(clearBtn);

    btnLayout->addStretch();

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    btnLayout->addWidget(bbox);

    mainLayout->addLayout(btnLayout);
}

Metadata MetadataDialog::metadata() const {
    Metadata meta;
    meta.title = m_titleEdit ? m_titleEdit->text().trimmed() : QString();
    meta.author = m_authorEdit ? m_authorEdit->text().trimmed() : QString();
    meta.copyright = m_copyrightEdit ? m_copyrightEdit->text().trimmed() : QString();
    meta.description = m_descriptionEdit ? m_descriptionEdit->toPlainText().trimmed() : QString();
    meta.creationDate = m_creationDateEdit ? m_creationDateEdit->text().trimmed() : QString();
    meta.software = m_softwareEdit ? m_softwareEdit->text().trimmed() : QString();
    return meta;
}

void MetadataDialog::onClearAll() {
    if (m_titleEdit) m_titleEdit->clear();
    if (m_authorEdit) m_authorEdit->clear();
    if (m_copyrightEdit) m_copyrightEdit->clear();
    if (m_descriptionEdit) m_descriptionEdit->clear();
    if (m_creationDateEdit) m_creationDateEdit->clear();
    if (m_softwareEdit) m_softwareEdit->clear();
}

void MetadataDialog::onSetCurrentDateTime() {
    if (m_creationDateEdit) {
        m_creationDateEdit->setText(QDateTime::currentDateTime().toString(Qt::ISODate));
    }
}

} // namespace pdn
