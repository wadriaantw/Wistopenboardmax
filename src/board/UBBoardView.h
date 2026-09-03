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



#ifndef UBBOARDVIEW_H_
#define UBBOARDVIEW_H_

#define CONTROLVIEW_OBJ_NAME "ControlView"

#include <QtGui>
#include <QGraphicsView>
#include <QRubberBand>

#include <QHash>
#include <QSet>
#include <QPixmap>
#include "core/UB.h"
#include "domain/UBGraphicsDelegateFrame.h"

class UBBoardController;
class UBGraphicsScene;
class UBGraphicsWidgetItem;
class UBRubberBand;
class UBSnapIndicator;

class UBBoardView : public QGraphicsView
{
    Q_OBJECT

public:

    UBBoardView(UBBoardController* pController, QWidget* pParent = 0, bool isControl = false, bool isDesktop = false);
    UBBoardView(UBBoardController* pController, int pStartLayer, int pEndLayer, QWidget* pParent = 0, bool isControl = false, bool isDesktop = false);
    virtual ~UBBoardView();

    std::shared_ptr<UBGraphicsScene> scene();

    void forcedTabletRelease();

    void setToolCursor(int tool);

    void rubberItems();
    void moveRubberedItems(QPointF movingVector);

    void setMultiselection(bool enable);

    // WistOpenboard fork: continuous vertical scrolling across all pages of the
    // document, instead of showing a single page at a time. Only the page nearest
    // the viewport centre is a live scene; neighbours are painted from cached
    // pixmaps in drawBackground().
    void setContinuousScroll(bool enabled);
    bool isContinuousScroll() const { return mContinuousScroll; }

    // WistOpenboard fork: page turn for continuous mode. Promotes the target
    // page the same way scrolling does, then puts its top at the top of the
    // viewport. Returns false when not in continuous mode (caller falls back
    // to the single-page path).
    bool goToPage(int target);
    // Recompute the scrollable strip after the active page changes by any other
    // route (page buttons, thumbnail click, document switch).
    void refreshContinuousLayout();
    bool isMultipleSelectionEnabled() { return mMultipleSelectionIsEnabled; }

    void setBoxing(const QMargins& margins);
    void updateSnapIndicator(Qt::Corner corner, QPointF snapPoint, double angle = 0);

    // work around for handling tablet events on MAC OS with Qt 4.8.0 and above
#if defined(Q_OS_OSX)
    bool directTabletEvent(QEvent *event);
    QWidget *widgetForTabletEvent(QWidget *w, const QPoint &pos);
#endif
signals:
    void resized(QResizeEvent* event);
    void shown();
    void mouseReleased();
    void painted(const QRectF region);

protected:

    bool itemIsLocked(QGraphicsItem *item);
    bool isUBItem(QGraphicsItem *item); // we should to determine items who is not UB and use general scene behavior for them.
    bool isCppTool(QGraphicsItem *item);
    void handleItemsSelection(QGraphicsItem *item);
    bool itemShouldReceiveMousePressEvent(QGraphicsItem *item);
    bool itemShouldReceiveSuspendedMousePressEvent(QGraphicsItem *item);
    bool itemHaveParentWithType(QGraphicsItem *item, int type);
    bool itemShouldBeMoved(QGraphicsItem *item);
    QGraphicsItem* determineItemToPress(QGraphicsItem *item);
    QGraphicsItem* determineItemToMove(QGraphicsItem *item);
    void handleItemMousePress(QMouseEvent *event);
    void handleItemMouseMove(QMouseEvent *event);

    virtual bool event (QEvent * e);

    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);
    virtual bool viewportEvent(QEvent *event);
    virtual void tabletEvent(QTabletEvent * event);
    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
    virtual void leaveEvent ( QEvent * event);

    virtual void focusOutEvent ( QFocusEvent * event );

    virtual void drawItems(QPainter *painter, int numItems,
                           QGraphicsItem *items[],
                           const QStyleOptionGraphicsItem options[]);

    virtual void dropEvent(QDropEvent *event);
    virtual void dragMoveEvent(QDragMoveEvent *event);
    virtual void dragEnterEvent(QDragEnterEvent *event); // WistOpenboard fork: needed for Desktop overlay drops

    virtual void resizeEvent(QResizeEvent * event);

    virtual void paintEvent(QPaintEvent *event);

    virtual void drawBackground(QPainter *painter, const QRectF &rect);
    virtual void drawForeground(QPainter *painter, const QRectF &rect);

    virtual void scrollContentsBy(int dx, int dy);

private:

    void init();

    inline bool shouldDisplayItem(QGraphicsItem *item)
    {
        bool ok;
        int itemLayerType = item->data(UBGraphicsItemData::ItemLayerType).toInt(&ok);
        return (ok && (itemLayerType >= mStartLayer && itemLayerType <= mEndLayer));
    }

    QList<QUrl> processMimeData(const QMimeData* pMimeData);

    UBBoardController* mController;

    int mStartLayer, mEndLayer;
    bool mFilterZIndex;

    bool mTabletStylusIsPressed;
    bool mUsingTabletEraser;

    bool mPendingStylusReleaseEvent;

    bool mMouseButtonIsPressed;
    QPointF mPreviousPoint;
    QPoint mMouseDownPos;

    bool mPenPressureSensitive;
    bool mMarkerPressureSensitive;
    bool mUseHighResTabletEvent;

    QRubberBand *mRubberBand;
    bool mIsCreatingTextZone;
    bool mIsCreatingSceneGrabZone;

    // WistOpenboard fork: Lasso (freeform polygon) selection state
    bool mLassoActive;
    QPainterPath mLassoPath;
    class QGraphicsPathItem *mLassoPathItem;
    void startLasso(const QPointF& scenePos);
    void extendLasso(const QPointF& scenePos);
    void finishLasso();
    void cancelLasso();

    // WistOpenboard fork: keep Lasso "selection" as an internal item set
    // (not Qt-selection, so no delegate frames). Click+drag on any of these
    // items in Lasso mode translates the whole set; click on empty space
    // clears the set and starts a new lasso.
    QList<QGraphicsItem*> mLassoSelection;
    bool mLassoDragging{false};
    QPointF mLassoDragLastScene;

    // WistOpenboard fork: the fork own interactive items (editable shapes,
    // curves) carry handles that sit OUTSIDE their body -- a delete chip, a
    // rotation dot, corner grips. Those land over page content, and a page
    // item (e.g. UBGraphicsPDFItem) wins the scene hit test there, so the
    // press was routed to the page and the handles were dead. When a press
    // lands on one of these items we dispatch press/move/release to it
    // directly and remember it here for the rest of the gesture.
    QGraphicsItem* mForkItemTarget{nullptr};
    // Gesture state carried between synthetic events: an item computes its
    // drag delta from lastScenePos(), which a hand-built event does not fill.
    QPointF mForkLastScenePos;
    QPoint  mForkLastScreenPos;
    QPointF mForkPressScenePos;
    QPoint  mForkPressScreenPos;
    QGraphicsItem* forkInteractiveItemAt(const QPointF& scenePos);
    QGraphicsItem* touchDirectTargetAt(const QPointF& scenePos);
    bool dispatchToForkItem(QEvent::Type type, const QPointF& scenePos, const QPoint& screenPos);

    bool isAbsurdPoint(QPoint point);

    bool mVirtualKeyboardActive;
    bool mOkOnWidget;

    bool mWidgetMoved;
    QPointF mFirstPressedMousePos;
    QPointF mLastPressedMousePos;
    QList<QPointF> mCornerPoints;

    /* when an item is moved around, the tracking must stop if the object is deleted */
    QGraphicsItem *_movingItem;

    QGraphicsItem *getMovingItem() {
        return _movingItem;
    }

    void setMovingItem(QGraphicsItem *item) {
        // if the current moving item is a qobject, it MUST be disconnected
        if (_movingItem) {
            QObject *moving_item_obj = dynamic_cast<QObject*>(_movingItem);
            if (moving_item_obj != nullptr)
                disconnect(moving_item_obj, &QObject::destroyed, this, &UBBoardView::movingItemDestroyed);
        }

        // attach the new moving item if relevant
        if (item) {
            QObject *item_obj = dynamic_cast<QObject*>(item);
            if (item_obj != nullptr)
                connect(item_obj, &QObject::destroyed, this, &UBBoardView::movingItemDestroyed);
        }
        _movingItem = item;
    }

    QMouseEvent *suspendedMousePressEvent;

    bool moveRubberBand;
    UBRubberBand *mUBRubberBand;

    QList<QGraphicsItem *> mRubberedItems;
    QSet<QGraphicsItem*> mJustSelectedItems;

    int mLongPressInterval;
    QTimer mLongPressTimer;

    bool mIsDragInProgress;
    bool mMultipleSelectionIsEnabled;
    bool mContinuousScroll; // WistOpenboard fork
    // WistOpenboard fork: continuous-scroll support. Neighbouring pages are
    // cached as pixmaps keyed by page index and blitted in drawBackground();
    // only the page nearest the viewport centre is ever a live scene.
    QHash<int, QPixmap> mStripPixmaps;
    QSet<int> mPendingStripPixmaps;
    bool mSwappingScene = false;

    qreal continuousStride() const;
    void  updateContinuousSceneRect();
    int   pageIndexAtSceneY(qreal y) const;
    void  ensureStripPixmap(int index);
    void  promoteToPage(int target);
    void  maybeSwapActivePage();
    void  drawContinuousNeighbours(QPainter* painter, const QRectF& rect);

    // finger-pan / pinch-zoom state
    bool mIsTouchPanning;

    // WistOpenboard fork: kinetic scrolling. The pan follows the finger 1:1;
    // on release, if the finger was still moving, the page keeps going and
    // coasts to a stop. Velocity is in viewport pixels per millisecond.
    QElapsedTimer mPanClock;
    QPointF mPanVelocity;
    QTimer* mFlingTimer = nullptr;
    QElapsedTimer mFlingClock;       // real time between coast ticks
    // Recent samples (viewport pos, ms since pan clock start): the release
    // velocity is taken over the last ~100 ms, not from the final event,
    // which is often a slow one as the finger lifts.
    QList<QPair<QPointF, qint64>> mPanSamples;
    void startFling();
    void stopFling();

public:
    void stopKineticScroll() { stopFling(); }

private:
    QPointF mTouchPanStart;
    int mTouchPanId;                 // finger id being tracked for pan (-1 = none)
    QPointF mTouchPanAnchor;         // fixed start anchor for movement-deadzone check
    bool mTouchPanArmed;             // contact passed size filter; awaiting deadzone
    qreal mTouchPinchStartDist;
    QPointF mTouchPinchScenePivot;
    int mTouchPinchId1;              // finger ids locked at pinch start (-1 = none)
    int mTouchPinchId2;
    bool mTouchOverWidget;           // current touch sequence is on an interactive widget

    // Space-hold panning: hold Space to drag the board around in any tool.
    // If Space is pressed and released with no mouse drag in between, fall
    // back to the legacy behaviour (Space → next scene).
    bool mSpaceHeld{false};
    bool mSpaceUsedForPan{false};
    QPoint mSpacePanLastViewPos;

    // macOS smartboard workaround: hold finger still for ~300 ms in a drawing
    // tool to enter pan mode (single-finger pan), since external touchscreens
    // on macOS don't expose multi-touch. Quick tap-and-drag still draws.
    class QTimer *mLongPressPanTimer{nullptr};
    bool mLongPressArmed{false};        // press deferred, waiting on timer or first move
    bool mLongPressPanning{false};      // timer fired, panning in progress
    bool mLongPressDispatching{false};  // re-entrancy guard while replaying the press
    QPoint mLongPressStartViewPos;
    QPoint mLongPressLastViewPos;
    QPointF mLongPressLocalPos;
    QPointF mLongPressScreenPos;
    Qt::MouseButton mLongPressButton{Qt::NoButton};
    Qt::MouseButtons mLongPressButtons{Qt::NoButton};
    Qt::KeyboardModifiers mLongPressModifiers{Qt::NoModifier};
    Qt::MouseEventSource mLongPressSource{Qt::MouseEventNotSynthesized};

private slots:
    void handleLongPressTimeout();

private:
    void dispatchDeferredPress();

    // floating page navigation bar (top-center pill: ◄ Page X / Y ►)
    class QWidget *mPageNavBar{nullptr};
    class QLabel *mPageLabel{nullptr};
    class QToolButton *mPrevPageButton{nullptr};
    class QToolButton *mNextPageButton{nullptr};
    void positionPageButtons();
    void updatePageLabel();
    bool bIsControl;
    bool bIsDesktop;
    bool mRubberBandInPlayMode;

    QMargins mMargins{};
    UBSnapIndicator* mSnapIndicator{nullptr};

    static bool hasSelectedParents(QGraphicsItem * item);

private slots:
    void settingChanged(QVariant newValue);
    void movingItemDestroyed(QObject* item = nullptr);

public slots:
    void virtualKeyboardActivated(bool b);
    void longPressEvent();

};

#endif /* UBBOARDVIEW_H_ */
