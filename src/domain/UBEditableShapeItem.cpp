#include "UBEditableShapeItem.h"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOptionGraphicsItem>
#include <QtMath>
#include <QCursor>

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
    // Small ✕ chip just outside the top-left of the bbox.
    const qreal s = HANDLE_SIZE * 1.1;
    QRectF bb = localBBox();
    return QRectF(bb.left() - s * 0.6, bb.top() - s * 1.2, s, s);
}

QPainterPath UBEditableShapeItem::shape() const
{
    QPainterPath body;
    if (mKind == Ellipse || mKind == Circle) {
        body.addEllipse(localBBox());
    } else {
        body.addPolygon(mVertices);
        body.closeSubpath();
    }

    if (!isSelected())
        return body;

    // Include the delete chip in the hit area when visible.
    body.addEllipse(deleteChipRect());

    // When selected, expand the hit-area to include the handles so they receive clicks.
    QPainterPath p = body;
    const qreal s = HANDLE_SIZE;
    for (const QPointF &h : handlePositions())
        p.addRect(QRectF(h.x() - s/2, h.y() - s/2, s, s));
    QPointF rp = rotationHandlePos();
    p.addEllipse(rp, s, s);
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
    if (deleteChipRect().contains(localPos)) return H_Delete;
    const qreal s = HANDLE_SIZE;
    // Rotation second (it's outside the body, easy hit).
    QPointF rp = rotationHandlePos();
    if (QLineF(localPos, rp).length() <= s/2 + 4) return H_Rotation;
    const QList<QPointF> hs = handlePositions();
    for (int i = 0; i < hs.size(); ++i) {
        QRectF hr(hs[i].x() - s/2, hs[i].y() - s/2, s, s);
        if (hr.contains(localPos)) return i;
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
}

void UBEditableShapeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    // Auto-select on hover so the user can interact (delete X, handles, drag)
    // without first having to click — and without first having to switch to
    // the Selector tool. Other items keep their own selection state.
    setSelected(true);
    QGraphicsObject::hoverEnterEvent(e);
}

void UBEditableShapeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
    // Don't drop selection if the user is currently dragging a handle/body —
    // the drag continues with mouseMove which can briefly leave the hover
    // region.
    if (mActiveHandle == H_None) {
        setSelected(false);
        unsetCursor();
    }
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
    else if (h == H_Rotation) setCursor(Qt::CrossCursor);
    else if (h >= 0)          setCursor(Qt::SizeAllCursor);
    else                      setCursor(Qt::OpenHandCursor);
    QGraphicsObject::hoverMoveEvent(e);
}

void UBEditableShapeItem::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    int h = handleAt(e->pos());
    QPainterPath body;
    if (mKind == Ellipse || mKind == Circle) body.addEllipse(localBBox());
    else { body.addPolygon(mVertices); body.closeSubpath(); }

    if (h == H_Delete) {
        // Click on the ✕ chip — remove this item from the scene immediately.
        if (QGraphicsScene *sc = scene()) {
            sc->removeItem(this);
        }
        deleteLater();
        e->accept();
        return;
    }

    if (h != H_None) {
        mActiveHandle = h;
    } else if (body.contains(e->pos())) {
        mActiveHandle = H_Body;
        setCursor(Qt::ClosedHandCursor);
    } else {
        e->ignore();
        return;
    }
    setSelected(true);
    setFocus();
    mPressScenePos = e->scenePos();
    mVerticesAtPress = mVertices;
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
    Q_UNUSED(e);
    mActiveHandle = H_None;
    unsetCursor();
}

QVariant UBEditableShapeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSelectedHasChanged) {
        prepareGeometryChange(); // bbox grows when selected (handles)
        update();
    }
    return QGraphicsObject::itemChange(change, value);
}
