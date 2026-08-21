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
            QPixmap pixmap(int(44 * scale), int(44 * scale));
            pixmap.setDevicePixelRatio(scale);
            pixmap.fill(Qt::transparent);

            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setOpacity(opacity);

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

void drawPen(QPainter& p)
{
    p.drawLine(QPointF(15, 29), QPointF(27, 17));
    p.drawLine(QPointF(27, 17), QPointF(30, 14));
    QPainterPath nib;
    nib.moveTo(15, 29);
    nib.lineTo(12.5, 31.5);
    nib.lineTo(15.8, 31.8);
    p.setBrush(p.pen().color());
    p.drawPath(nib);
    p.setBrush(Qt::NoBrush);
}

void drawMarker(QPainter& p)
{
    QPen fat = p.pen();
    fat.setWidthF(6.0);
    fat.setCapStyle(Qt::FlatCap);
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
    p.drawLine(QPointF(22, 12), QPointF(22, 32));
    p.drawLine(QPointF(12, 22), QPointF(32, 22));
    for (const auto& arrowhead : {QPointF(22, 12), QPointF(22, 32), QPointF(12, 22), QPointF(32, 22)})
    {
        const qreal dx = arrowhead.x() == 22 ? 3.2 : (arrowhead.x() < 22 ? 3.2 : -3.2);
        const qreal dy = arrowhead.y() == 22 ? 3.2 : (arrowhead.y() < 22 ? 3.2 : -3.2);

        if (arrowhead.x() == 22)
        {
            p.drawLine(arrowhead, arrowhead + QPointF(-3.2, dy));
            p.drawLine(arrowhead, arrowhead + QPointF(3.2, dy));
        }
        else
        {
            p.drawLine(arrowhead, arrowhead + QPointF(dx, -3.2));
            p.drawLine(arrowhead, arrowhead + QPointF(dx, 3.2));
        }
    }
}

void drawZoomIn(QPainter& p)
{
    p.drawEllipse(QPointF(20, 20), 8, 8);
    p.drawLine(QPointF(26, 26), QPointF(32, 32));
    p.drawLine(QPointF(16.5, 20), QPointF(23.5, 20));
    p.drawLine(QPointF(20, 16.5), QPointF(20, 23.5));
}

void drawZoomOut(QPainter& p)
{
    p.drawEllipse(QPointF(20, 20), 8, 8);
    p.drawLine(QPointF(26, 26), QPointF(32, 32));
    p.drawLine(QPointF(16.5, 20), QPointF(23.5, 20));
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
    p.drawLine(QPointF(28, 25), QPointF(34, 25));
    p.drawLine(QPointF(31, 22), QPointF(31, 28));
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
    };

    for (const auto& assignment : assignments)
    {
        if (assignment.action)
            assignment.action->setIcon(makeIcon(assignment.draw));
    }
}

}
