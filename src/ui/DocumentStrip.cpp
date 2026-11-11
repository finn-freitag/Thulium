#include "DocumentStrip.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QScrollBar>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

namespace pdn {

// --- DocumentTabItem ---

static constexpr int kMaxTitleWidth = 160; // longer titles are elided

DocumentTabItem::DocumentTabItem(std::shared_ptr<Document> doc, QWidget* parent)
    : QWidget(parent), m_doc(doc) {
    setObjectName("DocumentTabItem");
    setFixedHeight(36);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true); // plain QWidgets ignore stylesheet backgrounds otherwise

    setupUI();

    if (m_doc) {
        connect(m_doc.get(), &Document::documentChanged, this, &DocumentTabItem::updateTab);
        connect(m_doc.get(), &Document::pixelsChanged, this, [this]() {
            // Not restarted while pending, so the thumbnail keeps refreshing during a long stroke
            m_thumbDirty = true;
            if (!m_thumbTimer.isActive()) m_thumbTimer.start();
        });
        if (m_doc->undoStack()) {
            connect(m_doc->undoStack(), &QUndoStack::cleanChanged, this, [this](bool) {
                updateTab();
            });
        }
    }

    m_thumbTimer.setSingleShot(true);
    m_thumbTimer.setInterval(250);
    connect(&m_thumbTimer, &QTimer::timeout, this, &DocumentTabItem::generateThumbnail);

    generateThumbnail();
    updateTab();
}

QPixmap DocumentTabItem::makeCheckerboard(int w, int h, int squareSize) {
    QPixmap pm(w, h);
    QPainter p(&pm);
    QColor c1(230, 230, 230);
    QColor c2(255, 255, 255);
    for (int y = 0; y < h; y += squareSize) {
        for (int x = 0; x < w; x += squareSize) {
            p.fillRect(x, y, squareSize, squareSize, ((x / squareSize + y / squareSize) % 2 == 0) ? c1 : c2);
        }
    }
    return pm;
}

void DocumentTabItem::setupUI() {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 4, 2);
    layout->setSpacing(6);

    // Thumbnail preview
    m_thumbLabel = new QLabel(this);
    m_thumbLabel->setFixedSize(36, 24);
    m_thumbLabel->setAlignment(Qt::AlignCenter);

    // Document title
    m_titleLabel = new QLabel(this);

    // Close button
    m_closeBtn = new QToolButton(this);
    m_closeBtn->setFixedSize(18, 18);
    m_closeBtn->setText("×");
    m_closeBtn->setToolTip("Close (Ctrl+W)");
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    m_closeBtn->setCursor(Qt::ArrowCursor);
    connect(m_closeBtn, &QToolButton::clicked, this, &DocumentTabItem::closeClicked);

    layout->addWidget(m_thumbLabel);
    layout->addWidget(m_titleLabel, 1);
    layout->addWidget(m_closeBtn);

    updateStyle();
}

void DocumentTabItem::setActive(bool active) {
    if (m_active != active) {
        m_active = active;
        updateStyle();
        updateTab();
    }
}

void DocumentTabItem::updateStyle() {
    // Colours come from the application palette so the tabs match light and dark themes alike.
    const QString background = m_active ? "palette(base)" : (m_hovered ? "palette(midlight)" : "palette(window)");
    setStyleSheet(QString(
        "#DocumentTabItem {"
        "  border: 1px solid palette(mid);"
        "  border-bottom: none;"
        "  border-top: %1;"
        "  border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px;"
        "  background-color: %2;"
        "}")
        .arg(m_active ? "3px solid palette(highlight)" : "1px solid palette(mid)", background));

    m_titleLabel->setStyleSheet(QString("color: palette(window-text); font-weight: %1; font-size: 9pt;")
                                    .arg(m_active ? "bold" : "normal"));

    m_closeBtn->setStyleSheet(
        "QToolButton {"
        "  border: none;"
        "  background: transparent;"
        "  color: palette(window-text);"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border-radius: 9px;"
        "}"
        "QToolButton:hover {"
        "  background-color: #e81123;"
        "  color: #ffffff;"
        "}");
}

void DocumentTabItem::generateThumbnail() {
    if (!m_doc) return;

    int thumbW = 36;
    int thumbH = 24;

    QPixmap result = makeCheckerboard(thumbW, thumbH, 4);
    QPainter p(&result);

    // Point-sampled preview: independent of the document's resolution
    const QImage scaled = m_doc->thumbnail(QSize(thumbW - 2, thumbH - 2));
    if (!scaled.isNull()) {
        int ox = (thumbW - scaled.width()) / 2;
        int oy = (thumbH - scaled.height()) / 2;
        p.drawImage(ox, oy, scaled);
    }

    // 1px border
    p.setPen(QColor(180, 180, 180));
    p.drawRect(0, 0, thumbW - 1, thumbH - 1);
    p.end();

    m_thumbLabel->setPixmap(result);
    m_thumbDirty = false;
}

void DocumentTabItem::updateTab() {
    if (!m_doc) return;

    QString name = m_doc->fileName();
    if (m_doc->isModified()) {
        name += "*";
    }

    // Measure with the bold font so the title fits whether or not the tab is active
    QFont measureFont = m_titleLabel->font();
    measureFont.setBold(true);
    const QFontMetrics fm(measureFont);
    const QString elided = fm.elidedText(name, Qt::ElideRight, kMaxTitleWidth);
    m_titleLabel->setText(elided);
    m_titleLabel->setFixedWidth(fm.horizontalAdvance(elided) + 4);
    setFixedWidth(sizeHint().width());

    QString fullInfo = QString("%1 (%2 × %3 px)\n%4")
                           .arg(m_doc->fileName())
                           .arg(m_doc->width())
                           .arg(m_doc->height())
                           .arg(m_doc->filePath().isEmpty() ? "Unsaved document" : m_doc->filePath());
    if (m_doc->isModified()) {
        fullInfo += " [Unsaved changes]";
    }
    setToolTip(fullInfo);

    if (m_thumbDirty && !m_thumbTimer.isActive()) {
        m_thumbTimer.start();
    }
}

void DocumentTabItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
        event->accept();
    } else if (event->button() == Qt::MiddleButton) {
        emit closeClicked();
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void DocumentTabItem::contextMenuEvent(QContextMenuEvent* event) {
    emit contextMenuRequested(event->globalPos());
    event->accept();
}

void DocumentTabItem::enterEvent(QEnterEvent* /*event*/) {
    m_hovered = true;
    updateStyle();
}

void DocumentTabItem::leaveEvent(QEvent* /*event*/) {
    m_hovered = false;
    updateStyle();
}

// --- DocumentStrip ---

DocumentStrip::DocumentStrip(QWidget* parent) : QWidget(parent) {
    setObjectName("DocumentStrip");
    setFixedHeight(40);
    setupUI();
}

void DocumentStrip::setupUI() {
    QHBoxLayout* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(4, 4, 4, 0);
    rootLayout->setSpacing(2);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFixedHeight(36);

    // Style scrollbar to be slim and clean
    m_scrollArea->setStyleSheet(
        "QScrollArea { background-color: transparent; border: none; }"
        "QScrollBar:horizontal { height: 4px; background: #e0e0e0; border-radius: 2px; }"
        "QScrollBar::handle:horizontal { background: #a0a0a0; border-radius: 2px; min-width: 20px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
    );

    m_tabsContainer = new QWidget(m_scrollArea);
    m_tabsContainer->setStyleSheet("background-color: transparent;");
    m_tabsLayout = new QHBoxLayout(m_tabsContainer);
    m_tabsLayout->setContentsMargins(0, 0, 0, 0);
    m_tabsLayout->setSpacing(2);

    // New Document '+' Button
    m_newBtn = new QToolButton(m_tabsContainer);
    m_newBtn->setFixedSize(28, 28);
    m_newBtn->setText("+");
    m_newBtn->setToolTip("New Image (Ctrl+N)");
    m_newBtn->setFocusPolicy(Qt::NoFocus);
    m_newBtn->setCursor(Qt::PointingHandCursor);
    m_newBtn->setStyleSheet(
        "QToolButton {"
        "  border: 1px solid #c0c4c8;"
        "  border-radius: 4px;"
        "  background-color: #e4e8ec;"
        "  color: #333333;"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "}"
        "QToolButton:hover {"
        "  background-color: #ffffff;"
        "  border-color: #0078d7;"
        "  color: #0078d7;"
        "}");
    connect(m_newBtn, &QToolButton::clicked, this, &DocumentStrip::newDocumentRequested);

    m_tabsLayout->addWidget(m_newBtn);
    m_tabsLayout->addStretch(1);

    m_scrollArea->setWidget(m_tabsContainer);
    rootLayout->addWidget(m_scrollArea, 1);

    setStyleSheet("DocumentStrip { background-color: #eceff2; border-bottom: 1px solid #c0c4c8; }");
}

void DocumentStrip::wheelEvent(QWheelEvent* event) {
    // Scroll horizontally when mouse wheel rolls over strip
    int delta = event->angleDelta().y();
    if (delta == 0) {
        delta = event->angleDelta().x();
    }
    if (delta != 0 && m_scrollArea->horizontalScrollBar()) {
        m_scrollArea->horizontalScrollBar()->setValue(
            m_scrollArea->horizontalScrollBar()->value() - delta);
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void DocumentStrip::addDocument(std::shared_ptr<Document> doc) {
    insertDocument(static_cast<int>(m_tabs.size()), doc);
}

void DocumentStrip::insertDocument(int index, std::shared_ptr<Document> doc) {
    DocumentTabItem* item = new DocumentTabItem(doc, m_tabsContainer);
    connect(item, &DocumentTabItem::clicked, this, &DocumentStrip::onTabClicked);
    connect(item, &DocumentTabItem::closeClicked, this, &DocumentStrip::onTabCloseClicked);
    connect(item, &DocumentTabItem::contextMenuRequested, this, &DocumentStrip::onTabContextMenu);

    int insertIdx = std::clamp(index, 0, static_cast<int>(m_tabs.size()));
    m_tabs.insert(m_tabs.begin() + insertIdx, item);

    // Layout insertion: m_tabsLayout has items, plus m_newBtn and stretch
    m_tabsLayout->insertWidget(insertIdx, item);

    setCurrentIndex(insertIdx);
}

void DocumentStrip::removeDocument(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;

    DocumentTabItem* item = m_tabs[index];
    m_tabs.erase(m_tabs.begin() + index);
    m_tabsLayout->removeWidget(item);
    item->deleteLater();

    if (m_currentIndex >= static_cast<int>(m_tabs.size())) {
        m_currentIndex = static_cast<int>(m_tabs.size()) - 1;
    }
    if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_tabs.size())) {
        m_tabs[m_currentIndex]->setActive(true);
    }
}

void DocumentStrip::setCurrentIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) {
        m_currentIndex = -1;
        return;
    }

    m_currentIndex = index;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        m_tabs[i]->setActive(static_cast<int>(i) == m_currentIndex);
    }

    // Scroll to active tab
    if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_tabs.size())) {
        m_scrollArea->ensureWidgetVisible(m_tabs[m_currentIndex]);
    }
}

void DocumentStrip::updateDocument(int index) {
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
        m_tabs[index]->updateTab();
    }
}

void DocumentStrip::clear() {
    for (auto tab : m_tabs) {
        m_tabsLayout->removeWidget(tab);
        tab->deleteLater();
    }
    m_tabs.clear();
    m_currentIndex = -1;
}

void DocumentStrip::onTabClicked() {
    DocumentTabItem* item = qobject_cast<DocumentTabItem*>(sender());
    if (!item) return;

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i] == item) {
            setCurrentIndex(static_cast<int>(i));
            emit documentSelected(static_cast<int>(i));
            break;
        }
    }
}

void DocumentStrip::onTabCloseClicked() {
    DocumentTabItem* item = qobject_cast<DocumentTabItem*>(sender());
    if (!item) return;

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i] == item) {
            emit closeRequested(static_cast<int>(i));
            break;
        }
    }
}

void DocumentStrip::onTabContextMenu(const QPoint& globalPos) {
    DocumentTabItem* item = qobject_cast<DocumentTabItem*>(sender());
    if (!item) return;

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i] == item) {
            showContextMenu(static_cast<int>(i), globalPos);
            break;
        }
    }
}

void DocumentStrip::showContextMenu(int index, const QPoint& globalPos) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;

    auto doc = m_tabs[index]->document();
    if (!doc) return;

    QMenu menu(this);

    QAction* saveAct = menu.addAction("&Save", this, [this, index]() {
        emit saveRequested(index);
    });
    saveAct->setShortcut(QKeySequence::Save);

    QAction* saveAsAct = menu.addAction("Save &As...", this, [this, index]() {
        emit saveAsRequested(index);
    });
    saveAsAct->setShortcut(QKeySequence::SaveAs);

    menu.addSeparator();

    QAction* closeAct = menu.addAction("&Close", this, [this, index]() {
        emit closeRequested(index);
    });
    closeAct->setShortcut(QKeySequence::Close);

    menu.addAction("Close &Others", this, [this, index]() {
        emit closeOthersRequested(index);
    });

    menu.addAction("Close &All", this, [this]() {
        emit closeAllRequested();
    });

    if (!doc->filePath().isEmpty()) {
        menu.addSeparator();

        menu.addAction("Copy Full &Path", this, [doc]() {
            QClipboard* cb = QGuiApplication::clipboard();
            cb->setText(doc->filePath());
        });

        menu.addAction("Open &Containing Folder", this, [doc]() {
            QFileInfo fi(doc->filePath());
            QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
        });
    }

    menu.exec(globalPos);
}

} // namespace pdn
