#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QToolButton>
#include <memory>
#include "../core/Document.h"

namespace pdn {

class LayersDock : public QDockWidget {
    Q_OBJECT
public:
    explicit LayersDock(QWidget* parent = nullptr);

    void setDocument(std::shared_ptr<Document> doc);

public slots:
    void refreshLayerList();

private slots:
    void onAddLayer();
    void onDeleteLayer();
    void onDuplicateLayer();
    void onMergeDown();
    void onMoveUp();
    void onMoveDown();
    void onProperties();
    void onItemSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void setupUI();

    std::shared_ptr<Document> m_doc;
    QListWidget* m_listWidget;

    QToolButton* m_addBtn;
    QToolButton* m_deleteBtn;
    QToolButton* m_duplicateBtn;
    QToolButton* m_mergeDownBtn;
    QToolButton* m_moveUpBtn;
    QToolButton* m_moveDownBtn;
    QToolButton* m_propertiesBtn;

    bool m_updating = false;
};

} // namespace pdn
