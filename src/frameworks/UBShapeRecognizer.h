/*
 * WistOpenboard fork: Shape recognizer for snap-to-shape feature.
 *
 * Given a sequence of raw points captured during a freehand stroke,
 * classifies the stroke as a Line, Circle, Rectangle, Square, or
 * "no match." Pure functions — no Qt-scene dependencies — so the
 * recognizer is unit-testable in isolation.
 */

#ifndef UBSHAPERECOGNIZER_H_
#define UBSHAPERECOGNIZER_H_

#include <QPointF>
#include <QVector>
#include <QRectF>

class UBShapeRecognizer
{
public:
    enum ShapeKind {
        None = 0,
        Line,
        Circle,
        Rectangle, // axis-aligned
        Square,    // axis-aligned, w ~= h
        Triangle,  // any orientation, three vertices
        Parabola   // axis-aligned, y = a*x^2 + b*x + c
    };

    struct Result {
        ShapeKind kind = None;
        // For Line:    p1 = first endpoint, p2 = second endpoint
        QPointF p1, p2;
        // For Circle:  center, radius
        QPointF center;
        qreal radius = 0;
        // For Rect/Square: bounding rectangle (axis-aligned)
        QRectF rect;
        // For Triangle: three vertices
        QPointF triangle[3];
        // For Parabola: y = a*x^2 + b*x + c, drawn from xMin to xMax
        qreal parabolaA = 0, parabolaB = 0, parabolaC = 0;
        qreal parabolaXMin = 0, parabolaXMax = 0;
    };

    /**
     * Classify a stroke. Returns ShapeKind::None if no shape matches.
     *
     * The recognizer applies these heuristics, in order:
     *   1. Reject strokes that are too short (fewer than ~6 points or < 8px diagonal)
     *   2. If start ≈ end (closed path), try Circle then Rectangle/Square
     *   3. Else, try Line
     */
    static Result recognize(const QVector<QPointF>& points);

private:
    // Helpers (exposed only via recognize)
    static qreal pathLength(const QVector<QPointF>& pts);
    static QRectF boundingRect(const QVector<QPointF>& pts);
    static bool tryLine(const QVector<QPointF>& pts, Result& out);
    static bool tryCircle(const QVector<QPointF>& pts, Result& out);
    static bool tryRectangle(const QVector<QPointF>& pts, Result& out);
    static bool tryTriangle(const QVector<QPointF>& pts, Result& out);
    static bool tryParabola(const QVector<QPointF>& pts, Result& out);
};

#endif // UBSHAPERECOGNIZER_H_
