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




#include <QtGui>
#include <QWebEngineView>
#include <QWebEnginePage>

#include "frameworks/UBPlatformUtils.h"

#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "core/UBApplicationController.h"
#include "core/UBDisplayManager.h"

#include "gui/UBMainWindow.h"
#include "web/UBWebController.h"
#include "web/simplebrowser/browserwindow.h"
#include "web/simplebrowser/tabwidget.h"
#include "web/simplebrowser/webview.h"

#include "UBWebToolsPalette.h"
#include "UBResources.h"
#include "UBIconButton.h"

#include "core/memcheck.h"

UBWebToolsPalette::UBWebToolsPalette(QWidget *parent)
    : UBActionPalette(Qt::TopRightCorner, parent)
    , mMinimizeAction(nullptr)
    , mMinimized(false)
{
    QList<QAction*> actions;

    // WistOpenboard fork: drawing tools at the top so the user can annotate
    // the web page (canvas overlay is injected by UBWebController). Selector
    // turns annotation off and passes clicks through to the page.
    actions << UBApplication::mainWindow->actionPen;
    actions << UBApplication::mainWindow->actionMarker;
    actions << UBApplication::mainWindow->actionEraser;
    actions << UBApplication::mainWindow->actionSelector;

    // Clear all ink on the current web page (wipes the injected canvas and
    // its persisted strokes for this URL).
    QAction *clearInk = new QAction(QIcon(":/images/toolbar/clearPage.png"),
                                    tr("Erase All Ink"), this);
    clearInk->setToolTip(tr("Erase all ink on this page"));
    connect(clearInk, &QAction::triggered, this, [](){
        if (!UBApplication::webController) return;
        BrowserWindow *bw = UBApplication::webController->browserWindow();
        if (!bw || !bw->currentTab()) return;
        bw->currentTab()->page()->runJavaScript("if(window.__obClear)__obClear();");
    });
    actions << clearInk;

    actions << UBApplication::mainWindow->actionCaptureWebContent;

    actions << UBApplication::mainWindow->actionWebCustomCapture;
    actions << UBApplication::mainWindow->actionWebWindowCapture;
// NOTE @letsfindaway obsolete, covered by actionWebTrapFlash
//    actions << UBApplication::mainWindow->actionWebOEmbed;

    // WistOpenboard fork: actionWebShowHideOnDisplay removed — it only does
    // anything when a secondary display is connected and was confusing as a
    // dead button on single-screen setups.
    // actions << UBApplication::mainWindow->actionWebShowHideOnDisplay;

    if (UBPlatformUtils::hasVirtualKeyboard())
        actions << UBApplication::mainWindow->actionVirtualKeyboard;

    // WistOpenboard fork: minimize button collapses the floating toolbar to
    // just this single button so it gets out of the way; clicking it again
    // restores the full set of tools.
    // Match the arrow icon used by the desktop-mode floating toolbar's
    // minimize button so the affordance is consistent across modes.
    mMinimizeAction = new QAction(QIcon(":/images/toolbar/previous.png"),
                                  tr("Minimize toolbar"), this);
    mMinimizeAction->setToolTip(tr("Minimize toolbar"));
    actions << mMinimizeAction;

    setActions(actions);
    setButtonIconSize(QSize(42, 42));

    // Capture every button except the minimize button so we can collapse them
    // together. Using button visibility (not action visibility) avoids hiding
    // the same global QActions in OpenBoard's main toolbar.
    for (QAction *a : actions) {
        if (a == mMinimizeAction) continue;
        if (UBActionPaletteButton *btn = getButtonFromAction(a))
            mCollapsibleButtons << btn;
    }

    // Wire the toggle directly off the minimize button's clicked signal —
    // QToolButton::setDefaultAction routes the click through the action, but
    // hooking the button is one less hop and works regardless of how the
    // base palette consumes mouse events.
    if (UBActionPaletteButton *minBtn = getButtonFromAction(mMinimizeAction))
    {
        connect(minBtn, &QToolButton::clicked,
                this, &UBWebToolsPalette::toggleMinimize,
                Qt::UniqueConnection);
    }

    adjustSizeAndPosition();
}


void UBWebToolsPalette::toggleMinimize()
{
    mMinimized = !mMinimized;

    // Update the minimize action's properties FIRST. QAction::changed fires on
    // setIcon/setToolTip and the base palette's actionChanged() slot reacts by
    // resyncing every button's visibility from its action — which would undo
    // the hide we're about to do. Doing it first means the resync runs while
    // all buttons are still visible (a no-op), and only then do we hide them.
    if (mMinimizeAction)
    {
        mMinimizeAction->setToolTip(mMinimized ? tr("Restore toolbar")
                                               : tr("Minimize toolbar"));
    }

    for (UBActionPaletteButton *btn : mCollapsibleButtons)
    {
        if (btn) btn->setVisible(!mMinimized);
    }

    if (layout()) layout()->activate();
    resize(sizeHint());
    adjustSizeAndPosition();
    update();
}


UBWebToolsPalette::~UBWebToolsPalette()
{
    // NOOP
}



