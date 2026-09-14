#include "TextTool.h"
#include "../core/History.h"
#include <QPainter>
#include <QFontMetrics>

namespace pdn {

void TextTool::activate(Document* /*doc*/, ToolContext& ctx) {
    m_currentCtx = ctx;
}

void TextTool::deactivate(Document* doc, ToolContext& ctx) {
    m_currentCtx = ctx;
    commit(doc, ctx);
}

void TextTool::commit(Document* doc, const ToolContext& ctx) {
    if (!m_active || !doc) return;

    if (!m_text.isEmpty()) {
        auto layer = doc->activeLayer();
        if (layer) {
            QPainter p(&layer->image());
            if (!doc->selection().isEmpty()) {
                p.setClipPath(doc->selection().path());
            }
            p.setRenderHint(QPainter::TextAntialiasing, true);
            p.setFont(ctx.font);
            p.setPen(ctx.primaryColor);

            QFontMetrics fm(ctx.font);
            p.drawText(m_textPos + QPointF(0, fm.ascent()), m_text);
            p.end();

            doc->undoStack()->push(new LayerBitmapUndoCommand(doc, doc->activeLayerIndex(), m_undoSnapshot, "Text"));
        }
    }

    m_active = false;
    m_text.clear();
    emit doc->documentChanged();
}

void TextTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    m_currentCtx = ctx;
    if (m_active) {
        commit(doc, ctx);
    }

    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_undoSnapshot = layer->image().copy();
    m_textPos = docPos;
    m_text.clear();
    m_active = true;
    emit doc->documentChanged();
}

void TextTool::keyPress(QKeyEvent* event, Document* doc, ToolContext& ctx) {
    m_currentCtx = ctx;
    if (!m_active) return;

    if (event->key() == Qt::Key_Escape) {
        m_active = false;
        m_text.clear();
        emit doc->documentChanged();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        commit(doc, ctx);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Backspace) {
        if (!m_text.isEmpty()) {
            m_text.chop(1);
            emit doc->documentChanged();
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete) {
        event->accept();
        return;
    }

    QString txt = event->text();
    if (!txt.isEmpty() && txt.at(0).isPrint()) {
        m_text += txt;
        emit doc->documentChanged();
        event->accept();
    }
}

void TextTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_active) return;

    QPointF vpPos(opts.panOffset.x() + m_textPos.x() * opts.zoom,
                  opts.panOffset.y() + m_textPos.y() * opts.zoom);

    painter.save();
    painter.setPen(m_currentCtx.primaryColor);
    QFont vpFont = m_currentCtx.font;
    vpFont.setPointSizeF(vpFont.pointSizeF() * opts.zoom);
    painter.setFont(vpFont);

    QFontMetrics fm(vpFont);
    painter.drawText(vpPos + QPointF(0, fm.ascent()), m_text);

    // Draw text cursor
    int textW = fm.horizontalAdvance(m_text);
    QPointF cursorTop = vpPos + QPointF(textW + 2, 0);
    QPointF cursorBottom = cursorTop + QPointF(0, fm.height());
    painter.setPen(QPen(Qt::blue, 2.0));
    painter.drawLine(cursorTop, cursorBottom);

    painter.restore();
}

} // namespace pdn
