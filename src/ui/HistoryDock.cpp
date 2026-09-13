#include "HistoryDock.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace pdn {

HistoryDock::HistoryDock(QWidget* parent)
    : QDockWidget("History", parent) {
    setObjectName("HistoryDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupUI();
}

void HistoryDock::setupUI() {
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_undoView = new QUndoView(container);
    m_undoView->setEmptyLabel("Open Image");

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_undoBtn = new QToolButton(container);
    m_undoBtn->setText("Undo");
    m_undoBtn->setToolTip("Undo (Ctrl+Z)");
    m_undoBtn->setEnabled(false);

    m_redoBtn = new QToolButton(container);
    m_redoBtn->setText("Redo");
    m_redoBtn->setToolTip("Redo (Ctrl+Y)");
    m_redoBtn->setEnabled(false);

    btnLayout->addWidget(m_undoBtn);
    btnLayout->addWidget(m_redoBtn);
    btnLayout->addStretch();

    layout->addWidget(m_undoView);
    layout->addLayout(btnLayout);

    container->setLayout(layout);
    setWidget(container);
    setMinimumWidth(160);
}

void HistoryDock::setDocument(std::shared_ptr<Document> doc) {
    if (m_doc && m_doc->undoStack()) {
        disconnect(m_undoBtn, &QToolButton::clicked, m_doc->undoStack(), &QUndoStack::undo);
        disconnect(m_redoBtn, &QToolButton::clicked, m_doc->undoStack(), &QUndoStack::redo);
        disconnect(m_doc->undoStack(), &QUndoStack::canUndoChanged, m_undoBtn, &QToolButton::setEnabled);
        disconnect(m_doc->undoStack(), &QUndoStack::canRedoChanged, m_redoBtn, &QToolButton::setEnabled);
    }
    m_doc = doc;
    if (m_doc && m_doc->undoStack()) {
        m_undoView->setStack(m_doc->undoStack());
        connect(m_undoBtn, &QToolButton::clicked, m_doc->undoStack(), &QUndoStack::undo);
        connect(m_redoBtn, &QToolButton::clicked, m_doc->undoStack(), &QUndoStack::redo);
        connect(m_doc->undoStack(), &QUndoStack::canUndoChanged, m_undoBtn, &QToolButton::setEnabled);
        connect(m_doc->undoStack(), &QUndoStack::canRedoChanged, m_redoBtn, &QToolButton::setEnabled);
        m_undoBtn->setEnabled(m_doc->undoStack()->canUndo());
        m_redoBtn->setEnabled(m_doc->undoStack()->canRedo());
    } else {
        m_undoView->setStack(nullptr);
        m_undoBtn->setEnabled(false);
        m_redoBtn->setEnabled(false);
    }
}

} // namespace pdn
