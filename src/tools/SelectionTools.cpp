#include "SelectionTools.h"
#include "../core/FloodFill.h"
#include "../core/History.h"
#include <QPen>
#include <algorithm>
#include <cmath>

namespace pdn {

namespace {

constexpr int kClickTolerancePx = 2; // screen pixels the mouse may move and still count as a click

QRegion combineRegions(const QRegion& base, const QRegion& shape, SelectionCombineMode mode) {
    switch (mode) {
        case SelectionCombineMode::Union:     return base.united(shape);
        case SelectionCombineMode::Exclude:   return base.subtracted(shape);
        case SelectionCombineMode::Intersect: return base.intersected(shape);
        case SelectionCombineMode::Invert:    return base.xored(shape);
        case SelectionCombineMode::Replace:   break;
    }
    return shape;
}

// Pixel rectangle spanned by two drag positions, both end pixels included.
QRect pixelRect(const QPointF& a, const QPointF& b) {
    const int x0 = static_cast<int>(std::floor(a.x()));
    const int y0 = static_cast<int>(std::floor(a.y()));
    const int x1 = static_cast<int>(std::floor(b.x()));
    const int y1 = static_cast<int>(std::floor(b.y()));
    return QRect(QPoint(std::min(x0, x1), std::min(y0, y1)), QPoint(std::max(x0, x1), std::max(y0, y1)));
}

} // namespace

SelectionCombineMode selectionModeForEvent(const QMouseEvent* event, Qt::MouseButton button, const ToolContext& ctx) {
    const Qt::KeyboardModifiers mods = event ? event->modifiers() : Qt::NoModifier;
    const bool ctrl = mods & Qt::ControlModifier;
    if (button == Qt::RightButton) {
        return ctrl ? SelectionCombineMode::Invert : SelectionCombineMode::Exclude;
    }
    if (ctrl || (mods & Qt::ShiftModifier)) {
        return SelectionCombineMode::Union;
    }
    return ctx.selectionCombineMode;
}

void commitSelection(Document* doc, const QRegion& oldRegion, const QRegion& newRegion, const QString& actionName) {
    doc->selection().setRegion(newRegion);
    // Compare what the selection actually holds: it clips to the canvas
    if (doc->selection().region() != oldRegion) {
        doc->undoStack()->push(new SelectionUndoCommand(doc, oldRegion, doc->selection().region(), actionName));
    }
    emit doc->selectionChanged();
}

// --- DragSelectTool ---

SelectionCombineMode DragSelectTool::effectiveMode(const QMouseEvent* event, const ToolContext& ctx) const {
    return selectionModeForEvent(event, m_button, ctx);
}

void DragSelectTool::deactivate(Document* doc, ToolContext& /*ctx*/) {
    if (m_selecting && doc) {
        // Abandon the unfinished drag
        doc->selection().setRegion(m_baseRegion);
        emit doc->selectionChanged();
    }
    m_selecting = false;
}

void DragSelectTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!doc || m_selecting) return;
    if (doc->hasFloatingSelection()) {
        doc->bakeFloatingSelection();
    }
    m_button = event ? event->button() : Qt::LeftButton;
    m_pressScreenPos = event ? event->pos() : QPoint();
    m_startPos = docPos;
    m_currentPos = docPos;
    m_baseRegion = doc->selection().region();
    m_selecting = true;
    m_dragged = false;
}

void DragSelectTool::applyShape(Document* doc, SelectionCombineMode mode) {
    doc->selection().setRegion(combineRegions(m_baseRegion, shapeRegion(m_startPos, m_currentPos), mode));
    emit doc->selectionChanged();
}

void DragSelectTool::mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_selecting) return;
    m_currentPos = docPos;
    dragMoved(docPos);
    if (event && (event->pos() - m_pressScreenPos).manhattanLength() > kClickTolerancePx) {
        m_dragged = true;
    }
    // Show the real selection (marching ants and all) while dragging
    if (m_dragged && livePreview()) {
        applyShape(doc, effectiveMode(event, ctx));
    }
}

void DragSelectTool::mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!m_selecting) return;
    m_currentPos = docPos;
    dragMoved(docPos);
    if (event && (event->pos() - m_pressScreenPos).manhattanLength() > kClickTolerancePx) {
        m_dragged = true;
    }
    finish(doc, effectiveMode(event, ctx), m_dragged);
}

void DragSelectTool::finish(Document* doc, SelectionCombineMode mode, bool wasDrag) {
    m_selecting = false;
    if (wasDrag) {
        commitSelection(doc, m_baseRegion, combineRegions(m_baseRegion, shapeRegion(m_startPos, m_currentPos), mode), name());
    } else if (mode == SelectionCombineMode::Replace && !m_baseRegion.isEmpty()) {
        // A plain click deselects
        commitSelection(doc, m_baseRegion, QRegion(), "Deselect");
    } else {
        doc->selection().setRegion(m_baseRegion);
        emit doc->selectionChanged();
    }
}

// --- RectangleSelectTool ---

QRegion RectangleSelectTool::shapeRegion(const QPointF& start, const QPointF& current) const {
    return QRegion(pixelRect(start, current));
}

// --- EllipseSelectTool ---

QRegion EllipseSelectTool::shapeRegion(const QPointF& start, const QPointF& current) const {
    return Selection::regionFromEllipse(pixelRect(start, current));
}

// --- LassoSelectTool ---

void LassoSelectTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    m_polygon.clear();
    m_polygon << docPos;
    DragSelectTool::mousePress(event, doc, docPos, ctx);
}

void LassoSelectTool::dragMoved(const QPointF& docPos) {
    if (m_polygon.isEmpty() || m_polygon.last() != docPos) {
        m_polygon << docPos;
    }
}

QRegion LassoSelectTool::shapeRegion(const QPointF& /*start*/, const QPointF& /*current*/) const {
    if (m_polygon.size() < 3) return QRegion();
    QPainterPath path;
    path.addPolygon(m_polygon);
    path.closeSubpath();
    return Selection::regionFromPath(path);
}

void LassoSelectTool::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    if (!isSelecting() || m_polygon.size() < 2) return;

    QPolygonF vpPoly;
    vpPoly.reserve(m_polygon.size());
    for (const auto& pt : m_polygon) {
        vpPoly << opts.panOffset + pt * opts.zoom;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(0, 120, 215, 28));
    painter.setPen(QPen(Qt::white, 2.0));
    painter.drawPolygon(vpPoly);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::black, 1.0, Qt::DashLine));
    painter.drawPolygon(vpPoly);
    painter.restore();
}

// --- MagicWandTool ---

void MagicWandTool::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& ctx) {
    if (!doc) return;
    if (doc->hasFloatingSelection()) {
        doc->bakeFloatingSelection();
    }
    const SelectionCombineMode mode = selectionModeForEvent(event, event ? event->button() : Qt::LeftButton, ctx);
    const QRegion oldRegion = doc->selection().region();
    const QPoint seed(static_cast<int>(std::floor(docPos.x())), static_cast<int>(std::floor(docPos.y())));

    auto layer = doc->activeLayer();
    if (!layer || !layer->image().rect().contains(seed)) {
        // Clicking outside the canvas deselects
        if (mode == SelectionCombineMode::Replace && !oldRegion.isEmpty()) {
            commitSelection(doc, oldRegion, QRegion(), "Deselect");
        }
        return;
    }

    const FloodMask flood = floodFill(layer->image(), seed, ctx.tolerance, ctx.floodMode == FloodMode::Contiguous);
    commitSelection(doc, oldRegion, combineRegions(oldRegion, flood.toRegion(), mode), "Magic Wand");
}

} // namespace pdn
