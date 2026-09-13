#include "StatusWidget.h"
#include <QHBoxLayout>

namespace pdn {

StatusWidget::StatusWidget(QWidget* parent) : QWidget(parent) {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(16);

    m_cursorLabel = new QLabel("X: --, Y: --", this);
    m_cursorLabel->setMinimumWidth(90);

    m_sizeLabel = new QLabel("Size: -- x --", this);
    m_sizeLabel->setMinimumWidth(110);

    m_zoomLabel = new QLabel("100%", this);
    m_zoomLabel->setMinimumWidth(50);

    m_rendererLabel = new QLabel("Renderer: --", this);

    layout->addWidget(m_cursorLabel);
    layout->addWidget(m_sizeLabel);
    layout->addWidget(m_zoomLabel);
    layout->addStretch();
    layout->addWidget(m_rendererLabel);
}

void StatusWidget::setCursorPos(int x, int y) {
    m_cursorLabel->setText(QString("X: %1, Y: %2").arg(x).arg(y));
}

void StatusWidget::setDocumentSize(int w, int h) {
    m_sizeLabel->setText(QString("Size: %1 x %2 px").arg(w).arg(h));
}

void StatusWidget::setZoom(double zoom) {
    m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(zoom * 100 + 0.5)));
}

void StatusWidget::setRendererInfo(const QString& info) {
    m_rendererLabel->setText(QString("Renderer: %1").arg(info));
}

} // namespace pdn
