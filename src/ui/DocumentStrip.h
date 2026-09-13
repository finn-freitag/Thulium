#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QTimer>
#include <memory>
#include <vector>
#include "../core/Document.h"

namespace pdn {

class DocumentTabItem : public QWidget {
    Q_OBJECT
public:
    explicit DocumentTabItem(std::shared_ptr<Document> doc, QWidget* parent = nullptr);
    ~DocumentTabItem() override = default;

    std::shared_ptr<Document> document() const { return m_doc; }
    bool isActive() const { return m_active; }
    void setActive(bool active);

    void updateTab();

signals:
    void clicked();
    void closeClicked();
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void generateThumbnail();

private:
    void setupUI();
    void updateStyle();
    static QPixmap makeCheckerboard(int w, int h, int squareSize = 4);

    std::shared_ptr<Document> m_doc;
    bool m_active = false;
    bool m_hovered = false;

    QLabel* m_thumbLabel;
    QLabel* m_titleLabel;
    QToolButton* m_closeBtn;

    QTimer m_thumbTimer;
    bool m_thumbDirty = true;
};

class DocumentStrip : public QWidget {
    Q_OBJECT
public:
    explicit DocumentStrip(QWidget* parent = nullptr);
    ~DocumentStrip() override = default;

    void addDocument(std::shared_ptr<Document> doc);
    void insertDocument(int index, std::shared_ptr<Document> doc);
    void removeDocument(int index);
    void setCurrentIndex(int index);
    int currentIndex() const { return m_currentIndex; }
    int count() const { return static_cast<int>(m_tabs.size()); }

    void updateDocument(int index);
    void clear();

signals:
    void documentSelected(int index);
    void closeRequested(int index);
    void newDocumentRequested();
    void saveRequested(int index);
    void saveAsRequested(int index);
    void closeAllRequested();
    void closeOthersRequested(int index);

protected:
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onTabClicked();
    void onTabCloseClicked();
    void onTabContextMenu(const QPoint& globalPos);

private:
    void setupUI();
    void showContextMenu(int index, const QPoint& globalPos);

    QScrollArea* m_scrollArea;
    QWidget* m_tabsContainer;
    QHBoxLayout* m_tabsLayout;
    QToolButton* m_newBtn;

    std::vector<DocumentTabItem*> m_tabs;
    int m_currentIndex = -1;
};

} // namespace pdn
