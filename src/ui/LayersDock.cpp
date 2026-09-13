#include "LayersDock.h"
#include "Dialogs.h"
#include "../core/History.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>

namespace pdn {

LayersDock::LayersDock(QWidget* parent) : QDockWidget("Layers", parent) {
    setObjectName("LayersDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupUI();
}

void LayersDock::setupUI() {
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_listWidget = new QListWidget(container);
    m_listWidget->setIconSize(QSize(32, 32));
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &LayersDock::onItemSelectionChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &LayersDock::onItemDoubleClicked);
    connect(m_listWidget, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (m_updating || !m_doc) return;
        int layerIdx = item->data(Qt::UserRole).toInt();
        auto l = m_doc->layer(layerIdx);
        if (l) {
            bool visible = (item->checkState() == Qt::Checked);
            if (l->isVisible() != visible) {
                m_doc->undoStack()->push(new LayerPropertyUndoCommand(m_doc.get(), layerIdx,
                    l->name(), l->name(),
                    l->opacity(), l->opacity(),
                    l->blendMode(), l->blendMode(),
                    !visible, visible,
                    "Layer Visibility"));
            }
        }
    });

    // Toolbar buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(2);

    m_addBtn = new QToolButton(container);
    m_addBtn->setFocusPolicy(Qt::NoFocus);
    m_addBtn->setText("+");
    m_addBtn->setToolTip("Add New Layer (Ctrl+Shift+N)");
    connect(m_addBtn, &QToolButton::clicked, this, &LayersDock::onAddLayer);

    m_deleteBtn = new QToolButton(container);
    m_deleteBtn->setFocusPolicy(Qt::NoFocus);
    m_deleteBtn->setText("X");
    m_deleteBtn->setToolTip("Delete Layer");
    connect(m_deleteBtn, &QToolButton::clicked, this, &LayersDock::onDeleteLayer);

    m_duplicateBtn = new QToolButton(container);
    m_duplicateBtn->setFocusPolicy(Qt::NoFocus);
    m_duplicateBtn->setText("Dup");
    m_duplicateBtn->setToolTip("Duplicate Layer (Ctrl+Shift+D)");
    connect(m_duplicateBtn, &QToolButton::clicked, this, &LayersDock::onDuplicateLayer);

    m_mergeDownBtn = new QToolButton(container);
    m_mergeDownBtn->setFocusPolicy(Qt::NoFocus);
    m_mergeDownBtn->setText("Merge");
    m_mergeDownBtn->setToolTip("Merge Layer Down (Ctrl+M)");
    connect(m_mergeDownBtn, &QToolButton::clicked, this, &LayersDock::onMergeDown);

    m_moveUpBtn = new QToolButton(container);
    m_moveUpBtn->setFocusPolicy(Qt::NoFocus);
    m_moveUpBtn->setText("^");
    m_moveUpBtn->setToolTip("Move Layer Up");
    connect(m_moveUpBtn, &QToolButton::clicked, this, &LayersDock::onMoveUp);

    m_moveDownBtn = new QToolButton(container);
    m_moveDownBtn->setFocusPolicy(Qt::NoFocus);
    m_moveDownBtn->setText("v");
    m_moveDownBtn->setToolTip("Move Layer Down");
    connect(m_moveDownBtn, &QToolButton::clicked, this, &LayersDock::onMoveDown);

    m_propertiesBtn = new QToolButton(container);
    m_propertiesBtn->setFocusPolicy(Qt::NoFocus);
    m_propertiesBtn->setText("Prop");
    m_propertiesBtn->setToolTip("Layer Properties (F4)");
    connect(m_propertiesBtn, &QToolButton::clicked, this, &LayersDock::onProperties);

    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addWidget(m_duplicateBtn);
    btnLayout->addWidget(m_mergeDownBtn);
    btnLayout->addWidget(m_moveUpBtn);
    btnLayout->addWidget(m_moveDownBtn);
    btnLayout->addWidget(m_propertiesBtn);
    btnLayout->addStretch();

    layout->addWidget(m_listWidget);
    layout->addLayout(btnLayout);

    container->setLayout(layout);
    setWidget(container);
    setMinimumWidth(180);
}

void LayersDock::setDocument(std::shared_ptr<Document> doc) {
    m_doc = doc;
    if (m_doc) {
        connect(m_doc.get(), &Document::layerCountChanged, this, &LayersDock::refreshLayerList);
        connect(m_doc.get(), &Document::activeLayerChanged, this, &LayersDock::refreshLayerList);
        connect(m_doc.get(), &Document::layerPropertiesChanged, this, &LayersDock::refreshLayerList);
        connect(m_doc.get(), &Document::documentChanged, this, &LayersDock::refreshLayerList);
    }
    refreshLayerList();
}

void LayersDock::refreshLayerList() {
    if (!m_doc) {
        m_listWidget->clear();
        return;
    }

    m_updating = true;
    m_listWidget->clear();

    // Paint.NET shows top layer at the top of the list (reversed index)
    int count = m_doc->layerCount();
    for (int i = count - 1; i >= 0; --i) {
        auto layer = m_doc->layer(i);
        if (!layer) continue;

        QListWidgetItem* item = new QListWidgetItem();
        item->setText(layer->name());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setCheckState(layer->isVisible() ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, i);

        // Thumbnail
        QPixmap thumb = QPixmap::fromImage(layer->image().scaled(32, 32, Qt::KeepAspectRatio, Qt::FastTransformation));
        item->setIcon(QIcon(thumb));

        m_listWidget->addItem(item);

        if (i == m_doc->activeLayerIndex()) {
            m_listWidget->setCurrentItem(item);
        }
    }



    m_updating = false;
}

void LayersDock::onItemSelectionChanged() {
    if (m_updating || !m_doc) return;
    auto item = m_listWidget->currentItem();
    if (item) {
        int layerIdx = item->data(Qt::UserRole).toInt();
        m_doc->setActiveLayerIndex(layerIdx);
    }
}

void LayersDock::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item || !m_doc) return;
    int layerIdx = item->data(Qt::UserRole).toInt();
    LayerPropertiesDialog dlg(m_doc.get(), layerIdx, this);
    dlg.exec();
}

void LayersDock::onAddLayer() {
    if (m_doc) {
        m_doc->addLayer();
    }
}

void LayersDock::onDeleteLayer() {
    if (m_doc && m_doc->layerCount() > 1) {
        m_doc->removeLayer(m_doc->activeLayerIndex());
    }
}

void LayersDock::onDuplicateLayer() {
    if (m_doc) {
        m_doc->duplicateLayer(m_doc->activeLayerIndex());
    }
}

void LayersDock::onMergeDown() {
    if (m_doc && m_doc->activeLayerIndex() > 0) {
        m_doc->mergeLayerDown(m_doc->activeLayerIndex());
    }
}

void LayersDock::onMoveUp() {
    if (m_doc && m_doc->activeLayerIndex() < m_doc->layerCount() - 1) {
        m_doc->moveLayer(m_doc->activeLayerIndex(), m_doc->activeLayerIndex() + 1);
    }
}

void LayersDock::onMoveDown() {
    if (m_doc && m_doc->activeLayerIndex() > 0) {
        m_doc->moveLayer(m_doc->activeLayerIndex(), m_doc->activeLayerIndex() - 1);
    }
}

void LayersDock::onProperties() {
    if (m_doc) {
        LayerPropertiesDialog dlg(m_doc.get(), m_doc->activeLayerIndex(), this);
        dlg.exec();
    }
}

} // namespace pdn
