#include "UBEditableShapeItem.h"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOptionGraphicsItem>
#include <QtMath>
#include <QCursor>
#include <QTimer>
#include <QKeyEvent>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>

#include "core/UBApplication.h"
#include "domain/UBGraphicsScene.h"
#include "domain/UBGraphicsItemUndoCommand.h"
#include "core/UBSettings.h"
#include "board/UBDrawingController.h"

// WistOpenboard fork: shared diagnostic sink, also used by UBBoardView to
// trace where a click on a shape's delete chip actually goes.
void ubShapeDebugLog(const QString& line)
{
    // Off unless WISTOPENBOARD_SHAPE_DEBUG is set. The instrumentation earned
    // its keep tracking down the click-routing faults, so it stays in the
    // source -- but a teacher build must not write a log on every tap.
    static const bool enabled = !qEnvironmentVariableIsEmpty("WISTOPENBOARD_SHAPE_DEBUG");
    if (!enabled)
        return;

    static const QString path = UBSettings::userDataDirectory() + "/shape-debug.log";
    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream(&f) << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
                    << "  " << line << Qt::endl;
    f.close();
}

namespace {
// WistOpenboard fork: shape-interaction diagnostics. Selection here has
// proven hard to verify by eye ("no X appears"), and the input path differs
// between mouse, pen and finger. Every hover/press/release is appended to
//   %LOCALAPPDATA%/OpenBoard/shape-debug.log
// so a single user test yields evidence instead of guesswork.
void shapeLog(const QString& line)
{
    static const QString path = UBSettings::userDataDirectory() + "/shape-debug.log";
    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream(&f) << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
                    << "  " << line << "\n";
    f.close();
}

int currentToolId()
{
    if (UBDrawingController* dc = UBDrawingController::drawingController())
        return dc->stylusTool();
    return -1;
}
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

UBEditableShapeItem::UBEditableShapeItem(Kind kind, const QPolygonF &vertices, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , mKind(kind)
    , mVertices(vertices)
    , mPen(QColor(0, 0, 0), 4)
    , mBrush(QColor(255, 255, 255, 1))
{
    mPen.setJoinStyle(Qt::RoundJoin);
    mPen.setCapStyle(Qt::RoundCap);

    // We handle move ourselves so handle drags don't also move the body.
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsFocusable, true);
    setFlag(ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);

    // Rotate around the centroid of the initial vertices.
    setTransformOriginPoint(localBBox().center());
}

void UBEditableShapeItem::setStrokeColor(const QColor &c)
{
    mPen.setColor(c);
    update();
}

void UBEditableShapeItem::setStrokeWidth(qreal w)
{
    mPen.setWidthF(w);
    update();
}

QRectF UBEditableShapeItem::localBBox() const
{
    return mVertices.boundingRect();
}

QRectF UBEditableShapeItem::boundingRect() const
{
    qreal pad = HANDLE_SIZE + ROT_OFFSET + mPen.widthF() + 8;
    return localBBox().adjusted(-pad, -pad, pad, pad);
}

QRectF UBEditableShapeItem::deleteChipRect() const
{
    // ✕ chip outside the top-left of the bbox. Finger-sized: on the
    // classroom boards a chip under ~20px is nearly impossible to hit.
    const qreal s = HANDLE_SIZE * 1.5;
    QRectF bb = localBBox();
    return QRectF(bb.left() - s * 0.8, bb.top() - s * 1.3, s, s);
}

// The visible chip is small so it does not dominate the shape, but the
// TARGET is much larger. Logged near-misses of 1-4 px against the drawn
// rect were being silently discarded, which read to the user as "the X
// does not work" -- an unforgiving hit box, not a broken button.
QRectF UBEditableShapeItem::deleteChipHitRect() const
{
    const qreal halo = HANDLE_SIZE;   // ~18 units of slack on every side
    return deleteChipRect().adjusted(-halo, -halo, halo, halo);
}

// Polygons (triangle, rhombus, trapezoid, hexagon...) have per-vertex
// handles, which reshape rather than resize. They get an extra grip outside
// the bottom-right corner that scales the whole figure about its centre --
// what a teacher means by "make it bigger".
bool UBEditableShapeItem::hasScaleGrip() const
{
    return mKind == Polygon;
}

QRectF UBEditableShapeItem::scaleGripRect() const
{
    const qreal s = HANDLE_SIZE * 1.3;
    QRectF bb = localBBox();
    return QRectF(bb.right() + s * 0.3, bb.bottom() + s * 0.3, s, s);
}

QRectF UBEditableShapeItem::scaleGripHitRect() const
{
    const qreal halo = HANDLE_SIZE;
    return scaleGripRect().adjusted(-halo, -halo, halo, halo);
}

// Corner/vertex grips are drawn at HANDLE_SIZE but must be catchable with a
// fingertip on a smartboard, so the target is twice the drawn square.
QRectF UBEditableShapeItem::handleHitRect(const QPointF& centre) const
{
    const qreal s = HANDLE_SIZE * 2.0;
    return QRectF(centre.x() - s / 2, centre.y() - s / 2, s, s);
}

QPainterPath UBEditableShapeItem::shape() const
{
    QPainterPath body;
    // Winding fill, NOT the default odd-even: the selected hit region is built
    // from overlapping pieces (body, delete chip, handles, and the rect that
    // bridges the gap to handles sitting outside the body). Under odd-even the
    // overlaps CANCEL, which punched holes exactly where the user clicks --
    // selecting a shape made its own body and delete chip untouchable.
    body.setFillRule(Qt::WindingFill);

    if (mKind == Ellipse || mKind == Circle) {
        body.addEllipse(localBBox());
    } else {
        body.addPolygon(mVertices);
        body.closeSubpath();
    }

    if (!isSelected())
        return body;

    // Include the delete chip (with its halo) in the hit area when visible.
    body.addRect(deleteChipHitRect());

    // When selected, expand the hit-area to include the handles so they receive clicks.
    QPainterPath p = body;
    p.setFillRule(Qt::WindingFill);
    const qreal s = HANDLE_SIZE;
    for (const QPointF &h : handlePositions())
        p.addRect(handleHitRect(h));
    QPointF rp = rotationHandlePos();
    p.addEllipse(rp, s, s);
    if (hasScaleGrip())
        p.addRect(scaleGripHitRect());

    // Hover continuity: selection lives on hover, and the delete chip and
    // rotation handle sit OUTSIDE a non-rectangular body (e.g. a rhombus's
    // empty corners). Without bridging, moving the mouse from the body toward
    // the chip fires hoverLeave -> deselect and the chip vanishes before it
    // can be clicked. Cover the whole bbox plus a margin reaching the chip
    // and rotation handle; clicks in the dead space still fall through
    // (mousePressEvent ignores them).
    p.addRect(localBBox().adjusted(-s * 1.8, -(ROT_OFFSET + s), s * 1.8, s));
    return p;
}

QList<QPointF> UBEditableShapeItem::handlePositions() const
{
    if (mKind == Polygon) {
        QList<QPointF> r;
        for (const QPointF &v : mVertices) r << v;
        return r;
    }
    QRectF bb = localBBox();
    return { bb.topLeft(), bb.topRight(), bb.bottomRight(), bb.bottomLeft() };
}

QPointF UBEditableShapeItem::rotationHandlePos() const
{
    QRectF bb = localBBox();
    return QPointF(bb.center().x(), bb.top() - ROT_OFFSET);
}

int UBEditableShapeItem::handleAt(const QPointF &localPos) const
{
    if (!isSelected()) return H_None;
    // Delete chip first (small target, want priority over body hits).
    if (deleteChipHitRect().contains(localPos)) return H_Delete;
    const qreal s = HANDLE_SIZE;
    // Uniform-resize grip next: it sits outside the bbox like the chip.
    if (hasScaleGrip() && scaleGripHitRect().contains(localPos)) return H_Scale;
    // Rotation (it's outside the body, easy hit).
    QPointF rp = rotationHandlePos();
    if (QLineF(localPos, rp).length() <= s/2 + 4) return H_Rotation;
    const QList<QPointF> hs = handlePositions();
    for (int i = 0; i < hs.size(); ++i) {
        if (handleHitRect(hs[i]).contains(localPos)) return i;
    }
    return H_None;
}

void UBEditableShapeItem::paint(QPainter *p, const QStyleOptionGraphicsItem *o, QWidget *w)
{
    Q_UNUSED(o); Q_UNUSED(w);
    p->setRenderHint(QPainter::Antialiasing);

    p->setPen(mPen);
    p->setBrush(mBrush);
    if (mKind == Ellipse || mKind == Circle) {
        p->drawEllipse(localBBox());
    } else {
        p->drawPolygon(mVertices);
    }

    if (!isSelected()) return;

    // Delete chip — small red circle with a white ✕, only visible when
    // selected (selection happens on hover, see hoverEnterEvent).
    {
        QRectF dr = deleteChipRect();
        p->setPen(QPen(QColor(80, 80, 80), 1));
        p->setBrush(QColor(200, 60, 60, 220));
        p->drawEllipse(dr);
        p->setPen(QPen(QColor(255, 255, 255), 1.6));
        qreal pad = dr.width() * 0.28;
        p->drawLine(dr.left() + pad, dr.top() + pad, dr.right() - pad, dr.bottom() - pad);
        p->drawLine(dr.right() - pad, dr.top() + pad, dr.left() + pad, dr.bottom() - pad);
    }

    // Selection outline (subtle dashed bbox)
    QPen sel(QColor(80, 130, 200, 180), 1, Qt::DashLine);
    p->setPen(sel);
    p->setBrush(Qt::NoBrush);
    p->drawRect(localBBox());

    // Handles
    QPen handlePen(QColor(40, 90, 200), 1.5);
    QBrush handleBrush(QColor(255, 255, 255));
    p->setPen(handlePen);
    p->setBrush(handleBrush);
    const qreal s = HANDLE_SIZE;
    for (const QPointF &h : handlePositions())
        p->drawRect(QRectF(h.x() - s/2, h.y() - s/2, s, s));

    // Rotation: line from top-center to handle, green circle
    QPointF rp = rotationHandlePos();
    QPointF topMid(localBBox().center().x(), localBBox().top());
    p->setPen(QPen(QColor(80, 130, 200, 180), 1, Qt::DashLine));
    p->drawLine(topMid, rp);
    p->setPen(handlePen);
    p->setBrush(QColor(60, 200, 110));
    p->drawEllipse(rp, s/2 + 1, s/2 + 1);

    // Uniform-resize grip: a white square carrying a blue diagonal arrow,
    // echoing the resize corner used elsewhere in the app.
    if (hasScaleGrip()) {
        QRectF gr = scaleGripRect();
        p->setPen(handlePen);
        p->setBrush(QColor(255, 255, 255));
        p->drawRoundedRect(gr, 3, 3);
        p->setPen(QPen(QColor(40, 90, 200), 2.0, Qt::SolidLine, Qt::RoundCap));
        const qreal pad = gr.width() * 0.30;
        const qreal barb = gr.width() * 0.20;
        p->drawLine(QPointF(gr.left() + pad, gr.bottom() - pad),
                    QPointF(gr.right() - pad, gr.top() + pad));
        p->drawLine(QPointF(gr.right() - pad, gr.top() + pad),
                    QPointF(gr.right() - pad - barb, gr.top() + pad));
        p->drawLine(QPointF(gr.right() - pad, gr.top() + pad),
                    QPointF(gr.right() - pad, gr.top() + pad + barb));
        p->drawLine(QPointF(gr.left() + pad, gr.bottom() - pad),
                    QPointF(gr.left() + pad + barb, gr.bottom() - pad));
        p->drawLine(QPointF(gr.left() + pad, gr.bottom() - pad),
                    QPointF(gr.left() + pad, gr.bottom() - pad - barb));
    }
}

void UBEditableShapeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    // Auto-select on hover so the user can interact (delete X, handles, drag)
    // without first having to click — and without first having to switch to
    // the Selector tool. Other items keep their own selection state.
    shapeLog(QString("hoverEnter tool=%1 selected=%2").arg(currentToolId()).arg(isSelected()));
    setSelected(true);
    QGraphicsObject::hoverEnterEvent(e);
}

void UBEditableShapeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
    // Selection is STICKY: leaving the item must not drop it.
    //
    // It used to deselect here, which made the handles and the delete chip
    // vanish the instant the pointer left the body — and since the chip sits
    // OUTSIDE the body (in a rhombus's empty corner), it could never be
    // reached: moving toward it destroyed it. Selection now ends only when
    // the user selects something else or clicks empty canvas, which is what
    // every other object on the board does.
    shapeLog(QString("hoverLeave (selection kept) activeHandle=%1").arg(mActiveHandle));
    if (mActiveHandle == H_None)
        unsetCursor();
    QGraphicsObject::hoverLeaveEvent(e);
}

void UBEditableShapeItem::hoverMoveEvent(QGraphicsSceneHoverEvent *e)
{
    if (!isSelected()) {
        unsetCursor();
        QGraphicsObject::hoverMoveEvent(e);
        return;
    }
    int h = handleAt(e->pos());
    if (h == H_Delete)        setCursor(Qt::PointingHandCursor);
    else if (h == H_Scale)    setCursor(Qt::SizeFDiagCursor);
    else if (h == H_Rotation) setCursor(Qt::CrossCursor);
    else if (h >= 0)          setCursor(Qt::SizeAllCursor);
    else                      setCursor(Qt::OpenHandCursor);
    QGraphicsObject::hoverMoveEvent(e);
}

void UBEditableShapeItem::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    // Finger touch generates no hover events, so selection must happen on
    // the tap itself (same pattern as UBGraphicsCurveItem). The delete chip
    // only responds when it was already visible BEFORE this tap, so a blind
    // first tap next to an unselected shape can never silently delete it.
    const bool wasSelected = isSelected();
    setSelected(true);

    int h = handleAt(e->pos());
    shapeLog(QString("press tool=%1 wasSelected=%2 handle=%3 pos=(%4,%5)")
             .arg(currentToolId()).arg(wasSelected).arg(h)
             .arg(int(e->pos().x())).arg(int(e->pos().y())));
    QPainterPath body;
    if (mKind == Ellipse || mKind == Circle) body.addEllipse(localBBox());
    else { body.addPolygon(mVertices); body.closeSubpath(); }

    if (h == H_Delete) {
        if (wasSelected) {
            mActiveHandle = H_Delete;    // resolved on release
            e->accept();
            return;
        }
        h = H_None;
    }

    if (h != H_None) {
        mActiveHandle = h;
    } else if (body.contains(e->pos())) {
        mActiveHandle = H_Body;
        setCursor(Qt::ClosedHandCursor);
    } else {
        setSelected(wasSelected);
        e->ignore();
        return;
    }
    setFocus();
    mPressScenePos = e->scenePos();
    mVerticesAtPress = mVertices;

    if (mActiveHandle == H_Scale) {
        mScaleCentre = localBBox().center();
        mScaleStartDist = QLineF(mScaleCentre, e->pos()).length();
        if (mScaleStartDist < 1.0)
            mScaleStartDist = 1.0;
    }
    e->accept();
}

void UBEditableShapeItem::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    if (mActiveHandle == H_None) return;

    if (mActiveHandle == H_Body) {
        QPointF deltaScene = e->scenePos() - mPressScenePos;
        setPos(pos() + deltaScene);
        mPressScenePos = e->scenePos();
        return;
    }

    if (mActiveHandle == H_Scale) {
        const qreal dist = QLineF(mScaleCentre, e->pos()).length();
        qreal factor = dist / mScaleStartDist;
        factor = qBound(0.15, factor, 8.0);

        prepareGeometryChange();
        mVertices.clear();
        for (const QPointF& v : mVerticesAtPress)
            mVertices << mScaleCentre + (v - mScaleCentre) * factor;
        update();
        return;
    }

    if (mActiveHandle == H_Rotation) {
        QPointF centerScene = mapToScene(localBBox().center());
        QPointF v = e->scenePos() - centerScene;
        // 0° = handle pointing straight up
        qreal angle = qAtan2(v.y(), v.x()) * 180.0 / M_PI + 90.0;
        setTransformOriginPoint(localBBox().center());
        setRotation(angle);
        return;
    }

    // Vertex/corner drag
    prepareGeometryChange();
    const QPointF newPos = e->pos();

    if (mKind == Polygon) {
        if (mActiveHandle >= 0 && mActiveHandle < mVertices.size()) {
            mVertices[mActiveHandle] = newPos;
        }
    } else {
        // Rect/Square/Ellipse/Circle: opposite corner anchors; we re-compute the bbox.
        const int idx    = mActiveHandle;
        const int oppIdx = (idx + 2) % 4;
        const QPointF opposite = mVerticesAtPress[oppIdx];
        QRectF bb = QRectF(opposite, newPos).normalized();

        if (mKind == Square || mKind == Circle) {
            qreal side = qMax(bb.width(), bb.height());
            // Keep the opposite corner fixed; expand toward the dragged corner.
            qreal x = (newPos.x() < opposite.x()) ? opposite.x() - side : opposite.x();
            qreal y = (newPos.y() < opposite.y()) ? opposite.y() - side : opposite.y();
            bb = QRectF(x, y, side, side);
        }

        mVertices.clear();
        mVertices << bb.topLeft() << bb.topRight() << bb.bottomRight() << bb.bottomLeft();
    }
    update();
}

void UBEditableShapeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    const bool releasedOnChip = (mActiveHandle == H_Delete)
            && deleteChipHitRect().contains(e->pos());

    shapeLog(QString("release handle=%1 onChip=%2").arg(mActiveHandle).arg(releasedOnChip));

    mActiveHandle = H_None;
    unsetCursor();

    if (releasedOnChip) {
        // Deferred: removing the item from inside its own mouse handler
        // would pull the scene's mouse grabber out from under Qt.
        QTimer::singleShot(0, this, [this]() { removeSelfWithUndo(); });
    }
}

// Mirrors UBGraphicsCurveItem::removeSelfWithUndo(): out of the scene, one
// undo entry; the undo command takes over the item's lifetime.
void UBEditableShapeItem::removeSelfWithUndo()
{
    UBGraphicsScene* ubScene = dynamic_cast<UBGraphicsScene*>(scene());

    if (!ubScene) {
        if (scene())
            scene()->removeItem(this);
        deleteLater();
        return;
    }

    auto sharedScene = ubScene->shared_from_this();
    sharedScene->removeItem(this);

    UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(sharedScene, this, nullptr);
    UBApplication::undoStack->push(uc);
}

// Keyboard fallback: a selected shape responds to Delete/Backspace, so the
// shape can always be removed even if the chip is hard to hit on a given
// input device.
void UBEditableShapeItem::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        shapeLog("keyDelete");
        e->accept();
        QTimer::singleShot(0, this, [this]() { removeSelfWithUndo(); });
        return;
    }
    QGraphicsObject::keyPressEvent(e);
}

QVariant UBEditableShapeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSelectedHasChanged) {
        prepareGeometryChange(); // bbox grows when selected (handles)
        update();
    }
    return QGraphicsObject::itemChange(change, value);
}
