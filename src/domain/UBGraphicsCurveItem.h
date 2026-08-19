/*
 * WistOpenboard fork.
 *
 * An editable line: two end points plus a middle control point. Dragging the
 * ends changes length and angle; dragging the middle bends it into a curve.
 * Drawn as a quadratic Bezier, which degenerates to a straight line when the
 * control point sits on the midpoint -- so a freshly drawn line looks exactly
 * like the old baked-polygon one until the teacher bends it.
 *
 * This exists because the Line tool previously produced a UBGraphicsPolygonItem
 * (see UBGraphicsScene::drawLineTo): the geometry was flattened at draw time and
 * the end points were gone, so there was nothing left to grab.
 */

#ifndef UBGRAPHICSCURVEITEM_H_
#define UBGRAPHICSCURVEITEM_H_

#include <QGraphicsPathItem>
#include <QColor>
#include <QPointF>

#include "core/UB.h"
#include "UBItem.h"

class UBGraphicsCurveItem : public QGraphicsPathItem, public UBItem
{
    public:
        explicit UBGraphicsCurveItem(QGraphicsItem* parent = nullptr);
        ~UBGraphicsCurveItem() override;

        enum { Type = UBGraphicsItemType::EditableCurveItemType };
        int type() const override { return Type; }

        // Geometry. The control point is kept on the midpoint unless bent.
        void setLine(const QPointF& start, const QPointF& end);
        void setControlPoint(const QPointF& control);

        QPointF startPoint() const { return mStart; }
        QPointF endPoint() const { return mEnd; }
        QPointF controlPoint() const { return mControl; }

        // Where the bend handle is drawn and grabbed: a point actually ON the
        // curve, not the Bezier control point (which sits about twice as far from
        // the chord and looks detached).
        QPointF curveMidPoint() const;
        void setCurveMidPoint(const QPointF& onCurve);

        bool isStraight() const;
        void straighten();               // put the control back on the midpoint

        void setColor(const QColor& color);
        QColor color() const { return mColor; }

        void setStrokeWidth(qreal width);
        qreal strokeWidth() const { return mStrokeWidth; }

        // UBItem
        UBItem* deepCopy() const override;
        void copyItemParameters(UBItem* copy) const override;

        QRectF boundingRect() const override;
        QPainterPath shape() const override;

    protected:
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
        QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    private:
        enum Handle { HandleNone, HandleStart, HandleControl, HandleEnd, HandleDelete, HandleBody };

        void rebuildPath();
        QPointF deleteBadgePos() const;   // the little x, shown while selected
        void removeSelfWithUndo();
        qreal handleRadius() const;               // scene units, constant on screen
        Handle handleAt(const QPointF& scenePos) const;

        QPointF mStart;
        QPointF mEnd;
        QPointF mControl;

        QColor mColor = Qt::black;
        qreal mStrokeWidth = 3.0;

        Handle mActiveHandle = HandleNone;
        QPointF mDragOrigin;
};

#endif /* UBGRAPHICSCURVEITEM_H_ */
