/*
 * UBEditableShapeItem - geometry shape with vertex/corner handles, resize, and rotation.
 *
 * Designed for the math-classroom Geometry menu: instead of static SVG pictures,
 * users can move corners (polygons), resize (rect/ellipse via 4 corner handles),
 * and rotate (single rotation handle above the shape) right on the board.
 */
#ifndef UBEDITABLESHAPEITEM_H
#define UBEDITABLESHAPEITEM_H

#include <QGraphicsObject>
#include <QPolygonF>
#include <QPen>
#include <QBrush>

class UBEditableShapeItem : public QGraphicsObject
{
    Q_OBJECT
public:
    enum Kind {
        Rect,           // free-aspect rectangle
        Square,         // aspect-locked rectangle
        Ellipse,        // free-aspect ellipse
        Circle,         // aspect-locked ellipse
        Polygon         // free polygon — every vertex draggable
    };
    Q_ENUM(Kind)

    explicit UBEditableShapeItem(Kind kind, const QPolygonF &vertices,
                                 QGraphicsItem *parent = nullptr);
    ~UBEditableShapeItem() override = default;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *o, QWidget *w) override;

    Kind kind() const { return mKind; }
    void setStrokeColor(const QColor &c);
    void setStrokeWidth(qreal w);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *e) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *e) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    static constexpr qreal HANDLE_SIZE = 14.0;
    static constexpr qreal ROT_OFFSET  = 28.0;

    enum {
        H_None      = -1,
        H_Body      = -1000,
        H_Rotation  = -1001,
        H_Delete    = -1002
    };

    Kind        mKind;
    QPolygonF   mVertices;          // for Polygon: real vertices.
                                    // for Rect/Square/Ellipse/Circle: 4 bbox corners (TL, TR, BR, BL)
    QPen        mPen;
    QBrush      mBrush;

    int         mActiveHandle = H_None;
    QPointF     mPressScenePos;
    QPolygonF   mVerticesAtPress;

    QList<QPointF> handlePositions() const;
    QPointF        rotationHandlePos() const;
    QRectF         deleteChipRect() const;
    int            handleAt(const QPointF &localPos) const;
    QRectF         localBBox() const;
};

#endif
