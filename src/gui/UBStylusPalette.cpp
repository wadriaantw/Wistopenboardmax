/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * Copyright (C) 2013 Open Education Foundation
 *
 * Copyright (C) 2010-2013 Groupement d'Intérêt Public pour
 * l'Education Numérique en Afrique (GIP ENA)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */




#include "UBStylusPalette.h"

#include <QtGui>

#include "UBMainWindow.h"

#include <QToolBar>
#include <QScreen>
#include <QSlider>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QShowEvent>
#include <QHideEvent>
#include <QCursor>
#include <QToolButton>
#include <QPainter>
#include <QPixmap>

#include "board/UBBoardController.h"
#include "domain/UBGraphicsScene.h"

#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "core/UBApplicationController.h"
#include "core/UBShortcutManager.h"


#include "board/UBDrawingController.h"

#include "frameworks/UBPlatformUtils.h"

#include "core/memcheck.h"

// ---------------------------------------------------------------------------
//  UBPenPropertiesPopup
// ---------------------------------------------------------------------------

static bool ubOnDarkBackground()
{
    if (UBApplication::boardController && UBApplication::boardController->activeScene())
        return UBApplication::boardController->activeScene()->isDarkBackground();

    return false;
}

// NOT a Qt::Popup. A popup auto-closes on any press outside itself, and on a
// touchscreen the synthesised mouse events around a long-press arrive in an
// order that dismissed and re-triggered it -- visible as flicker. As a plain
// frameless tool window WE own the close logic: an app-wide event filter
// closes it on a press outside, but only after a grace period, so stray
// synthesised events from the opening gesture cannot touch it.
UBPenPropertiesPopup::UBPenPropertiesPopup(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);   // lets the rounded corners clip
    setAttribute(Qt::WA_StyledBackground);        // custom QWidget subclasses skip stylesheet backgrounds without this
    setObjectName("ubPenPropertiesPopup");
    setStyleSheet("QWidget#ubPenPropertiesPopup { background-color: #FFFFFF;"
                  " border: 1px solid #E0E0DD; border-radius: 12px; }"
                  "QLabel { color: #3A3A38; }");

    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 8, 10, 8);
    outer->setSpacing(6);

    mColourGrid = new QGridLayout();
    mColourGrid->setSpacing(2);
    outer->addLayout(mColourGrid);

    QHBoxLayout* widthRow = new QHBoxLayout();
    widthRow->setSpacing(8);

    // Stepper buttons flank the slider: a fingertip can nudge the width by one
    // step reliably, which dragging a slider on a touchscreen often cannot.
    const QString stepperCss =
            "QToolButton { color: #3A3A38; background: #F2F2F0; border: 1px solid #D0D0CC;"
            " border-radius: 8px; font-size: 17px; font-weight: bold; }"
            "QToolButton:pressed { background: #E5E3DC; }";

    QToolButton* decrease = new QToolButton(this);
    decrease->setText(QStringLiteral("-"));
    decrease->setFixedSize(32, 32);
    decrease->setAutoRepeat(true);
    decrease->setStyleSheet(stepperCss);
    decrease->setToolTip(tr("Thinner"));

    QToolButton* increase = new QToolButton(this);
    increase->setText(QStringLiteral("+"));
    increase->setFixedSize(32, 32);
    increase->setAutoRepeat(true);
    increase->setStyleSheet(stepperCss);
    increase->setToolTip(tr("Thicker"));

    mWidthSlider = new QSlider(Qt::Horizontal, this);
    mWidthSlider->setMinimum(1);
    mWidthSlider->setMaximum(60);          // halves; see widthChanged()
    mWidthSlider->setMinimumWidth(150);
    mWidthSlider->setFixedHeight(30);
    mWidthSlider->setPageStep(2);
    // A fat groove and a large handle -- the default Qt slider is far too fine
    // to grab with a fingertip on a smartboard.
    mWidthSlider->setStyleSheet(
            "QSlider::groove:horizontal { height: 6px; background: #E5E3DC;"
            " border-radius: 3px; }"
            "QSlider::sub-page:horizontal { height: 6px; background: #3A3A38;"
            " border-radius: 3px; }"
            "QSlider::handle:horizontal { width: 20px; margin: -7px 0;"
            " background: #FFFFFF; border: 1px solid #B0B0AC; border-radius: 10px; }");
    connect(mWidthSlider, &QSlider::valueChanged, this, &UBPenPropertiesPopup::widthChanged);

    connect(decrease, &QToolButton::clicked, this, [this]() {
        mWidthSlider->setValue(mWidthSlider->value() - 1);
    });
    connect(increase, &QToolButton::clicked, this, [this]() {
        mWidthSlider->setValue(mWidthSlider->value() + 1);
    });

    mWidthPreview = new QLabel(this);
    mWidthPreview->setFixedSize(44, 30);

    widthRow->addWidget(decrease);
    widthRow->addWidget(mWidthSlider);
    widthRow->addWidget(increase);
    widthRow->addWidget(mWidthPreview);
    outer->addLayout(widthRow);

    buildColourGrid();
}

void UBPenPropertiesPopup::showEvent(QShowEvent* event)
{
    mShownAt.start();
    qApp->installEventFilter(this);
    QWidget::showEvent(event);
}

void UBPenPropertiesPopup::hideEvent(QHideEvent* event)
{
    qApp->removeEventFilter(this);
    QWidget::hideEvent(event);
}

bool UBPenPropertiesPopup::eventFilter(QObject* watched, QEvent* event)
{
    if (isVisible()
        && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin))
    {
        // Ignore everything in the first 400 ms -- leftovers of the gesture
        // that opened us.
        if (mShownAt.elapsed() > 400)
        {
            QPoint globalPos = QCursor::pos();

            if (event->type() == QEvent::MouseButtonPress)
                globalPos = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();

            if (!geometry().contains(globalPos))
                close();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void UBPenPropertiesPopup::buildColourGrid()
{
    // Clear any previous swatches (the palette differs on dark backgrounds).
    while (QLayoutItem* item = mColourGrid->takeAt(0))
    {
        if (item->widget())
            item->widget()->deleteLater();

        delete item;
    }

    const bool onDark = ubOnDarkBackground();
    const QList<QColor> colours = UBSettings::settings()->penColors(onDark);

    int row = 0;
    int col = 0;
    const int columns = 6;

    for (int i = 0; i < colours.count(); ++i)
    {
        QToolButton* swatch = new QToolButton(this);
        swatch->setFixedSize(26, 26);
        swatch->setAutoRaise(true);
        swatch->setCursor(Qt::PointingHandCursor);
        swatch->setStyleSheet("QToolButton { border: none; background: transparent; }");

        // Round chip with a light ring.
        QPixmap pm(22, 22);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(colours.at(i));
        p.setPen(QPen(QColor(0xD0, 0xD0, 0xCC), 1));
        p.drawEllipse(QRectF(0.5, 0.5, 21., 21.));
        p.end();

        swatch->setIcon(QIcon(pm));
        swatch->setIconSize(QSize(22, 22));
        swatch->setToolTip(colours.at(i).name());

        connect(swatch, &QToolButton::clicked, this, [this, i]() {
            UBDrawingController::drawingController()->setColorIndex(i);
            close();
        });

        mColourGrid->addWidget(swatch, row, col);

        if (++col >= columns) { col = 0; ++row; }
    }

    // Anything not in the palette: writes the chosen colour into the current
    // index, which setPenColor() supports directly.
    QToolButton* more = new QToolButton(this);
    more->setFixedSize(26, 26);
    more->setAutoRaise(true);
    more->setText(QStringLiteral("+"));
    more->setToolTip(tr("Choose another colour"));
    more->setStyleSheet("QToolButton { color: #3A3A38; font-size: 15px; font-weight: bold;"
                        " border: 1px solid #D0D0CC; border-radius: 13px; background: transparent; }");
    connect(more, &QToolButton::clicked, this, &UBPenPropertiesPopup::pickCustomColour);
    mColourGrid->addWidget(more, row, col);
}

void UBPenPropertiesPopup::pickCustomColour()
{
    const bool onDark = ubOnDarkBackground();
    const int index = UBDrawingController::drawingController()->currentToolColorIndex();

    QColor chosen = QColorDialog::getColor(UBSettings::settings()->penColor(onDark),
                                           this, tr("Pen colour"));

    if (chosen.isValid())
    {
        UBDrawingController::drawingController()->setPenColor(onDark, chosen, index);
        UBDrawingController::drawingController()->setColorIndex(index);
        buildColourGrid();
    }

    close();
}

void UBPenPropertiesPopup::widthChanged(int sliderValue)
{
    // The slider is continuous, but OpenBoard stores one width per size index --
    // so the value lands on whichever of fine/medium/strong is selected. The three
    // toolbar sizes keep working; they just become the teacher's own three.
    const qreal width = sliderValue / 2.0;      // 0.5 .. 30.0
    UBSettings* settings = UBSettings::settings();

    switch (settings->penWidthIndex())
    {
        case UBWidth::Medium: settings->boardPenMediumWidth->set(width); break;
        case UBWidth::Strong: settings->boardPenStrongWidth->set(width); break;
        case UBWidth::Fine:
        default:              settings->boardPenFineWidth->set(width);   break;
    }

    // Preview dot, drawn at the real width in the real colour.
    QPixmap pm(mWidthPreview->size());
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(UBSettings::settings()->penColor(ubOnDarkBackground()));
    p.setPen(Qt::NoPen);
    const qreal d = qMin<qreal>(width, pm.height() - 2);
    p.drawEllipse(QPointF(pm.width() / 2.0, pm.height() / 2.0), d / 2.0, d / 2.0);
    p.end();
    mWidthPreview->setPixmap(pm);
}

void UBPenPropertiesPopup::refresh()
{
    buildColourGrid();

    const int slider = qBound(1, int(UBSettings::settings()->currentPenWidth() * 2.0), 60);
    mWidthSlider->blockSignals(true);
    mWidthSlider->setValue(slider);
    mWidthSlider->blockSignals(false);
    widthChanged(slider);

    adjustSize();
}

UBStylusPalette::UBStylusPalette(QWidget *parent, Qt::Orientation orient)
    : UBActionPalette(Qt::TopLeftCorner, parent, orient)
    , mLastSelectedId(-1)
{
    QList<QAction*> actions;

    actions << UBApplication::mainWindow->actionPen;
    // WistOpenboard fork: snap-to-shape placed next to stylus (Pen) per user request.
    // It is a toggle, not an exclusive tool selection.
    actions << UBApplication::mainWindow->actionSnapToShape;
    UBApplication::mainWindow->actionSnapToShape->setProperty("ungrouped", true);

    actions << UBApplication::mainWindow->actionEraser;

    // WistOpenboard fork: undo/redo on the dock -- in zen mode the toolbar that
    // used to carry them is hidden.
    actions << UBApplication::mainWindow->actionUndo;
    UBApplication::mainWindow->actionUndo->setProperty("ungrouped", true);
    actions << UBApplication::mainWindow->actionRedo;
    UBApplication::mainWindow->actionRedo->setProperty("ungrouped", true);
    actions << UBApplication::mainWindow->actionMarker;
    actions << UBApplication::mainWindow->actionSelector;
    actions << UBApplication::mainWindow->actionPlay;

    actions << UBApplication::mainWindow->actionHand;
    actions << UBApplication::mainWindow->actionZoomIn;
    actions << UBApplication::mainWindow->actionZoomOut;

    // Laser pointer removed
    actions << UBApplication::mainWindow->actionLine;
    actions << UBApplication::mainWindow->actionText;
    actions << UBApplication::mainWindow->actionCapture;

    // WistOpenboard fork: desktop mode, so annotating over other apps does not
    // mean going back up to the top toolbar (hidden in full screen).
    actions << UBApplication::mainWindow->actionDesktop;
    UBApplication::mainWindow->actionDesktop->setProperty("ungrouped", true);

    if(UBPlatformUtils::hasVirtualKeyboard())
    {
        actions << UBApplication::mainWindow->actionVirtualKeyboard;
        UBApplication::mainWindow->actionVirtualKeyboard->setProperty("ungrouped", true);
    }

    // Snap-to-grid/angle removed

    // WistOpenboard fork: the full-screen toggle used to live here; zen mode made
    // it redundant (chrome is hidden by default and the title bar carries the
    // window controls), so it was removed to keep the dock short.

    // Three vertical dots, painted in code (no font or theme pixmap involved).
    // The same icon serves both states -- dots read as "more" either way.
    QPixmap dotsPixmap(24, 34);
    dotsPixmap.fill(Qt::transparent);
    {
        QPainter p(&dotsPixmap);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x30, 0x30, 0x30));
        for (int i = 0; i < 3; ++i)
            p.drawEllipse(QPointF(12, 11 + i * 6), 2.2, 2.2);
    }

    mCollapseAction = new QAction(QIcon(dotsPixmap), tr("Show fewer tools"), this);
    mCollapseAction->setCheckable(true);
    mCollapseAction->setToolTip(tr("Collapse to pen and eraser"));
    mCollapseAction->setProperty("ungrouped", true);
    connect(mCollapseAction, &QAction::toggled, this, &UBStylusPalette::setCollapsed);
    actions << mCollapseAction;

    setActions(actions);
    // WistOpenboard fork: 34px instead of the original 42px -- noticeably more
    // compact while staying just above a usable fingertip target. Tune here if
    // the smartboard digitizer proves coarser than the laptop screen.
    setButtonIconSize(QSize(34, 34));
    groupActions();

    // Keep the collapse arrow visually small -- the whole point is a palette
    // that stays out of the way.
    if (UBActionPaletteButton* collapseButton = getButtonFromAction(mCollapseAction))
    {
        // Small ICON, big TARGET. The button is flat, so only the arrow is
        // visible -- but the tappable area stays finger-sized. At 22x22 most
        // touches missed and fell through to the palette background, whose
        // mousePressEvent treats any left press as the start of a palette drag,
        // so tapping the arrow appeared to do nothing at all.
        collapseButton->setIconSize(QSize(24, 34));
        collapseButton->setFixedSize(24, 34);   // narrow: the dots column needs no width
    }

    UBShortcutManager::shortcutManager()->addActionGroup(mActionGroup);

    updateLayout();

    adjustSizeAndPosition();

    initPosition();

    // WistOpenboard fork: start minimised -- pen, eraser and the dots. The
    // toggle re-centres it, so it comes up as a small pill at bottom-centre.
    if (mCollapseAction)
        mCollapseAction->setChecked(true);

    // WistOpenboard fork: hold the pen button for colour and width.
    if (UBActionPaletteButton* penButton = getButtonFromAction(UBApplication::mainWindow->actionPen))
        connect(penButton, &UBActionPaletteButton::longPressed, this, &UBStylusPalette::showPenProperties);

    foreach(const UBActionPaletteButton* button, mButtons)
    {
        connect(button, SIGNAL(doubleClicked()), this, SLOT(stylusToolDoubleClicked()));
    }

}

void UBStylusPalette::initPosition()
{
    QWidget* pParentW = parentWidget();
    if(!pParentW) return ;

    mCustomPosition = true;

    QPoint pos;
    int parentWidth = pParentW->width();
    int parentHeight = pParentW->height();

    if(UBSettings::settings()->appToolBarOrientationVertical->get().toBool()){
        int posX = border();
        int posY = (parentHeight / 2) - (height() / 2);
        pos.setX(posX);
        pos.setY(posY);
    }
    else {
        int posX = (parentWidth / 2) - (width() / 2);
        int posY = parentHeight - border() - height();
        pos.setX(posX);
        pos.setY(posY);
    }
    moveInsideParent(pos);
}

UBStylusPalette::~UBStylusPalette()
{
    if (mActionGroup)
    {
        UBShortcutManager::shortcutManager()->removeActionGroup(mActionGroup);
    }
}

void UBStylusPalette::updateLayout()
{
    UBActionPalette::updateLayout();

    // Margins hug the buttons on three sides; the LEADING edge keeps an 18px
    // strip of background as the finger-drag handle -- left edge when the
    // palette is horizontal, top edge when vertical. (First version put the
    // strip on top unconditionally, which just made a horizontal bar taller.)
    QBoxLayout* box = qobject_cast<QBoxLayout*>(layout());
    const bool horizontal = box
            && (box->direction() == QBoxLayout::LeftToRight
                || box->direction() == QBoxLayout::RightToLeft);

    if (horizontal)
        layout()->setContentsMargins(18, 7, 7, 7);
    else
        layout()->setContentsMargins(7, 18, 7, 7);

    layout()->setSpacing(4);
}

void UBStylusPalette::setCollapsed(bool collapsed)
{
    UBMainWindow* mainWindow = UBApplication::mainWindow;

    if (!mainWindow)
        return;

    // Everything else folds away; these stay so the palette is still usable, and
    // the arrow stays so there is a way back.
    const QList<QAction*> alwaysShown{mainWindow->actionPen,
                                      mainWindow->actionEraser,
                                      mCollapseAction};

    for (auto it = mMapActionToButton.constBegin(); it != mMapActionToButton.constEnd(); ++it)
    {
        if (!it.value())
            continue;

        const bool hideIt = collapsed && !alwaysShown.contains(it.key());

        // The property is what actionChanged() honours; setVisible alone gets
        // reverted by the next QAction::changed signal (see UBActionPalette).
        it.value()->setProperty("collapsedHidden", hideIt);
        it.value()->setVisible(!hideIt && it.key()->isVisible());
    }

    mCollapseAction->setToolTip(collapsed ? tr("Show all tools")
                                          : tr("Collapse to pen and eraser"));

    // Hiding the buttons is not enough on its own: preferredSize() reads
    // sizeHint(), and without invalidating the layout first that hint is still
    // the old full-width one -- the buttons vanished but the palette stayed the
    // same size, which just looks broken.
    if (layout())
    {
        layout()->invalidate();
        layout()->activate();
    }

    updateGeometry();
    resize(preferredSize());
    adjustSizeAndPosition();

    // Re-centre on the bottom edge after every collapse/expand, so the dock
    // always sits centred regardless of the size it just changed to. (This
    // also keeps it on-screen, which the explicit clamp used to handle.)
    initPosition();
}

void UBStylusPalette::showPenProperties()
{
    UBMainWindow* mainWindow = UBApplication::mainWindow;

    if (!mainWindow)
        return;

    if (!mPenPropertiesPopup)
        mPenPropertiesPopup = new UBPenPropertiesPopup(this);

    mPenPropertiesPopup->refresh();

    // Centre it directly over the pen button and sit it just above, so the
    // teacher's hand is already in the right place. Falls below the button if
    // there is no room above.
    UBActionPaletteButton* penButton = getButtonFromAction(mainWindow->actionPen);
    QWidget* anchor = penButton ? static_cast<QWidget*>(penButton) : static_cast<QWidget*>(this);

    const QSize popupSize = mPenPropertiesPopup->sizeHint();
    const QPoint anchorTopLeft = anchor->mapToGlobal(QPoint(0, 0));

    int x = anchorTopLeft.x() + anchor->width() / 2 - popupSize.width() / 2;
    int y = anchorTopLeft.y() - popupSize.height() - 8;

    const QRect available = screen() ? screen()->availableGeometry() : QRect();

    if (available.isValid())
    {
        if (y < available.top())
            y = anchorTopLeft.y() + anchor->height() + 8;      // no room above

        x = qBound(available.left() + 4, x, available.right() - popupSize.width() - 4);
    }

    mPenPropertiesPopup->move(x, y);
    mPenPropertiesPopup->show();
    mPenPropertiesPopup->raise();
}

void UBStylusPalette::toggleFullScreen(bool on)
{
    UBMainWindow* mainWindow = UBApplication::mainWindow;

    if (!mainWindow)
        return;

    if (on)
    {
        // Remember exactly which toolbars were up: only the one for the current
        // main mode is visible, so blindly showing all three on the way out would
        // surface toolbars that are meant to stay hidden.
        mHiddenToolBars.clear();
        mWasMaximized = mainWindow->isMaximized();

        const QList<QToolBar*> bars{mainWindow->boardToolBar,
                                    mainWindow->webToolBar,
                                    mainWindow->documentToolBar};

        for (QToolBar* bar : bars)
        {
            if (bar && bar->isVisible())
            {
                mHiddenToolBars << bar;
                bar->hide();
            }
        }

        mainWindow->showFullScreen();
    }
    else
    {
        for (const QPointer<QToolBar>& bar : mHiddenToolBars)
        {
            if (bar)
                bar->show();
        }

        mHiddenToolBars.clear();

        // Come back to whatever the window was before, not an arbitrary size.
        if (mWasMaximized)
            mainWindow->showMaximized();
        else
            mainWindow->showNormal();
    }
}

void UBStylusPalette::stylusToolDoubleClicked()
{
    emit stylusToolDoubleClicked(mActionGroup->checkedAction()->property("id").toInt());
}


