/*
 * WistOpenboard fork. See UBModernIcons.h.
 *
 * Drawing convention: a 44x44 logical canvas, glyphs inside the central
 * 24x24 box, 2.6px rounded strokes. Rendered at 2x for high-DPI. Colours
 * come from UBTheme so the set follows light/dark automatically.
 */

#include "UBModernIcons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QIcon>
#include <QAction>

#include "core/UBTheme.h"
#include "gui/UBMainWindow.h"

namespace
{

using Painterfunction = void (*)(QPainter&);

QIcon makeIcon(Painterfunction draw)
{
    QIcon icon;

    for (const qreal scale : {1.0, 2.0})
    {
        auto renderAt = [draw, scale](const QColor& colour, qreal opacity) {
            // 36px canvas, art authored on a 44px grid centred at 22: the -4
            // shift keeps every glyph centred while shedding the dead padding
            // that made icons look small in 32px toolbar slots.
            QPixmap pixmap(int(36 * scale), int(36 * scale));
            pixmap.setDevicePixelRatio(scale);
            pixmap.fill(Qt::transparent);

            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setOpacity(opacity);
            painter.translate(-4, -4);

            QPen pen(colour);
            pen.setWidthF(2.6);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);

            draw(painter);
            painter.end();
            return pixmap;
        };

        icon.addPixmap(renderAt(UBTheme::ink(), 1.0), QIcon::Normal);
        icon.addPixmap(renderAt(UBTheme::ink(), 0.3), QIcon::Disabled);
    }

    return icon;
}

/* --- glyphs ------------------------------------------------------------ */

// Switch the painter to an accent stroke, preserving width and caps.
void accent(QPainter& p, const QColor& colour)
{
    QPen pen = p.pen();
    pen.setColor(colour);
    p.setPen(pen);
}

void drawPen(QPainter& p)
{
    p.drawLine(QPointF(15, 29), QPointF(27, 17));
    p.drawLine(QPointF(27, 17), QPointF(30, 14));
    QPainterPath nib;
    nib.moveTo(15, 29);
    nib.lineTo(12.5, 31.5);
    nib.lineTo(15.8, 31.8);
    p.save();
    accent(p, UBTheme::accentRed());
    p.setBrush(UBTheme::accentRed());
    p.drawPath(nib);
    p.restore();
}

void drawMarker(QPainter& p)
{
    QPen fat = p.pen();
    fat.setWidthF(6.0);
    fat.setCapStyle(Qt::FlatCap);
    fat.setColor(UBTheme::accentAmber());
    p.save();
    p.setPen(fat);
    p.drawLine(QPointF(16, 28), QPointF(28, 16));
    p.restore();
    p.drawLine(QPointF(13, 32), QPointF(21, 32));
}

void drawEraser(QPainter& p)
{
    p.save();
    p.translate(22, 22);
    p.rotate(-45);
    p.drawRoundedRect(QRectF(-8, -5, 16, 10), 3, 3);
    p.drawLine(QPointF(-2, -5), QPointF(-2, 5));
    p.restore();
    p.drawLine(QPointF(14, 32), QPointF(30, 32));
}

void drawSelector(QPainter& p)
{
    QPainterPath arrow;
    arrow.moveTo(16, 12);
    arrow.lineTo(16, 30);
    arrow.lineTo(21, 25);
    arrow.lineTo(25, 31);
    arrow.lineTo(28, 29);
    arrow.lineTo(24, 23);
    arrow.lineTo(30, 22);
    arrow.closeSubpath();
    p.drawPath(arrow);
}

void drawPlay(QPainter& p)
{
    QPainterPath triangle;
    triangle.moveTo(17, 13);
    triangle.lineTo(31, 22);
    triangle.lineTo(17, 31);
    triangle.closeSubpath();
    p.drawPath(triangle);
}

void drawHand(QPainter& p)
{
    // Four fingers, thumb, palm -- a flat "stop" hand, the classic pan glyph.
    p.drawLine(QPointF(16.5, 23), QPointF(16.5, 15.5));
    p.drawLine(QPointF(20.5, 23), QPointF(20.5, 12.5));
    p.drawLine(QPointF(24.5, 23), QPointF(24.5, 13.5));
    p.drawLine(QPointF(28.5, 23), QPointF(28.5, 16.5));

    QPainterPath palm;
    palm.moveTo(13.5, 20.5);
    palm.lineTo(13.5, 25);
    palm.cubicTo(13.5, 30.5, 17.5, 32.5, 22, 32.5);
    palm.cubicTo(26.5, 32.5, 30.5, 30.5, 30.5, 25);
    palm.lineTo(30.5, 23);
    p.drawPath(palm);

    p.drawLine(QPointF(13.5, 22.5), QPointF(11, 19));
}

void drawZoomIn(QPainter& p)
{
    p.drawEllipse(QPointF(20, 20), 8, 8);
    p.drawLine(QPointF(26, 26), QPointF(32, 32));
    p.save();
    accent(p, UBTheme::accentBlue());
    p.drawLine(QPointF(16.5, 20), QPointF(23.5, 20));
    p.drawLine(QPointF(20, 16.5), QPointF(20, 23.5));
    p.restore();
}

void drawZoomOut(QPainter& p)
{
    p.drawEllipse(QPointF(20, 20), 8, 8);
    p.drawLine(QPointF(26, 26), QPointF(32, 32));
    p.save();
    accent(p, UBTheme::accentRed());
    p.drawLine(QPointF(16.5, 20), QPointF(23.5, 20));
    p.restore();
}

void drawLineTool(QPainter& p)
{
    p.drawLine(QPointF(14, 30), QPointF(30, 14));
    p.setBrush(p.pen().color());
    p.drawEllipse(QPointF(14, 30), 2.2, 2.2);
    p.drawEllipse(QPointF(30, 14), 2.2, 2.2);
    p.setBrush(Qt::NoBrush);
}

void drawText(QPainter& p)
{
    p.drawLine(QPointF(14, 14), QPointF(30, 14));
    p.drawLine(QPointF(22, 14), QPointF(22, 31));
    p.drawLine(QPointF(18, 31), QPointF(26, 31));
}

void drawCapture(QPainter& p)
{
    p.drawRoundedRect(QRectF(12, 16, 20, 14), 3, 3);
    p.drawEllipse(QPointF(22, 23), 4, 4);
    p.drawLine(QPointF(18, 16), QPointF(20, 13));
    p.drawLine(QPointF(20, 13), QPointF(24, 13));
    p.drawLine(QPointF(24, 13), QPointF(26, 16));
}

void drawUndo(QPainter& p)
{
    QPainterPath arc;
    arc.moveTo(30, 27);
    arc.arcTo(QRectF(14, 14, 16, 16), -45, 200);
    p.drawPath(arc);
    p.drawLine(QPointF(14.5, 18), QPointF(13, 24));
    p.drawLine(QPointF(14.5, 18), QPointF(20, 19));
}

void drawRedo(QPainter& p)
{
    p.save();
    p.scale(-1, 1);
    p.translate(-44, 0);
    drawUndo(p);
    p.restore();
}

void drawNewPage(QPainter& p)
{
    p.drawRoundedRect(QRectF(14, 12, 14, 18), 2, 2);
    p.save();
    accent(p, UBTheme::accentGreen());
    p.drawLine(QPointF(28, 25), QPointF(34, 25));
    p.drawLine(QPointF(31, 22), QPointF(31, 28));
    p.restore();
}

void drawBackgrounds(QPainter& p)
{
    p.drawRoundedRect(QRectF(13, 13, 18, 18), 2, 2);
    p.drawLine(QPointF(13, 19), QPointF(31, 19));
    p.drawLine(QPointF(13, 25), QPointF(31, 25));
    p.drawLine(QPointF(19, 13), QPointF(19, 31));
    p.drawLine(QPointF(25, 13), QPointF(25, 31));
}

void drawBoard(QPainter& p)
{
    p.drawRoundedRect(QRectF(12, 13, 20, 15), 2, 2);
    p.drawLine(QPointF(16, 21), QPointF(24, 17));
    p.drawLine(QPointF(18, 31), QPointF(26, 31));
}

void drawWeb(QPainter& p)
{
    p.drawEllipse(QPointF(22, 22), 9, 9);
    p.drawEllipse(QPointF(22, 22), 4.2, 9);
    p.drawLine(QPointF(13.5, 22), QPointF(30.5, 22));
}

void drawDocuments(QPainter& p)
{
    QPainterPath folder;
    folder.moveTo(12, 16);
    folder.lineTo(18, 16);
    folder.lineTo(20.5, 18.5);
    folder.lineTo(31, 18.5);
    folder.lineTo(31, 30);
    folder.lineTo(12, 30);
    folder.closeSubpath();
    p.drawPath(folder);
}

void drawDesktop(QPainter& p)
{
    p.drawRoundedRect(QRectF(12, 13, 20, 14), 2, 2);
    p.drawLine(QPointF(19, 31), QPointF(25, 31));
    p.drawLine(QPointF(22, 27), QPointF(22, 31));
}

void drawKeyboard(QPainter& p)
{
    p.drawRoundedRect(QRectF(11, 16, 22, 13), 2, 2);
    for (int i = 0; i < 4; ++i)
        p.drawPoint(QPointF(15 + i * 4.5, 20.5));
    p.drawLine(QPointF(16, 25), QPointF(28, 25));
}

void drawClear(QPainter& p)
{
    p.drawLine(QPointF(15, 17), QPointF(17, 31));
    p.drawLine(QPointF(29, 17), QPointF(27, 31));
    p.drawLine(QPointF(17, 31), QPointF(27, 31));
    p.drawLine(QPointF(13, 17), QPointF(31, 17));
    p.drawLine(QPointF(19, 17), QPointF(19.5, 14));
    p.drawLine(QPointF(19.5, 14), QPointF(24.5, 14));
    p.drawLine(QPointF(24.5, 14), QPointF(25, 17));
}

void drawSnap(QPainter& p)
{
    p.drawRect(QRectF(13, 17, 11, 11));
    QPen dashed = p.pen();
    dashed.setStyle(Qt::DashLine);
    dashed.setWidthF(1.8);
    p.save();
    p.setPen(dashed);
    p.drawEllipse(QPointF(27, 24), 6.5, 6.5);
    p.restore();
}

void drawNewDocument(QPainter& p)
{
    p.drawRoundedRect(QRectF(13, 12, 14, 18), 2, 2);
    p.save();
    accent(p, UBTheme::accentGreen());
    p.drawLine(QPointF(27, 26), QPointF(33, 26));
    p.drawLine(QPointF(30, 23), QPointF(30, 29));
    p.restore();
}

void drawNewFolder(QPainter& p)
{
    QPainterPath folder;
    folder.moveTo(12, 15);
    folder.lineTo(18, 15);
    folder.lineTo(20.5, 17.5);
    folder.lineTo(28, 17.5);
    folder.lineTo(28, 28);
    folder.lineTo(12, 28);
    folder.closeSubpath();
    p.drawPath(folder);
    p.save();
    accent(p, UBTheme::accentGreen());
    p.drawLine(QPointF(28, 25), QPointF(34, 25));
    p.drawLine(QPointF(31, 22), QPointF(31, 28));
    p.restore();
}

void drawImport(QPainter& p)
{
    p.save();
    accent(p, UBTheme::accentGreen());
    p.drawLine(QPointF(22, 12), QPointF(22, 24));
    p.drawLine(QPointF(22, 24), QPointF(17.5, 19.5));
    p.drawLine(QPointF(22, 24), QPointF(26.5, 19.5));
    p.restore();
    QPainterPath tray;
    tray.moveTo(13, 24);
    tray.lineTo(13, 30);
    tray.lineTo(31, 30);
    tray.lineTo(31, 24);
    p.drawPath(tray);
}

void drawExport(QPainter& p)
{
    p.save();
    accent(p, UBTheme::accentAmber());
    p.drawLine(QPointF(22, 24), QPointF(22, 12));
    p.drawLine(QPointF(22, 12), QPointF(17.5, 16.5));
    p.drawLine(QPointF(22, 12), QPointF(26.5, 16.5));
    p.restore();
    QPainterPath tray;
    tray.moveTo(13, 24);
    tray.lineTo(13, 30);
    tray.lineTo(31, 30);
    tray.lineTo(31, 24);
    p.drawPath(tray);
}

void drawRename(QPainter& p)
{
    p.drawLine(QPointF(14, 30), QPointF(30, 30));
    p.drawLine(QPointF(16, 25), QPointF(26, 15));
    p.drawLine(QPointF(26, 15), QPointF(29, 18));
    p.drawLine(QPointF(29, 18), QPointF(19, 28));
    p.drawLine(QPointF(19, 28), QPointF(15.5, 28.8));
    p.drawLine(QPointF(15.5, 28.8), QPointF(16, 25));
}

void drawFavorite(QPainter& p)
{
    accent(p, UBTheme::accentAmber());
    QPainterPath star;
    star.moveTo(22, 12);
    star.lineTo(25.1, 18.6);
    star.lineTo(32.2, 19.5);
    star.lineTo(27, 24.4);
    star.lineTo(28.3, 31.4);
    star.lineTo(22, 28);
    star.lineTo(15.7, 31.4);
    star.lineTo(17, 24.4);
    star.lineTo(11.8, 19.5);
    star.lineTo(18.9, 18.6);
    star.closeSubpath();
    p.drawPath(star);
}

void drawDuplicateDoc(QPainter& p)
{
    p.drawRoundedRect(QRectF(12, 12, 13, 13), 3, 3);
    QPainterPath rest;
    rest.moveTo(28, 18);
    rest.lineTo(31, 18);
    rest.lineTo(31, 31);
    rest.lineTo(18, 31);
    rest.lineTo(18, 28);
    p.drawPath(rest);
}

void drawOpenDoc(QPainter& p)
{
    QPainterPath box;
    box.moveTo(19, 14);
    box.lineTo(13, 14);
    box.lineTo(13, 30);
    box.lineTo(29, 30);
    box.lineTo(29, 24);
    p.drawPath(box);
    p.drawLine(QPointF(21, 22), QPointF(31, 12));
    p.drawLine(QPointF(31, 12), QPointF(25, 12));
    p.drawLine(QPointF(31, 12), QPointF(31, 18));
}

void drawAddToWorking(QPainter& p)
{
    p.drawRoundedRect(QRectF(16, 14, 13, 17), 2, 2);
    p.drawLine(QPointF(8, 22.5), QPointF(19, 22.5));
    p.drawLine(QPointF(19, 22.5), QPointF(15, 18.5));
    p.drawLine(QPointF(19, 22.5), QPointF(15, 26.5));
}

void drawAddCircle(QPainter& p)
{
    p.drawEllipse(QPointF(22, 22), 9.5, 9.5);
    p.drawLine(QPointF(17, 22), QPointF(27, 22));
    p.drawLine(QPointF(22, 17), QPointF(22, 27));
}

void drawStylus(QPainter& p)
{
    p.save();
    accent(p, UBTheme::accentRed());
    p.drawLine(QPointF(14, 30), QPointF(24, 20));
    p.drawLine(QPointF(24, 20), QPointF(27, 17));
    p.restore();
    p.drawLine(QPointF(19, 32), QPointF(29, 22));
    p.drawLine(QPointF(29, 22), QPointF(32, 19));
    p.drawLine(QPointF(12, 32.5), QPointF(16.5, 32.5));
}

void drawPagesMenu(QPainter& p)
{
    p.drawRoundedRect(QRectF(13, 12, 13, 17), 2, 2);
    p.save();
    accent(p, UBTheme::accentGreen());
    p.drawLine(QPointF(27, 26), QPointF(33, 26));
    p.drawLine(QPointF(30, 23), QPointF(30, 29));
    p.restore();
}

void drawPagePrev(QPainter& p)
{
    p.drawRoundedRect(QRectF(17, 12, 14, 18), 2, 2);
    p.save();
    accent(p, UBTheme::accentBlue());
    p.drawLine(QPointF(15, 21), QPointF(9, 21));
    p.drawLine(QPointF(9, 21), QPointF(13, 17));
    p.drawLine(QPointF(9, 21), QPointF(13, 25));
    p.restore();
}

void drawPageNext(QPainter& p)
{
    p.drawRoundedRect(QRectF(13, 12, 14, 18), 2, 2);
    p.save();
    accent(p, UBTheme::accentBlue());
    p.drawLine(QPointF(29, 21), QPointF(35, 21));
    p.drawLine(QPointF(35, 21), QPointF(31, 17));
    p.drawLine(QPointF(35, 21), QPointF(31, 25));
    p.restore();
}

void drawTools(QPainter& p)
{
    p.save();
    accent(p, UBTheme::accentAmber());
    QPainterPath wrench;
    wrench.moveTo(27.5, 16.5);
    wrench.arcTo(QRectF(23, 10, 10, 10), 30, 210);
    p.drawPath(wrench);
    p.restore();
    p.drawLine(QPointF(25, 19), QPointF(14, 30));
    p.drawLine(QPointF(14, 30), QPointF(12.5, 31.5));
}

void drawMathTools(QPainter& p)
{
    p.save();
    accent(p, UBTheme::accentBlue());
    p.drawLine(QPointF(12, 24), QPointF(15, 24));
    p.drawLine(QPointF(15, 24), QPointF(18, 30));
    p.drawLine(QPointF(18, 30), QPointF(22, 14));
    p.restore();
    p.drawLine(QPointF(22, 14), QPointF(32, 14));
    p.drawLine(QPointF(25, 20), QPointF(31, 26));
    p.drawLine(QPointF(31, 20), QPointF(25, 26));
}

void drawCaptureArea(QPainter& p)
{
    QPen dashed = p.pen();
    dashed.setStyle(Qt::DashLine);
    dashed.setWidthF(2.0);
    p.save();
    p.setPen(dashed);
    p.drawRect(QRectF(13, 13, 18, 18));
    p.restore();
    p.save();
    accent(p, UBTheme::accentBlue());
    p.drawLine(QPointF(22, 18), QPointF(22, 26));
    p.drawLine(QPointF(18, 22), QPointF(26, 22));
    p.restore();
}

void drawCaptureScreen(QPainter& p)
{
    p.drawRoundedRect(QRectF(12, 13, 20, 14), 2, 2);
    p.drawLine(QPointF(19, 31), QPointF(25, 31));
    p.drawLine(QPointF(22, 27), QPointF(22, 31));
    p.save();
    accent(p, UBTheme::accentBlue());
    p.drawEllipse(QPointF(22, 20), 3, 3);
    p.restore();
}

void drawEyeOpen(QPainter& p)
{
    QPainterPath eye;
    eye.moveTo(12, 22);
    eye.cubicTo(16, 15.5, 28, 15.5, 32, 22);
    eye.cubicTo(28, 28.5, 16, 28.5, 12, 22);
    p.drawPath(eye);
    p.setBrush(p.pen().color());
    p.drawEllipse(QPointF(22, 22), 3, 3);
    p.setBrush(Qt::NoBrush);
}

void drawEyeClosed(QPainter& p)
{
    QPainterPath lid;
    lid.moveTo(12, 20);
    lid.cubicTo(16, 26.5, 28, 26.5, 32, 20);
    p.drawPath(lid);
    p.drawLine(QPointF(15, 24.5), QPointF(13, 28));
    p.drawLine(QPointF(22, 26), QPointF(22, 30));
    p.drawLine(QPointF(29, 24.5), QPointF(31, 28));
}

void drawChevronLeft(QPainter& p)
{
    p.drawLine(QPointF(25, 14), QPointF(18, 22));
    p.drawLine(QPointF(18, 22), QPointF(25, 30));
}

void drawChevronRight(QPainter& p)
{
    p.drawLine(QPointF(19, 14), QPointF(26, 22));
    p.drawLine(QPointF(26, 22), QPointF(19, 30));
}

void drawArrowLeft(QPainter& p)
{
    p.drawLine(QPointF(31, 22), QPointF(13, 22));
    p.drawLine(QPointF(13, 22), QPointF(19, 16));
    p.drawLine(QPointF(13, 22), QPointF(19, 28));
}

void drawArrowRight(QPainter& p)
{
    p.save();
    p.scale(-1, 1);
    p.translate(-44, 0);
    drawArrowLeft(p);
    p.restore();
}

void drawReload(QPainter& p)
{
    QPainterPath arc;
    arc.arcMoveTo(QRectF(14, 14, 16, 16), 100);
    arc.arcTo(QRectF(14, 14, 16, 16), 100, -320);
    p.drawPath(arc);
    p.save();
    accent(p, UBTheme::accentBlue());
    p.drawLine(QPointF(15.9, 16.9), QPointF(14.6, 22.4));
    p.drawLine(QPointF(15.9, 16.9), QPointF(10.8, 19.2));
    p.restore();
}

void drawStop(QPainter& p)
{
    p.drawEllipse(QPointF(22, 22), 9, 9);
    p.save();
    accent(p, UBTheme::accentRed());
    p.drawLine(QPointF(18.5, 18.5), QPointF(25.5, 25.5));
    p.drawLine(QPointF(25.5, 18.5), QPointF(18.5, 25.5));
    p.restore();
}

void drawHome(QPainter& p)
{
    p.drawLine(QPointF(12, 22.5), QPointF(22, 13));
    p.drawLine(QPointF(22, 13), QPointF(32, 22.5));
    p.drawLine(QPointF(15, 20.5), QPointF(15, 31));
    p.drawLine(QPointF(29, 20.5), QPointF(29, 31));
    p.drawLine(QPointF(15, 31), QPointF(29, 31));
    p.save();
    accent(p, UBTheme::accentAmber());
    p.drawRoundedRect(QRectF(19.5, 25, 5, 6), 1.5, 1.5);
    p.restore();
}

void drawBookmark(QPainter& p)
{
    p.save();
    accent(p, UBTheme::accentAmber());
    QPainterPath ribbon;
    ribbon.moveTo(16, 13);
    ribbon.lineTo(28, 13);
    ribbon.lineTo(28, 31);
    ribbon.lineTo(22, 26);
    ribbon.lineTo(16, 31);
    ribbon.closeSubpath();
    p.drawPath(ribbon);
    p.restore();
}

void drawGeometry(QPainter& p)
{
    // drawing compass
    p.drawLine(QPointF(22, 15), QPointF(16, 30));
    p.drawLine(QPointF(22, 15), QPointF(28, 30));
    p.drawEllipse(QPointF(22, 13), 2.2, 2.2);
    p.save();
    accent(p, UBTheme::accentBlue());
    QPainterPath arc;
    arc.moveTo(14.5, 27.5);
    arc.quadTo(22, 33.5, 29.5, 27.5);
    p.drawPath(arc);
    p.restore();
}

void drawScience(QPainter& p)
{
    // Erlenmeyer flask
    p.drawLine(QPointF(19.5, 12), QPointF(19.5, 19));
    p.drawLine(QPointF(24.5, 12), QPointF(24.5, 19));
    p.drawLine(QPointF(17.5, 12), QPointF(26.5, 12));
    QPainterPath body;
    body.moveTo(19.5, 19);
    body.lineTo(14, 29.5);
    body.quadTo(13.2, 31.5, 15.5, 31.5);
    body.lineTo(28.5, 31.5);
    body.quadTo(30.8, 31.5, 30, 29.5);
    body.lineTo(24.5, 19);
    p.drawPath(body);
    p.save();
    accent(p, UBTheme::accentGreen());
    p.drawLine(QPointF(16.5, 26.5), QPointF(27.5, 26.5));
    p.restore();
}

void drawMedia(QPainter& p)
{
    p.drawRoundedRect(QRectF(12, 13, 20, 18), 3, 3);
    p.save();
    accent(p, UBTheme::accentRed());
    p.setBrush(UBTheme::accentRed());
    QPainterPath tri;
    tri.moveTo(19.5, 18);
    tri.lineTo(19.5, 26);
    tri.lineTo(26.5, 22);
    tri.closeSubpath();
    p.drawPath(tri);
    p.restore();
}

void drawMenu(QPainter& p)
{
    p.drawEllipse(QPointF(22, 22), 8.5, 8.5);
    p.setBrush(p.pen().color());
    p.drawEllipse(QPointF(22, 22), 2.0, 2.0);
    p.setBrush(Qt::NoBrush);

    for (int i = 0; i < 8; ++i)
    {
        const qreal angle = i * M_PI / 4.0;
        const QPointF direction(qCos(angle), qSin(angle));
        p.drawLine(QPointF(22, 22) + direction * 8.5, QPointF(22, 22) + direction * 11.5);
    }
}

}

namespace UBModernIcons
{

QIcon zoomInIcon()  { return makeIcon(drawZoomIn); }
QIcon geometryIcon()       { return makeIcon(drawGeometry); }
QIcon scienceIcon()        { return makeIcon(drawScience); }
QIcon mediaIcon()          { return makeIcon(drawMedia); }
QIcon clearIcon()          { return makeIcon(drawClear); }
QIcon bookmarkIcon()       { return makeIcon(drawBookmark); }
QIcon boardIcon()          { return makeIcon(drawBoard); }
QIcon mathToolsIcon()      { return makeIcon(drawMathTools); }
QIcon captureAreaIcon()    { return makeIcon(drawCaptureArea); }
QIcon captureScreenIcon()  { return makeIcon(drawCaptureScreen); }
QIcon chevronLeftIcon()    { return makeIcon(drawChevronLeft); }
QIcon chevronRightIcon()   { return makeIcon(drawChevronRight); }

QIcon showHideIcon()
{
    QIcon icon;
    const QIcon open = makeIcon(drawEyeOpen);
    const QIcon closed = makeIcon(drawEyeClosed);
    for (const QSize& size : open.availableSizes())
        icon.addPixmap(open.pixmap(size), QIcon::Normal, QIcon::On);
    for (const QSize& size : closed.availableSizes())
        icon.addPixmap(closed.pixmap(size), QIcon::Normal, QIcon::Off);
    return icon;
}
QIcon zoomOutIcon() { return makeIcon(drawZoomOut); }
QIcon toolsIcon()   { return makeIcon(drawTools); }

void apply(UBMainWindow* mainWindow)
{
    if (!mainWindow)
        return;

    const struct { QAction* action; Painterfunction draw; } assignments[] = {
        { mainWindow->actionPen,             drawPen },
        { mainWindow->actionMarker,          drawMarker },
        { mainWindow->actionEraser,          drawEraser },
        { mainWindow->actionSelector,        drawSelector },
        { mainWindow->actionPlay,            drawPlay },
        { mainWindow->actionHand,            drawHand },
        { mainWindow->actionZoomIn,          drawZoomIn },
        { mainWindow->actionZoomOut,         drawZoomOut },
        { mainWindow->actionLine,            drawLineTool },
        { mainWindow->actionText,            drawText },
        { mainWindow->actionCapture,         drawCapture },
        { mainWindow->actionUndo,            drawUndo },
        { mainWindow->actionRedo,            drawRedo },
        { mainWindow->actionNewPage,         drawNewPage },
        { mainWindow->actionBackgrounds,     drawBackgrounds },
        { mainWindow->actionBoard,           drawBoard },
        { mainWindow->actionWeb,             drawWeb },
        { mainWindow->actionDocument,        drawDocuments },
        { mainWindow->actionDesktop,         drawDesktop },
        { mainWindow->actionVirtualKeyboard, drawKeyboard },
        { mainWindow->actionErase,           drawClear },
        { mainWindow->actionSnapToShape,     drawSnap },
        { mainWindow->actionMenu,            drawMenu },
        { mainWindow->actionNewDocument,     drawNewDocument },
        { mainWindow->actionNewFolder,       drawNewFolder },
        { mainWindow->actionImport,          drawImport },
        { mainWindow->actionExport,          drawExport },
        { mainWindow->actionRename,          drawRename },
        { mainWindow->actionAddDocumentToFavorites, drawFavorite },
        { mainWindow->actionDuplicate,       drawDuplicateDoc },
        { mainWindow->actionDelete,          drawClear },
        { mainWindow->actionOpen,            drawOpenDoc },
        { mainWindow->actionAddToWorkingDocument, drawAddToWorking },
        { mainWindow->actionDocumentAdd,     drawAddCircle },
        { mainWindow->actionStylus,          drawStylus },
        { mainWindow->actionPages,           drawPagesMenu },
        { mainWindow->actionBack,            drawPagePrev },
        { mainWindow->actionForward,         drawPageNext },
        { mainWindow->actionWebTools,        drawTools },
        { mainWindow->actionWebBack,         drawArrowLeft },
        { mainWindow->actionWebForward,      drawArrowRight },
        { mainWindow->actionWebReload,       drawReload },
        { mainWindow->actionStopLoading,     drawStop },
        { mainWindow->actionHome,            drawHome },
        { mainWindow->actionWebBigger,       drawZoomIn },
        { mainWindow->actionWebSmaller,      drawZoomOut },
        { mainWindow->actionBookmarks,       drawBookmark },
        { mainWindow->actionAddBookmark,     drawBookmark },
        { mainWindow->actionWebCustomCapture, drawCaptureArea },
        { mainWindow->actionWebWindowCapture, drawCaptureScreen },
    };

    for (const auto& assignment : assignments)
    {
        if (assignment.action)
            assignment.action->setIcon(makeIcon(assignment.draw));
    }

    // Checkable with distinct on/off art -- outside the single-glyph table.
    if (mainWindow->actionWebShowHideOnDisplay)
        mainWindow->actionWebShowHideOnDisplay->setIcon(showHideIcon());
}

}
