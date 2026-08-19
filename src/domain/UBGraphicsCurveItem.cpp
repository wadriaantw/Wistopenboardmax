/*
 * WistOpenboard fork. See UBGraphicsCurveItem.h.
 */

#include "UBGraphicsCurveItem.h"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QStyleOptionGraphicsItem>
#include <QtMath>
#include <QUuid>

namespace
{
    // Handle size in screen pixels. Kept constant on screen so the grab target
    // does not shrink to nothing when the teacher zooms out.
    const qreal sHandleScreenRadius = 9.0;
    const qreal sGrabSlack = 1.6;            // forgiving hit area for fingers
}

UBGraphicsCurveItem::UBGraphicsCurveItem(QGraphicsItem* parent)
    : QGraphicsPathItem(parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    // Must be the itemLayerType key, not the deprecated ItemLayerType one: the
    // scene assigns z-values from this, and without it the line is drawn behind
    // the page background and is simply invisible. DrawingItem puts it in the
    // same z band as pen strokes.
    setData(UBGraphicsItemData::itemLayerType, QVariant(itemLayerType::DrawingItem));
    setUuid(QUuid::createUuid());
}

UBGraphicsCurveItem::~UBGraphicsCurveItem()
{
}

void UBGraphicsCurveItem::setLine(const QPointF& start, const QPointF& end)
{
    prepareGeometryChange();
    mStart = start;
    mEnd = end;
    mControl = (mStart + mEnd) / 2.0;      // straight until bent
    rebuildPath();
}

void UBGraphicsCurveItem::setControlPoint(const QPointF& control)
{
    prepareGeometryChange();
    mControl = control;
    rebuildPath();
}

// A quadratic Bezier at t=0.5 is (P0 + 2C + P2) / 4.
QPointF UBGraphicsCurveItem::curveMidPoint() const
{
    return (mStart + 2.0 * mControl + mEnd) / 4.0;
}

// Invert the above so the curve passes through where the teacher dragged to.
void UBGraphicsCurveItem::setCurveMidPoint(const QPointF& onCurve)
{
    setControlPoint(2.0 * onCurve - (mStart + mEnd) / 2.0);
}

bool UBGraphicsCurveItem::isStraight() const
{
    const QPointF midpoint = (mStart + mEnd) / 2.0;
    const QPointF delta = mControl - midpoint;

    return QPointF::dotProduct(delta, delta) < 0.01;
}

void UBGraphicsCurveItem::straighten()
{
    setControlPoint((mStart + mEnd) / 2.0);
}

void UBGraphicsCurveItem::setColor(const QColor& color)
{
    mColor = color;
    update();
}

void UBGraphicsCurveItem::setStrokeWidth(qreal width)
{
    prepareGeometryChange();
    mStrokeWidth = qMax<qreal>(0.1, width);
    rebuildPath();
}

void UBGraphicsCurveItem::rebuildPath()
{
    QPainterPath path(mStart);

    if (isStraight())
        path.lineTo(mEnd);
    else
        path.quadTo(mControl, mEnd);

    setPath(path);
    update();
}

// One screen pixel is 1/m11 scene units, so this keeps the handles the same
// physical size whatever the zoom.
qreal UBGraphicsCurveItem::handleRadius() const
{
    qreal scale = 1.0;

    // Qualified: UBItem also declares scene(), returning a different type.
    const QGraphicsScene* gScene = QGraphicsPathItem::scene();

    if (gScene && !gScene->views().isEmpty())
    {
        const qreal m11 = gScene->views().first()->transform().m11();

        if (!qFuzzyIsNull(m11))
            scale = qAbs(m11);
    }

    return sHandleScreenRadius / scale;
}

QRectF UBGraphicsCurveItem::boundingRect() const
{
    const qreal pad = qMax(mStrokeWidth, handleRadius() * 2.0) + 2.0;

    return path().boundingRect().adjusted(-pad, -pad, pad, pad);
}

QPainterPath UBGraphicsCurveItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(qMax(mStrokeWidth, handleRadius() * 2.0));

    QPainterPath hit = stroker.createStroke(path());

    // The handles must be grabbable even when they sit off the stroke itself.
    for (const QPointF& handle : {mStart, curveMidPoint(), mEnd})
    {
        QPainterPath dot;
        dot.addEllipse(handle, handleRadius() * sGrabSlack, handleRadius() * sGrabSlack);
        hit = hit.united(dot);
    }

    return hit;
}

void UBGraphicsCurveItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen pen(mColor);
    pen.setWidthF(mStrokeWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());

    if (!isSelected())
        return;

    // Handles: ends solid, the bend control hollow so they read differently.
    const qreal r = handleRadius();

    QPen handlePen(QColor(0, 120, 215));
    handlePen.setWidthF(qMax<qreal>(1.0, r * 0.25));
    painter->setPen(handlePen);

    painter->setBrush(QColor(255, 255, 255, 230));
    painter->drawEllipse(mStart, r, r);
    painter->drawEllipse(mEnd, r, r);

    painter->setBrush(QColor(0, 120, 215, 90));
    painter->drawEllipse(curveMidPoint(), r * 0.85, r * 0.85);
}

UBGraphicsCurveItem::Handle UBGraphicsCurveItem::handleAt(const QPointF& pos) const
{
    const qreal reach = handleRadius() * sGrabSlack;

    struct { Handle id; QPointF at; } candidates[] = {
        { HandleStart,   mStart },
        { HandleEnd,     mEnd },
        { HandleControl, curveMidPoint() }
    };

    for (const auto& candidate : candidates)
    {
        const QPointF delta = pos - candidate.at;

        if (QPointF::dotProduct(delta, delta) <= reach * reach)
            return candidate.id;
    }

    return HandleBody;
}

void UBGraphicsCurveItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    setSelected(true);

    mActiveHandle = handleAt(event->pos());
    mDragOrigin = event->pos();

    event->accept();
}

void UBGraphicsCurveItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    const QPointF pos = event->pos();

    switch (mActiveHandle)
    {
        case HandleStart:
        {
            prepareGeometryChange();
            const bool wasStraight = isStraight();
            mStart = pos;
            if (wasStraight)
                mControl = (mStart + mEnd) / 2.0;   // stay straight while dragging an end
            rebuildPath();
            break;
        }

        case HandleEnd:
        {
            prepareGeometryChange();
            const bool wasStraight = isStraight();
            mEnd = pos;
            if (wasStraight)
                mControl = (mStart + mEnd) / 2.0;
            rebuildPath();
            break;
        }

        case HandleControl:
            setCurveMidPoint(pos);
            break;

        case HandleBody:
        {
            const QPointF delta = pos - mDragOrigin;
            prepareGeometryChange();
            mStart += delta;
            mEnd += delta;
            mControl += delta;
            mDragOrigin = pos;
            rebuildPath();
            break;
        }

        default:
            break;
    }

    event->accept();
}

void UBGraphicsCurveItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    mActiveHandle = HandleNone;
    event->accept();
}

QVariant UBGraphicsCurveItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == QGraphicsItem::ItemSelectedHasChanged)
        update();       // handles appear and disappear with selection

    return QGraphicsPathItem::itemChange(change, value);
}

UBItem* UBGraphicsCurveItem::deepCopy() const
{
    UBGraphicsCurveItem* copy = new UBGraphicsCurveItem();
    copyItemParameters(copy);

    return copy;
}

void UBGraphicsCurveItem::copyItemParameters(UBItem* copy) const
{
    UBGraphicsCurveItem* curve = dynamic_cast<UBGraphicsCurveItem*>(copy);

    if (!curve)
        return;

    curve->mStart = mStart;
    curve->mEnd = mEnd;
    curve->mControl = mControl;
    curve->mColor = mColor;
    curve->mStrokeWidth = mStrokeWidth;
    curve->setTransform(transform());
    curve->setPos(pos());
    curve->setZValue(zValue());
    curve->rebuildPath();
}
