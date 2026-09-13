#pragma once

#include <QWidget>
#include <QLabel>

namespace pdn {

class StatusWidget : public QWidget {
    Q_OBJECT
public:
    explicit StatusWidget(QWidget* parent = nullptr);

public slots:
    void setCursorPos(int x, int y);
    void setDocumentSize(int w, int h);
    void setZoom(double zoom);
    void setRendererInfo(const QString& info);

private:
    QLabel* m_cursorLabel;
    QLabel* m_sizeLabel;
    QLabel* m_zoomLabel;
    QLabel* m_rendererLabel;
};

} // namespace pdn
