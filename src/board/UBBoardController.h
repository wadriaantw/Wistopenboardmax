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




#ifndef UBBOARDCONTROLLER_H_
#define UBBOARDCONTROLLER_H_

#include <QtGui>

#include <QObject>
#include <QHBoxLayout>
#include <QUndoCommand>
#include <QUndoStack>
#include <QPointer>

#include "core/UB.h"
#include "core/UBApplicationController.h"
#include "document/UBDocumentContainer.h"

class UBMainWindow;
class UBApplication;
class UBBoardView;

class UBDocument;
class UBDocumentController;
class UBMessageWindow;
class UBGraphicsScene;
class UBDocumentProxy;
class UBEmbedController;
class UBBlackoutWidget;
class UBToolWidget;
class UBVersion;
class UBSoftwareUpdate;
class UBSoftwareUpdateDialog;
class UBGraphicsMediaItem;
class UBGraphicsWidgetItem;
class UBBoardPaletteManager;
class QToolButton;
class UBItem;
class UBGraphicsItem;


class UBBoardController : public UBDocumentContainer
{
    Q_OBJECT

    public:
        enum SaveFlag {
            sf_none = 0x0,
            sf_showProgress = 0x1
        };
    Q_DECLARE_FLAGS(SaveFlags, SaveFlag)

    public:
        UBBoardController(UBMainWindow *mainWindow);
        virtual ~UBBoardController();

        void init();
        void setupLayout();

        std::shared_ptr<UBGraphicsScene> activeScene() const;
        int activeSceneIndex() const;

        // WistOpenboard fork: lets the board view re-announce the current page
        // after continuous scrolling promotes a new one, so the page readout and
        // the thumbnail sidebar follow along.
        void notifyPageSelectionChanged() { emit pageSelectionChanged(activeSceneIndex()); }

        // WistOpenboard fork: zen mode -- whether the top chrome (toolbar + tabs)
        // is currently tucked away. showBoard() consults this, because it
        // unconditionally re-shows the toolbar on every switch to Board mode.
        bool zenChromeHidden() const { return mZenChromeHidden; }
        void positionZenButtons(); // WistOpenboard fork
        static QString zenButtonCss(); // WistOpenboard fork
        void setActiveSceneIndex(int i);
        void closing();

        int currentPage() const;

        QWidget* controlContainer() const
        {
            return mControlContainer;
        }

        UBBoardView* controlView() const
        {
            return mControlView;
        }

        UBBoardView* displayView() const
        {
            return mDisplayView;
        }

        void setPenColorOnDarkBackground(const QColor& pColor)
        {
            if (mPenColorOnDarkBackground == pColor)
                return;

            mPenColorOnDarkBackground = pColor;
            emit penColorChanged();
        }

        void setPenColorOnLightBackground(const QColor& pColor)
        {
            if (mPenColorOnLightBackground == pColor)
                return;

            mPenColorOnLightBackground = pColor;
            emit penColorChanged();
        }

        void setMarkerColorOnDarkBackground(const QColor& pColor)
        {
            mMarkerColorOnDarkBackground = pColor;
        }

        void setMarkerColorOnLightBackground(const QColor& pColor)
        {
            mMarkerColorOnLightBackground = pColor;
        }

        QColor penColorOnDarkBackground() const
        {
            return mPenColorOnDarkBackground;
        }

        QColor penColorOnLightBackground() const
        {
            return mPenColorOnLightBackground;
        }

        QColor markerColorOnDarkBackground() const
        {
            return mMarkerColorOnDarkBackground;
        }

        QColor markerColorOnLightBackground() const
        {
            return mMarkerColorOnLightBackground;
        }

        qreal systemScaleFactor() const
        {
            return mSystemScaleFactor;
        }
        qreal currentZoom() const;
        void persistViewPositionOnCurrentScene() const;
        void restoreViewPositionOnCurrentScene() const;
        void persistCurrentScene(bool isAnAutomaticBackup = false, bool forceImmediateSave = false);
        void showNewVersionAvailable(bool automatic, const UBVersion &installedVersion, const UBSoftwareUpdate &softwareUpdate);
        void setBoxing(QRect displayRect);
        void setCursorFromAngle(qreal angle, const QPoint offset = {});
        void setToolbarTexts();
        bool eventFilter(QObject *obj, QEvent *event) override;
        static QUrl expandWidgetToTempDir(const QByteArray& pZipedData, const QString& pExtension = QString("wgt"));

        void setPageSize(QSize newSize);
        UBBoardPaletteManager *paletteManager() const
        {
            return mPaletteManager;
        }

        void notifyCache(bool visible);
        void notifyPageChanged();
        void displayMetaData(QMap<QString, QString> metadatas);

        void findUniquesItems(const QUndoCommand *parent, QSet<QGraphicsItem *> &items);
        void ClearUndoStack();
        std::shared_ptr<UBGraphicsScene> setActiveDocumentScene(std::shared_ptr<UBDocumentProxy> pDocumentProxy, int pSceneIndex = 0, bool forceReload = false, bool onImport = false);
        std::shared_ptr<UBGraphicsScene> setActiveDocumentScene(int pSceneIndex);

        void duplicateScene(int index);
        UBGraphicsItem *duplicateItem(UBItem *item);
        void deleteScene(int index);

        bool cacheIsVisible() const {return mCacheWidgetIsEnabled;}

        QString actionGroupText() const { return mActionGroupText;}
        QString actionUngroupText() const { return mActionUngroupText;}

        std::shared_ptr<UBGraphicsScene> initialDocumentScene() const
        {
            return mInitialDocumentScene;
        }

        // Close the tab for a document that is being deleted from the library
        // (no-op if the document is not currently open in a tab).
        void closeDocumentTabForProxy(std::shared_ptr<UBDocumentProxy> proxy);

        // When true, the next document opened goes into a NEW tab (set by the
        // "+" button). Otherwise a normal open replaces the active "main" tab.
        void setOpenNextInNewTab(bool b) { mOpenNextInNewTab = b; }

    public slots:
        void activateDocumentTab(int index);
        void closeDocumentTab(int index);
        void closeDocumentTabFromButton();
        void documentTabMoved(int from, int to);

        void showDocumentsDialog();
        void showKeyboard(bool show);
        void togglePodcast(bool checked);
        void blackout();
        void addScene();
        void addScene(std::shared_ptr<UBDocumentProxy> proxy, int sceneIndex, bool replaceActiveIfEmpty = false);
        void addScene(std::shared_ptr<UBGraphicsScene> scene, bool replaceActiveIfEmpty = false);
        void duplicateScene();
        void importPage();
        void clearScene();
        void clearSceneItems();
        void clearSceneAnnotation();
        void clearSceneBackground();
        void zoomIn(QPointF scenePoint = QPointF(0,0));
        void zoomOut(QPointF scenePoint = QPointF(0,0));
        void zoomRestore();
        void centerRestore();
        void centerOn(QPointF scenePoint = QPointF(0,0)) const;
        void zoom(const qreal ratio, QPointF scenePoint);
        void handScroll(qreal dx, qreal dy);
        void previousScene();
        void nextScene();
        void firstScene();
        void lastScene();
        void downloadURL(const QUrl& url, QString contentSourceUrl = QString(), const QPointF& pPos = QPointF(0.0, 0.0), const QSize& pSize = QSize(), bool isBackground = false, bool internalData = false, UBGraphicsScene* pTargetScene = nullptr);
        UBItem *downloadFinished(bool pSuccess, QUrl sourceUrl, QUrl contentUrl, QString pHeader,
                                 QByteArray pData, QPointF pPos, QSize pSize,
                                 bool isBackground = false, bool internalData = false, UBGraphicsScene* pTargetScene = nullptr);
        void changeBackground(bool isDark, UBPageBackground pageBackground);
        void setToolCursor(int tool);
        void showMessage(const QString& message, bool showSpinningWheel = false);
        void hideMessage();
        void setDisabled(bool disable);
        void setColorIndex(int pColorIndex);
        void removeTool(UBToolWidget* toolWidget);
        void hide();
        void show();
        void setWidePageSize(bool checked);
        void setRegularPageSize(bool checked);
        void stylusToolChanged(int tool);
        void grabScene(const QRectF& pSceneRect);
        UBGraphicsMediaItem* addVideo(const QUrl& pUrl, bool startPlay, const QPointF& pos, bool bUseSource = false);
        UBGraphicsMediaItem* addAudio(const QUrl& pUrl, bool startPlay, const QPointF& pos, bool bUseSource = false);
        UBGraphicsWidgetItem *addW3cWidget(const QUrl& pUrl, const QPointF& pos);
        void adjustDisplayViews();
        void cut();
        void copy();
        void paste();
        void processMimeData(const QMimeData* pMimeData, const QPointF& pPos, UBGraphicsScene* pTargetScene = nullptr);
        void moveGraphicsWidgetToControlView(UBGraphicsWidgetItem* graphicWidget);
        void moveToolWidgetToScene(UBToolWidget* toolWidget);
        void addItem();

        void freezeW3CWidgets(bool freeze);
        void freezeW3CWidget(QGraphicsItem* item, bool freeze);
        void startScript();
        void stopScript();

        void saveData(SaveFlags fls = sf_none);

        void documentSceneDuplicated(std::shared_ptr<UBDocumentProxy> proxy, int index);
        void documentSceneMoved(std::shared_ptr<UBDocumentProxy> proxy, int fromIndex, int toIndex);
        void documentSceneDeleted(std::shared_ptr<UBDocumentProxy> proxy, int index);

    signals:
        void newPageAdded();
        void activeSceneChanged();
        void zoomChanged(qreal pZoomFactor);
        void penColorChanged();
        void controlViewportChanged();
        void backgroundChanged();
        void cacheEnabled();
        void documentReorganized(int index);
        void displayMetadata(QMap<QString, QString> metadata);
        void pageSelectionChanged(int index);
        void npapiWidgetCreated(const QString &Url);

    protected:
        void setupViews();
        void setupToolbar();
        void connectToolbar();
        void initToolbarTexts();
        void updateActionStates();
        void updateSystemScaleFactor();
        QString truncate(QString text, int maxWidth) const;

    protected slots:
        void selectionChanged();
        void undoRedoStateChange(bool canUndo);
        void documentSceneChanged(std::shared_ptr<UBDocumentProxy> proxy, int pIndex);

    private slots:
        void autosaveTimeout();
        void appMainModeChanged(UBApplicationController::MainMode);

    private:
        void initBackgroundGridSize();
        void updatePageSizeState();
        int autosaveTimeoutFromSettings() const;

        // --- Multiple open documents (tabs) ---
        struct UBOpenDocument {
            std::shared_ptr<UBDocumentProxy> proxy;
            QPointer<QUndoStack> undoStack;
            int lastSceneIndex = 0;
        };
        int indexOfOpenDocument(std::shared_ptr<UBDocumentProxy> proxy) const;
        void registerOpenDocument(std::shared_ptr<UBDocumentProxy> proxy);
        void prepareTabForDocument(std::shared_ptr<UBDocumentProxy> proxy);

        // Session restore for the document tab bar.
        // Captured at startup BEFORE the resumed document registers its own
        // tab -- that registration saves the list, which would otherwise
        // overwrite the very set being restored.
        QStringList mPendingRestoreTabPaths;
        void saveOpenDocumentTabs();
        void restoreOpenDocumentTabs();
        void replaceActiveTabDocument(std::shared_ptr<UBDocumentProxy> proxy);
        QUndoStack* undoStackForProxy(std::shared_ptr<UBDocumentProxy> proxy) const;
        void rebindUndoStack(QUndoStack* newStack);
        QString documentTabTitle(std::shared_ptr<UBDocumentProxy> proxy) const;
        void syncCurrentTab();

        QList<UBOpenDocument> mOpenDocuments;
        QAction* mToolsMenuAction = nullptr;   // the Tools dropdown on the toolbar
        bool mTabSyncInProgress = false;
        bool mOpenNextInNewTab = false;   // "+" was pressed → next open is a new tab
        bool mReplacingActiveTab = false; // current open is replacing the active tab's doc

        UBMainWindow *mMainWindow;
        std::shared_ptr<UBGraphicsScene> mActiveScene;
        int mActiveSceneIndex;
        int mSwitchToSceneIndex{-1};
        UBBoardPaletteManager *mPaletteManager;
        UBSoftwareUpdateDialog *mSoftwareUpdateDialog;
        UBMessageWindow *mMessageWindow;
        UBEmbedController *mEmbedController;
        UBBoardView *mControlView;
        UBBoardView *mDisplayView;
        QWidget *mControlContainer;
        QHBoxLayout *mControlLayout;
        qreal mZoomFactor;
        bool mIsClosing;
        QColor mPenColorOnDarkBackground;
        QColor mPenColorOnLightBackground;
        QColor mMarkerColorOnDarkBackground;
        QColor mMarkerColorOnLightBackground;
        qreal mSystemScaleFactor;
        mutable bool mDocumentJustOpened = false;
        mutable bool mResetViewToTopOnSceneChange = false;
        bool mZenChromeHidden = false; // WistOpenboard fork
        QToolButton* mZenChromeButton = nullptr; // WistOpenboard fork
        QToolButton* mZenPrevButton = nullptr;   // WistOpenboard fork
        QToolButton* mZenNextButton = nullptr;   // WistOpenboard fork
        QToolButton* mZenPageChip = nullptr;     // WistOpenboard fork
        QToolButton* mZenUndoButton = nullptr;   // WistOpenboard fork
        QToolButton* mZenRedoButton = nullptr;   // WistOpenboard fork
        bool mInInit = false;
        bool mInitialIsFreshlyCreated = true;
        bool mCleanupDone;
        QMap<QAction*, QPair<QString, QString> > mActionTexts;
        bool mCacheWidgetIsEnabled;
        QGraphicsItem* mLastCreatedItem;
        int mDeletingSceneIndex;
        int mMovingSceneIndex;
        QString mActionGroupText;
        QString mActionUngroupText;
        std::shared_ptr<UBGraphicsScene> mInitialDocumentScene;
        QList<std::shared_ptr<UBDocument>> mRecentDocuments;

        QTimer *mAutosaveTimer;

    private slots:
        void stylusToolDoubleClicked(int tool);
        void boardViewResized(QResizeEvent* event);
        void updateBackgroundActionsState(bool isDark, UBPageBackground pageBackground);
        void colorPaletteChanged();
        void libraryDialogClosed(int ret);
        void lastWindowClosed();
        void onDownloadModalFinished();

};


#endif /* UBBOARDCONTROLLER_H_ */
