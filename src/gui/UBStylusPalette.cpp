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

UBPenPropertiesPopup::UBPenPropertiesPopup(QWidget* parent)
    : QWidget(parent, Qt::Popup)      // Popup closes itself when tapped away from
{
    setObjectName("ubPenPropertiesPopup");
    setStyleSheet("QWidget#ubPenPropertiesPopup { background-color: #303030;"
                  " border: 1px solid #808080; border-radius: 6px; }"
                  "QLabel { color: white; }");

    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    mColourGrid = new QGridLayout();
    mColourGrid->setSpacing(4);
    outer->addLayout(mColourGrid);

    QHBoxLayout* widthRow = new QHBoxLayout();
    widthRow->setSpacing(8);

    // Stepper buttons flank the slider: a fingertip can nudge the width by one
    // step reliably, which dragging a slider on a touchscreen often cannot.
    const QString stepperCss =
            "QToolButton { color: white; background: #4a4a4a; border: none;"
            " border-radius: 6px; font-size: 22px; font-weight: bold; }"
            "QToolButton:pressed { background: #6a6a6a; }";

    QToolButton* decrease = new QToolButton(this);
    decrease->setText(QStringLiteral("-"));
    decrease->setFixedSize(44, 44);
    decrease->setAutoRepeat(true);
    decrease->setStyleSheet(stepperCss);
    decrease->setToolTip(tr("Thinner"));

    QToolButton* increase = new QToolButton(this);
    increase->setText(QStringLiteral("+"));
    increase->setFixedSize(44, 44);
    increase->setAutoRepeat(true);
    increase->setStyleSheet(stepperCss);
    increase->setToolTip(tr("Thicker"));

    mWidthSlider = new QSlider(Qt::Horizontal, this);
    mWidthSlider->setMinimum(1);
    mWidthSlider->setMaximum(60);          // halves; see widthChanged()
    mWidthSlider->setMinimumWidth(240);
    mWidthSlider->setFixedHeight(44);
    mWidthSlider->setPageStep(2);
    // A fat groove and a large handle -- the default Qt slider is far too fine
    // to grab with a fingertip on a smartboard.
    mWidthSlider->setStyleSheet(
            "QSlider::groove:horizontal { height: 12px; background: #555555;"
            " border-radius: 6px; }"
            "QSlider::sub-page:horizontal { height: 12px; background: #9a9a9a;"
            " border-radius: 6px; }"
            "QSlider::handle:horizontal { width: 32px; margin: -11px 0;"
            " background: #f0f0f0; border-radius: 16px; }");
    connect(mWidthSlider, &QSlider::valueChanged, this, &UBPenPropertiesPopup::widthChanged);

    connect(decrease, &QToolButton::clicked, this, [this]() {
        mWidthSlider->setValue(mWidthSlider->value() - 1);
    });
    connect(increase, &QToolButton::clicked, this, [this]() {
        mWidthSlider->setValue(mWidthSlider->value() + 1);
    });

    mWidthPreview = new QLabel(this);
    mWidthPreview->setFixedSize(72, 44);

    widthRow->addWidget(decrease);
    widthRow->addWidget(mWidthSlider);
    widthRow->addWidget(increase);
    widthRow->addWidget(mWidthPreview);
    outer->addLayout(widthRow);

    buildColourGrid();
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
        swatch->setFixedSize(30, 30);
        swatch->setAutoRaise(true);
        swatch->setCursor(Qt::PointingHandCursor);

        QPixmap pm(24, 24);
        pm.fill(colours.at(i));
        QPainter p(&pm);
        p.setPen(QPen(QColor(0, 0, 0, 120), 1));
        p.drawRect(0, 0, pm.width() - 1, pm.height() - 1);
        p.end();

        swatch->setIcon(QIcon(pm));
        swatch->setIconSize(QSize(24, 24));
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
    more->setFixedSize(30, 30);
    more->setAutoRaise(true);
    more->setText(QStringLiteral("+"));
    more->setToolTip(tr("Choose another colour"));
    more->setStyleSheet("QToolButton { color: white; font-size: 18px; font-weight: bold; }");
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

    // WistOpenboard fork: full-screen toggle. Sits with the tools rather than in a
    // top toolbar, because the whole point is to make the top toolbars go away.
    mFullScreenAction = new QAction(QIcon(":/images/toolbar/display.png"), tr("Full screen"), this);
    mFullScreenAction->setCheckable(true);
    mFullScreenAction->setToolTip(tr("Full screen - hide the title bar and top toolbar"));
    mFullScreenAction->setProperty("ungrouped", true);   // a toggle, not a tool choice
    connect(mFullScreenAction, &QAction::toggled, this, &UBStylusPalette::toggleFullScreen);
    actions << mFullScreenAction;

    mCollapseAction = new QAction(QIcon(":/images/toolbar/previous.png"), tr("Show fewer tools"), this);
    mCollapseAction->setCheckable(true);
    mCollapseAction->setToolTip(tr("Collapse to pen and eraser"));
    mCollapseAction->setProperty("ungrouped", true);
    connect(mCollapseAction, &QAction::toggled, this, &UBStylusPalette::setCollapsed);
    actions << mCollapseAction;

    setActions(actions);
    setButtonIconSize(QSize(42, 42));
    groupActions();

    // Keep the collapse arrow visually small -- the whole point is a palette
    // that stays out of the way.
    if (UBActionPaletteButton* collapseButton = getButtonFromAction(mCollapseAction))
    {
        collapseButton->setIconSize(QSize(14, 14));
        collapseButton->setFixedSize(22, 22);
    }

    UBShortcutManager::shortcutManager()->addActionGroup(mActionGroup);

    adjustSizeAndPosition();

    initPosition();

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
        if (it.value())
            it.value()->setVisible(!collapsed || alwaysShown.contains(it.key()));
    }

    mCollapseAction->setIcon(QIcon(collapsed ? ":/images/toolbar/next.png"
                                             : ":/images/toolbar/previous.png"));
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


