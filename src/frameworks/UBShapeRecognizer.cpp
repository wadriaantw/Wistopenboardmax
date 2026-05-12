/*
 * WistOpenboard fork: Shape recognizer implementation.
 */

#include "UBShapeRecognizer.h"

#include <QtMath>
#include <QLineF>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------
// Tunables — adjust these to make snapping more/less forgiving.
// ---------------------------------------------------------------------
namespace {
    constexpr int    kMinPoints = 6;
    constexpr qreal  kMinDiagonal = 12.0;     // px in scene coords

    // Closed-path test: distance(start, end) / pathLength
    constexpr qreal  kClosedRatio = 0.30;

    // Line: max perpendicular distance / line length
    constexpr qreal  kLineMaxDeviation = 0.07;

    // Circle: stdDev(radii) / meanRadius
    constexpr qreal  kCircleRadiusStdDevRatio = 0.18;
    // Circle: aspect ratio of bounding box (min(w,h) / max(w,h))
    constexpr qreal  kCircleMinAspect = 0.65;

    // Rectangle: max distance from any point to nearest box edge / box diagonal
    constexpr qreal  kRectMaxEdgeDistance = 0.10;
    // Rectangle: each side must have at least this fraction of total points near it
    constexpr qreal  kRectMinPointsPerSide = 0.05;

    // Square: width / height must be within this tolerance
    constexpr qreal  kSquareTolerance = 0.18;

    // Triangle: Ramer-Douglas-Peucker tolerance as fraction of bbox diagonal
    constexpr qreal  kTriangleRdpEps = 0.10;
    // Triangle: minimum area as fraction of (diagonal^2) to reject degenerate strokes
    constexpr qreal  kTriangleMinAreaRatio = 0.04;

    // Parabola: RMS residual from quadratic fit / vertical-range
    constexpr qreal  kParabolaMaxResidualRatio = 0.06;
    // Parabola: |a|*width^2 must be at least this fraction of bbox height
    // (rejects strokes that are basically straight lines fit by an almost-zero a)
    constexpr qreal  kParabolaMinCurvatureRatio = 0.30;
}

qreal UBShapeRecognizer::pathLength(const QVector<QPointF>& pts)
{
    qreal total = 0;
    for (int i = 1; i < pts.size(); ++i)
        total += QLineF(pts[i-1], pts[i]).length();
    return total;
}

QRectF UBShapeRecognizer::boundingRect(const QVector<QPointF>& pts)
{
    if (pts.isEmpty())
        return QRectF();
    qreal xMin = pts.first().x(), xMax = pts.first().x();
    qreal yMin = pts.first().y(), yMax = pts.first().y();
    for (const QPointF& p : pts) {
        xMin = std::min(xMin, p.x());
        xMax = std::max(xMax, p.x());
        yMin = std::min(yMin, p.y());
        yMax = std::max(yMax, p.y());
    }
    return QRectF(QPointF(xMin, yMin), QPointF(xMax, yMax));
}

// Fit y = a + b*x via least squares; return (a, b) and max perpendicular
// residual scaled by line length. We work in a rotated frame if the line is
// near-vertical to avoid blowing up the slope.
bool UBShapeRecognizer::tryLine(const QVector<QPointF>& pts, Result& out)
{
    const int n = pts.size();
    if (n < 2)
        return false;

    // Fit a line by least squares using the orthogonal regression
    // (PCA-based) so it handles vertical lines.
    qreal mx = 0, my = 0;
    for (const QPointF& p : pts) { mx += p.x(); my += p.y(); }
    mx /= n; my /= n;

    qreal sxx = 0, syy = 0, sxy = 0;
    for (const QPointF& p : pts) {
        const qreal dx = p.x() - mx;
        const qreal dy = p.y() - my;
        sxx += dx*dx; syy += dy*dy; sxy += dx*dy;
    }

    // Direction = principal eigenvector of covariance matrix
    const qreal trace = sxx + syy;
    const qreal det   = sxx*syy - sxy*sxy;
    const qreal disc  = std::max<qreal>(0.0, trace*trace/4.0 - det);
    const qreal lambda = trace/2.0 + std::sqrt(disc);

    QPointF dir;
    if (std::abs(sxy) > 1e-9) {
        dir = QPointF(lambda - syy, sxy);
    } else {
        dir = (sxx >= syy) ? QPointF(1, 0) : QPointF(0, 1);
    }
    const qreal dlen = std::hypot(dir.x(), dir.y());
    if (dlen < 1e-9)
        return false;
    dir = QPointF(dir.x()/dlen, dir.y()/dlen);

    // Normal to direction
    const QPointF nrm(-dir.y(), dir.x());

    // Compute max perpendicular deviation and project endpoints onto the line.
    qreal maxDev = 0;
    qreal tMin = std::numeric_limits<qreal>::max();
    qreal tMax = -std::numeric_limits<qreal>::max();
    for (const QPointF& p : pts) {
        const qreal dx = p.x() - mx;
        const qreal dy = p.y() - my;
        const qreal proj = dx*dir.x() + dy*dir.y();         // along line
        const qreal perp = dx*nrm.x() + dy*nrm.y();         // perpendicular
        maxDev = std::max(maxDev, std::abs(perp));
        tMin = std::min(tMin, proj);
        tMax = std::max(tMax, proj);
    }
    const qreal lineLength = tMax - tMin;
    if (lineLength < kMinDiagonal)
        return false;
    if (maxDev / lineLength > kLineMaxDeviation)
        return false;

    out.kind = Line;
    out.p1 = QPointF(mx + dir.x()*tMin, my + dir.y()*tMin);
    out.p2 = QPointF(mx + dir.x()*tMax, my + dir.y()*tMax);
    return true;
}

bool UBShapeRecognizer::tryCircle(const QVector<QPointF>& pts, Result& out)
{
    const int n = pts.size();
    if (n < kMinPoints)
        return false;

    // Quick reject via bounding-box aspect ratio
    const QRectF bb = boundingRect(pts);
    if (bb.width() < kMinDiagonal && bb.height() < kMinDiagonal)
        return false;
    const qreal w = bb.width(), h = bb.height();
    const qreal aspect = std::min(w, h) / std::max(w, h);
    if (aspect < kCircleMinAspect)
        return false;

    // Center estimate: bounding-box center is a robust starting point
    // for hand-drawn circles. Radius = mean(distance from center).
    const QPointF c = bb.center();
    qreal sumR = 0;
    for (const QPointF& p : pts)
        sumR += QLineF(c, p).length();
    const qreal meanR = sumR / n;
    if (meanR < kMinDiagonal/2)
        return false;

    // Std deviation of radii (relative to mean)
    qreal sumSq = 0;
    for (const QPointF& p : pts) {
        const qreal r = QLineF(c, p).length();
        const qreal d = r - meanR;
        sumSq += d*d;
    }
    const qreal std = std::sqrt(sumSq / n);
    if (std / meanR > kCircleRadiusStdDevRatio)
        return false;

    out.kind = Circle;
    out.center = c;
    out.radius = meanR;
    return true;
}

bool UBShapeRecognizer::tryRectangle(const QVector<QPointF>& pts, Result& out)
{
    const int n = pts.size();
    if (n < kMinPoints)
        return false;

    const QRectF bb = boundingRect(pts);
    if (bb.width() < kMinDiagonal || bb.height() < kMinDiagonal)
        return false;

    // For each point, compute distance to nearest box edge (axis-aligned).
    const qreal diag = std::hypot(bb.width(), bb.height());
    if (diag < 1e-6)
        return false;

    int leftN = 0, rightN = 0, topN = 0, bottomN = 0;
    qreal maxEdgeDist = 0;
    for (const QPointF& p : pts) {
        const qreal dl = std::abs(p.x() - bb.left());
        const qreal dr = std::abs(p.x() - bb.right());
        const qreal dt = std::abs(p.y() - bb.top());
        const qreal db = std::abs(p.y() - bb.bottom());
        const qreal d = std::min({dl, dr, dt, db});
        maxEdgeDist = std::max(maxEdgeDist, d);

        // Track which edge each point is closest to (for coverage check)
        if      (d == dl) ++leftN;
        else if (d == dr) ++rightN;
        else if (d == dt) ++topN;
        else              ++bottomN;
    }

    if (maxEdgeDist / diag > kRectMaxEdgeDistance)
        return false;

    // All four sides need a minimum share of points (otherwise it's
    // a triangle / open shape that happens to fit a bounding box).
    const qreal minCount = kRectMinPointsPerSide * n;
    if (leftN < minCount || rightN < minCount || topN < minCount || bottomN < minCount)
        return false;

    // Square if width and height match within tolerance
    const qreal w = bb.width(), h = bb.height();
    const qreal aspect = std::abs(w - h) / std::max(w, h);
    out.rect = bb;
    if (aspect <= kSquareTolerance) {
        // Snap to square: use the average side length, recenter the box
        const qreal side = (w + h) / 2.0;
        const QPointF c = bb.center();
        out.rect = QRectF(c.x() - side/2, c.y() - side/2, side, side);
        out.kind = Square;
    } else {
        out.kind = Rectangle;
    }
    return true;
}

// Ramer-Douglas-Peucker — recursive helper that fills `keep` with internal indices
// (start/end are appended by the public wrapper).
static void rdpRecurse(const QVector<QPointF>& pts, int start, int end,
                       qreal eps, QVector<int>& keep)
{
    if (end <= start + 1)
        return;
    const QPointF& a = pts[start];
    const QPointF& b = pts[end];
    const qreal dx = b.x() - a.x();
    const qreal dy = b.y() - a.y();
    const qreal len = std::hypot(dx, dy);
    qreal dmax = 0;
    int idx = -1;
    for (int i = start + 1; i < end; ++i) {
        qreal d;
        if (len < 1e-9) {
            d = QLineF(pts[i], a).length();
        } else {
            // Perpendicular distance from pts[i] to the line a-b
            d = std::abs(dy * pts[i].x() - dx * pts[i].y()
                         + b.x()*a.y() - b.y()*a.x()) / len;
        }
        if (d > dmax) { dmax = d; idx = i; }
    }
    if (dmax > eps && idx > 0) {
        rdpRecurse(pts, start, idx, eps, keep);
        keep.append(idx);
        rdpRecurse(pts, idx, end, eps, keep);
    }
}

bool UBShapeRecognizer::tryTriangle(const QVector<QPointF>& pts, Result& out)
{
    const int n = pts.size();
    if (n < kMinPoints)
        return false;

    const QRectF bb = boundingRect(pts);
    const qreal diag = std::hypot(bb.width(), bb.height());
    if (diag < kMinDiagonal)
        return false;

    // Run RDP — for a closed triangle with corner-tolerance epsilon, the
    // simplified polyline should retain ~3 corners (plus duplicated start/end).
    const qreal eps = diag * kTriangleRdpEps;
    QVector<int> keep;
    keep.append(0);
    rdpRecurse(pts, 0, n - 1, eps, keep);
    keep.append(n - 1);

    QVector<QPointF> simp;
    simp.reserve(keep.size());
    for (int i : keep)
        simp.append(pts[i]);

    // Closed path: drop the trailing point if it duplicates the first.
    if (simp.size() > 1 && QLineF(simp.first(), simp.last()).length() < eps)
        simp.removeLast();

    if (simp.size() != 3)
        return false;

    // Reject degenerate (near-collinear) triangles using Heron's formula.
    const qreal a = QLineF(simp[0], simp[1]).length();
    const qreal b = QLineF(simp[1], simp[2]).length();
    const qreal c = QLineF(simp[2], simp[0]).length();
    const qreal s = (a + b + c) / 2.0;
    const qreal area = std::sqrt(std::max<qreal>(0,
                            s * (s-a) * (s-b) * (s-c)));
    if (area < diag * diag * kTriangleMinAreaRatio)
        return false;

    out.kind = Triangle;
    out.triangle[0] = simp[0];
    out.triangle[1] = simp[1];
    out.triangle[2] = simp[2];
    return true;
}

// Solve a 3x3 linear system Mx = r via Cramer's rule. Returns false if M is singular.
static bool solve3x3(const qreal m[3][3], const qreal r[3], qreal x[3])
{
    auto det3 = [](qreal a, qreal b, qreal c,
                   qreal d, qreal e, qreal f,
                   qreal g, qreal h, qreal i) {
        return a*(e*i - f*h) - b*(d*i - f*g) + c*(d*h - e*g);
    };
    const qreal D = det3(m[0][0], m[0][1], m[0][2],
                         m[1][0], m[1][1], m[1][2],
                         m[2][0], m[2][1], m[2][2]);
    if (std::abs(D) < 1e-9) return false;
    const qreal Dx = det3(r[0], m[0][1], m[0][2],
                          r[1], m[1][1], m[1][2],
                          r[2], m[2][1], m[2][2]);
    const qreal Dy = det3(m[0][0], r[0], m[0][2],
                          m[1][0], r[1], m[1][2],
                          m[2][0], r[2], m[2][2]);
    const qreal Dz = det3(m[0][0], m[0][1], r[0],
                          m[1][0], m[1][1], r[1],
                          m[2][0], m[2][1], r[2]);
    x[0] = Dx / D; x[1] = Dy / D; x[2] = Dz / D;
    return true;
}

bool UBShapeRecognizer::tryParabola(const QVector<QPointF>& pts, Result& out)
{
    const int n = pts.size();
    if (n < 8)
        return false;

    const QRectF bb = boundingRect(pts);
    if (bb.width() < kMinDiagonal || bb.height() < kMinDiagonal)
        return false;
    // Open path (parabola is not closed); a closed shape is unlikely a parabola.
    const qreal startEnd = QLineF(pts.first(), pts.last()).length();
    if (startEnd < bb.width() * 0.25)
        return false;

    // Least-squares fit y = a*x^2 + b*x + c. Centre x to improve conditioning.
    qreal mx = 0;
    for (const QPointF& p : pts) mx += p.x();
    mx /= n;

    qreal Sx = 0, Sx2 = 0, Sx3 = 0, Sx4 = 0;
    qreal Sy = 0, Sxy = 0, Sx2y = 0;
    for (const QPointF& p : pts) {
        const qreal x = p.x() - mx;
        const qreal y = p.y();
        const qreal x2 = x*x;
        Sx  += x;   Sx2 += x2;  Sx3 += x2*x;  Sx4 += x2*x2;
        Sy  += y;   Sxy += x*y; Sx2y += x2*y;
    }
    // [n   Sx  Sx2] [c']   [Sy]
    // [Sx  Sx2 Sx3] [b'] = [Sxy]
    // [Sx2 Sx3 Sx4] [a']   [Sx2y]
    const qreal M[3][3] = {{(qreal)n, Sx, Sx2}, {Sx, Sx2, Sx3}, {Sx2, Sx3, Sx4}};
    const qreal R[3] = {Sy, Sxy, Sx2y};
    qreal coef[3];
    if (!solve3x3(M, R, coef))
        return false;
    // y = a'*(x-mx)^2 + b'*(x-mx) + c' = a'*x^2 + (b' - 2*a'*mx)*x + (c' - b'*mx + a'*mx^2)
    const qreal aP = coef[2];
    const qreal bP = coef[1] - 2.0*aP*mx;
    const qreal cP = coef[0] - coef[1]*mx + aP*mx*mx;

    // Residual check
    qreal sumSq = 0;
    qreal yMin = pts.first().y(), yMax = pts.first().y();
    for (const QPointF& p : pts) {
        const qreal yPred = aP*p.x()*p.x() + bP*p.x() + cP;
        const qreal d = p.y() - yPred;
        sumSq += d*d;
        yMin = std::min(yMin, p.y());
        yMax = std::max(yMax, p.y());
    }
    const qreal rms = std::sqrt(sumSq / n);
    const qreal yRange = std::max<qreal>(yMax - yMin, bb.height());
    if (yRange < 1e-6)
        return false;
    if (rms / yRange > kParabolaMaxResidualRatio)
        return false;

    // Reject quasi-linear strokes that fit a*~0
    const qreal w = bb.width();
    if (std::abs(aP) * w * w < kParabolaMinCurvatureRatio * bb.height())
        return false;

    out.kind = Parabola;
    out.parabolaA = aP;
    out.parabolaB = bP;
    out.parabolaC = cP;
    out.parabolaXMin = bb.left();
    out.parabolaXMax = bb.right();
    return true;
}

UBShapeRecognizer::Result UBShapeRecognizer::recognize(const QVector<QPointF>& points)
{
    Result r;
    const int n = points.size();
    if (n < kMinPoints)
        return r;

    const QRectF bb = boundingRect(points);
    const qreal diag = std::hypot(bb.width(), bb.height());
    if (diag < kMinDiagonal)
        return r;

    const qreal startEnd = QLineF(points.first(), points.last()).length();
    const qreal pathLen  = pathLength(points);
    if (pathLen < 1e-6)
        return r;

    const bool closed = (startEnd / pathLen) < kClosedRatio;

    if (closed) {
        // Order matters: rectangle before circle (otherwise squares pass the
        // circle aspect-ratio test and get misclassified). Triangle before
        // circle for the same reason — a closed triangle's bbox aspect can
        // be near-square.
        if (tryRectangle(points, r))
            return r;
        if (tryTriangle(points, r))
            return r;
        if (tryCircle(points, r))
            return r;
        // No closed-shape match; fall through to line attempt below.
    } else {
        // Open path — try parabola first (curved), then line.
        if (tryParabola(points, r))
            return r;
    }

    if (tryLine(points, r))
        return r;

    r.kind = None;
    return r;
}
