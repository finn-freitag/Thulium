#include "TextTool.h"
#include "LayerEdit.h"
#include <QFontMetricsF>
#include <QPainter>
#include <algorithm>

namespace pdn {

namespace {

// Horizontal offset of a line of the given width relative to the anchor point.
qreal alignmentOffset(Qt::Alignment align, qreal lineWidth) {
    if (align & Qt::AlignHCenter) return -lineWidth / 2.0;
    if (align & Qt::AlignRight) return -lineWidth;
    return 0.0;
}

} // namespace

void TextTool::activate(Document* /*doc*/, ToolContext& ctx) {
    m_currentCtx = ctx;
}

void TextTool::deactivate(Document* doc, ToolContext& ctx) {
    m_currentCtx = ctx;
    commit(doc, ctx);
}

void TextTool::cancel() {
    m_active = false;
    m_text.clear();
    m_caret = 0;
}

QRectF TextTool::textBounds(const ToolContext& ctx) const {
    const QFontMetricsF fm(ctx.font);
    qreal maxWidth = 0.0;
    const QStringList all = lines();
    for (const QString& line : all) {
        maxWidth = std::max(maxWidth, fm.horizontalAdvance(line));
    }
    const qreal left = m_textPos.x() + alignmentOffset(ctx.textAlign, maxWidth);
    return QRectF(left, m_textPos.y(), maxWidth, fm.lineSpacing() * all.size());
}

void TextTool::renderText(QPainter& p, const QPointF& origin, qreal scale, const ToolContext& ctx, bool drawCaret) const {
    p.save();
    // Scaling the painter (rather than the font size) keeps the preview's line breaks and
    // glyph positions identical to what gets committed.
    p.translate(origin);
    p.scale(scale, scale);
    p.setRenderHint(QPainter::Antialiasing, ctx.antiAliasing);
    p.setRenderHint(QPainter::TextAntialiasing, ctx.antiAliasing);
    QFont font = ctx.font;
    if (!ctx.antiAliasing) font.setStyleStrategy(QFont::NoAntialias);
    p.setFont(font);
    p.setPen(ctx.primaryColor);

    const QFontMetricsF fm(font);
    const QStringList all = lines();
    int consumed = 0; // characters before the current line, including newlines
    for (int i = 0; i < all.size(); ++i) {
        const QString& line = all[i];
        const qreal x = alignmentOffset(ctx.textAlign, fm.horizontalAdvance(line));
        const qreal top = i * fm.lineSpacing();
        p.drawText(QPointF(x, top + fm.ascent()), line);

        if (drawCaret && m_caret >= consumed && m_caret <= consumed + line.size()) {
            const qreal caretX = x + fm.horizontalAdvance(line.left(m_caret - consumed));
            p.setPen(QPen(Qt::white, 3.0 / scale));
            p.drawLine(QPointF(caretX, top), QPointF(caretX, top + fm.height()));
            p.setPen(QPen(Qt::black, 1.0 / scale));
            p.drawLine(QPointF(caretX, top), QPointF(caretX, top + fm.height()));
            p.setPen(ctx.primaryColor);
        }
        consumed += line.size() + 1;
    }
    p.restore();
}

void TextTool::commit(Document* doc, const ToolContext& ctx) {
    if (!m_active || !doc) return;

    if (!m_text.trimmed().isEmpty()) {
        LayerEdit edit;
        if (edit.begin(doc)) {
            const QRect touched = textBounds(ctx).toAlignedRect().adjusted(-4, -4, 4, 4);
            edit.prepare(touched);
            {
                QPainter p(&edit.layer()->image());
                if (!doc->selection().isEmpty()) {
                    p.setClipRegion(doc->selection().region());
                }
                renderText(p, m_textPos, 1.0, ctx, false);
            }
            edit.markDirty(touched);
            edit.commit("Text");
        }
    }
    cancel();
}

void TextTool::mousePress(QMouseEvent* /*event*/, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    m_currentCtx = ctx;
    if (m_active) {
        commit(doc, ctx);
    }

    auto layer = doc->activeLayer();
    if (!layer || !layer->isVisible()) return;

    m_textPos = docPos;
    m_text.clear();
    m_caret = 0;
    m_active = true;
}

void TextTool::keyPress(QKeyEvent* event, Document* doc, ToolContext& ctx) {
    m_currentCtx = ctx;
    if (!m_active) return;

    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    switch (event->key()) {
        case Qt::Key_Escape:
            cancel();
            event->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (ctrl) {
                commit(doc, ctx);
            } else {
                m_text.insert(m_caret++, '\n');
            }
            event->accept();
            return;
        case Qt::Key_Backspace:
            if (m_caret > 0) m_text.remove(--m_caret, 1);
            event->accept();
            return;
        case Qt::Key_Delete:
            if (m_caret < m_text.size()) m_text.remove(m_caret, 1);
            event->accept();
            return;
        case Qt::Key_Left:
            m_caret = std::max(0, m_caret - 1);
            event->accept();
            return;
        case Qt::Key_Right:
            m_caret = std::min<int>(m_text.size(), m_caret + 1);
            event->accept();
            return;
        case Qt::Key_Home: {
            const int lineStart = m_text.lastIndexOf('\n', std::max(0, m_caret - 1));
            m_caret = (m_caret == 0) ? 0 : lineStart + 1;
            event->accept();
            return;
        }
        case Qt::Key_End: {
            const int lineEnd = m_text.indexOf('\n', m_caret);
            m_caret = lineEnd < 0 ? m_text.size() : lineEnd;
            event->accept();
            return;
        }
        default:
            break;
    }

    // Leave Ctrl/Alt shortcuts (undo, paste, ...) to the application
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) return;

    const QString txt = event->text();
    if (!txt.isEmpty() && txt.at(0).isPrint()) {
        m_text.insert(m_caret, txt);
        m_caret += txt.size();
        event->accept();
    }
}

void TextTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!m_active) return;
    renderText(painter, opts.panOffset + m_textPos * opts.zoom, opts.zoom, m_currentCtx, true);
}

} // namespace pdn
