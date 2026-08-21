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
#include <QToolTip>
#include <QStackedLayout>
#include <QStyle>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QHBoxLayout>

#include "UBMainWindow.h"
#include "core/UBApplication.h"
#include "core/UBTheme.h"
#include "core/UBApplicationController.h"
#include "board/UBBoardController.h"
#include "core/UBDisplayManager.h"
#include "core/UBShortcutManager.h"
#include "frameworks/UBPlatformUtils.h"

// work around for handling tablet events on MAC OS with Qt 4.8.0 and above
#if defined(Q_OS_OSX)
#include "board/UBBoardView.h"
#endif

#include "core/memcheck.h"

UBMainWindow::UBMainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
    , mBoardWidget(0)
    , mWebWidget(0)
    , mDocumentsWidget(0)
    , mpDownloadWidget(NULL)
{
    Ui::MainWindow::setupUi(this);

    mpDownloadWidget = new UBDownloadWidget();
    mpDownloadWidget->setWindowModality(Qt::ApplicationModal);

    //Setting tooltip colors staticly, since they look not quite well on different color themes
    QPalette toolTipPalette;
    toolTipPalette.setColor(QPalette::ToolTipBase, QColor("#FFFFDC"));
    toolTipPalette.setColor(QPalette::ToolTipText, Qt::black);
    QToolTip::setPalette(toolTipPalette);

    QWidget* centralWidget = new QWidget(this);
    mStackedLayout = new QStackedLayout(centralWidget);
    setCentralWidget(centralWidget);

#ifdef Q_OS_OSX
    // MacBooks with a camera notch hide the top toolbar behind the notch.
    // Insert a fixed-height spacer above all toolbars (via setMenuWidget,
    // which QMainWindow lays out *above* the toolbar area) so the toolbar
    // is fully visible. 38 px clears the notch on every current MacBook
    // model; non-notch Macs just see a small matching strip at the top.
    {
        QWidget* notchSpacer = new QWidget(this);
        notchSpacer->setFixedHeight(38);
        notchSpacer->setAttribute(Qt::WA_StyledBackground, true);
        notchSpacer->setStyleSheet(QString("background-color: %1;").arg(UBTheme::hex(UBTheme::surface()))); // WistOpenboard fork
        notchSpacer->setObjectName("macNotchSpacer");
        setMenuWidget(notchSpacer);
    }

    actionPreferences->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    actionQuit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
#elif defined(Q_OS_WIN)
    actionPreferences->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
    // this code, because it unusable, system key combination can`t be triggered, even we add it manually
    actionQuit->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));
#else
    actionQuit->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));
#endif

    UBShortcutManager::shortcutManager()->addMainActions(this);

    // Wire the in-toolbar Minimize button (frameless main window has no OS title bar).
    // On macOS, QWidget::showMinimized() is a no-op on a frameless window, so route
    // through UBPlatformUtils which calls -miniaturize: on the NSWindow directly.
    actionMinimize->setIcon(style()->standardIcon(QStyle::SP_TitleBarMinButton));
    connect(actionMinimize, &QAction::triggered, this, [this]() {
        UBPlatformUtils::minimizeMainWindow(this);
    });

    // --- Document tabs bar -------------------------------------------------
    // A black bar above the grey board toolbar that lists every open document
    // as a tab (multiple documents open at once). The tab bar itself is
    // populated/driven by UBBoardController; here we only build and place it.
    const int kTabBarHeight = 30;

    mDocumentTabBar = new QTabBar(this);
    mDocumentTabBar->setObjectName("documentTabBar");
    mDocumentTabBar->setTabsClosable(false); // we install our own centered close button per tab
    mDocumentTabBar->setMovable(true);
    mDocumentTabBar->setExpanding(false);          // tabs keep natural width, left-aligned
    mDocumentTabBar->setDrawBase(false);
    // No scroll buttons: with them enabled the bar's minimum width collapses
    // inside the toolbar, so it starves the tabs of room and clips/scrolls even
    // a single tab. Disabling makes the bar claim its full natural width.
    mDocumentTabBar->setUsesScrollButtons(false);
    mDocumentTabBar->setElideMode(Qt::ElideRight);
    mDocumentTabBar->setFocusPolicy(Qt::NoFocus);
    // Do NOT fix the bar height: let it size to its tabs, and let the (fixed-
    // height) toolbar vertically center it. The styles below also override the
    // global OpenBoard.css QTabBar rules (height:14px, margin-top:6px, border)
    // that were skewing the vertical alignment.
    // WistOpenboard fork: Fluent-light tab strip (was the #1d1d1f dark band).
    mDocumentTabBar->setStyleSheet(QString(
        "QTabBar { background:%1; }"
        "QTabBar::tab { background:%2; color:%3; border:none; height:22px;"
        " padding:3px 6px 3px 12px; margin:0 2px 0 0; min-width:90px; max-width:240px;"
        " border-top-left-radius:6px; border-top-right-radius:6px; }"
        "QTabBar::tab:selected { background:%4; color:%5; }"
        "QTabBar::tab:hover { background:%4; }")
        .arg(UBTheme::hex(UBTheme::surface()), UBTheme::hex(UBTheme::surfaceMuted()),
             UBTheme::hex(UBTheme::inkMuted()), UBTheme::hex(UBTheme::surfacePressed()),
             UBTheme::hex(UBTheme::ink())));

    // "+" button: opens the Documents library so the user picks another doc.
    QToolButton* addTabButton = new QToolButton(this);
    addTabButton->setText(QStringLiteral("+"));
    addTabButton->setToolTip(tr("Open another document"));
    addTabButton->setAutoRaise(true);
    addTabButton->setFocusPolicy(Qt::NoFocus);
    addTabButton->setFixedHeight(kTabBarHeight);
    addTabButton->setStyleSheet(QString(
        "QToolButton { color:%1; font-size:16px; font-weight:bold; padding:0 10px; border:0; }"
        "QToolButton:hover { color:%2; }")
        .arg(UBTheme::hex(UBTheme::inkMuted()), UBTheme::hex(UBTheme::ink())));
    connect(addTabButton, &QToolButton::clicked, this, [this]() {
        // "+" means: the next document I open should go into a NEW tab.
        if (UBApplication::boardController)
            UBApplication::boardController->setOpenNextInNewTab(true);
        if (actionDocument)
            actionDocument->trigger();
    });

    mDocumentTabsToolBar = new QToolBar(tr("Open Documents"), this);
    mDocumentTabsToolBar->setObjectName("documentTabsToolBar");
    mDocumentTabsToolBar->setMovable(false);
    mDocumentTabsToolBar->setFloatable(false);
    mDocumentTabsToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    mDocumentTabsToolBar->setIconSize(QSize(16, 16));   // keep the toolbar row short
    mDocumentTabsToolBar->setStyleSheet(QString("QToolBar { background:%1; border:0; border-bottom:1px solid %2; padding:0; margin:0; spacing:0; }")
        .arg(UBTheme::hex(UBTheme::surface()), UBTheme::hex(UBTheme::hairline())));
    // Tab bar and "+" go directly into the toolbar as separate items. With
    // scroll buttons disabled the bar's size hint equals the full width of all
    // its tabs, so the toolbar gives it that width (every tab shows, left-
    // anchored) and the "+" button sits immediately after the last tab.
    mDocumentTabsToolBar->addWidget(mDocumentTabBar);
    mDocumentTabsToolBar->addWidget(addTabButton);
    if (mDocumentTabsToolBar->layout())
    {
        mDocumentTabsToolBar->layout()->setContentsMargins(0, 0, 0, 0);
        mDocumentTabsToolBar->layout()->setSpacing(0);
    }
    mDocumentTabsToolBar->setFixedHeight(kTabBarHeight); // single compact line, no black bands
    mDocumentTabsToolBar->hide(); // shown only in Board mode (see UBApplicationController)

    // NOTE: actual placement in the toolbar area happens in
    // placeDocumentTabsToolBar(), called from UBApplication::toolBarPositionChanged()
    // AFTER the three mode toolbars are (re)added — otherwise that re-add scrambles
    // our position and the bar ends up orphaned/invisible.
}

void UBMainWindow::placeDocumentTabsToolBar(Qt::ToolBarArea area)
{
    if (!mDocumentTabsToolBar)
        return;

    const bool wasVisible = mDocumentTabsToolBar->isVisible();

    // Detach (if already placed) then put it on its own row directly above the
    // board toolbar. On macOS this row sits just under the notch spacer
    // (setMenuWidget lays out above all toolbars), giving the separate top bar
    // that platform needs.
    removeToolBar(mDocumentTabsToolBar);
    addToolBar(area, mDocumentTabsToolBar);
    insertToolBar(boardToolBar, mDocumentTabsToolBar);
    insertToolBarBreak(boardToolBar);

    mDocumentTabsToolBar->setVisible(wasVisible);
}

void UBMainWindow::refreshDocumentTabsLayout()
{
    if (mDocumentTabBar)
        mDocumentTabBar->updateGeometry();
    if (mDocumentTabsToolBar && mDocumentTabsToolBar->layout())
    {
        mDocumentTabsToolBar->layout()->invalidate();
        mDocumentTabsToolBar->layout()->activate();
    }
}

void UBMainWindow::setDocumentTabsVisible(bool visible)
{
    if (mDocumentTabsToolBar)
        mDocumentTabsToolBar->setVisible(visible);
}

UBMainWindow::~UBMainWindow()
{
    if(NULL != mpDownloadWidget)
    {
        delete mpDownloadWidget;
        mpDownloadWidget = NULL;
    }
}

void UBMainWindow::addBoardWidget(QWidget *pWidget)
{
    if (!mBoardWidget)
    {
        mBoardWidget = pWidget;
        mStackedLayout->addWidget(mBoardWidget);
    }
}

void UBMainWindow::switchToBoardWidget()
{
    if (mBoardWidget)
    {
        mStackedLayout->setCurrentWidget(mBoardWidget);
    }
}

void UBMainWindow::addWebWidget(QWidget *pWidget)
{
    if (!mWebWidget)
    {
        mWebWidget = pWidget;
        mStackedLayout->addWidget(mWebWidget);
    }
}

void UBMainWindow::switchToWebWidget()
{
    qDebug() << "popped out from StackedLayout size height: " << mWebWidget->height() << " width: " << mWebWidget->width();
    if (mWebWidget)
    {
        mStackedLayout->setCurrentWidget(mWebWidget);
    }
}


void UBMainWindow::addDocumentsWidget(QWidget *pWidget)
{
    if (!mDocumentsWidget)
    {
        mDocumentsWidget = pWidget;
        mStackedLayout->addWidget(mDocumentsWidget);
    }
}

void UBMainWindow::switchToDocumentsWidget()
{
    if (mDocumentsWidget)
    {
        mStackedLayout->setCurrentWidget(mDocumentsWidget);
    }
}

void UBMainWindow::keyPressEvent(QKeyEvent *event)
{
    QMainWindow::keyPressEvent(event);
}

void UBMainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    emit closeEvent_Signal(event);
}

// work around for handling tablet events on MAC OS with Qt 4.8.0 and above
#if defined(Q_OS_OSX)
bool UBMainWindow::event(QEvent *event)
{
    bool bRes = QMainWindow::event(event);

    if (NULL != UBApplication::boardController)
    {
        UBBoardView *controlV = UBApplication::boardController->controlView();
        if (controlV && controlV->isVisible())
        {
            switch (event->type())
            {
            case QEvent::TabletEnterProximity:
            case QEvent::TabletLeaveProximity:
            case QEvent::TabletMove:
            case QEvent::TabletPress:
            case QEvent::TabletRelease:
                {
                    return controlV->directTabletEvent(event);
                }
            }
        }
    }
    return bRes;
}
#endif

void UBMainWindow::onExportDone()
{
    // HACK :  When opening the file save dialog during the document exportation,
    //         some buttons of the toolbar become disabled without any reason. We
    //         re-enable them here.
    actionExport->setEnabled(true);
    actionNewDocument->setEnabled(true);
    actionRename->setEnabled(true);
    actionDuplicate->setEnabled(true);
    actionDelete->setEnabled(true);
    actionOpen->setEnabled(true);
    actionDocumentAdd->setEnabled(true);
}

bool UBMainWindow::yesNoQuestion(QString windowTitle, QString text, const QPixmap &iconPixmap, const QMessageBox::Icon icon)
{
    QMessageBox messageBox;
    messageBox.setParent(this);
    messageBox.setWindowTitle(windowTitle);
    messageBox.setText(text);
    QPushButton* yesButton = messageBox.addButton(tr("Yes"),QMessageBox::YesRole);
    messageBox.addButton(tr("No"),QMessageBox::NoRole);
    if (iconPixmap.isNull())
        messageBox.setIcon(icon);
    else
        messageBox.setIconPixmap(iconPixmap);


#ifdef Q_OS_LINUX
    // to avoid to be handled by x11. This allows us to keep to the back all the windows manager stuff like palette, toolbar ...
    messageBox.setWindowFlags(Qt::Dialog | Qt::X11BypassWindowManagerHint);
#else
    messageBox.setWindowFlags(Qt::Dialog);
#endif

    messageBox.exec();
    return messageBox.clickedButton() == yesButton;
}

void UBMainWindow::oneButtonMessageBox(QString windowTitle, QString text, QMessageBox::Icon type)
{
    QMessageBox messageBox;
    messageBox.setParent(this);
    messageBox.setWindowFlags(Qt::Dialog);
    messageBox.setWindowTitle(windowTitle);
    messageBox.setText(text);
    messageBox.addButton(tr("Ok"),QMessageBox::YesRole);
    messageBox.setIcon(type);
    messageBox.exec();
}

void UBMainWindow::warning(QString windowTitle, QString text)
{
    oneButtonMessageBox(windowTitle,text, QMessageBox::Warning);
}

void UBMainWindow::information(QString windowTitle, QString text)
{
    oneButtonMessageBox(windowTitle, text, QMessageBox::Information);
}

void UBMainWindow::showDownloadWidget()
{
    if(NULL != mpDownloadWidget)
    {
        mpDownloadWidget->show();
    }
}

void UBMainWindow::hideDownloadWidget()
{
    if(NULL != mpDownloadWidget)
    {
        mpDownloadWidget->hide();
    }
}
