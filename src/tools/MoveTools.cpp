#include "MoveTools.h"
#include "../core/History.h"
#include <QPainter>
#include <QLineF>
#include <cmath>
#include <algorithm>

namespace pdn {

QCursor MoveToolBase::s_rotateCursor;
bool MoveToolBase::s_rotateCursorInitialized = false;

static qreal distanceToSegment(const QPointF& pt, const QPointF& a, const QPointF& b) {
    QPointF ab = b - a;
    qreal lenSq = ab.x() * ab.x() + ab.y() * ab.y();
    if (lenSq < 1e-6) {
        return QLineF(pt, a).length();
    }
    qreal t = std::clamp(((pt.x() - a.x()) * ab.x() + (pt.y() - a.y()) * ab.y()) / lenSq, 0.0, 1.0);
    QPointF proj = a + ab * t;
    return QLineF(pt, proj).length();
}

void MoveToolBase::initRotateCursor() {
    if (s_rotateCursorInitialized) return;
    s_rotateCursorInitialized = true;

    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRectF arcRect(7, 7, 18, 18);

    // Outer black stroke for contrast on bright surfaces
    QPen blackPen(Qt::black, 3.5, Qt::SolidLine, Qt::FlatCap);
    p.setPen(blackPen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(arcRect, 35 * 16, 110 * 16);
    p.drawArc(arcRect, 215 * 16, 110 * 16);

    // Inner white stroke for contrast on dark surfaces
    QPen whitePen(Qt::white, 1.8, Qt::SolidLine, Qt::FlatCap);
    p.setPen(whitePen);
    p.drawArc(arcRect, 35 * 16, 110 * 16);
    p.drawArc(arcRect, 215 * 16, 110 * 16);

    auto drawArrow = [&](const QPointF& tip, qreal angleDeg) {
        p.save();
        p.translate(tip);
        p.rotate(angleDeg);
        QPolygonF poly;
        poly << QPointF(0, 0) << QPointF(-5, -3) << QPointF(-4, 0) << QPointF(-5, 3);
        p.setPen(QPen(Qt::black, 1.0));
        p.setBrush(Qt::white);
        p.drawPolygon(poly);
        p.restore();
    };

    qreal r = 9.0;
    QPointF c(16.0, 16.0);
    qreal a1 = 145.0 * M_PI / 180.0;
    QPointF tip1(c.x() + r * std::cos(a1), c.y() - r * std::sin(a1));
    drawArrow(tip1, -145.0 - 90.0);

    qreal a2 = (145.0 + 180.0) * M_PI / 180.0;
    QPointF tip2(c.x() + r * std::cos(a2), c.y() - r * std::sin(a2));
    drawArrow(tip2, -145.0 - 90.0 + 180.0);

    p.end();

    s_rotateCursor = QCursor(pix, 16, 16);
}

QCursor MoveToolBase::cursor() const {
    if (!s_rotateCursorInitialized) {
        initRotateCursor();
    }

    if (m_dragAction == DragAction::Resizing) {
        switch (m_activeHandle) {
            case TransformHandle::NW:
            case TransformHandle::SE: return Qt::SizeFDiagCursor;
            case TransformHandle::NE:
            case TransformHandle::SW: return Qt::SizeBDiagCursor;
            case TransformHandle::N:
            case TransformHandle::S: return Qt::SizeVerCursor;
            case TransformHandle::E:
            case TransformHandle::W: return Qt::SizeHorCursor;
            default: return Qt::SizeAllCursor;
        }
    }
    if (m_dragAction == DragAction::Rotating) {
        return s_rotateCursor;
    }
    if (m_dragAction == DragAction::Translating) {
        return Qt::SizeAllCursor;
    }

    if (m_hoverHandle != TransformHandle::None) {
        if (m_mode == TransformMode::Rotate) {
            return s_rotateCursor;
        }
        switch (m_hoverHandle) {
            case TransformHandle::NW:
            case TransformHandle::SE: return Qt::SizeFDiagCursor;
            case TransformHandle::NE:
            case TransformHandle::SW: return Qt::SizeBDiagCursor;
            case TransformHandle::N:
            case TransformHandle::S: return Qt::SizeVerCursor;
            case TransformHandle::E:
            case TransformHandle::W: return Qt::SizeHorCursor;
            default: return Qt::ArrowCursor;
        }
    }

    if (m_hoverInside) {
        return Qt::SizeAllCursor;
    }

    return Qt::ArrowCursor;
}

std::array<QPointF, 4> MoveToolBase::currentQuad() const {
    std::array<QPointF, 4> quad;
    quad[0] = m_transform.map(QPointF(0, 0));
    quad[1] = m_transform.map(QPointF(m_localRect.width(), 0));
    quad[2] = m_transform.map(QPointF(m_localRect.width(), m_localRect.height()));
    quad[3] = m_transform.map(QPointF(0, m_localRect.height()));
    return quad;
}

QPointF MoveToolBase::currentCenter() const {
    auto quad = currentQuad();
    return (quad[0] + quad[1] + quad[2] + quad[3]) / 4.0;
}

QPointF MoveToolBase::docToVp(const QPointF& docPt) const {
    qreal zoom = m_lastRenderOpts.zoom > 0 ? m_lastRenderOpts.zoom : 1.0;
    return m_lastRenderOpts.panOffset + docPt * zoom;
}

TransformHandle MoveToolBase::hitTestHandle(const QPointF& docPos, const std::array<QPointF, 4>& quad) const {
    qreal zoom = m_lastRenderOpts.zoom > 0 ? m_lastRenderOpts.zoom : 1.0;
    qreal handleTol = 7.0 / zoom;

    // Corners
    if (QLineF(docPos, quad[0]).length() <= handleTol) return TransformHandle::NW;
    if (QLineF(docPos, quad[1]).length() <= handleTol) return TransformHandle::NE;
    if (QLineF(docPos, quad[2]).length() <= handleTol) return TransformHandle::SE;
    if (QLineF(docPos, quad[3]).length() <= handleTol) return TransformHandle::SW;

    // Edge midpoints
    QPointF midN = (quad[0] + quad[1]) / 2.0;
    QPointF midE = (quad[1] + quad[2]) / 2.0;
    QPointF midS = (quad[2] + quad[3]) / 2.0;
    QPointF midW = (quad[3] + quad[0]) / 2.0;

    if (QLineF(docPos, midN).length() <= handleTol) return TransformHandle::N;
    if (QLineF(docPos, midE).length() <= handleTol) return TransformHandle::E;
    if (QLineF(docPos, midS).length() <= handleTol) return TransformHandle::S;
    if (QLineF(docPos, midW).length() <= handleTol) return TransformHandle::W;

    return TransformHandle::None;
}

bool MoveToolBase::hitTestInside(const QPointF& docPos, Document* doc, const std::array<QPointF, 4>& quad) const {
    if (doc && !doc->selection().isEmpty()) {
        return doc->selection().contains(docPos);
    }
    QPolygonF poly;
    poly << quad[0] << quad[1] << quad[2] << quad[3];
    return poly.containsPoint(docPos, Qt::OddEvenFill);
}

void MoveToolBase::activate(Document* doc, ToolContext& /*ctx*/) {
    m_currentDoc = doc;
    m_hasSession = false;
    m_dragAction = DragAction::None;
    m_activeHandle = TransformHandle::None;
    m_hoverHandle = TransformHandle::None;
    m_hoverInside = false;
    if (doc) {
        initSessionFromDoc(doc);
    }
}

void MoveToolBase::deactivate(Document* doc, ToolContext& /*ctx*/) {
    if (doc && m_hasSession) {
        onSessionEnded(doc);
    }
    m_currentDoc = nullptr;
    m_hasSession = false;
    m_dragAction = DragAction::None;
    m_activeHandle = TransformHandle::None;
    m_hoverHandle = TransformHandle::None;
    m_hoverInside = false;
}

void MoveToolBase::initSessionFromDoc(Document* doc) {
    if (!doc) return;
    m_hasSession = false;
    onSessionStarted(doc);
}

void MoveToolBase::updateDocTransform(Document* doc) {
    if (!doc) return;
    onTransformUpdated(doc);
}

void MoveToolBase::mousePress(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!doc) return;

    if (!m_hasSession) {
        initSessionFromDoc(doc);
    } else if (isPixelTool()) {
        if (!doc->hasFloatingSelection() || m_transform != doc->floatingTransform()) {
            initSessionFromDoc(doc);
        }
    } else {
        if (m_transform.map(m_localPath) != doc->selection().path()) {
            initSessionFromDoc(doc);
        }
    }
    if (!m_hasSession) return;

    m_pressDocPos = docPos;
    m_pressVpPos = event->pos();
    m_isDrag = false;
    m_initialTransform = m_transform;
    m_initialQuad = currentQuad();
    m_rotateCenter = currentCenter();
    m_initialSelectionPath = doc->selection().path();
    m_needsLiftOnDrag = (isPixelTool() && !doc->hasFloatingSelection() && !doc->selection().isEmpty());
    m_wasLiftedInThisDrag = false;

    auto quad = m_initialQuad;
    TransformHandle hitHandle = hitTestHandle(docPos, quad);
    bool hitInside = hitTestInside(docPos, doc, quad);

    if ((hitInside || hitHandle != TransformHandle::None) && isPixelTool() && !doc->hasFloatingSelection() && !doc->selection().isEmpty()) {
        m_liftedLayerIndex = doc->activeLayerIndex();
        if (auto layer = doc->activeLayer()) {
            m_preLiftImage = layer->image().copy();
            doc->liftSelectionToFloating();
            m_postLiftImage = layer->image().copy();
            m_liftedFloatingImage = doc->floatingImage();
            m_initialTransform = doc->floatingTransform();
            m_transform = m_initialTransform;
            m_localRect = QRectF(0, 0, m_liftedFloatingImage.width(), m_liftedFloatingImage.height());
            m_localPath = doc->selection().path();
            QTransform inv = m_initialTransform.inverted();
            m_localPath = inv.map(m_localPath);
            m_initialQuad = currentQuad();
            m_rotateCenter = currentCenter();
            quad = m_initialQuad;
            m_wasLiftedInThisDrag = true;
        }
    }

    if (event->button() == Qt::RightButton) {
        m_dragAction = DragAction::Rotating;
        qreal dx = docPos.x() - m_rotateCenter.x();
        qreal dy = docPos.y() - m_rotateCenter.y();
        m_startAngleRad = std::atan2(dy, dx);
        m_prevAngleRad = m_startAngleRad;
        m_accumulatedAngleDeg = 0.0;
        m_baseQuadAngleDeg = std::atan2(m_initialQuad[1].y() - m_initialQuad[0].y(), m_initialQuad[1].x() - m_initialQuad[0].x()) * 180.0 / M_PI;
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (hitHandle != TransformHandle::None) {
            if (m_mode == TransformMode::Rotate) {
                m_dragAction = DragAction::Rotating;
                qreal dx = docPos.x() - m_rotateCenter.x();
                qreal dy = docPos.y() - m_rotateCenter.y();
                m_startAngleRad = std::atan2(dy, dx);
                m_prevAngleRad = m_startAngleRad;
                m_accumulatedAngleDeg = 0.0;
                m_baseQuadAngleDeg = std::atan2(m_initialQuad[1].y() - m_initialQuad[0].y(), m_initialQuad[1].x() - m_initialQuad[0].x()) * 180.0 / M_PI;
            } else {
                m_dragAction = DragAction::Resizing;
                m_activeHandle = hitHandle;
            }
        } else if (hitInside) {
            m_dragAction = DragAction::Translating;
        } else {
            if (isPixelTool()) {
                onSessionEnded(doc);
                m_hasSession = false;
            }
        }
    }
}

void MoveToolBase::mouseMove(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (!doc) return;

    if (m_dragAction != DragAction::None) {
        if ((event->pos() - m_pressVpPos).manhattanLength() > 3) {
            m_isDrag = true;
        }

        bool shiftHeld = (event->modifiers() & Qt::ShiftModifier);

        if (m_dragAction == DragAction::Translating) {
            QPointF delta = docPos - m_pressDocPos;
            QTransform t;
            t.translate(delta.x(), delta.y());
            m_transform = m_initialTransform * t;
            updateDocTransform(doc);
        } else if (m_dragAction == DragAction::Rotating) {
            qreal dx = docPos.x() - m_rotateCenter.x();
            qreal dy = docPos.y() - m_rotateCenter.y();
            if (std::hypot(dx, dy) > 1e-4) {
                qreal currentAngleRad = std::atan2(dy, dx);
                qreal stepRad = currentAngleRad - m_prevAngleRad;
                while (stepRad > M_PI) stepRad -= 2.0 * M_PI;
                while (stepRad < -M_PI) stepRad += 2.0 * M_PI;
                m_accumulatedAngleDeg += stepRad * 180.0 / M_PI;
                m_prevAngleRad = currentAngleRad;
            }

            qreal deltaDeg = m_accumulatedAngleDeg;

            if (shiftHeld) {
                qreal totalAngleDeg = m_baseQuadAngleDeg + deltaDeg;
                qreal snappedAngleDeg = std::round(totalAngleDeg / 15.0) * 15.0;
                deltaDeg = snappedAngleDeg - m_baseQuadAngleDeg;
            }

            QTransform rot;
            rot.translate(m_rotateCenter.x(), m_rotateCenter.y());
            rot.rotate(deltaDeg);
            rot.translate(-m_rotateCenter.x(), -m_rotateCenter.y());
            m_transform = m_initialTransform * rot;
            updateDocTransform(doc);
        } else if (m_dragAction == DragAction::Resizing) {
            applyResize(docPos, shiftHeld);
            updateDocTransform(doc);
        }
        return;
    }

    if (doc) {
        if (isPixelTool()) {
            if (doc->hasFloatingSelection()) {
                if (!m_hasSession || m_transform != doc->floatingTransform()) {
                    initSessionFromDoc(doc);
                }
            } else if (!doc->selection().isEmpty()) {
                if (!m_hasSession) initSessionFromDoc(doc);
            } else {
                m_hasSession = false;
            }
        } else {
            if (!doc->selection().isEmpty()) {
                if (!m_hasSession) initSessionFromDoc(doc);
            } else {
                m_hasSession = false;
            }
        }
    }

    if (m_hasSession) {
        auto quad = currentQuad();
        m_hoverHandle = hitTestHandle(docPos, quad);
        m_hoverInside = hitTestInside(docPos, doc, quad);
    } else {
        m_hoverHandle = TransformHandle::None;
        m_hoverInside = false;
    }
}

void MoveToolBase::mouseRelease(QMouseEvent* event, Document* doc, const QPointF& docPos, ToolContext& /*ctx*/) {
    if (m_isDrag && doc) {
        QString actionName;
        if (m_dragAction == DragAction::Translating) actionName = "Move";
        else if (m_dragAction == DragAction::Resizing) actionName = "Resize";
        else if (m_dragAction == DragAction::Rotating) actionName = "Rotate";
        else actionName = "Transform";

        if (isPixelTool()) {
            if (m_wasLiftedInThisDrag) {
                m_wasLiftedInThisDrag = false;
                doc->undoStack()->push(new LiftFloatingUndoCommand(doc, m_liftedLayerIndex,
                                                                   m_preLiftImage, m_postLiftImage,
                                                                   m_liftedFloatingImage,
                                                                   m_initialTransform, m_transform,
                                                                   m_initialSelectionPath, doc->selection().path(),
                                                                   actionName + " Pixels"));
            } else if (doc->hasFloatingSelection()) {
                doc->undoStack()->push(new TransformFloatingUndoCommand(doc,
                                                                       m_initialTransform, m_transform,
                                                                       m_initialSelectionPath, doc->selection().path(),
                                                                       actionName + " Pixels"));
            }
        } else {
            doc->undoStack()->push(new TransformSelectionUndoCommand(doc,
                                                                     m_initialSelectionPath, doc->selection().path(),
                                                                     actionName + " Selection"));
        }
    } else if (!m_isDrag && event->button() == Qt::LeftButton) {
        if (m_hasSession) {
            auto quad = currentQuad();
            if (hitTestInside(docPos, doc, quad)) {
                m_mode = (m_mode == TransformMode::Resize) ? TransformMode::Rotate : TransformMode::Resize;
                m_hoverHandle = hitTestHandle(docPos, quad);
                m_hoverInside = true;
                if (doc) {
                    emit doc->documentChanged();
                }
            }
        }
    }

    m_dragAction = DragAction::None;
    m_activeHandle = TransformHandle::None;
    m_isDrag = false;
    m_needsLiftOnDrag = false;
    m_wasLiftedInThisDrag = false;
    m_accumulatedAngleDeg = 0.0;
}

void MoveToolBase::nudge(Document* doc, qreal dx, qreal dy) {
    if (!doc) return;
    if (!m_hasSession) {
        initSessionFromDoc(doc);
    }
    if (!m_hasSession) return;

    QTransform t;
    t.translate(dx, dy);
    m_transform = m_transform * t;
    updateDocTransform(doc);
}

void MoveToolBase::keyPress(QKeyEvent* event, Document* doc, ToolContext& /*ctx*/) {
    if (!doc) return;
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (isPixelTool() && doc->hasFloatingSelection()) {
            onSessionEnded(doc);
            m_hasSession = false;
            event->accept();
        }
    } else if (event->key() == Qt::Key_Escape) {
        if (isPixelTool() && doc->hasFloatingSelection()) {
            onSessionEnded(doc);
            m_hasSession = false;
            event->accept();
        }
    }
}

void MoveToolBase::drawOverlay(QPainter& painter, const RenderOptions& opts) {
    m_lastRenderOpts = opts;
    if (m_currentDoc && !m_isDrag) {
        if (isPixelTool()) {
            if (m_currentDoc->hasFloatingSelection()) {
                if (!m_hasSession || m_transform != m_currentDoc->floatingTransform()) {
                    initSessionFromDoc(m_currentDoc);
                }
            } else if (!m_currentDoc->selection().isEmpty()) {
                if (!m_hasSession) initSessionFromDoc(m_currentDoc);
            } else {
                m_hasSession = false;
            }
        } else {
            if (!m_currentDoc->selection().isEmpty()) {
                if (!m_hasSession || m_transform.map(m_localPath) != m_currentDoc->selection().path()) {
                    initSessionFromDoc(m_currentDoc);
                }
            } else {
                m_hasSession = false;
            }
        }
    }
    if (!m_hasSession) return;

    auto quad = currentQuad();

    std::array<QPointF, 4> vpQuad;
    for (int i = 0; i < 4; ++i) {
        vpQuad[i] = docToVp(quad[i]);
    }

    painter.save();

    std::array<QPointF, 8> handles;
    handles[0] = vpQuad[0];                     // NW
    handles[1] = (vpQuad[0] + vpQuad[1]) / 2.0; // N
    handles[2] = vpQuad[1];                     // NE
    handles[3] = (vpQuad[1] + vpQuad[2]) / 2.0; // E
    handles[4] = vpQuad[2];                     // SE
    handles[5] = (vpQuad[2] + vpQuad[3]) / 2.0; // S
    handles[6] = vpQuad[3];                     // SW
    handles[7] = (vpQuad[3] + vpQuad[0]) / 2.0; // W

    qreal hSize = 7.0;
    qreal hHalf = hSize / 2.0;

    if (m_mode == TransformMode::Resize) {
        for (const auto& h : handles) {
            QRectF r(h.x() - hHalf, h.y() - hHalf, hSize, hSize);
            painter.fillRect(r, Qt::white);
            painter.setPen(QPen(QColor(40, 40, 40), 1.0));
            painter.drawRect(r);
        }
    } else {
        QPointF vpCenter = docToVp(currentCenter());
        painter.setPen(QPen(QColor(30, 30, 30), 1.2));
        painter.setBrush(QBrush(QColor(255, 255, 255, 200)));
        painter.drawEllipse(vpCenter, 5.0, 5.0);
        painter.drawLine(vpCenter - QPointF(8, 0), vpCenter + QPointF(8, 0));
        painter.drawLine(vpCenter - QPointF(0, 8), vpCenter + QPointF(0, 8));

        for (const auto& h : handles) {
            painter.setPen(QPen(QColor(30, 30, 30), 1.2));
            painter.setBrush(Qt::white);
            painter.drawEllipse(h, 4.0, 4.0);
        }
    }

    painter.restore();
}

void MoveToolBase::applyResize(const QPointF& docPos, bool shiftHeld) {
    qreal origW = m_localRect.width();
    qreal origH = m_localRect.height();
    if (origW <= 0 || origH <= 0) return;

    QPointF p0 = m_initialQuad[0];
    QPointF p1 = m_initialQuad[1];
    QPointF p2 = m_initialQuad[2];
    QPointF p3 = m_initialQuad[3];

    QPointF u_vec = p1 - p0;
    QPointF v_vec = p3 - p0;
    qreal currW = std::sqrt(u_vec.x() * u_vec.x() + u_vec.y() * u_vec.y());
    qreal currH = std::sqrt(v_vec.x() * v_vec.x() + v_vec.y() * v_vec.y());
    if (currW < 1e-4 || currH < 1e-4) return;

    QPointF u_unit = u_vec / currW;
    QPointF v_unit = v_vec / currH;

    QPointF new_p0 = p0, new_p1 = p1, new_p2 = p2, new_p3 = p3;

    switch (m_activeHandle) {
        case TransformHandle::SE: {
            QPointF vec = docPos - p0;
            qreal w = vec.x() * u_unit.x() + vec.y() * u_unit.y();
            qreal h = vec.x() * v_unit.x() + vec.y() * v_unit.y();
            if (shiftHeld) {
                qreal s = std::max(std::abs(w) / origW, std::abs(h) / origH);
                w = (w >= 0 ? 1 : -1) * s * origW;
                h = (h >= 0 ? 1 : -1) * s * origH;
            }
            new_p0 = p0;
            new_p1 = p0 + u_unit * w;
            new_p3 = p0 + v_unit * h;
            new_p2 = p0 + u_unit * w + v_unit * h;
            break;
        }
        case TransformHandle::NW: {
            QPointF vec = docPos - p2;
            qreal w = vec.x() * (-u_unit.x()) + vec.y() * (-u_unit.y());
            qreal h = vec.x() * (-v_unit.x()) + vec.y() * (-v_unit.y());
            if (shiftHeld) {
                qreal s = std::max(std::abs(w) / origW, std::abs(h) / origH);
                w = (w >= 0 ? 1 : -1) * s * origW;
                h = (h >= 0 ? 1 : -1) * s * origH;
            }
            new_p2 = p2;
            new_p3 = p2 - u_unit * w;
            new_p1 = p2 - v_unit * h;
            new_p0 = p2 - u_unit * w - v_unit * h;
            break;
        }
        case TransformHandle::NE: {
            QPointF vec = docPos - p3;
            qreal w = vec.x() * u_unit.x() + vec.y() * u_unit.y();
            qreal h = vec.x() * (-v_unit.x()) + vec.y() * (-v_unit.y());
            if (shiftHeld) {
                qreal s = std::max(std::abs(w) / origW, std::abs(h) / origH);
                w = (w >= 0 ? 1 : -1) * s * origW;
                h = (h >= 0 ? 1 : -1) * s * origH;
            }
            new_p3 = p3;
            new_p0 = p3 - v_unit * h;
            new_p2 = p3 + u_unit * w;
            new_p1 = p3 + u_unit * w - v_unit * h;
            break;
        }
        case TransformHandle::SW: {
            QPointF vec = docPos - p1;
            qreal w = vec.x() * (-u_unit.x()) + vec.y() * (-u_unit.y());
            qreal h = vec.x() * v_unit.x() + vec.y() * v_unit.y();
            if (shiftHeld) {
                qreal s = std::max(std::abs(w) / origW, std::abs(h) / origH);
                w = (w >= 0 ? 1 : -1) * s * origW;
                h = (h >= 0 ? 1 : -1) * s * origH;
            }
            new_p1 = p1;
            new_p0 = p1 - u_unit * w;
            new_p2 = p1 + v_unit * h;
            new_p3 = p1 - u_unit * w + v_unit * h;
            break;
        }
        case TransformHandle::E: {
            QPointF vec = docPos - p0;
            qreal w = vec.x() * u_unit.x() + vec.y() * u_unit.y();
            if (shiftHeld) {
                qreal h = currH * (std::abs(w) / currW);
                qreal dh = (h - currH) / 2.0;
                new_p0 = p0 - v_unit * dh;
                new_p3 = p3 + v_unit * dh;
                new_p1 = new_p0 + u_unit * w;
                new_p2 = new_p3 + u_unit * w;
            } else {
                new_p0 = p0;
                new_p3 = p3;
                new_p1 = p0 + u_unit * w;
                new_p2 = p3 + u_unit * w;
            }
            break;
        }
        case TransformHandle::W: {
            QPointF vec = docPos - p1;
            qreal w = vec.x() * (-u_unit.x()) + vec.y() * (-u_unit.y());
            if (shiftHeld) {
                qreal h = currH * (std::abs(w) / currW);
                qreal dh = (h - currH) / 2.0;
                new_p1 = p1 - v_unit * dh;
                new_p2 = p2 + v_unit * dh;
                new_p0 = new_p1 - u_unit * w;
                new_p3 = new_p2 - u_unit * w;
            } else {
                new_p1 = p1;
                new_p2 = p2;
                new_p0 = p1 - u_unit * w;
                new_p3 = p2 - u_unit * w;
            }
            break;
        }
        case TransformHandle::S: {
            QPointF vec = docPos - p0;
            qreal h = vec.x() * v_unit.x() + vec.y() * v_unit.y();
            if (shiftHeld) {
                qreal w = currW * (std::abs(h) / currH);
                qreal dw = (w - currW) / 2.0;
                new_p0 = p0 - u_unit * dw;
                new_p1 = p1 + u_unit * dw;
                new_p3 = new_p0 + v_unit * h;
                new_p2 = new_p1 + v_unit * h;
            } else {
                new_p0 = p0;
                new_p1 = p1;
                new_p3 = p0 + v_unit * h;
                new_p2 = p1 + v_unit * h;
            }
            break;
        }
        case TransformHandle::N: {
            QPointF vec = docPos - p3;
            qreal h = vec.x() * (-v_unit.x()) + vec.y() * (-v_unit.y());
            if (shiftHeld) {
                qreal w = currW * (std::abs(h) / currH);
                qreal dw = (w - currW) / 2.0;
                new_p3 = p3 - u_unit * dw;
                new_p2 = p2 + u_unit * dw;
                new_p0 = new_p3 - v_unit * h;
                new_p1 = new_p2 - v_unit * h;
            } else {
                new_p3 = p3;
                new_p2 = p2;
                new_p0 = p3 - v_unit * h;
                new_p1 = p2 - v_unit * h;
            }
            break;
        }
        default:
            break;
    }

    qreal m11 = (new_p1.x() - new_p0.x()) / origW;
    qreal m12 = (new_p1.y() - new_p0.y()) / origW;
    qreal m21 = (new_p3.x() - new_p0.x()) / origH;
    qreal m22 = (new_p3.y() - new_p0.y()) / origH;
    qreal dx  = new_p0.x();
    qreal dy  = new_p0.y();
    m_transform = QTransform(m11, m12, 0, m21, m22, 0, dx, dy, 1);
}

// --- MoveSelectionTool ---
void MoveSelectionTool::initSessionFromDoc(Document* doc) {
    if (!doc || doc->selection().isEmpty()) {
        m_hasSession = false;
        return;
    }
    m_startRegion = doc->selection().region();
    QRectF bounds = doc->selection().boundingRect();
    if (bounds.isEmpty() || bounds.width() <= 0 || bounds.height() <= 0) {
        m_hasSession = false;
        return;
    }
    m_localRect = QRectF(0, 0, bounds.width(), bounds.height());
    m_transform = QTransform::fromTranslate(bounds.x(), bounds.y());
    m_localPath = doc->selection().path();
    m_localPath.translate(-bounds.x(), -bounds.y());
    m_hasSession = true;
}

void MoveSelectionTool::onSessionStarted(Document* doc) {
    initSessionFromDoc(doc);
}

void MoveSelectionTool::onTransformUpdated(Document* doc) {
    if (!doc) return;
    doc->selection().setPath(m_transform.map(m_localPath));
    emit doc->selectionChanged();
    emit doc->documentChanged();
}

void MoveSelectionTool::onSessionEnded(Document* doc) {
    Q_UNUSED(doc);
    m_hasSession = false;
}

void MoveSelectionTool::nudge(Document* doc, qreal dx, qreal dy) {
    if (!doc || doc->selection().isEmpty()) return;
    if (!m_hasSession) {
        initSessionFromDoc(doc);
    }
    if (!m_hasSession) return;

    QPainterPath oldPath = doc->selection().path();
    QTransform t;
    t.translate(dx, dy);
    m_transform = m_transform * t;
    doc->selection().setPath(m_transform.map(m_localPath));
    QPainterPath newPath = doc->selection().path();

    doc->undoStack()->push(new TransformSelectionUndoCommand(doc, oldPath, newPath, "Move Selection"));
    emit doc->selectionChanged();
    emit doc->documentChanged();
}

// --- MoveSelectedPixelsTool ---
void MoveSelectedPixelsTool::initSessionFromDoc(Document* doc) {
    if (!doc) {
        m_hasSession = false;
        return;
    }
    if (doc->hasFloatingSelection()) {
        const QImage& img = doc->originalFloatingImage().isNull() ? doc->floatingImage() : doc->originalFloatingImage();
        if (img.width() <= 0 || img.height() <= 0) {
            m_hasSession = false;
            return;
        }
        m_localRect = QRectF(0, 0, img.width(), img.height());
        m_transform = doc->floatingTransform();
        m_localPath = doc->selection().path();
        QTransform inv = m_transform.inverted();
        m_localPath = inv.map(m_localPath);
        m_hasSession = true;
        return;
    }
    if (!doc->selection().isEmpty()) {
        QRectF bounds = doc->selection().boundingRect();
        if (bounds.width() <= 0 || bounds.height() <= 0) {
            m_hasSession = false;
            return;
        }
        m_localRect = QRectF(0, 0, bounds.width(), bounds.height());
        m_transform = QTransform::fromTranslate(bounds.x(), bounds.y());
        m_localPath = doc->selection().path();
        m_localPath.translate(-bounds.x(), -bounds.y());
        m_hasSession = true;
        return;
    }
    m_hasSession = false;
}

void MoveSelectedPixelsTool::onSessionStarted(Document* doc) {
    initSessionFromDoc(doc);
}

void MoveSelectedPixelsTool::onTransformUpdated(Document* doc) {
    if (!doc || !doc->hasFloatingSelection()) return;
    doc->setFloatingTransform(m_transform);
    doc->selection().setPath(m_transform.map(m_localPath));
    emit doc->selectionChanged();
    emit doc->documentChanged();
}

void MoveSelectedPixelsTool::onSessionEnded(Document* doc) {
    commit(doc);
}

void MoveSelectedPixelsTool::commit(Document* doc) {
    if (!doc) return;
    if (doc->hasFloatingSelection()) {
        doc->bakeFloatingSelection(true, "Deselect");
    }
    m_hasSession = false;
}

void MoveSelectedPixelsTool::nudge(Document* doc, qreal dx, qreal dy) {
    if (!doc) return;
    if (!doc->hasFloatingSelection()) {
        if (doc->selection().isEmpty()) return;
        int layerIdx = doc->activeLayerIndex();
        auto layer = doc->activeLayer();
        if (!layer) return;
        QImage preLift = layer->image().copy();
        QPainterPath initialSel = doc->selection().path();
        doc->liftSelectionToFloating();
        QImage postLift = layer->image().copy();
        QImage floatImg = doc->floatingImage();
        QTransform initXform = doc->floatingTransform();
        initSessionFromDoc(doc);
        QTransform t;
        t.translate(dx, dy);
        m_transform = m_transform * t;
        doc->setFloatingTransform(m_transform);
        doc->selection().setPath(m_transform.map(m_localPath));
        doc->undoStack()->push(new LiftFloatingUndoCommand(doc, layerIdx, preLift, postLift, floatImg, initXform, m_transform, initialSel, doc->selection().path(), "Move Pixels"));
        emit doc->selectionChanged();
        emit doc->documentChanged();
        return;
    }
    initSessionFromDoc(doc);
    QTransform oldXform = doc->floatingTransform();
    QPainterPath oldPath = doc->selection().path();
    QTransform t;
    t.translate(dx, dy);
    m_transform = m_transform * t;
    doc->setFloatingTransform(m_transform);
    doc->selection().setPath(m_transform.map(m_localPath));
    doc->undoStack()->push(new TransformFloatingUndoCommand(doc, oldXform, m_transform, oldPath, doc->selection().path(), "Move Pixels"));
    emit doc->selectionChanged();
    emit doc->documentChanged();
}

} // namespace pdn
