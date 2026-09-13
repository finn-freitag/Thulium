#pragma once

#include <QDockWidget>
#include <QUndoView>
#include <QToolButton>
#include <memory>
#include "../core/Document.h"

namespace pdn {

class HistoryDock : public QDockWidget {
    Q_OBJECT
public:
    explicit HistoryDock(QWidget* parent = nullptr);

    void setDocument(std::shared_ptr<Document> doc);

private:
    void setupUI();

    std::shared_ptr<Document> m_doc;
    QUndoView* m_undoView;
    QToolButton* m_undoBtn;
    QToolButton* m_redoBtn;
};

} // namespace pdn
