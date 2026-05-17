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




#include "UBBoardController.h"

#include <QtWidgets>
#include <functional>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "adaptors/UBMetadataDcSubsetAdaptor.h"
#include "adaptors/UBSvgSubsetAdaptor.h"

#include "board/UBBoardPaletteManager.h"
#include "board/UBBoardView.h"
#include "board/UBDrawingController.h"
#include "board/UBFeaturesController.h"

#include "frameworks/UBFileSystemUtils.h"
#include "frameworks/UBPlatformUtils.h"

#include "core/UBApplication.h"
#include "core/UBApplicationController.h"
#include "web/UBWebController.h"
#include "core/UBDisplayManager.h"
#include "core/UBDocumentManager.h"
#include "core/UBDownloadManager.h"
#include "core/UBMimeData.h"
#include "core/UBPersistenceManager.h"
#include "core/UBSetting.h"
#include "core/UBSettings.h"
#include "core/UBSettings.h"

#include "document/UBDocument.h"
#include "document/UBDocumentController.h"
#include "document/UBDocumentProxy.h"

#include "domain/UBGraphicsGroupContainerItem.h"
#include "domain/UBGraphicsItemUndoCommand.h"
#include "domain/UBGraphicsMediaItem.h"
#include "domain/UBGraphicsPDFItem.h"
#include "domain/UBGraphicsPixmapItem.h"
#include "domain/UBGraphicsSvgItem.h"
#include "domain/UBGraphicsTextItem.h"
#include "domain/UBGraphicsWidgetItem.h"
#include "domain/UBEditableShapeItem.h"
#include "domain/UBItem.h"
#include "domain/UBPageSizeUndoCommand.h"

#include "gui/UBFeaturesWidget.h"
#include "gui/UBKeyboardPalette.h"
#include "gui/UBMagnifer.h"
#include "gui/UBMainWindow.h"
#include "gui/UBMessageWindow.h"
#include "gui/UBThumbnailScene.h"
#include "gui/UBToolWidget.h"
#include "gui/UBToolbarButtonGroup.h"

#include "podcast/UBPodcastController.h"

#include "tools/UBToolsManager.h"

#include "web/UBEmbedController.h"
#include "web/UBEmbedParser.h"

#include "core/memcheck.h"

UBBoardController::UBBoardController(UBMainWindow* mainWindow)
    : UBDocumentContainer(mainWindow->centralWidget())
    , mMainWindow(mainWindow)
    , mActiveScene(0)
    , mActiveSceneIndex(-1)
    , mPaletteManager(0)
    , mSoftwareUpdateDialog(0)
    , mMessageWindow(0)
    , mEmbedController(nullptr)
    , mControlView(0)
    , mDisplayView(0)
    , mControlContainer(0)
    , mControlLayout(0)
    , mZoomFactor(1.0)
    , mIsClosing(false)
    , mSystemScaleFactor(1.0)
    , mCleanupDone(false)
    , mCacheWidgetIsEnabled(false)
    , mDeletingSceneIndex(-1)
    , mMovingSceneIndex(-1)
    , mActionGroupText(tr("Group"))
    , mActionUngroupText(tr("Ungroup"))
    , mAutosaveTimer(0)
{
    mZoomFactor = UBSettings::settings()->boardZoomFactor->get().toDouble();

    int penColorIndex = UBSettings::settings()->penColorIndex();
    int markerColorIndex = UBSettings::settings()->markerColorIndex();

    mPenColorOnDarkBackground = UBSettings::settings()->penColors(true).at(penColorIndex);
    mPenColorOnLightBackground = UBSettings::settings()->penColors(false).at(penColorIndex);
    mMarkerColorOnDarkBackground = UBSettings::settings()->markerColors(true).at(markerColorIndex);
    mMarkerColorOnLightBackground = UBSettings::settings()->markerColors(false).at(markerColorIndex);
}


void UBBoardController::init()
{
    setupViews();
    setupToolbar();

    // Adapt toolbar (icon size + label visibility) on window resize so it
    // stays usable on small laptop screens.
    if (mMainWindow)
        mMainWindow->installEventFilter(this);

    connect(UBApplication::undoStack, SIGNAL(canUndoChanged(bool))
            , this, SLOT(undoRedoStateChange(bool)));

    connect(UBApplication::undoStack, SIGNAL(canRedoChanged(bool))
            , this, SLOT(undoRedoStateChange(bool)));

    connect(UBDrawingController::drawingController(), SIGNAL(stylusToolChanged(int))
            , this, SLOT(setToolCursor(int)));

    connect(UBDrawingController::drawingController(), SIGNAL(stylusToolChanged(int))
            , this, SLOT(stylusToolChanged(int)));

    connect(UBApplication::app(), SIGNAL(lastWindowClosed())
            , this, SLOT(lastWindowClosed()));

    connect(UBDownloadManager::downloadManager(), SIGNAL(downloadModalFinished()), this, SLOT(onDownloadModalFinished()));
    connect(UBDownloadManager::downloadManager(), SIGNAL(addDownloadedFileToBoard(bool,QUrl,QUrl,QString,QByteArray,QPointF,QSize,bool)), this, SLOT(downloadFinished(bool,QUrl,QUrl,QString,QByteArray,QPointF,QSize,bool)));

    auto persistenceManager{UBPersistenceManager::persistenceManager()};
    connect(persistenceManager, &UBPersistenceManager::documentSceneDuplicated, this, &UBBoardController::documentSceneDuplicated);
    connect(persistenceManager, &UBPersistenceManager::documentSceneMoved, this, &UBBoardController::documentSceneMoved);
    connect(persistenceManager, &UBPersistenceManager::documentSceneDeleted, this, &UBBoardController::documentSceneDeleted);

    // Create a placeholder blank doc synchronously (required for the rest of
    // UBApplication init to find a valid active scene). After the event loop
    // starts we try to swap to the user's last-opened doc (or the most-recently-
    // modified existing one) and delete the placeholder so blanks don't pile up.
    mInInit = true;
    std::shared_ptr<UBDocumentProxy> blankDoc = UBPersistenceManager::persistenceManager()->createNewDocument();
    if (blankDoc)
        mInitialDocumentScene = setActiveDocumentScene(blankDoc);
    mInitialIsFreshlyCreated = true;
    mInInit = false;

    // Defer slightly so the document tree is fully populated and signal slots
    // are wired up before we try to swap.
    QTimer::singleShot(150, this, [this, blankDoc]() {
        const QString rawLast = UBSettings::settings()->lastOpenedDocumentPath->get().toString();
        const QString lastNorm = QDir::cleanPath(rawLast);

        std::shared_ptr<UBDocumentProxy> bySaved;
        std::shared_ptr<UBDocumentProxy> mostRecent;
        QDateTime mostRecentTime;
        int totalDocs = 0;

        std::function<void(UBDocumentTreeNode*)> walk = [&](UBDocumentTreeNode* n) {
            if (!n) return;
            if (n->nodeType() == UBDocumentTreeNode::Document) {
                auto p = n->proxyData();
                if (p && !p->isBroken() && p != blankDoc) {
                    totalDocs++;
                    if (!lastNorm.isEmpty() && QDir::cleanPath(p->persistencePath()) == lastNorm)
                        bySaved = p;
                    QDateTime ts = p->lastUpdate();
                    if (ts.isValid() && ts > mostRecentTime) {
                        mostRecentTime = ts;
                        mostRecent = p;
                    }
                }
            }
            for (auto* c : n->children()) walk(c);
        };
        auto* tree = UBPersistenceManager::persistenceManager()->mDocumentTreeStructureModel;
        if (tree && tree->rootNode()) walk(tree->rootNode());

        std::shared_ptr<UBDocumentProxy> target = bySaved ? bySaved : mostRecent;

        // Diagnostic — show on screen AND append to log file for inspection later.
        QString savedState = bySaved ? "yes" : (lastNorm.isEmpty() ? "none-set" : "miss");
        auto titleOf = [](std::shared_ptr<UBDocumentProxy> p) -> QString {
            if (!p) return QString("none");
            QString g = p->metaData(UBSettings::documentGroupName).toString();
            QString n = p->metaData(UBSettings::documentName).toString();
            return (g.isEmpty() ? QString() : g + " / ") + (n.isEmpty() ? QString("(untitled)") : n);
        };
        QString pickedTitle = titleOf(target);
        QString msg = QString("Resume: docs=%1 saved=%2 → %3")
                        .arg(totalDocs).arg(savedState).arg(pickedTitle);
        UBApplication::showMessage(msg);
        QFile logFile("C:/openboard-fork/last-startup.log");
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream ts(&logFile);
            ts << "Time:  " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
            ts << "Msg:   " << msg << "\n";
            ts << "lastOpenedDocumentPath (raw):  '" << rawLast << "'\n";
            ts << "lastOpenedDocumentPath (norm): '" << lastNorm << "'\n";
            ts << "Tree-walk found " << totalDocs << " doc(s):\n";
            // Just dump the saved-target and most-recent picks with their titles
            ts << "By-saved-path: " << titleOf(bySaved) << " — " << (bySaved ? bySaved->persistencePath() : QString("(none)")) << "\n";
            ts << "Most-recent:   " << titleOf(mostRecent) << " — " << (mostRecent ? mostRecent->persistencePath() : QString("(none)")) << "\n";
            ts << "Picked:        " << titleOf(target) << "\n";
            // Also dump active doc AFTER the swap completes
            QTimer::singleShot(50, this, [this]() {
                auto cur = selectedDocument();
                QFile f("C:/openboard-fork/last-startup.log");
                if (f.open(QIODevice::Append | QIODevice::Text)) {
                    QTextStream s(&f);
                    s << "After-swap selectedDocument: "
                      << (cur ? cur->persistencePath() : QString("(none)"))
                      << " — group='" << (cur ? cur->metaData(UBSettings::documentGroupName).toString() : QString())
                      << "' name='" << (cur ? cur->metaData(UBSettings::documentName).toString() : QString())
                      << "'\n";
                }
            });
            logFile.close();
        }

        if (target && target != blankDoc) {
            int savedIdx = UBSettings::settings()->lastOpenedSceneIndex->get().toInt();
            int pageCount = target->pageCount();
            if (savedIdx < 0) savedIdx = 0;
            if (pageCount > 0 && savedIdx >= pageCount) savedIdx = pageCount - 1;
            setActiveDocumentScene(target, savedIdx);
            // Reassign the "initial" scene to the swapped target. Otherwise
            // lastWindowClosed() will see mInitialDocumentScene pointing at the
            // about-to-be-deleted blank with mInitialIsFreshlyCreated=true and
            // either crash or wrongly clobber the user's real document.
            mInitialDocumentScene = mActiveScene;
            mInitialIsFreshlyCreated = false;
            if (blankDoc && blankDoc != target) {
                auto* pm = UBPersistenceManager::persistenceManager();
                auto* model = pm->mDocumentTreeStructureModel;
                if (model) {
                    QModelIndex idx = model->indexForProxy(blankDoc);
                    if (idx.isValid()) {
                        model->removeRow(idx.row(), idx.parent());
                    }
                }
                pm->deleteDocument(blankDoc);
            }
            // Show the document navigator with the resumed doc highlighted —
            // expanded along its full folder path. (User preference: open into
            // the directory structure showing where they were, not the doc
            // content itself.)
            QTimer::singleShot(20, this, [target](){
                if (UBApplication::applicationController)
                    UBApplication::applicationController->showDocument();
                if (UBApplication::documentController && target)
                    UBApplication::documentController->selectDocument(target, true);
            });
        }
    });

    connect(UBApplication::displayManager, &UBDisplayManager::screenRolesAssigned, this, [this](){
        initBackgroundGridSize();
    });

    undoRedoStateChange(true);
}


UBBoardController::~UBBoardController()
{
    delete mDisplayView;
}

/**
 * @brief Set the default background grid size to appear as roughly 1cm on screen
 */
void UBBoardController::initBackgroundGridSize()
{
    // Besides adjusting for DPI, we also need to scale the grid size by the ratio of the control view size
    // to document size. Here we approximate this ratio as (document resolution) / (screen resolution).
    // Later on, this is calculated by `updateSystemScaleFactor` and stored in `mSystemScaleFactor`.

    qreal dpi = UBApplication::displayManager->logicalDpi(ScreenRole::Control);

    //qDebug() << "dpi: " << dpi;

    qreal screenY = UBApplication::displayManager->screenSize(ScreenRole::Control).height();
    qreal documentY = mActiveScene->nominalSize().height();
    qreal resolutionRatio = documentY / screenY;

    //qDebug() << "resolution ratio: " << resolutionRatio;

    int gridSize = (resolutionRatio * 10. * dpi) / UBGeometryUtils::inchSize;

    UBSettings::settings()->crossSize = gridSize;
    UBSettings::settings()->defaultCrossSize = gridSize;
    mActiveScene->setBackgroundGridSize(gridSize);

    //qDebug() << "grid size: " << gridSize;
}

int UBBoardController::currentPage() const
{
    return mActiveSceneIndex + 1;
}

void UBBoardController::setupViews()
{
    mControlContainer = new QWidget(mMainWindow->centralWidget());

    mControlLayout = new QHBoxLayout(mControlContainer);
    mControlLayout->setContentsMargins(0, 0, 0, 0);

    mControlView = new UBBoardView(this, mControlContainer, true, false);
    mControlView->setObjectName(CONTROLVIEW_OBJ_NAME);
    mControlView->setInteractive(true);
    mControlView->setMouseTracking(true);

    mControlView->grabGesture(Qt::SwipeGesture);

    mControlView->setTransformationAnchor(QGraphicsView::NoAnchor);

    mControlLayout->addWidget(mControlView);
    mControlContainer->setObjectName("ubBoardControlContainer");
    mMainWindow->addBoardWidget(mControlContainer);

    connect(mControlView, SIGNAL(resized(QResizeEvent*)), this, SLOT(boardViewResized(QResizeEvent*)));

    // TODO UB 4.x Optimization do we have to create the display view even if their is
    // only 1 screen
    //
    mDisplayView = new UBBoardView(this, UBItemLayerType::FixedBackground, UBItemLayerType::Tool, 0);
    mDisplayView->setInteractive(false);
    mDisplayView->setTransformationAnchor(QGraphicsView::NoAnchor);

    mPaletteManager = new UBBoardPaletteManager(mControlContainer, this);

    mMessageWindow = new UBMessageWindow(mControlContainer);
    mMessageWindow->hide();

    connect(this, SIGNAL(activeSceneChanged()), mPaletteManager, SLOT(activeSceneChanged()));
}


void UBBoardController::setupLayout()
{
    if(mPaletteManager)
        mPaletteManager->setupLayout();
}


void UBBoardController::setBoxing(QRect displayRect)
{
    if (displayRect.isNull())
    {
        mControlView->setBoxing({});
        return;
    }

    // compute boxing based on the assumed widget size for fullscreen
    QSize centralWidgetSize = mMainWindow->centralWidget()->size();
    QSize controlWindowSize = mMainWindow->size();
    QSize controlScreenSize = UBApplication::displayManager->screenSize(ScreenRole::Control);
    qreal controlWidth = controlScreenSize.width();
    qreal controlHeight = controlScreenSize.height() - controlWindowSize.height() + centralWidgetSize.height();
    qreal displayWidth = (qreal)displayRect.width();
    qreal displayHeight = (qreal)displayRect.height();

    qreal displayRatio = displayWidth / displayHeight;
    qreal controlRatio = controlWidth / controlHeight;

    if (displayRatio < controlRatio)
    {
        // Pillarboxing
        int boxWidth = (centralWidgetSize.width() - (displayWidth * (controlHeight / displayHeight))) / 2;

        if (boxWidth < 0)
        {
            boxWidth = 0;
        }

        mControlView->setBoxing({boxWidth, 0, boxWidth, 0});
    }
    else if (displayRatio > controlRatio)
    {
        // Letterboxing
        int boxHeight = (centralWidgetSize.height() - (displayHeight * (controlWidth / displayWidth))) / 2;

        if (boxHeight < 0)
        {
            boxHeight = 0;
        }

        mControlView->setBoxing({0, boxHeight, 0, boxHeight});
    }
    else
    {
        // No boxing
        mControlView->setBoxing({});
    }
}

void UBBoardController::setCursorFromAngle(qreal angle, const QPoint offset)
{
        QString displayedAngle = QString::number(angle, 'f', 1);
        QWidget *controlViewport = controlView()->viewport();

        QSize cursorSize(45,30);
        QSize bitmapSize = cursorSize;
        int hotX = -1;
        int hotY = -1;

        if (!offset.isNull())
        {
            bitmapSize.setWidth(std::max(bitmapSize.width(), 2 * std::abs(offset.x())));
            bitmapSize.setHeight(std::max(bitmapSize.height(), 2 * std::abs(offset.y())));
            hotX = bitmapSize.width() / 2 - offset.x();
            hotY = bitmapSize.height() / 2 - offset.y();
        }

        QSize origin = (bitmapSize - cursorSize) / 2;

        QImage mask_img(bitmapSize, QImage::Format_Mono);
        mask_img.fill(0xff);
        QPainter mask_ptr(&mask_img);
        mask_ptr.setBrush( QBrush( QColor(0, 0, 0) ) );
        mask_ptr.drawRoundedRect(origin.width(), origin.height(), cursorSize.width()-1, cursorSize.height()-1, 6, 6);
        QBitmap bmpMask = QBitmap::fromImage(mask_img);


        QPixmap pixCursor(bitmapSize);
        pixCursor.fill(QColor(Qt::white));

        QPainter painter(&pixCursor);

        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        painter.setBrush(QBrush(Qt::white));
        painter.setPen(QPen(QColor(Qt::black)));
        painter.drawRoundedRect(origin.width() + 1, origin.height() + 1,cursorSize.width()-2,cursorSize.height()-2,6,6);
        painter.setFont(QFont("Arial", 10));
        painter.drawText(origin.width() + 1, origin.height() + 1,cursorSize.width(),cursorSize.height(), Qt::AlignCenter, displayedAngle.append(QChar(176)));
        painter.end();

        pixCursor.setMask(bmpMask);
        controlViewport->setCursor(QCursor(pixCursor, hotX, hotY));
}


void UBBoardController::setupToolbar()
{
    UBSettings *settings = UBSettings::settings();

    // Setup color choice widget
    QList<QAction *> colorActions;
    colorActions.append(mMainWindow->actionColor0);
    colorActions.append(mMainWindow->actionColor1);
    colorActions.append(mMainWindow->actionColor2);
    colorActions.append(mMainWindow->actionColor3);
    colorActions.append(mMainWindow->actionColor4);

    UBToolbarButtonGroup *colorChoice =
            new UBToolbarButtonGroup(mMainWindow->boardToolBar, colorActions);
    colorChoice->setLabel(tr("Color"));

    mMainWindow->boardToolBar->insertWidget(mMainWindow->actionBackgrounds, colorChoice);

    connect(settings->appToolBarDisplayText, SIGNAL(changed(QVariant)), colorChoice, SLOT(displayText(QVariant)));
    connect(colorChoice, SIGNAL(activated(int)), this, SLOT(setColorIndex(int)));
    connect(UBDrawingController::drawingController(), SIGNAL(colorIndexChanged(int)), colorChoice, SLOT(setCurrentIndex(int)));
    connect(UBDrawingController::drawingController(), SIGNAL(colorIndexChanged(int)), UBDrawingController::drawingController(), SIGNAL(colorPaletteChanged()));
    connect(UBDrawingController::drawingController(), SIGNAL(colorPaletteChanged()), colorChoice, SLOT(colorPaletteChanged()));
    connect(UBDrawingController::drawingController(), SIGNAL(colorPaletteChanged()), this, SLOT(colorPaletteChanged()));

    colorChoice->displayText(QVariant(settings->appToolBarDisplayText->get().toBool()));
    colorChoice->colorPaletteChanged();
    colorChoice->setCurrentIndex(settings->penColorIndex());
    colorActions.at(settings->penColorIndex())->setChecked(true);

    // Setup line width choice widget
    QList<QAction *> lineWidthActions;
    lineWidthActions.append(mMainWindow->actionLineSmall);
    lineWidthActions.append(mMainWindow->actionLineMedium);
    lineWidthActions.append(mMainWindow->actionLineLarge);

    UBToolbarButtonGroup *lineWidthChoice =
            new UBToolbarButtonGroup(mMainWindow->boardToolBar, lineWidthActions);

    connect(settings->appToolBarDisplayText, SIGNAL(changed(QVariant)), lineWidthChoice, SLOT(displayText(QVariant)));

    connect(lineWidthChoice, SIGNAL(activated(int))
            , UBDrawingController::drawingController(), SLOT(setLineWidthIndex(int)));

    connect(UBDrawingController::drawingController(), SIGNAL(lineWidthIndexChanged(int))
            , lineWidthChoice, SLOT(setCurrentIndex(int)));

    lineWidthChoice->displayText(QVariant(settings->appToolBarDisplayText->get().toBool()));
    lineWidthChoice->setCurrentIndex(settings->penWidthIndex());
    lineWidthActions.at(settings->penWidthIndex())->setChecked(true);

    mMainWindow->boardToolBar->insertWidget(mMainWindow->actionBackgrounds, lineWidthChoice);

    //-----------------------------------------------------------//
    // Eraser width choice removed from toolbar (eraser is now touch-sensitive).

    //----------------------- Zoom buttons (immediate) ----------//

    {
        auto viewCenterScene = [this]() -> QPointF {
            if (!mControlView) return QPointF(0, 0);
            return mControlView->mapToScene(mControlView->viewport()->rect().center());
        };

        QToolButton *zoomInBtn = new QToolButton(mMainWindow->boardToolBar);
        zoomInBtn->setIcon(QIcon(":/images/stylusPalette/zoomIn.png"));
        zoomInBtn->setIconSize(QSize(32, 32));
        zoomInBtn->setText(tr("Zoom In"));
        zoomInBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        zoomInBtn->setAutoRaise(true);
        zoomInBtn->setToolTip(tr("Zoom in"));
        connect(zoomInBtn, &QToolButton::clicked, this, [this, viewCenterScene](){
            zoomIn(viewCenterScene());
        });

        QToolButton *zoomOutBtn = new QToolButton(mMainWindow->boardToolBar);
        zoomOutBtn->setIcon(QIcon(":/images/stylusPalette/zoomOut.png"));
        zoomOutBtn->setIconSize(QSize(32, 32));
        zoomOutBtn->setText(tr("Zoom Out"));
        zoomOutBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        zoomOutBtn->setAutoRaise(true);
        zoomOutBtn->setToolTip(tr("Zoom out"));
        connect(zoomOutBtn, &QToolButton::clicked, this, [this, viewCenterScene](){
            zoomOut(viewCenterScene());
        });

        mMainWindow->boardToolBar->insertWidget(mMainWindow->actionUndo, zoomInBtn);
        mMainWindow->boardToolBar->insertWidget(mMainWindow->actionUndo, zoomOutBtn);
        mMainWindow->boardToolBar->insertSeparator(mMainWindow->actionUndo);
    }

    //--- Helpers shared by Math + Geometry dropdowns ---//
    auto viewCenterSceneM = [this]() -> QPointF {
            if (!mControlView) return QPointF(0, 0);
            return mControlView->mapToScene(mControlView->viewport()->rect().center());
        };

    auto addAppWidget = [this, viewCenterSceneM](const QString& wgtName) {
            if (!mActiveScene) return;
            QString libDir = UBSettings::settings()->applicationApplicationsLibraryDirectory();
            QString wgtPath = libDir + "/" + wgtName;
            if (!QDir(wgtPath).exists()) {
                UBApplication::showMessage(tr("Widget not found: %1").arg(wgtName));
                return;
            }
            UBGraphicsWidgetItem *item = mActiveScene->addW3CWidget(QUrl::fromLocalFile(wgtPath), QPointF(0, 0));
            if (item) {
                item->setUuid(QUuid::createUuid());
                item->setSourceUrl(QUrl::fromLocalFile(wgtPath));

                // WistOpenboard fork: shrink newly-inserted widgets so the
                // user doesn't have to manually resize every time. Target is
                // 40% of the document width, but never below 80% of the
                // widget's designed nominal size — small math widgets like
                // Grid have minimum-usable layouts that break if we squeeze
                // them too small. Aspect ratio is preserved.
                const QSizeF sceneSize = mActiveScene->sceneRect().size();
                if (sceneSize.width() > 0 && sceneSize.height() > 0) {
                    const QSize nom = item->nominalSize();
                    qreal targetW = sceneSize.width() * 0.40;
                    qreal aspect  = 0.66;
                    if (nom.width() > 0 && nom.height() > 0) {
                        aspect = qreal(nom.height()) / qreal(nom.width());
                        const qreal minW = qreal(nom.width()) * 0.80;
                        if (targetW < minW)
                            targetW = minW;
                    }
                    item->resize(targetW, targetW * aspect);
                }

                // Center the widget on the current view so the user sees it
                // immediately, regardless of where the scene origin sits.
                const QPointF c = viewCenterSceneM();
                const QRectF br = item->boundingRect();
                item->setPos(c - QPointF(br.width() / 2.0, br.height() / 2.0));

                UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
            }
        };

        // Insert shapes as native Qt graphics items so they're fully editable on
        // the board: drag the body to move, drag a corner of the bounding rect to
        // resize, click to select. A nearly-transparent fill makes the interior
        // hit-testable so users can grab the shape from inside, not just on the
        // stroke.
        auto addShape = [this, viewCenterSceneM](const QString& kind) -> void {
            if (!this->mActiveScene) return;
            QPointF c = viewCenterSceneM();

            auto poly = [](std::initializer_list<QPointF> pts) {
                QPolygonF p; for (auto &pt : pts) p << pt; return p;
            };

            const qreal W = 200, H = 200;
            UBEditableShapeItem::Kind k = UBEditableShapeItem::Rect;
            QPolygonF verts;

            if (kind == "rect") {
                k = UBEditableShapeItem::Rect;
                verts = poly({{-W*0.6,-H*0.4},{W*0.6,-H*0.4},{W*0.6,H*0.4},{-W*0.6,H*0.4}});
            } else if (kind == "square") {
                k = UBEditableShapeItem::Square;
                verts = poly({{-W/2,-H/2},{W/2,-H/2},{W/2,H/2},{-W/2,H/2}});
            } else if (kind == "circle") {
                k = UBEditableShapeItem::Circle;
                verts = poly({{-W/2,-H/2},{W/2,-H/2},{W/2,H/2},{-W/2,H/2}});
            } else if (kind == "ellipse") {
                k = UBEditableShapeItem::Ellipse;
                verts = poly({{-W*0.6,-H*0.4},{W*0.6,-H*0.4},{W*0.6,H*0.4},{-W*0.6,H*0.4}});
            } else if (kind == "triangle") {
                k = UBEditableShapeItem::Polygon;
                verts = poly({{0,-H/2},{-W/2,H/2},{W/2,H/2}});
            } else if (kind == "rightTriangle") {
                k = UBEditableShapeItem::Polygon;
                verts = poly({{-W/2,-H/2},{-W/2,H/2},{W/2,H/2}});
            } else if (kind == "equilTriangle") {
                k = UBEditableShapeItem::Polygon;
                verts = poly({{0,-H*0.55},{-W*0.5,H*0.32},{W*0.5,H*0.32}});
            } else if (kind == "isoscelesTriangle") {
                k = UBEditableShapeItem::Polygon;
                verts = poly({{0,-H/2},{-W*0.4,H/2},{W*0.4,H/2}});
            } else if (kind == "parallelogram") {
                k = UBEditableShapeItem::Polygon;
                verts = poly({{-W*0.45,-H*0.3},{W*0.5,-H*0.3},{W*0.3,H*0.3},{-W*0.65,H*0.3}});
            } else if (kind == "trapezoid") {
                k = UBEditableShapeItem::Polygon;
                verts = poly({{-W*0.3,-H*0.3},{W*0.3,-H*0.3},{W*0.5,H*0.3},{-W*0.5,H*0.3}});
            } else if (kind == "rhombus") {
                k = UBEditableShapeItem::Polygon;
                verts = poly({{0,-H/2},{W/2,0},{0,H/2},{-W/2,0}});
            } else if (kind == "pentagon" || kind == "hexagon" || kind == "octagon") {
                k = UBEditableShapeItem::Polygon;
                int n = (kind == "pentagon") ? 5 : (kind == "hexagon") ? 6 : 8;
                qreal start = (kind == "octagon") ? -M_PI/8 : -M_PI/2;
                if (kind == "hexagon") start = 0;
                const qreal r = 100;
                for (int i = 0; i < n; ++i) {
                    qreal a = start + i * (2*M_PI/n);
                    verts << QPointF(r*qCos(a), r*qSin(a));
                }
            } else {
                return;
            }

            auto *item = new UBEditableShapeItem(k, verts);
            // Tag the layer so UBBoardView::shouldDisplayItem includes it; without
            // this the shape is added to the scene but never painted (filtered out
            // because the layer-type data tag is missing).
            item->setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Object));
            // Lowercase `itemLayerType` controls z-order assignment; without
            // this the shape ends up below the page content and looks "missing".
            item->setData(UBGraphicsItemData::itemLayerType, QVariant(itemLayerType::ObjectItem));
            this->mActiveScene->addItem(item);
            item->setPos(c);
            item->setData(Qt::UserRole, QString("shape:") + kind);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        };

    //----------------------- Tools dropdown (Math + Geometry + Media) ---//

    {
        QToolButton *toolsBtn = new QToolButton(mMainWindow->boardToolBar);
        toolsBtn->setText(tr("Tools"));
        toolsBtn->setIcon(QIcon(":/images/toolbar/tools.png"));
        toolsBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        toolsBtn->setPopupMode(QToolButton::InstantPopup);
        toolsBtn->setAutoRaise(true);
        toolsBtn->setStyleSheet("QToolButton::menu-indicator { image: none; }");

        QMenu *toolsMenu = new QMenu(toolsBtn);

        // ----- Math submenu -----
        QMenu *mathMenu = toolsMenu->addMenu(QIcon(":/images/toolbar/extraTool.png"), tr("Math"));
        QAction *formAct   = mathMenu->addAction(QIcon(":/images/toolbar/extraTool.png"), tr("Formula (LaTeX)"));
        QAction *ggbAct2   = mathMenu->addAction(QIcon(":/images/toolbar/tools.png"), tr("GeoGebra"));
        QAction *gridAct   = mathMenu->addAction(QIcon(":/images/toolbar/background.png"), tr("Grapher / Grid"));
        QAction *calcAct   = mathMenu->addAction(QIcon(":/images/toolbar/tools.png"), tr("Calculator"));
        QAction *numLnAct  = mathMenu->addAction(tr("Number Line"));
        QAction *uCirAct   = mathMenu->addAction(tr("Unit Circle"));
        QAction *spinAct   = mathMenu->addAction(tr("Spinner"));
        QAction *diceAct   = mathMenu->addAction(tr("Dice"));

        connect(formAct,  &QAction::triggered, this, [addAppWidget](){ addAppWidget("Formula.wgt"); });
        connect(ggbAct2,  &QAction::triggered, this, [addAppWidget](){ addAppWidget("GeoGebra.wgt"); });
        connect(gridAct,  &QAction::triggered, this, [addAppWidget](){ addAppWidget("Grid.wgt"); });
        connect(calcAct,  &QAction::triggered, this, [addAppWidget](){ addAppWidget("SciCalc.wgt"); });
        connect(numLnAct, &QAction::triggered, this, [addAppWidget](){ addAppWidget("NumberLine.wgt"); });
        connect(uCirAct,  &QAction::triggered, this, [addAppWidget](){ addAppWidget("UnitCircle.wgt"); });
        connect(spinAct,  &QAction::triggered, this, [addAppWidget](){ addAppWidget("Spinner.wgt"); });
        connect(diceAct,  &QAction::triggered, this, [addAppWidget](){ addAppWidget("DiceRoller.wgt"); });

        // ----- Geometry submenu -----
        QMenu *geoMenu = toolsMenu->addMenu(QIcon(":/images/toolbar/tools.png"), tr("Geometry"));

        // Group 1: Interactive instruments
        QAction *protAct   = geoMenu->addAction(tr("Protractor"));
        QAction *compAct   = geoMenu->addAction(tr("Compass"));
        QAction *rulerAct  = geoMenu->addAction(tr("Ruler"));
        QAction *axesAct   = geoMenu->addAction(tr("Set Square"));
        QAction *drTriAct  = geoMenu->addAction(tr("Drafting Triangle"));
        geoMenu->addSeparator();

        // Group 2: Quadrilaterals & basic shapes
        QAction *rectAct    = geoMenu->addAction(tr("Rectangle"));
        QAction *sqAct      = geoMenu->addAction(tr("Square"));
        QAction *parallAct  = geoMenu->addAction(tr("Parallelogram"));
        QAction *trapAct    = geoMenu->addAction(tr("Trapezoid"));
        QAction *rhomAct    = geoMenu->addAction(tr("Rhombus"));
        geoMenu->addSeparator();

        // Group 3: Circles & ellipses
        QAction *circAct    = geoMenu->addAction(tr("Circle"));
        QAction *ellipAct   = geoMenu->addAction(tr("Ellipse"));
        geoMenu->addSeparator();

        // Group 4: Triangles
        QAction *triAct     = geoMenu->addAction(tr("Triangle (general)"));
        QAction *rTriAct    = geoMenu->addAction(tr("Right Triangle"));
        QAction *eTriAct    = geoMenu->addAction(tr("Equilateral Triangle"));
        QAction *iTriAct    = geoMenu->addAction(tr("Isosceles Triangle"));
        geoMenu->addSeparator();

        // Group 5: Regular polygons
        QAction *pentAct    = geoMenu->addAction(tr("Pentagon"));
        QAction *hexAct     = geoMenu->addAction(tr("Hexagon"));
        QAction *octAct     = geoMenu->addAction(tr("Octagon"));
        geoMenu->addSeparator();

        // Group 6: Construction workspace
        QAction *ggbGeoAct  = geoMenu->addAction(tr("GeoGebra Geometry (constructions)"));

        connect(protAct,    &QAction::triggered, this, [this, viewCenterSceneM](){ if (mActiveScene) mActiveScene->addProtractor(viewCenterSceneM()); });
        connect(compAct,    &QAction::triggered, this, [this, viewCenterSceneM](){ if (mActiveScene) mActiveScene->addCompass(viewCenterSceneM()); });
        connect(rulerAct,   &QAction::triggered, this, [this, viewCenterSceneM](){ if (mActiveScene) mActiveScene->addRuler(viewCenterSceneM()); });
        connect(axesAct,    &QAction::triggered, this, [this, viewCenterSceneM](){ if (mActiveScene) mActiveScene->addAxes(viewCenterSceneM()); });
        connect(drTriAct,   &QAction::triggered, this, [this, viewCenterSceneM](){ if (mActiveScene) mActiveScene->addTriangle(viewCenterSceneM()); });

        connect(rectAct,    &QAction::triggered, this, [addShape](){ addShape("rect"); });
        connect(sqAct,      &QAction::triggered, this, [addShape](){ addShape("square"); });
        connect(parallAct,  &QAction::triggered, this, [addShape](){ addShape("parallelogram"); });
        connect(trapAct,    &QAction::triggered, this, [addShape](){ addShape("trapezoid"); });
        connect(rhomAct,    &QAction::triggered, this, [addShape](){ addShape("rhombus"); });

        connect(circAct,    &QAction::triggered, this, [addShape](){ addShape("circle"); });
        connect(ellipAct,   &QAction::triggered, this, [addShape](){ addShape("ellipse"); });

        connect(triAct,     &QAction::triggered, this, [addShape](){ addShape("triangle"); });
        connect(rTriAct,    &QAction::triggered, this, [addShape](){ addShape("rightTriangle"); });
        connect(eTriAct,    &QAction::triggered, this, [addShape](){ addShape("equilTriangle"); });
        connect(iTriAct,    &QAction::triggered, this, [addShape](){ addShape("isoscelesTriangle"); });

        connect(pentAct,    &QAction::triggered, this, [addShape](){ addShape("pentagon"); });
        connect(hexAct,     &QAction::triggered, this, [addShape](){ addShape("hexagon"); });
        connect(octAct,     &QAction::triggered, this, [addShape](){ addShape("octagon"); });

        connect(ggbGeoAct,  &QAction::triggered, this, [addAppWidget](){ addAppWidget("GeoGebraGeo.wgt"); });

        // ----- Science submenu -----
        // Most science sites (PhET, MolView, Wolfram Alpha, BioDigital) refuse
        // iframe embedding via X-Frame-Options/frame-ancestors. Open them in
        // the built-in Web app instead. Periodic Table previously iframed
        // ptable.com, but Cloudflare's JS challenge fails inside QtWebEngine,
        // so it now also opens LANL's plain-HTML periodic table in the Web app.
        QMenu *sciMenu = toolsMenu->addMenu(QIcon(":/images/toolbar/tools.png"), tr("Science"));
        QAction *ptAct       = sciMenu->addAction(tr("Periodic Table (widget)"));
        QAction *phetAct     = sciMenu->addAction(tr("PhET Simulations (physics, chem, bio)"));
        QAction *molAct      = sciMenu->addAction(tr("MolView (3D molecules)"));
        QAction *stellAct    = sciMenu->addAction(tr("Stellarium (sky map)"));
        QAction *bodyAct     = sciMenu->addAction(tr("BioDigital Human (anatomy)"));
        QAction *wolfAct     = sciMenu->addAction(tr("Wolfram Alpha"));
        sciMenu->addSeparator();
        QAction *nasaAct     = sciMenu->addAction(tr("NASA Eyes (solar system)"));
        QAction *climateAct  = sciMenu->addAction(tr("NOAA Climate (data viewer)"));

        auto openInWeb = [](const QString& url) {
            if (UBApplication::webController)
                UBApplication::webController->loadUrl(QUrl(url));
        };

        connect(ptAct,      &QAction::triggered, this, [addAppWidget](){ addAppWidget("PeriodicTable.wgt"); });
        connect(phetAct,    &QAction::triggered, this, [openInWeb](){ openInWeb("https://phet.colorado.edu/en/simulations/browse"); });
        connect(molAct,     &QAction::triggered, this, [openInWeb](){ openInWeb("https://molview.org/"); });
        connect(stellAct,   &QAction::triggered, this, [openInWeb](){ openInWeb("https://stellarium-web.org/"); });
        connect(bodyAct,    &QAction::triggered, this, [openInWeb](){ openInWeb("https://human.biodigital.com/"); });
        connect(wolfAct,    &QAction::triggered, this, [openInWeb](){ openInWeb("https://www.wolframalpha.com/"); });
        connect(nasaAct,    &QAction::triggered, this, [openInWeb](){ openInWeb("https://eyes.nasa.gov/apps/solar-system/"); });
        connect(climateAct, &QAction::triggered, this, [openInWeb](){ openInWeb("https://www.climate.gov/maps-data/dataset"); });

        // ----- Media submenu -----
        QMenu *mediaMenu = toolsMenu->addMenu(QIcon(":/images/toolbar/tools.png"), tr("Media"));
        QAction *timerAct = mediaMenu->addAction(tr("Timer"));
        QAction *ggbAct   = mediaMenu->addAction(tr("GeoGebra"));
        QAction *ytAct    = mediaMenu->addAction(tr("YouTube"));

        connect(timerAct, &QAction::triggered, this, [addAppWidget](){ addAppWidget("Stopwatch.wgt"); });
        connect(ggbAct,   &QAction::triggered, this, [addAppWidget](){ addAppWidget("GeoGebra.wgt"); });
        connect(ytAct,    &QAction::triggered, this, [addAppWidget](){ addAppWidget("YouTube.wgt"); });

        toolsBtn->setMenu(toolsMenu);

        mMainWindow->boardToolBar->insertWidget(mMainWindow->actionErase, toolsBtn);
        mMainWindow->boardToolBar->insertSeparator(mMainWindow->actionErase);
    }

    //----------------------- Taiwan time clock -----------------//
    {
        // Expanding spacer so the clock + window-control chips that follow
        // are pushed to the far right edge of the toolbar regardless of
        // window width.
        QWidget *boardRightSpacer = new QWidget(mMainWindow->boardToolBar);
        boardRightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        mMainWindow->boardToolBar->addWidget(boardRightSpacer);

        QLabel *clockLabel = new QLabel(mMainWindow->boardToolBar);
        clockLabel->setObjectName("taiwanClockLabel");
        clockLabel->setAlignment(Qt::AlignCenter);
        clockLabel->setContentsMargins(8, 0, 12, 0);
        clockLabel->setStyleSheet(
            "QLabel#taiwanClockLabel {"
            " color: white;"
            " font-size: 16px;"
            " font-weight: bold;"
            " padding-left: 8px;"
            " padding-right: 12px;"
            "}");
        clockLabel->setToolTip(tr("Current time in Taiwan"));

        auto updateClock = [clockLabel]() {
            QDateTime nowTw = QDateTime::currentDateTimeUtc()
                                  .toTimeZone(QTimeZone("Asia/Taipei"));
            clockLabel->setText(nowTw.toString("HH:mm"));
        };
        updateClock();

        QTimer *clockTimer = new QTimer(clockLabel);
        clockTimer->setInterval(1000);
        connect(clockTimer, &QTimer::timeout, clockLabel, updateClock);
        clockTimer->start();

        mMainWindow->boardToolBar->addWidget(clockLabel);
    }

    //--- Compact window controls (Minimize / Maximize / Close) ------------//
    // Hide the bulky text-under-icon actionMinimize / actionQuit from the
    // toolbars and replace them with three flat Windows-titlebar-style chips
    // appended to each toolbar's right edge. Icons are drawn as Unicode line
    // glyphs so they don't depend on theme pixmaps.
    {
        mMainWindow->actionMinimize->setVisible(false);
        mMainWindow->actionQuit->setVisible(false);

        const QString baseCss =
            "QToolButton { color: white; background: transparent; border: none;"
            " font-size: 14px; padding: 0px; }"
            "QToolButton:hover { background: rgba(255,255,255,40); border-radius: 3px; }";
        const QString closeCss =
            "QToolButton { color: white; background: transparent; border: none;"
            " font-size: 14px; padding: 0px; }"
            "QToolButton:hover { background: #e81123; border-radius: 3px; }";

        auto addChips = [this, baseCss, closeCss](QToolBar *bar, bool addRightSpacer){
            if (!bar) return;

            if (addRightSpacer) {
                QWidget *spacer = new QWidget(bar);
                spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                bar->addWidget(spacer);
            }

            QToolButton *minBtn = new QToolButton(bar);
            minBtn->setText(QString::fromUtf8("\xE2\x94\x80")); // ─
            minBtn->setToolTip(tr("Minimize"));
            minBtn->setAutoRaise(true);
            minBtn->setFixedSize(34, 22);
            minBtn->setStyleSheet(baseCss);
            connect(minBtn, &QToolButton::clicked, this, [](){
                if (UBApplication::mainWindow) UBApplication::mainWindow->showMinimized();
            });
            bar->addWidget(minBtn);

            QToolButton *maxBtn = new QToolButton(bar);
            maxBtn->setText(QString::fromUtf8("\xE2\x96\xA1")); // ▢
            maxBtn->setToolTip(tr("Maximize"));
            maxBtn->setAutoRaise(true);
            maxBtn->setFixedSize(34, 22);
            maxBtn->setStyleSheet(baseCss);
            connect(maxBtn, &QToolButton::clicked, this, [maxBtn](){
                QWidget *w = UBApplication::mainWindow;
                if (!w) return;
                if (w->isMaximized()) {
                    w->showNormal();
                    maxBtn->setText(QString::fromUtf8("\xE2\x96\xA1")); // ▢
                    maxBtn->setToolTip(tr("Maximize"));
                } else {
                    w->showMaximized();
                    maxBtn->setText(QString::fromUtf8("\xE2\xA7\x89")); // ⧉
                    maxBtn->setToolTip(tr("Restore"));
                }
            });
            bar->addWidget(maxBtn);

            QToolButton *closeBtn = new QToolButton(bar);
            closeBtn->setText(QString::fromUtf8("\xC3\x97")); // ×
            closeBtn->setToolTip(tr("Close"));
            closeBtn->setAutoRaise(true);
            closeBtn->setFixedSize(34, 22);
            closeBtn->setStyleSheet(closeCss);
            connect(closeBtn, &QToolButton::clicked, this, [](){
                if (UBApplication::mainWindow) UBApplication::mainWindow->close();
            });
            bar->addWidget(closeBtn);
        };

        // boardToolBar already has an expanding spacer inserted before its clock,
        // so the chips inherit right-alignment from that. Web and document toolbars
        // need their own spacer so the chips stick to the window's right edge.
        addChips(mMainWindow->boardToolBar, false);
        addChips(mMainWindow->webToolBar, true);
        addChips(mMainWindow->documentToolBar, true);
    }

    //-----------------------------------------------------------//

    UBApplication::app()->insertSpaceToToolbarBeforeAction(mMainWindow->boardToolBar, mMainWindow->actionBoard, 16);

    UBApplication::app()->decorateActionMenu(mMainWindow->actionMenu);

    mMainWindow->actionBoard->setVisible(false);

    mMainWindow->webToolBar->hide();
    mMainWindow->documentToolBar->hide();

    connectToolbar();
    initToolbarTexts();

    UBApplication::app()->toolBarDisplayTextChanged(QVariant(settings->appToolBarDisplayText->get().toBool()));
}


void UBBoardController::setToolCursor(int tool)
{
    if (mActiveScene)
        mActiveScene->setToolCursor(tool);

    mControlView->setToolCursor(tool);
}


void UBBoardController::connectToolbar()
{
    connect(mMainWindow->actionAdd, SIGNAL(triggered()), this, SLOT(addItem()));
    connect(mMainWindow->actionNewPage, SIGNAL(triggered()), this, SLOT(addScene()));
    connect(mMainWindow->actionDuplicatePage, SIGNAL(triggered()), this, SLOT(duplicateScene()));

    connect(mMainWindow->actionClearPage, SIGNAL(triggered()), this, SLOT(clearScene()));
    connect(mMainWindow->actionEraseItems, SIGNAL(triggered()), this, SLOT(clearSceneItems()));
    connect(mMainWindow->actionEraseAnnotations, SIGNAL(triggered()), this, SLOT(clearSceneAnnotation()));
    connect(mMainWindow->actionEraseBackground,SIGNAL(triggered()),this,SLOT(clearSceneBackground()));

    connect(mMainWindow->actionUndo, SIGNAL(triggered()), UBApplication::undoStack, SLOT(undo()));
    connect(mMainWindow->actionRedo, SIGNAL(triggered()), UBApplication::undoStack, SLOT(redo()));
    connect(mMainWindow->actionRedo, SIGNAL(triggered()), this, SLOT(startScript()));
    connect(mMainWindow->actionBack, SIGNAL( triggered()), this, SLOT(previousScene()));
    connect(mMainWindow->actionForward, SIGNAL(triggered()), this, SLOT(nextScene()));
    connect(mMainWindow->actionSleep, SIGNAL(triggered()), this, SLOT(stopScript()));
    connect(mMainWindow->actionSleep, SIGNAL(triggered()), this, SLOT(blackout()));
    connect(mMainWindow->actionVirtualKeyboard, SIGNAL(triggered(bool)), this, SLOT(showKeyboard(bool)));
    connect(mMainWindow->actionImportPage, SIGNAL(triggered()), this, SLOT(importPage()));
}

void UBBoardController::startScript()
{
    freezeW3CWidgets(false);
}

void UBBoardController::stopScript()
{
    freezeW3CWidgets(true);
}

void UBBoardController::saveData(SaveFlags fls)
{
    bool verbose = fls | sf_showProgress;
    if (verbose) {
        UBApplication::showMessage(tr("Saving document..."));
    }
    if (mActiveScene && mActiveScene->isModified()) {
        persistCurrentScene(true);
    }
    if (verbose) {
        UBApplication::showMessage(tr("Document has just been saved..."));
    }
}

void UBBoardController::documentSceneDuplicated(std::shared_ptr<UBDocumentProxy> proxy, int index)
{
    // index is duplicated page
    if (selectedDocument() == proxy)
    {
        if (UBApplication::applicationController->displayMode() == UBApplicationController::Board)
        {
            // directly change scene to new duplicate
            setActiveDocumentScene(index);
        }
        else if (index <= mActiveSceneIndex)
        {
            // just shift selection and remember for the next time we switch to Board mode
            mSwitchToSceneIndex = mActiveSceneIndex + 1;
        }
    }
}

void UBBoardController::documentSceneMoved(std::shared_ptr<UBDocumentProxy> proxy, int fromIndex, int toIndex)
{
    if (selectedDocument() == proxy)
    {
        int nextSceneIndex = mActiveSceneIndex;

        if (fromIndex < mActiveSceneIndex && toIndex >= mActiveSceneIndex)
        {
            --nextSceneIndex;
        }
        else if (fromIndex > mActiveSceneIndex && toIndex <= mActiveSceneIndex)
        {
            ++nextSceneIndex;
        }
        else if (fromIndex == mActiveSceneIndex)
        {
            nextSceneIndex = toIndex;
        }

        if (nextSceneIndex == mActiveSceneIndex)
        {
            // no change
            return;
        }

        if (UBApplication::applicationController->displayMode() == UBApplicationController::Board)
        {
            // directly change scene
            setActiveDocumentScene(nextSceneIndex);
        }
        else
        {
            // just remember for the next time we switch to Board mode
            mSwitchToSceneIndex = nextSceneIndex;
        }
    }
}

void UBBoardController::documentSceneDeleted(std::shared_ptr<UBDocumentProxy> proxy, int index)
{
    if (selectedDocument() == proxy)
    {
        int nextSceneIndex = mActiveSceneIndex;

        if (index < mActiveSceneIndex || (index == mActiveSceneIndex && index == proxy->pageCount() && index > 0))
        {
            --nextSceneIndex;
        }

        if (UBApplication::applicationController->displayMode() == UBApplicationController::Board)
        {
            // directly change scene
            setActiveDocumentScene(nextSceneIndex);
        }
        else
        {
            // just remember for the next time we switch to Board mode
            mSwitchToSceneIndex = nextSceneIndex;
        }
    }
}

void UBBoardController::initToolbarTexts()
{
    QList<QAction*> allToolbarActions;

    allToolbarActions << mMainWindow->boardToolBar->actions();
    allToolbarActions << mMainWindow->webToolBar->actions();
    allToolbarActions << mMainWindow->documentToolBar->actions();

    foreach(QAction* action, allToolbarActions)
    {
        QString nominalText = action->text();
        QString shortText = truncate(nominalText, 48);
        QPair<QString, QString> texts(nominalText, shortText);

        mActionTexts.insert(action, texts);
    }
}


void UBBoardController::setToolbarTexts()
{
    // WistOpenboard fork: three-tier responsive sizing so the toolbar fits on
    // 1366×768 laptops, 1920×1080, and ultra-wide smartboards alike.
    //   < 1280 px : small icons, no labels (icon-only).
    //   < 1600 px : small icons WITH labels.
    //   ≥ 1600 px : large icons WITH labels.
    QSize iconSize;
    Qt::ToolButtonStyle btnStyle;
    bool useShortText = false;

    const int w = mMainWindow->width();

    if (w <= 1280) {
        iconSize = QSize(22, 22);
        btnStyle = Qt::ToolButtonIconOnly;
        useShortText = true;          // keeps tool-tips tidy
    } else if (w <= 1600) {
        iconSize = QSize(28, 28);
        btnStyle = Qt::ToolButtonTextUnderIcon;
        useShortText = true;
    } else {
        iconSize = QSize(48, 32);
        btnStyle = Qt::ToolButtonTextUnderIcon;
        useShortText = false;
    }

    QToolBar *bars[] = { mMainWindow->boardToolBar,
                         mMainWindow->webToolBar,
                         mMainWindow->documentToolBar };
    for (QToolBar *bar : bars) {
        if (!bar) continue;
        bar->setIconSize(iconSize);
        bar->setToolButtonStyle(btnStyle);
    }

    foreach(QAction* action, mActionTexts.keys())
    {
        QPair<QString, QString> texts = mActionTexts.value(action);
        action->setText(useShortText ? texts.second : texts.first);
        action->setToolTip(texts.first);
    }
}

bool UBBoardController::eventFilter(QObject *obj, QEvent *event)
{
    // Re-run the responsive toolbar logic whenever the main window is
    // resized — covers the user dragging the window across monitors,
    // maximizing/restoring, or starting on a low-res screen.
    if (obj == mMainWindow && event->type() == QEvent::Resize) {
        setToolbarTexts();
    }
    return UBDocumentContainer::eventFilter(obj, event);
}


QString UBBoardController::truncate(QString text, int maxWidth) const
{
    QFontMetricsF fontMetrics(mMainWindow->font());
    return fontMetrics.elidedText(text, Qt::ElideRight, maxWidth);
}


void UBBoardController::stylusToolDoubleClicked(int tool)
{
    if (tool == UBStylusTool::ZoomIn || tool == UBStylusTool::ZoomOut)
    {
        zoomRestore();
    }
    else if (tool == UBStylusTool::Hand)
    {
        centerRestore();
    }
}



void UBBoardController::addScene()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    persistViewPositionOnCurrentScene();
    persistCurrentScene(false,true);

    auto document = UBDocument::getDocument(selectedDocument());
    document->createPage(mActiveSceneIndex + 1);

    QDateTime now = QDateTime::currentDateTime();
    selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));

    setActiveDocumentScene(mActiveSceneIndex + 1);
    QApplication::restoreOverrideCursor();

    UBPersistenceManager::persistenceManager()->persistDocumentMetadata(selectedDocument());
}

void UBBoardController::addScene(std::shared_ptr<UBGraphicsScene> scene, bool replaceActiveIfEmpty)
{
    if (scene)
    {
        std::shared_ptr<UBGraphicsScene> clone = scene->sceneDeepCopy();

        if (scene->document() && (scene->document() != selectedDocument()))
        {
            foreach(QUrl relativeFile, scene->relativeDependencies())
            {
                QString source = scene->document()->persistencePath() + "/" + relativeFile.path();
                QString destination = selectedDocument()->persistencePath() + "/" + relativeFile.path();

                UBFileSystemUtils::copy(source, destination, true);
            }
        }

        auto document = UBDocument::getDocument(selectedDocument());

        if (replaceActiveIfEmpty && mActiveScene->isEmpty())
        {
            document->insertPage(clone, mActiveSceneIndex);
            setActiveDocumentScene(mActiveSceneIndex);
            deleteScene(mActiveSceneIndex + 1);
        }
        else
        {
            persistCurrentScene(false,true);
            document->insertPage(clone, mActiveSceneIndex + 1);
            setActiveDocumentScene(mActiveSceneIndex + 1);
        }

        QDateTime now = QDateTime::currentDateTime();
        selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
    }
}


void UBBoardController::addScene(std::shared_ptr<UBDocumentProxy> proxy, int sceneIndex, bool replaceActiveIfEmpty)
{
    std::shared_ptr<UBGraphicsScene> scene = UBPersistenceManager::persistenceManager()->loadDocumentScene(proxy, sceneIndex);

    if (scene)
    {
        addScene(scene, replaceActiveIfEmpty);
    }
}

void UBBoardController::duplicateScene(int nIndex)
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    persistCurrentScene(false,true);

    duplicatePage(nIndex);

    QDateTime now = QDateTime::currentDateTime();
    selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));

    setActiveDocumentScene(nIndex + 1);
    QApplication::restoreOverrideCursor();
}

void UBBoardController::duplicateScene()
{
    if (UBApplication::applicationController->displayMode() != UBApplicationController::Board)
        return;
    duplicateScene(mActiveSceneIndex);
}

UBGraphicsItem *UBBoardController::duplicateItem(UBItem *item)
{
    if (!item)
        return NULL;

    UBGraphicsItem *retItem = NULL;

    mLastCreatedItem = NULL;

    QUrl sourceUrl;
    QByteArray pData;

    //common parameters for any item
    QPointF itemPos;
    QSizeF itemSize;

    QGraphicsItem *commonItem = dynamic_cast<QGraphicsItem*>(item);
    if (commonItem)
    {
        qreal shifting = UBSettings::settings()->objectFrameWidth;
        itemPos = commonItem->pos() + QPointF(shifting,shifting);
        itemSize = commonItem->boundingRect().size();
        commonItem->setSelected(false);

    }

    UBMimeType::Enum itemMimeType;

    QString srcFile = item->sourceUrl().toLocalFile();
    if (srcFile.isEmpty())
        srcFile = item->sourceUrl().toString();

    QString contentTypeHeader;
    if (!srcFile.isEmpty())
        contentTypeHeader = UBFileSystemUtils::mimeTypeFromFileName(srcFile);

    if(NULL != qgraphicsitem_cast<UBGraphicsGroupContainerItem*>(commonItem))
        itemMimeType = UBMimeType::Group;
    else
        itemMimeType = UBFileSystemUtils::mimeTypeFromString(contentTypeHeader);

    switch(static_cast<int>(itemMimeType))
    {
    case UBMimeType::AppleWidget:
    case UBMimeType::W3CWidget:
        {
            UBGraphicsWidgetItem *witem = dynamic_cast<UBGraphicsWidgetItem*>(item);
            if (witem)
            {
                sourceUrl = witem->getOwnFolder();
            }
        }break;

    case UBMimeType::Video:
    case UBMimeType::Audio:
        {
            UBGraphicsMediaItem *mitem = dynamic_cast<UBGraphicsMediaItem*>(item);
            if (mitem)
            {
                sourceUrl = mitem->mediaFileUrl();
                downloadURL(sourceUrl, srcFile, itemPos, QSize(itemSize.width(), itemSize.height()), false, false);
                return NULL; // async operation
            }
        }break;

    case UBMimeType::VectorImage:
        {
            UBGraphicsSvgItem *viitem = dynamic_cast<UBGraphicsSvgItem*>(item);
            if (viitem)
            {
                pData = viitem->fileData();
                sourceUrl = item->sourceUrl();
            }
        }break;

    case UBMimeType::RasterImage:
        {
            UBGraphicsPixmapItem *pixitem = dynamic_cast<UBGraphicsPixmapItem*>(item);
            if (pixitem)
            {
                 QBuffer buffer(&pData);
                 buffer.open(QIODevice::WriteOnly);
                 QString format = UBFileSystemUtils::extension(item->sourceUrl().toString(QUrl::DecodeReserved));
                 pixitem->pixmap().save(&buffer, format.toLatin1());
            }
        }break;

    case UBMimeType::Group:
    {
        UBGraphicsGroupContainerItem* groupItem = dynamic_cast<UBGraphicsGroupContainerItem*>(item);
        UBGraphicsGroupContainerItem* duplicatedGroup = NULL;

        QList<QGraphicsItem*> duplicatedItems;
        QList<QGraphicsItem*> children = groupItem->childItems();

        mActiveScene->setURStackEnable(false);
        foreach(QGraphicsItem* pIt, children){
            UBItem* pItem = dynamic_cast<UBItem*>(pIt);
            if(pItem)
            {
                QGraphicsItem * itemToGroup = dynamic_cast<QGraphicsItem *>(duplicateItem(pItem));
                if (itemToGroup)
                {
                    itemToGroup->setZValue(pIt->zValue());
                    itemToGroup->setData(UBGraphicsItemData::ItemOwnZValue, pIt->data(UBGraphicsItemData::ItemOwnZValue).toReal());
                    duplicatedItems.append(itemToGroup);
                }
            }
        }
        duplicatedGroup = mActiveScene->createGroup(duplicatedItems);
        duplicatedGroup->setTransform(groupItem->transform());
        groupItem->copyItemParameters(duplicatedGroup);
        groupItem->setSelected(false);


        retItem = dynamic_cast<UBGraphicsItem *>(duplicatedGroup);

        QGraphicsItem * itemToAdd = dynamic_cast<QGraphicsItem *>(retItem);
        if (itemToAdd)
        {
            mActiveScene->addItem(itemToAdd);
            itemToAdd->setSelected(true);
        }
        mActiveScene->setURStackEnable(true);
    }break;

    case UBMimeType::UNKNOWN:
        {
            QGraphicsItem *copiedItem = dynamic_cast<QGraphicsItem*>(item);
            QGraphicsItem *gitem = dynamic_cast<QGraphicsItem*>(item->deepCopy());
            if (gitem)
            {
                mActiveScene->addItem(gitem);

                if (copiedItem)
                {
                    if (mActiveScene->tools().contains(copiedItem))
                    {
                        mActiveScene->registerTool(gitem);
                    }
                }
                gitem->setPos(itemPos);

                mLastCreatedItem = gitem;
                gitem->setSelected(true);
            }
            retItem = dynamic_cast<UBGraphicsItem *>(gitem);
        }break;
    }

    if (retItem)
    {
        QGraphicsItem *graphicsRetItem = dynamic_cast<QGraphicsItem *>(retItem);
        if (mActiveScene->isURStackIsEnabled()) { //should be deleted after scene own undo stack implemented
             UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(mActiveScene, 0, graphicsRetItem);
             UBApplication::undoStack->push(uc);
        }
        return retItem;
    }

    UBItem *createdItem = downloadFinished(true, sourceUrl, QUrl::fromLocalFile(srcFile), contentTypeHeader, pData, itemPos, QSize(itemSize.width(), itemSize.height()), false);
    if (createdItem)
    {
        createdItem->setSourceUrl(item->sourceUrl());
        item->copyItemParameters(createdItem);

        QGraphicsItem *createdGitem = dynamic_cast<QGraphicsItem*>(createdItem);
        if (createdGitem)
            createdGitem->setPos(itemPos);
        mLastCreatedItem = dynamic_cast<QGraphicsItem*>(createdItem);
        mLastCreatedItem->setSelected(true);

        retItem = dynamic_cast<UBGraphicsItem *>(createdItem);
    }

    return retItem;
}

void UBBoardController::deleteScene(int nIndex)
{
    if (selectedDocument()->pageCount()>=2)
    {
        mDeletingSceneIndex = nIndex;
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        persistCurrentScene();
        UBApplication::showMessage(tr("Deleting page %1").arg(nIndex+1), true);

        auto document = UBDocument::getDocument(selectedDocument());
        document->deletePages({nIndex});

        QDateTime now = QDateTime::currentDateTime();
        selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
        UBMetadataDcSubsetAdaptor::persist(selectedDocument());

        if (nIndex >= pageCount())
            nIndex = pageCount()-1;
        setActiveDocumentScene(nIndex);
        UBApplication::showMessage(tr("Page %1 deleted").arg(nIndex+1));
        QApplication::restoreOverrideCursor();
        mDeletingSceneIndex = -1;
    }
}


// Helper: previously asked the user before doing something destructive on
// the page. Removed per user request — the erase menu already exposes four
// explicit choices (items, annotations, background, all), so the extra
// "are you sure?" dialog is redundant. Kept as an inline no-op so call
// sites and translations don't have to change.
static bool confirmErase(const QString& /*detail*/)
{
    return true;
}

void UBBoardController::clearScene()
{
    if (mActiveScene)
    {
        if (mActiveScene->isEmpty()) return;
        if (!confirmErase(tr("erase all items and annotations"))) return;
        freezeW3CWidgets(true);
        mActiveScene->clearContent(UBGraphicsScene::clearItemsAndAnnotations);
        updateActionStates();
    }
}


void UBBoardController::clearSceneItems()
{
    if (mActiveScene)
    {
        if (mActiveScene->isEmpty()) return;
        if (!confirmErase(tr("erase all items"))) return;
        freezeW3CWidgets(true);
        mActiveScene->clearContent(UBGraphicsScene::clearItems);
        updateActionStates();
    }
}


void UBBoardController::clearSceneAnnotation()
{
    if (mActiveScene)
    {
        if (mActiveScene->isEmpty()) return;
        if (!confirmErase(tr("erase all annotations (ink)"))) return;
        mActiveScene->clearContent(UBGraphicsScene::clearAnnotations);
        updateActionStates();
    }
}

void UBBoardController::clearSceneBackground()
{
    if (mActiveScene)
    {
        if (!confirmErase(tr("clear the page background"))) return;
        mActiveScene->clearContent(UBGraphicsScene::clearBackground);
        updateActionStates();
    }
}

void UBBoardController::showDocumentsDialog()
{
    if (selectedDocument())
        persistCurrentScene();

    UBApplication::mainWindow->actionLibrary->setChecked(false);

}

void UBBoardController::libraryDialogClosed(int ret)
{
    Q_UNUSED(ret);

    mMainWindow->actionLibrary->setChecked(false);
}


void UBBoardController::blackout()
{
    UBApplication::applicationController->blackout();
}

void UBBoardController::showKeyboard(bool show)
{
    if(show)
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);

    if(UBSettings::settings()->useSystemOnScreenKeyboard->get().toBool())
        UBPlatformUtils::showOSK(show);
    else
        mPaletteManager->showVirtualKeyboard(show);
}


void UBBoardController::zoomIn(QPointF scenePoint)
{
    if (mControlView->transform().m11() > UB_MAX_ZOOM)
    {
        qApp->beep();
        return;
    }
    zoom(mZoomFactor, scenePoint);
}


void UBBoardController::zoomOut(QPointF scenePoint)
{
    if ((mControlView->horizontalScrollBar()->maximum() == 0) && (mControlView->verticalScrollBar()->maximum() == 0))
    {
        // Do not zoom out if we reached the maximum
        qApp->beep();
        return;
    }

    qreal newZoomFactor = 1 / mZoomFactor;

    zoom(newZoomFactor, scenePoint);
}


void UBBoardController::zoomRestore()
{
    QTransform tr;

    tr.scale(mSystemScaleFactor, mSystemScaleFactor);
    mControlView->setTransform(tr);

    centerRestore();

    emit zoomChanged(1.0);

    emit controlViewportChanged();
    mActiveScene->setBackgroundZoomFactor(mControlView->transform().m11());
}


void UBBoardController::centerRestore()
{
    // reset transformation and scrollbar values
    centerOn({0, 0});
    mControlView->centerOn({0, 0});

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    // workaround: foreground not repainted after scrolling on Qt5 (fixed in Qt6)
    // setForegroundBrush internally invokes the private function uopdateAll() unconditionally
    mControlView->setForegroundBrush(mControlView->foregroundBrush());
#endif

    persistViewPositionOnCurrentScene();
    UBApplication::applicationController->adjustDisplayView();
}


void UBBoardController::centerOn(QPointF scenePoint) const
{
    // centerOn without using scroll bars
    const auto before = mControlView->transform();

    // create a transformation with the same scaling where the scenePoint is in the center
    QTransform after;
    after.scale(before.m11(), before.m22());
    after.translate(-scenePoint.x(), -scenePoint.y());
    mControlView->setTransform(after);

    if (UBApplication::applicationController)
    {
        UBApplication::applicationController->adjustDisplayView();
    }
}


void UBBoardController::zoom(const qreal ratio, QPointF scenePoint)
{
    qreal currentZoom = ratio * mControlView->transform().m11() / mSystemScaleFactor;
    qreal usedRatio = ratio;

    if (currentZoom > UB_MAX_ZOOM)
    {
        currentZoom = UB_MAX_ZOOM;
        usedRatio = currentZoom * mSystemScaleFactor / mControlView->transform().m11();
    }

    /*
     * The shiftFactor is calculated from the condition that the scenePoint should have the
     * same coordinates on the view after zooming. Let m11, m31 be the transformation parameters
     * before zoom and m11', m31' the parameters after zoom. The equations for scenePoint.x:
     *   m11' = m11 * ratio
     *   x * m11 + m31 = x * m11' + m31'
     * We now solve this equation to get the additional translation m31' - m31:
     *   m31' - m31 = x * (m11 - m11')
     *              = x * m11 * (1 - ratio)
     *              = x * m11' * (1 - ratio) / ratio
     * The translate function works in scene coordinates and multiplies its parameter internally
     * by the scale factor m11', so we omit this factor in the function call below.
     */
    const auto shiftFactor = (1 - usedRatio) / usedRatio;
    mControlView->scale(usedRatio, usedRatio);
    mControlView->translate(scenePoint.x() * shiftFactor, scenePoint.y() * shiftFactor);

    emit zoomChanged(currentZoom);
    UBApplication::applicationController->adjustDisplayView();

    emit controlViewportChanged();
    mActiveScene->setBackgroundZoomFactor(mControlView->transform().m11());
}


void UBBoardController::handScroll(qreal dx, qreal dy)
{
    qreal antiScaleRatio = 1/(mSystemScaleFactor * currentZoom());
    mControlView->translate(dx*antiScaleRatio, dy*antiScaleRatio);

    UBApplication::applicationController->adjustDisplayView();

    emit controlViewportChanged();
}

void UBBoardController::persistViewPositionOnCurrentScene() const
{
    if (mActiveScene)
    {
        // calculate center from transformation
        const QPointF viewRelativeCenter = mControlView->transform().inverted().map(QPointF{0, 0});
        UBGraphicsScene::SceneViewState viewState
        {
            mControlView->transform().m11() / mSystemScaleFactor,
            mControlView->horizontalScrollBar()->value(),
            mControlView->verticalScrollBar()->value(),
            viewRelativeCenter
        };

        mActiveScene->setViewState(viewState);
    }
}

void UBBoardController::restoreViewPositionOnCurrentScene() const
{
    if (mActiveScene)
    {
        if (mDocumentJustOpened)
        {
            mDocumentJustOpened = false;
            const QSize page = mActiveScene->nominalSize();
            const QSize view = mControlView->size();
            if (page.width() > 0 && page.height() > 0 && view.width() > 0 && view.height() > 0)
            {
                qreal fitScale = (qreal)view.width() / (qreal)page.width() * 0.98;
                QTransform transform;
                transform.scale(fitScale, fitScale);
                mControlView->setTransform(transform);
                qreal topY = -page.height() / 2.0 + (view.height() / fitScale) / 2.0;
                centerOn(QPointF(0, topY));
                return;
            }
        }
        const auto viewState = mActiveScene->viewState();
        mControlView->horizontalScrollBar()->setValue(viewState.horizontalPosition);
        mControlView->verticalScrollBar()->setValue(viewState.verticalPostition);
        QTransform transform;
        double scale = viewState.zoomFactor * mSystemScaleFactor;
        transform.scale(scale, scale);
        mControlView->setTransform(transform);
        centerOn(viewState.mLastSceneCenter);
    }
}

void UBBoardController::previousScene()
{
    if (mActiveSceneIndex > 0)
    {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        setActiveDocumentScene(mActiveSceneIndex - 1);
        QApplication::restoreOverrideCursor();
    }

    updateActionStates();
}


void UBBoardController::nextScene()
{
    if (mActiveSceneIndex < selectedDocument()->pageCount() - 1)
    {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        setActiveDocumentScene(mActiveSceneIndex + 1);
        QApplication::restoreOverrideCursor();
    }

    updateActionStates();
}


void UBBoardController::firstScene()
{
    if (mActiveSceneIndex > 0)
    {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        setActiveDocumentScene(0);
        QApplication::restoreOverrideCursor();
    }

    updateActionStates();
}


void UBBoardController::lastScene()
{
    if (mActiveSceneIndex < selectedDocument()->pageCount() - 1)
    {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        setActiveDocumentScene(selectedDocument()->pageCount() - 1);
        QApplication::restoreOverrideCursor();
    }

    updateActionStates();
}

void UBBoardController::downloadURL(const QUrl& url, QString contentSourceUrl, const QPointF& pPos, const QSize& pSize, bool isBackground, bool internalData, UBGraphicsScene* pTargetScene)
{
    QString sUrl = url.toString();
    qDebug() << "something has been dropped on the board! Url is: " << sUrl.left(255);

    QGraphicsItem *oldBackgroundObject = NULL;
    if (isBackground)
        oldBackgroundObject = mActiveScene->backgroundObject();

    if(url.scheme() == "openboardtool")
    {
        downloadFinished(true, url, QUrl(), "application/openboard-tool", QByteArray(), pPos, pSize, isBackground, false, pTargetScene);
    }
    else if (url.scheme() == "file" || url.scheme() == "")
    {
        QUrl formedUrl = url.scheme() == "file" ? url : QUrl::fromLocalFile(sUrl);
        QString fileName = formedUrl.toLocalFile();
        QString contentType = UBFileSystemUtils::mimeTypeFromFileName(fileName);

        // directly add local file to document without copying
        QFile file(fileName);
        QByteArray data;

        if (file.open(QIODevice::ReadOnly))
        {
            data = file.readAll();
        }

        downloadFinished(true, formedUrl, QUrl(), contentType, data, pPos, pSize, isBackground, internalData, pTargetScene);
        file.close();
    }
    else
    {
        // When we fall there, it means that we are dropping something from the web to the board
        sDownloadFileDesc desc;
        desc.modal = true;
        desc.srcUrl = sUrl;
        desc.currentSize = 0;
        desc.name = url.scheme() == "data" ? "Local data" : url.fileName();
        desc.totalSize = 0; // The total size will be retrieved during the download
        desc.pos = pPos;
        desc.size = pSize;
        desc.isBackground = isBackground;

        UBDownloadManager::downloadManager()->addFileToDownload(desc);
    }

    if (isBackground && oldBackgroundObject != mActiveScene->backgroundObject())
    {
        if (mActiveScene->isURStackIsEnabled()) { //should be deleted after scene own undo stack implemented
            UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(mActiveScene, oldBackgroundObject, mActiveScene->backgroundObject());
            UBApplication::undoStack->push(uc);
        }
    }


}


UBItem *UBBoardController::downloadFinished(bool pSuccess, QUrl sourceUrl, QUrl contentUrl, QString pContentTypeHeader,
                                            QByteArray pData, QPointF pPos, QSize pSize,
                                            bool isBackground, bool internalData, UBGraphicsScene* pTargetScene)
{
    // WistOpenboard fork: target scene for synchronous tool drops on the Desktop overlay.
    // Async download paths (UBDownloadManager signal) pass nullptr → fall back to mActiveScene.
    UBGraphicsScene* targetScene = pTargetScene ? pTargetScene : mActiveScene.get();
    QString mimeType = pContentTypeHeader;

    // In some cases "image/jpeg;charset=" is retourned by the drag-n-drop. That is
    // why we will check if an ; exists and take the first part (the standard allows this kind of mimetype)
    if(mimeType.isEmpty())
      mimeType = UBFileSystemUtils::mimeTypeFromFileName(sourceUrl.toString());

    int position=mimeType.indexOf(";");
    if(position != -1)
        mimeType=mimeType.left(position);

    UBMimeType::Enum itemMimeType = UBFileSystemUtils::mimeTypeFromString(mimeType);

    if (!pSuccess)
    {
        UBApplication::showMessage(tr("Downloading content %1 failed").arg(sourceUrl.toString()));
        return NULL;
    }


    mActiveScene->deselectAllItems();
    const QString scheme = sourceUrl.scheme();

    if (scheme != "file" && scheme != "openboardtool" && scheme != "data")
        UBApplication::showMessage(tr("Download finished"));

    if (UBMimeType::RasterImage == itemMimeType)
    {

        qDebug() << "accepting mime type" << mimeType << "as raster image";

        if (pData.length() == 0)
        {
            QFile file(sourceUrl.toLocalFile());

            if (file.open(QFile::ReadOnly))
            {
                pData = file.readAll();
                file.close();
            }
        }

        UBGraphicsPixmapItem* pixItem = mActiveScene->addImage(pData, nullptr, pPos, 1.);

        if (scheme == "data")
        {
            // create a shorter, but still unique URL using a hash function
            QCryptographicHash hash(QCryptographicHash::Md5);
            hash.addData(sourceUrl.toString().toLatin1());
            QByteArray result = hash.result();
            QString hashedUrl = "md5:" + result.toBase64();
            pixItem->setSourceUrl(hashedUrl);
        }
        else
        {
            pixItem->setSourceUrl(sourceUrl);
        }

        if (isBackground)
        {
            mActiveScene->setAsBackgroundObject(pixItem, true);
        }
        else
        {
            mActiveScene->scaleToFitDocumentSize(pixItem, true, UBSettings::objectInControlViewMargin);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }

        return pixItem;
    }
    else if (UBMimeType::VectorImage == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "as vector image";

        UBGraphicsSvgItem* svgItem = mActiveScene->addSvg(sourceUrl, pPos, pData);
        svgItem->setSourceUrl(sourceUrl);

        if (isBackground)
        {
            mActiveScene->setAsBackgroundObject(svgItem);
        }
        else
        {
            mActiveScene->scaleToFitDocumentSize(svgItem, true, UBSettings::objectInControlViewMargin);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }

        return svgItem;
    }
    else if (UBMimeType::AppleWidget == itemMimeType) //mime type invented by us :-(
    {
        qDebug() << "accepting mime type" << mimeType << "as Apple widget";

        QUrl widgetUrl = sourceUrl;

        if (pData.length() > 0)
        {
            widgetUrl = expandWidgetToTempDir(pData, "wdgt");
        }

        UBGraphicsWidgetItem* appleWidgetItem = mActiveScene->addAppleWidget(widgetUrl, pPos);

        appleWidgetItem->setSourceUrl(sourceUrl);

        if (isBackground)
        {
            mActiveScene->setAsBackgroundObject(appleWidgetItem);
        }
        else
        {
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }

        return appleWidgetItem;
    }
    else if (UBMimeType::W3CWidget == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "as W3C widget";
        QUrl widgetUrl = sourceUrl;

        if (pData.length() > 0)
        {
            widgetUrl = expandWidgetToTempDir(pData);
        }

        UBGraphicsWidgetItem *w3cWidgetItem = addW3cWidget(widgetUrl, pPos);

        if (isBackground)
        {
            mActiveScene->setAsBackgroundObject(w3cWidgetItem);
        }
        else
        {
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }

        return w3cWidgetItem;
    }
    else if (UBMimeType::Video == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "as video";

        UBGraphicsMediaItem *mediaVideoItem = 0;
        QUuid uuid = QUuid::createUuid();
        if (pData.length() > 0)
        {
            QString destFile;
            bool b = UBPersistenceManager::persistenceManager()->addFileToDocument(selectedDocument(),
                sourceUrl.toString(),
                UBPersistenceManager::videoDirectory,
                uuid,
                destFile,
                &pData);
            if (!b)
            {
                UBApplication::showMessage(tr("Add file operation failed: file copying error"));
                return NULL;
            }

            QUrl url = QUrl::fromLocalFile(destFile);

            mediaVideoItem = mActiveScene->addMedia(url, false, pPos);
        }
        else
        {
            qDebug() << sourceUrl.toString();
            mediaVideoItem = addVideo(sourceUrl, false, pPos, true);
        }

        if(mediaVideoItem){
            if (contentUrl.isEmpty())
                mediaVideoItem->setSourceUrl(sourceUrl);
            else
                mediaVideoItem->setSourceUrl(contentUrl);
            mediaVideoItem->setUuid(uuid);
            connect(this, SIGNAL(activeSceneChanged()), mediaVideoItem, SLOT(activeSceneChanged()));
        }

        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);

        return mediaVideoItem;
    }
    else if (UBMimeType::Audio == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "as audio";

        UBGraphicsMediaItem *audioMediaItem = 0;

        QUuid uuid = QUuid::createUuid();
        if (pData.length() > 0)
        {
            QString destFile;
            bool b = UBPersistenceManager::persistenceManager()->addFileToDocument(selectedDocument(),
                sourceUrl.toString(),
                UBPersistenceManager::audioDirectory,
                uuid,
                destFile,
                &pData);
            if (!b)
            {
                UBApplication::showMessage(tr("Add file operation failed: file copying error"));
                return NULL;
            }

            QUrl url = QUrl::fromLocalFile(destFile);

            audioMediaItem = mActiveScene->addMedia(url, false, pPos);
        }
        else
        {
            audioMediaItem = addAudio(sourceUrl, false, pPos, true);
        }

        if(audioMediaItem){
            if (contentUrl.isEmpty())
                audioMediaItem->setSourceUrl(sourceUrl);
            else
                audioMediaItem->setSourceUrl(contentUrl);
            audioMediaItem->setUuid(uuid);
            connect(this, SIGNAL(activeSceneChanged()), audioMediaItem, SLOT(activeSceneChanged()));
        }

        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);

        return audioMediaItem;
    }
    else if (UBMimeType::Flash == itemMimeType)
    {

        qDebug() << "accepting mime type" << mimeType << "as flash";

        QString sUrl = sourceUrl.toString();

        if (sUrl.startsWith("file://") || sUrl.startsWith("/"))
        {
            sUrl = sourceUrl.toLocalFile();
        }

        QSize size;

        if (pSize.height() > 0 && pSize.width() > 0)
            size = pSize;
        else
            size = mActiveScene->nominalSize() * .8;

        Q_UNUSED(internalData)

        QString widgetUrl = UBGraphicsW3CWidgetItem::createNPAPIWrapper(sUrl, mimeType, size);
        UBFileSystemUtils::deleteFile(sourceUrl.toLocalFile());
        emit npapiWidgetCreated(widgetUrl);

        if (widgetUrl.length() > 0)
        {
            UBGraphicsWidgetItem *widgetItem = mActiveScene->addW3CWidget(QUrl::fromLocalFile(widgetUrl), pPos);
            widgetItem->setUuid(QUuid::createUuid());
            widgetItem->setSourceUrl(QUrl::fromLocalFile(widgetUrl));
            qDebug() << widgetItem->getOwnFolder();
            qDebug() << widgetItem->getSnapshotPath();

            widgetItem->setSnapshotPath(widgetItem->getOwnFolder());

            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);

            return widgetItem;
        }
    }
    else if (UBMimeType::PDF == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "as PDF";
        qDebug() << "pdf data length: " << pData.size();
        qDebug() << "sourceurl : " + sourceUrl.toString();
        QString sUrl = sourceUrl.toString();

        int numberOfImportedDocuments = 0;

        if (!sourceUrl.isEmpty() && (sUrl.startsWith("file://") || sUrl.startsWith("/")))
        {
            QStringList fileNames;
            fileNames << sourceUrl.toLocalFile();
            numberOfImportedDocuments = UBDocumentManager::documentManager()->addFilesToDocument(selectedDocument(), fileNames);
        }
        else if(pData.size()){
            QTemporaryFile pdfFile("XXXXXX.pdf");
            if (pdfFile.open())
            {
                pdfFile.write(pData);
                pdfFile.close();
                QStringList fileNames;
                fileNames << pdfFile.fileName();
                numberOfImportedDocuments = UBDocumentManager::documentManager()->addFilesToDocument(selectedDocument(), fileNames);
            }
        }

        if (numberOfImportedDocuments > 0)
        {
            QDateTime now = QDateTime::currentDateTime();
            selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
            updateActionStates();
        }
    }
    else if (UBMimeType::OpenboardTool == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "OpenBoard Tool"
                 << "targetScene=" << targetScene
                 << "(active=" << mActiveScene.get() << ")"
                 << "pos=" << pPos;

        if (sourceUrl.toString() == UBToolsManager::manager()->compass.id)
        {
            targetScene->addCompass(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->ruler.id)
        {
            targetScene->addRuler(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->axes.id)
        {
            targetScene->addAxes(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->protractor.id)
        {
            targetScene->addProtractor(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->triangle.id)
        {
            targetScene->addTriangle(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->cache.id)
        {
            targetScene->addCache();
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->magnifier.id)
        {
            UBMagnifierParams params;
            params.x = controlContainer()->geometry().width() / 2;
            params.y = controlContainer()->geometry().height() / 2;
            params.zoom = 2;
            params.sizePercentFromScene = 20;
            targetScene->addMagnifier(params);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->mask.id)
        {
            targetScene->addMask(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else
        {
            UBApplication::showMessage(tr("Unknown tool type %1").arg(sourceUrl.toString()));
        }
    }
    else if (UBMimeType::Html == itemMimeType)
    {
        if (!mEmbedController)
        {
            mEmbedController = new UBEmbedController(mControlView);
        }

        static const QRegularExpression matchTitle("<title>([^<]*)</title>");

        QRegularExpressionMatch match = matchTitle.match(pData);
        QString title = match.hasMatch() ? match.captured(1) : tr("Untitled");

        mEmbedController->pageTitleChanged(title);
        mEmbedController->pageUrlChanged(sourceUrl);
        mEmbedController->showEmbedDialog();

        UBEmbedParser* parser = new UBEmbedParser(this);
        connect(parser, &UBEmbedParser::parseResult, this, [this,parser](bool hasEmbeddedContent){
            QList<UBEmbedContent> list = parser->embeddedContent();
            mEmbedController->updateListOfEmbeddableContent(list);
            parser->deleteLater();
        });

        parser->parse(pData);
    }
    else if (UBMimeType::Document == itemMimeType)
    {
        QString documentFolderName = sourceUrl.toString().section('/', -2, -2); //section before "/metadata.rdf" is documentFolderName

        std::shared_ptr<UBDocumentProxy> document = UBPersistenceManager::persistenceManager()->mDocumentTreeStructureModel->findDocumentByFolderName(documentFolderName);

        if (document)
        {
            setActiveDocumentScene(document, document->lastVisitedSceneIndex());
        }
        else
        {
            UBApplication::showMessage(tr("Could not find document."));
        }
    }
    else
    {
        UBApplication::showMessage(tr("Unknown content type %1").arg(pContentTypeHeader));
        qWarning() << "ignoring mime type" << pContentTypeHeader ;
    }

    return NULL;
}

std::shared_ptr<UBGraphicsScene> UBBoardController::setActiveDocumentScene(int pSceneIndex)
{
    return setActiveDocumentScene(selectedDocument(), pSceneIndex);
}

std::shared_ptr<UBGraphicsScene> UBBoardController::setActiveDocumentScene(std::shared_ptr<UBDocumentProxy> pDocumentProxy, const int pSceneIndex, bool forceReload, bool onImport)
{
    UBApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    persistViewPositionOnCurrentScene();

    bool documentChange = selectedDocument() != pDocumentProxy;
    if (documentChange)
        mDocumentJustOpened = true;
    if (documentChange && !mInInit && pDocumentProxy && !pDocumentProxy->persistencePath().isEmpty())
    {
        UBSettings::settings()->lastOpenedDocumentPath->set(pDocumentProxy->persistencePath());
        UBSettings::settings()->save();
    }

    int index = pSceneIndex;
    int sceneCount = pDocumentProxy->pageCount();
    if (index >= sceneCount && sceneCount > 0)
        index = sceneCount - 1;

    std::shared_ptr<UBGraphicsScene> targetScene = UBPersistenceManager::persistenceManager()->loadDocumentScene(pDocumentProxy, index);

    bool sceneChange = targetScene != mActiveScene;

    if (targetScene)
    {
        if (mActiveScene && !onImport)
        {
            persistCurrentScene();
            freezeW3CWidgets(true);
            ClearUndoStack();
        }else
        {
            UBApplication::undoStack->clear();
        }

        mActiveScene = targetScene;
        mActiveSceneIndex = index;

        setDocument(pDocumentProxy, forceReload);

        updateSystemScaleFactor();

        if (mControlView->scene())
        {
            disconnect(UBApplication::undoStack.data(), SIGNAL(indexChanged(int)), mControlView->scene().get(), SLOT(updateSelectionFrameWrapper(int)));
        }

        mControlView->setScene(mActiveScene.get());
        connect(UBApplication::undoStack.data(), SIGNAL(indexChanged(int)), mControlView->scene().get(), SLOT(updateSelectionFrameWrapper(int)));

        mDisplayView->setScene(mActiveScene.get());
        restoreViewPositionOnCurrentScene(); // setScene() resets the transform; restore after both views are set
        mActiveScene->setBackgroundZoomFactor(mControlView->transform().m11());
        pDocumentProxy->setDefaultDocumentSize(mActiveScene->nominalSize());
        updatePageSizeState();

        adjustDisplayViews();

        UBSettings::settings()->setDarkBackground(mActiveScene->isDarkBackground());
        UBSettings::settings()->setPageBackground(mActiveScene->pageBackground());

        freezeW3CWidgets(false);

        selectionChanged();

        updateBackgroundActionsState(mActiveScene->isDarkBackground(), mActiveScene->pageBackground());

        if (documentChange)
        {
            UBGraphicsTextItem::lastUsedTextColor = QColor(Qt::black);
        }

        if (sceneChange)
        {
            emit activeSceneChanged();
        }

        pDocumentProxy->setLastVisitedSceneIndex(mActiveSceneIndex);

        UBFeaturesController* featuresController = paletteManager()->featuresWidget()->getFeaturesController();

        QUrl url = QUrl::fromLocalFile(pDocumentProxy->persistencePath() + "/metadata.rdf");
        QString documentFolderName = pDocumentProxy->documentFolderName();

        if (!featuresController->isDocumentInFavoriteList(documentFolderName) && !featuresController->isInRecentlyOpenDocuments(documentFolderName))
        {
            featuresController->addToFavorite(url, pDocumentProxy->name(), true);

            // keep recent UBDocument instances alive for fast switching
            auto document = UBDocument::getDocument(pDocumentProxy);

            if (!mRecentDocuments.contains(document))
            {
                mRecentDocuments.append(UBDocument::getDocument(pDocumentProxy));
            }
        }

        auto document = UBDocument::getDocument(pDocumentProxy);
        document->thumbnailScene()->hightlightItem(mActiveSceneIndex, true);
    }
    else
    {
        qWarning() << "could not load document scene : '" << pDocumentProxy->persistencePath() << "', page index : " << pSceneIndex;
    }
    UBApplication::restoreOverrideCursor();

    return targetScene;
}

void UBBoardController::findUniquesItems(const QUndoCommand *parent, QSet<QGraphicsItem*> &items)
{
    if (parent->childCount()) {
        for (int i = 0; i < parent->childCount(); i++) {
            findUniquesItems(parent->child(i), items);
        }
    }

    // Undo command transaction macros. Process separatedly
    if (parent->text() == UBSettings::undoCommandTransactionName) {
        return;
    }

    const UBUndoCommand *undoCmd = static_cast<const UBUndoCommand*>(parent);
    if(undoCmd->getType() != UBUndoType::undotype_GRAPHICITEM)
        return;

    const UBGraphicsItemUndoCommand *cmd = dynamic_cast<const UBGraphicsItemUndoCommand*>(parent);

    // go through all added and removed objects, for create list of unique objects
    // grouped items will be deleted by groups, so we don't need do delete that items.
    QSetIterator<QGraphicsItem*> itAdded(cmd->GetAddedList());
    while (itAdded.hasNext())
    {
        QGraphicsItem* item = itAdded.next();
        if (!items.contains(item) &&
            !(item->parentItem() && UBGraphicsGroupContainerItem::Type == item->parentItem()->type()) &&
            !items.contains(item->parentItem())
            )
        {
            items.insert(item);
        }
    }

    QSetIterator<QGraphicsItem*> itRemoved(cmd->GetRemovedList());
    while (itRemoved.hasNext())
    {
        QGraphicsItem* item = itRemoved.next();
        if (!items.contains(item) &&
            !(item->parentItem() && UBGraphicsGroupContainerItem::Type == item->parentItem()->type()) &&
            !items.contains(item->parentItem())
            )
        {
            items.insert(item);
        }
    }
}

void UBBoardController::ClearUndoStack()
{
    QSet<QGraphicsItem*> uniqueItems;
    // go through all stack command
    for (int i = 0; i < UBApplication::undoStack->count(); i++) {
        findUniquesItems(UBApplication::undoStack->command(i), uniqueItems);
    }

    // Get items from clipboard in order not to delete an item that was cut
    // (using source URL of graphics items as a surrogate for equality testing)
    // This ensures that we can cut and paste a media item, widget, etc. from one page to the next.
    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData* data = clipboard->mimeData();
    QList<QUrl> sourceURLs;

    if (data && data->hasFormat(UBApplication::mimeTypeUniboardPageItem)) {
        const UBMimeDataGraphicsItem* mimeDataGI = qobject_cast <const UBMimeDataGraphicsItem*>(data);

        if (mimeDataGI) {
            foreach (UBItem* sourceItem, mimeDataGI->items()) {
                sourceURLs << sourceItem->sourceUrl();
            }
        }
    }

    // go through all unique items, and check, if they are on scene, or not.
    // if not on scene, than item can be deleted
    QSetIterator<QGraphicsItem*> itUniq(uniqueItems);
    while (itUniq.hasNext())
    {
        QGraphicsItem* item = itUniq.next();
        UBGraphicsScene* scene = nullptr;
        if (item->scene()) {
            scene = dynamic_cast<UBGraphicsScene*>(item->scene());
        }

        bool inClipboard = false;
        UBItem* ubi = dynamic_cast<UBItem*>(item);
        if (ubi && sourceURLs.contains(ubi->sourceUrl()))
            inClipboard = true;

        if(!scene && !inClipboard)
        {
            if (!mActiveScene->deleteItem(item)){
                delete item;
                item = 0;
            }
        }
    }

    // clear stack, and command list
    UBApplication::undoStack->clear();
}

void UBBoardController::adjustDisplayViews()
{
    if (UBApplication::applicationController)
    {
        UBApplication::applicationController->adjustDisplayView();
        UBApplication::applicationController->adjustPreviousViews(mActiveSceneIndex, selectedDocument());
    }
}


int UBBoardController::autosaveTimeoutFromSettings() const
{
    int value = UBSettings::settings()->autoSaveInterval->get().toInt();
    int minute = 60 * 1000;

    return value * minute;
}

void UBBoardController::changeBackground(bool isDark, UBPageBackground pageBackground)
{
    bool currentIsDark = mActiveScene->isDarkBackground();
    UBPageBackground currentBackgroundType = mActiveScene->pageBackground();

    if ((isDark != currentIsDark) || (currentBackgroundType != pageBackground))
    {
        UBSettings::settings()->setDarkBackground(isDark);
        UBSettings::settings()->setPageBackground(pageBackground);

        mActiveScene->setBackground(isDark, pageBackground);

        emit backgroundChanged();
    }
}

void UBBoardController::boardViewResized(QResizeEvent* event)
{
    Q_UNUSED(event);

    int innerMargin = UBSettings::boardMargin;
    int userHeight = mControlContainer->height() - (2 * innerMargin);

    mMessageWindow->move(innerMargin, innerMargin + userHeight - mMessageWindow->height());
    mMessageWindow->adjustSizeAndPosition();

    UBApplication::applicationController->initViewState(
                mControlView->horizontalScrollBar()->value(),
                mControlView->verticalScrollBar()->value());

    updateSystemScaleFactor();

    mControlView->centerOn(0,0);

    if (mDisplayView && UBApplication::displayManager->hasDisplay()) {
        UBApplication::applicationController->adjustDisplayView();
        mDisplayView->centerOn(0,0);
        setBoxing(mDisplayView->geometry());
    }

    mPaletteManager->containerResized();

    UBApplication::boardController->controlView()->scene()->moveMagnifier();

}


void UBBoardController::showMessage(const QString& message, bool showSpinningWheel)
{
    mMessageWindow->showMessage(message, showSpinningWheel);
}


void UBBoardController::hideMessage()
{
    mMessageWindow->hideMessage();
}


void UBBoardController::setDisabled(bool disable)
{
    mMainWindow->boardToolBar->setDisabled(disable);
    mControlView->setDisabled(disable);
}


void UBBoardController::selectionChanged()
{
    updateActionStates();
    emit pageSelectionChanged(activeSceneIndex());
}


void UBBoardController::undoRedoStateChange(bool canUndo)
{
    Q_UNUSED(canUndo);

    mMainWindow->actionUndo->setEnabled(UBApplication::undoStack->canUndo());
    mMainWindow->actionRedo->setEnabled(UBApplication::undoStack->canRedo());

    updateActionStates();
}


void UBBoardController::updateActionStates()
{
    mMainWindow->actionBack->setEnabled(selectedDocument() && (mActiveSceneIndex > 0));
    mMainWindow->actionForward->setEnabled(selectedDocument() && (mActiveSceneIndex < selectedDocument()->pageCount() - 1));
    mMainWindow->actionErase->setEnabled(mActiveScene && !mActiveScene->isEmpty());
}


std::shared_ptr<UBGraphicsScene> UBBoardController::activeScene() const
{
    return mActiveScene;
}


int UBBoardController::activeSceneIndex() const
{
    return mActiveSceneIndex;
}

void UBBoardController::setActiveSceneIndex(int i)
{
    mActiveSceneIndex = i;
}

void UBBoardController::documentSceneChanged(std::shared_ptr<UBDocumentProxy> pDocumentProxy, int pIndex)
{
    Q_UNUSED(pIndex);

    if(selectedDocument() == pDocumentProxy)
    {
        setActiveDocumentScene(mActiveSceneIndex);
    }
}

void UBBoardController::autosaveTimeout()
{
    if (UBApplication::applicationController->displayMode() != UBApplicationController::Board) {
        //perform autosave only in board mode
        return;
    }

    saveData(sf_showProgress);
    UBSettings::settings()->save();
}

void UBBoardController::appMainModeChanged(UBApplicationController::MainMode md)
{
    int autoSaveInterval = autosaveTimeoutFromSettings();
    if (!autoSaveInterval) {
        return;
    }

    if (!mAutosaveTimer) {
        mAutosaveTimer = new QTimer(this);
        connect(mAutosaveTimer, SIGNAL(timeout()), this, SLOT(autosaveTimeout()));
    }

    if (md == UBApplicationController::Board) {
        mAutosaveTimer->start(autoSaveInterval);
    } else if (mAutosaveTimer->isActive()) {
        mAutosaveTimer->stop();
    }
}

void UBBoardController::closing()
{
    mIsClosing = true;

    // Persist the path of the currently-active document so the next launch
    // resumes here. Otherwise the path only updates on a document change, which
    // doesn't happen if the user works on the same doc for the whole session.
    auto curDoc = selectedDocument();
    if (curDoc && !curDoc->persistencePath().isEmpty()) {
        UBSettings::settings()->lastOpenedDocumentPath->set(curDoc->persistencePath());
        UBSettings::settings()->lastOpenedSceneIndex->set(mActiveSceneIndex);
        UBSettings::settings()->save();
    }

    lastWindowClosed();
    ClearUndoStack();
#ifdef Q_OS_OSX
    if (!UBPlatformUtils::errorOpeningVirtualKeyboard)
        showKeyboard(false);
#else
        showKeyboard(false);
#endif
}

void UBBoardController::lastWindowClosed()
{
    if (!mCleanupDone)
    {
        if (initialDocumentScene() && initialDocumentScene()->document())
        {
            if (mInitialIsFreshlyCreated && initialDocumentScene()->isEmpty() && (initialDocumentScene()->document()->documentDate() == initialDocumentScene()->document()->lastUpdate()))
            {
                // intial scene or document have not been modified at all, so we can delete the document.
                UBPersistenceManager::persistenceManager()->deleteDocument(initialDocumentScene()->document());

                //if current scene is not the initial document scene, we still need to persist it to ensure no data can be lost this way
                if (activeScene() != initialDocumentScene())
                {
                    persistCurrentScene();
                    UBPersistenceManager::persistenceManager()->persistDocumentMetadata(selectedDocument());
                }
            }
            else
            {
                // if intial scene or document changed, then rather the initial document scene is the current scene,
                // or current scene changed and initial document scene has already been persisted.
                // Now, we just persist the current scene before closing the app, to ensure no data can be lost this way.
                persistCurrentScene();
                UBPersistenceManager::persistenceManager()->persistDocumentMetadata(selectedDocument());
            }
        }
        else
        { //should not happen ?
            persistCurrentScene();
            UBPersistenceManager::persistenceManager()->persistDocumentMetadata(selectedDocument());
        }

        mCleanupDone = true;
    }
}



void UBBoardController::setColorIndex(int pColorIndex)
{
    UBDrawingController::drawingController()->setColorIndex(pColorIndex);

    if (UBDrawingController::drawingController()->stylusTool() != UBStylusTool::Marker &&
            UBDrawingController::drawingController()->stylusTool() != UBStylusTool::Line &&
            UBDrawingController::drawingController()->stylusTool() != UBStylusTool::Text &&
            UBDrawingController::drawingController()->stylusTool() != UBStylusTool::Selector)
    {
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pen);
    }

    if (UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Pen ||
            UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Line ||
            UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Text ||
            UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Selector)
    {
        mPenColorOnDarkBackground = UBSettings::settings()->penColors(true).at(pColorIndex);
        mPenColorOnLightBackground = UBSettings::settings()->penColors(false).at(pColorIndex);

        if (UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Selector)
        {
            // If we are in mode board, then do that
            if(UBApplication::applicationController->displayMode() == UBApplicationController::Board)
            {
                UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pen);
                mMainWindow->actionPen->setChecked(true);
            }
        }

        emit penColorChanged();
    }
    else if (UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Marker)
    {
        mMarkerColorOnDarkBackground = UBSettings::settings()->markerColors(true).at(pColorIndex);
        mMarkerColorOnLightBackground = UBSettings::settings()->markerColors(false).at(pColorIndex);
    }
}

void UBBoardController::colorPaletteChanged()
{
    mPenColorOnDarkBackground = UBSettings::settings()->penColor(true);
    mPenColorOnLightBackground = UBSettings::settings()->penColor(false);
    mMarkerColorOnDarkBackground = UBSettings::settings()->markerColor(true);
    mMarkerColorOnLightBackground = UBSettings::settings()->markerColor(false);
}


qreal UBBoardController::currentZoom() const
{
    if (mControlView)
        return mControlView->transform().m11() / mSystemScaleFactor;
    else
        return 1.0;
}

void UBBoardController::removeTool(UBToolWidget* toolWidget)
{
    toolWidget->remove();
}

void UBBoardController::hide()
{
    UBApplication::mainWindow->actionLibrary->setChecked(false);
}

void UBBoardController::show()
{
    UBApplication::mainWindow->actionLibrary->setChecked(false);

    if (mSwitchToSceneIndex >= 0)
    {
        setActiveDocumentScene(mSwitchToSceneIndex);
        mSwitchToSceneIndex = -1;
    }
    else
    {
        setActiveDocumentScene(mActiveSceneIndex);
    }
}

void UBBoardController::persistCurrentScene(bool isAnAutomaticBackup, bool forceImmediateSave)
{
    if(UBPersistenceManager::persistenceManager()
            && selectedDocument() && mActiveScene && mActiveSceneIndex != mDeletingSceneIndex
            && (mActiveSceneIndex >= 0) && mActiveSceneIndex != mMovingSceneIndex)
    {
        mActiveScene->saveWidgetSnapshots();

        if (mActiveScene->isModified())
        {
            auto document = UBDocument::getDocument(selectedDocument());
            document->persistPage(mActiveScene, mActiveSceneIndex, isAnAutomaticBackup, forceImmediateSave);

            // Anchor the resume path AND scene index to whatever the user is
            // currently editing — protects against crashes / forced kills.
            if (!mInInit && !selectedDocument()->persistencePath().isEmpty()) {
                const QString curPath = UBSettings::settings()->lastOpenedDocumentPath->get().toString();
                const int curIdx = UBSettings::settings()->lastOpenedSceneIndex->get().toInt();
                bool dirty = false;
                if (curPath != selectedDocument()->persistencePath()) {
                    UBSettings::settings()->lastOpenedDocumentPath->set(selectedDocument()->persistencePath());
                    dirty = true;
                }
                if (curIdx != mActiveSceneIndex) {
                    UBSettings::settings()->lastOpenedSceneIndex->set(mActiveSceneIndex);
                    dirty = true;
                }
                if (dirty) UBSettings::settings()->save();
            }
        }
    }
}

void UBBoardController::updateSystemScaleFactor()
{
    if (mActiveScene)
    {
        qreal newScaleFactor = 1.0;
        QSize pageNominalSize = mActiveScene->nominalSize();
        // disabled: we're going to keep scale factor untouched if the size is custom
        QMap<DocumentSizeRatio::Enum, QSize> sizesMap = UBSettings::settings()->documentSizes;
      //  if(pageNominalSize == sizesMap.value(DocumentSizeRatio::Ratio16_9) || pageNominalSize == sizesMap.value(DocumentSizeRatio::Ratio4_3))
        {
            qreal hFactor = ((qreal)controlView()->size().width()) / ((qreal)pageNominalSize.width());
            qreal vFactor = ((qreal)controlView()->size().height()) / ((qreal)pageNominalSize.height());

            newScaleFactor = qMin(hFactor, vFactor);
        }

        if (mSystemScaleFactor != newScaleFactor)
            mSystemScaleFactor = newScaleFactor;

        restoreViewPositionOnCurrentScene();
        mActiveScene->setBackgroundZoomFactor(mControlView->transform().m11());
    }
    else
    {
        mSystemScaleFactor = 1.0;
    }
}


void UBBoardController::setWidePageSize(bool checked)
{
    Q_UNUSED(checked);
    QSize newSize = UBSettings::settings()->documentSizes.value(DocumentSizeRatio::Ratio16_9);

    if (mActiveScene->nominalSize() != newSize)
    {
        UBPageSizeUndoCommand* uc = new UBPageSizeUndoCommand(mActiveScene, mActiveScene->nominalSize(), newSize);
        UBApplication::undoStack->push(uc);

        setPageSize(newSize);
    }
}


void UBBoardController::setRegularPageSize(bool checked)
{
    Q_UNUSED(checked);
    QSize newSize = UBSettings::settings()->documentSizes.value(DocumentSizeRatio::Ratio4_3);

    if (mActiveScene->nominalSize() != newSize)
    {
        UBPageSizeUndoCommand* uc = new UBPageSizeUndoCommand(mActiveScene, mActiveScene->nominalSize(), newSize);
        UBApplication::undoStack->push(uc);

        setPageSize(newSize);
    }
}


void UBBoardController::setPageSize(QSize newSize)
{
    if (mActiveScene->nominalSize() != newSize)
    {
        mActiveScene->setNominalSize(newSize);

        persistViewPositionOnCurrentScene();

        updateSystemScaleFactor();
        updatePageSizeState();
        adjustDisplayViews();
        QDateTime now = QDateTime::currentDateTime();
        selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));

        UBSettings::settings()->pageSize->set(newSize);
    }
}

void UBBoardController::notifyCache(bool visible)
{
    if(visible)
        emit cacheEnabled();

    mCacheWidgetIsEnabled = visible;
}

void UBBoardController::updatePageSizeState()
{
    if (mActiveScene->nominalSize() == UBSettings::settings()->documentSizes.value(DocumentSizeRatio::Ratio16_9))
    {
        mMainWindow->actionWidePageSize->setChecked(true);
    }
    else if(mActiveScene->nominalSize() == UBSettings::settings()->documentSizes.value(DocumentSizeRatio::Ratio4_3))
    {
        mMainWindow->actionRegularPageSize->setChecked(true);
    }
    else
    {
        mMainWindow->actionCustomPageSize->setChecked(true);
    }
}


void UBBoardController::stylusToolChanged(int tool)
{
    if (UBPlatformUtils::hasVirtualKeyboard() && mPaletteManager->mKeyboardPalette)
    {
        UBStylusTool::Enum eTool = (UBStylusTool::Enum)tool;
        if(eTool != UBStylusTool::Selector && eTool != UBStylusTool::Text)
        {
            if(mPaletteManager->mKeyboardPalette->m_isVisible)
            {
#ifdef Q_OS_OSX
                if (!UBPlatformUtils::errorOpeningVirtualKeyboard)
                    UBApplication::mainWindow->actionVirtualKeyboard->activate(QAction::Trigger);
#else
                UBApplication::mainWindow->actionVirtualKeyboard->activate(QAction::Trigger);
#endif
            }
        }
    }

}


QUrl UBBoardController::expandWidgetToTempDir(const QByteArray& pZipedData, const QString& ext)
{
    QUrl widgetUrl;
    QTemporaryFile tmp;

    if (tmp.open())
    {
        tmp.write(pZipedData);
        tmp.flush();
        tmp.close();

        QString tmpDir = UBFileSystemUtils::createTempDir() + "." + ext;

        if (UBFileSystemUtils::expandZipToDir(tmp, tmpDir))
        {
            widgetUrl = QUrl::fromLocalFile(tmpDir);
        }
    }

    return widgetUrl;
}


void UBBoardController::grabScene(const QRectF& pSceneRect)
{
    if (mActiveScene)
    {
        /*
         * To get the pixel size on screen for the screenshot
         * we use the system scale factor to align to the pixels
         * and use an additional factor of two to provide more details
         */
        const auto scalingFactor = 2. * mSystemScaleFactor;
        const auto pixelSize = pSceneRect.size() * scalingFactor;
        QImage image(pixelSize.toSize(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);

        QRectF targetRect{{0, 0}, pixelSize};
        QPainter painter(&image);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::LosslessImageRendering);

        mActiveScene->setRenderingContext(UBGraphicsScene::NonScreen);
        mActiveScene->setRenderingQuality(UBItem::RenderingQualityHigh, UBItem::CacheNotAllowed);

        mActiveScene->render(&painter, targetRect, pSceneRect);

        mActiveScene->setRenderingContext(UBGraphicsScene::Screen);
//        mActiveScene->setRenderingQuality(UBItem::RenderingQualityNormal);
        mActiveScene->setRenderingQuality(UBItem::RenderingQualityHigh, UBItem::CacheAllowed);

        mPaletteManager->addItem(QPixmap::fromImage(image), QPointF{}, 1. / scalingFactor);
        QDateTime now = QDateTime::currentDateTime();
        selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
    }
}

UBGraphicsMediaItem* UBBoardController::addVideo(const QUrl& pSourceUrl, bool startPlay, const QPointF& pos, bool bUseSource)
{
    QUuid uuid = QUuid::createUuid();
    QUrl concreteUrl = pSourceUrl;

    // media file is not in document folder yet
    if (!bUseSource)
    {
        QString destFile;
        bool b = UBPersistenceManager::persistenceManager()->addFileToDocument(selectedDocument(),
                    pSourceUrl.toLocalFile(),
                    UBPersistenceManager::videoDirectory,
                    uuid,
                    destFile);
        if (!b)
        {
            UBApplication::showMessage(tr("Add file operation failed: file copying error"));
            return NULL;
        }
        concreteUrl = QUrl::fromLocalFile(destFile);
    }// else we just use source Url.


    UBGraphicsMediaItem* vi = mActiveScene->addMedia(concreteUrl, startPlay, pos);
    QDateTime now  = QDateTime::currentDateTime();
    selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));

    if (vi) {
        vi->setUuid(uuid);
        vi->setSourceUrl(pSourceUrl);
    }

    return vi;

}

UBGraphicsMediaItem* UBBoardController::addAudio(const QUrl& pSourceUrl, bool startPlay, const QPointF& pos, bool bUseSource)
{
    QUuid uuid = QUuid::createUuid();
    QUrl concreteUrl = pSourceUrl;

    // media file is not in document folder yet
    if (!bUseSource)
    {
        QString destFile;
        bool b = UBPersistenceManager::persistenceManager()->addFileToDocument(selectedDocument(),
            pSourceUrl.toLocalFile(),
            UBPersistenceManager::audioDirectory,
            uuid,
            destFile);
        if (!b)
        {
            UBApplication::showMessage(tr("Add file operation failed: file copying error"));
            return NULL;
        }
        concreteUrl = QUrl::fromLocalFile(destFile);
    }// else we just use source Url.

    UBGraphicsMediaItem* ai = mActiveScene->addMedia(concreteUrl, startPlay, pos);
    QDateTime now = QDateTime::currentDateTime();
    selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));

    if (ai){
        ai->setUuid(uuid);
        ai->setSourceUrl(pSourceUrl);
    }

    return ai;

}

UBGraphicsWidgetItem *UBBoardController::addW3cWidget(const QUrl &pUrl, const QPointF &pos)
{
    UBGraphicsWidgetItem* w3cWidgetItem = 0;

    QUuid uuid = QUuid::createUuid();

    QString destPath;
    if (!UBPersistenceManager::persistenceManager()->addGraphicsWidgetToDocument(selectedDocument(), pUrl.toLocalFile(), uuid, destPath))
        return NULL;
    QUrl newUrl = QUrl::fromLocalFile(destPath);

    w3cWidgetItem = mActiveScene->addW3CWidget(newUrl, pos);

    if (w3cWidgetItem) {
        w3cWidgetItem->setUuid(uuid);
        w3cWidgetItem->setOwnFolder(newUrl);
        w3cWidgetItem->setSourceUrl(pUrl);

        QString struuid = UBStringUtils::toCanonicalUuid(uuid);
        QString snapshotPath = selectedDocument()->persistencePath() +  "/" + UBPersistenceManager::widgetDirectory + "/" + struuid + ".png";
        w3cWidgetItem->setSnapshotPath(QUrl::fromLocalFile(snapshotPath));
    }

    return w3cWidgetItem;
}

void UBBoardController::cut()
{
    //---------------------------------------------------------//

    QList<QGraphicsItem*> selectedItems;
    foreach(QGraphicsItem* gi, mActiveScene->selectedItems())
        selectedItems << gi;

    //---------------------------------------------------------//

    QList<UBItem*> selected;
    foreach(QGraphicsItem* gi, selectedItems)
    {
        gi->setSelected(false);

        UBItem* ubItem = dynamic_cast<UBItem*>(gi);
        UBGraphicsItem *ubGi =  dynamic_cast<UBGraphicsItem*>(gi);

        if (ubItem && ubGi && !mActiveScene->tools().contains(gi))
        {
            selected << ubItem->deepCopy();
            ubGi->remove();
        }
    }

    //---------------------------------------------------------//

    if (selected.size() > 0)
    {
        QClipboard *clipboard = QApplication::clipboard();

        UBMimeDataGraphicsItem*  mimeGi = new UBMimeDataGraphicsItem(selected);

        mimeGi->setData(UBApplication::mimeTypeUniboardPageItem, QByteArray());
        clipboard->setMimeData(mimeGi);

        QDateTime now = QDateTime::currentDateTime();
        selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
    }

    //---------------------------------------------------------//
}


void UBBoardController::copy()
{
    QList<UBItem*> selected;

    foreach(QGraphicsItem* gi, mActiveScene->selectedItems())
    {
        UBItem* ubItem = dynamic_cast<UBItem*>(gi);

        if (ubItem && !mActiveScene->tools().contains(gi))
            selected << ubItem;
    }

    if (selected.size() > 0)
    {
        QClipboard *clipboard = QApplication::clipboard();

        UBMimeDataGraphicsItem*  mimeGi = new UBMimeDataGraphicsItem(selected);

        mimeGi->setData(UBApplication::mimeTypeUniboardPageItem, QByteArray());
        clipboard->setMimeData(mimeGi);

    }
}


void UBBoardController::paste()
{
    QClipboard *clipboard = QApplication::clipboard();
    qreal xPosition = ((qreal)QRandomGenerator::global()->bounded(RAND_MAX)/(qreal)RAND_MAX) * 400;
    qreal yPosition = ((qreal)QRandomGenerator::global()->bounded(RAND_MAX)/(qreal)RAND_MAX) * 200;
    QPointF randomPos(xPosition -200 , yPosition - 100);
    QRect rect = mControlView->rect();
    QPoint center(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2);
    QPointF viewRelativeCenter = mControlView->mapToScene(center);

    processMimeData(clipboard->mimeData(), viewRelativeCenter + randomPos);

    QDateTime now = QDateTime::currentDateTime();
    selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
}


bool zLevelLessThan( UBItem* s1, UBItem* s2)
{
    qreal s1Zvalue = dynamic_cast<QGraphicsItem*>(s1)->data(UBGraphicsItemData::ItemOwnZValue).toReal();
    qreal s2Zvalue = dynamic_cast<QGraphicsItem*>(s2)->data(UBGraphicsItemData::ItemOwnZValue).toReal();
    return s1Zvalue < s2Zvalue;
}

void UBBoardController::processMimeData(const QMimeData* pMimeData, const QPointF& pPos, UBGraphicsScene* pTargetScene)
{
    // WistOpenboard fork: default to active board scene; for Desktop overlay drops the caller passes the overlay scene
    UBGraphicsScene* targetScene = pTargetScene ? pTargetScene : mActiveScene.get();
    if (pMimeData->hasFormat(UBApplication::mimeTypeUniboardPageItem))
    {
        const UBMimeDataGraphicsItem* mimeData = qobject_cast <const UBMimeDataGraphicsItem*>(pMimeData);

        if (mimeData)
        {
            QList<UBItem*> items = mimeData->items();
            std::stable_sort(items.begin(),items.end(),zLevelLessThan);
            foreach(UBItem* item, items)
            {
                QGraphicsItem* pItem = dynamic_cast<QGraphicsItem*>(item);
                if(NULL != pItem){
                    duplicateItem(item);
                }
            }

            return;
        }
    }

    if(pMimeData->hasHtml())
    {
        QString qsHtml = pMimeData->html();
        QString url = UBApplication::urlFromHtml(qsHtml);

        if("" != url)
        {
            downloadURL(url, QString(), pPos, QSize(), false, false, pTargetScene);
            return;
        }
    }

    if (pMimeData->hasUrls())
    {
        QList<QUrl> urls = pMimeData->urls();

        int index = 0;

        const UBFeaturesMimeData *internalMimeData = qobject_cast<const UBFeaturesMimeData*>(pMimeData);
        bool internalData = false;
        if (internalMimeData) {
            internalData = true;
        }

        foreach(const QUrl url, urls){
            QPointF pos(pPos + QPointF(index * 15, index * 15));

            downloadURL(url, QString(), pos, QSize(), false,  internalData, pTargetScene);
            index++;
        }

        return;
    }

    if (pMimeData->hasImage())
    {
        const QStringList formats = pMimeData->formats();
        QString selectedFormat;
        UBMimeType::Enum ubMimeType{UBMimeType::UNKNOWN};

        for (const QString& format : formats)
        {            
            ubMimeType = UBFileSystemUtils::mimeTypeFromString(format);

            if (ubMimeType == UBMimeType::VectorImage || ubMimeType == UBMimeType::RasterImage)
            {
                selectedFormat = format;
                break;
            }
        }

        QBuffer buffer;

        if (selectedFormat.isEmpty())
        {
            // should never happen, but just in case
            // create an image and fill the buffer with PNG data
            QImage img = qvariant_cast<QImage> (pMimeData->imageData());
            img.save(&buffer, "png");
            ubMimeType = UBMimeType::RasterImage;
        }
        else
        {
            // get data from mime data
            buffer.setData(pMimeData->data(selectedFormat));
        }

        if (ubMimeType == UBMimeType::VectorImage)
        {
            targetScene->addSvg({}, pPos, buffer.data());
            return;
        }
        else if (ubMimeType == UBMimeType::RasterImage)
        {
            // validate that the image is really an image, webkit does not fill properly the image mime data
            if (!buffer.data().isEmpty())
            {
                targetScene->addImage(buffer.data(), nullptr, pPos, 1.);
                return;
            }
        }
    }

    if (pMimeData->hasText())
    {
        if("" != pMimeData->text()){
            // Sometimes, it is possible to have an URL as text. we check here if it is the case
            QString qsTmp = pMimeData->text().remove(QChar('\0'));
            if(qsTmp.startsWith("http"))
                downloadURL(QUrl(qsTmp), QString(), pPos, QSize(), false, false, pTargetScene);
            else{
                if(targetScene->selectedItems().count() && targetScene->selectedItems().at(0)->type() == UBGraphicsItemType::TextItemType)
                    dynamic_cast<UBGraphicsTextItem*>(targetScene->selectedItems().at(0))->setHtml(pMimeData->text());
                else
                    targetScene->addTextHtml("", pPos)->setHtml(pMimeData->text());
            }
        }
        else{
#ifdef Q_OS_OSX
                //  With Safari, in 95% of the drops, the mime datas are hidden in Apple Web Archive pasteboard type.
                //  This is due to the way Safari is working so we have to dig into the pasteboard in order to retrieve
                //  the data.
                QString qsUrl = UBPlatformUtils::urlFromClipboard();
                if("" != qsUrl){
                    // We finally got the url of the dropped ressource! Let's import it!
                    downloadURL(qsUrl, qsUrl, pPos, QSize(), false, false, pTargetScene);
                    return;
                }
#endif
        }
    }
}


void UBBoardController::togglePodcast(bool checked)
{
    if (UBPodcastController::instance())
        UBPodcastController::instance()->toggleRecordingPalette(checked);
}

void UBBoardController::moveGraphicsWidgetToControlView(UBGraphicsWidgetItem* graphicsWidget)
{
    mActiveScene->setURStackEnable(false);
     UBGraphicsItem *toolW3C = duplicateItem(dynamic_cast<UBItem *>(graphicsWidget));
    UBGraphicsWidgetItem *copyedGraphicsWidget = NULL;

    if (toolW3C)
    {
        if (UBGraphicsWidgetItem::Type == toolW3C->type())
            copyedGraphicsWidget = static_cast<UBGraphicsWidgetItem *>(toolW3C);

        UBToolWidget *toolWidget = new UBToolWidget(copyedGraphicsWidget, mControlView);

        graphicsWidget->remove(false);
        mActiveScene->addItemToDeletion(graphicsWidget);

        mActiveScene->setURStackEnable(true);

        QPoint controlViewPos = mControlView->mapFromScene(graphicsWidget->sceneBoundingRect().center());
        toolWidget->centerOn(mControlView->mapTo(mControlContainer, controlViewPos));
        toolWidget->show();
    }
}


void UBBoardController::moveToolWidgetToScene(UBToolWidget* toolWidget)
{
    UBGraphicsWidgetItem *widgetToScene = toolWidget->toolWidget();

    widgetToScene->resetTransform();

    QPoint mainWindowCenter = toolWidget->mapTo(mMainWindow, QPoint(toolWidget->width(), toolWidget->height()) / 2);
    QPoint controlViewCenter = mControlView->mapFrom(mMainWindow, mainWindowCenter);
    QPointF scenePos = mControlView->mapToScene(controlViewCenter);

    widgetToScene->setWebActive(true);
    mActiveScene->addGraphicsWidget(widgetToScene, scenePos);

    toolWidget->remove();
}


void UBBoardController::updateBackgroundActionsState(bool isDark, UBPageBackground pageBackground)
{
    switch (pageBackground) {

        case UBPageBackground::crossed:
            if (isDark)
                mMainWindow->actionCrossedDarkBackground->setChecked(true);
            else
                mMainWindow->actionCrossedLightBackground->setChecked(true);
        break;

        case UBPageBackground::ruled:
        {
            QAction* actionRuledBackground = nullptr;
            if(UBSettings::settings()->isSeyesRuledBackground())
                if(isDark)
                    actionRuledBackground = mMainWindow->actionSeyesRuledDarkBackground;
                else
                    actionRuledBackground = mMainWindow->actionSeyesRuledLightBackground;
            else
                if(isDark)
                    actionRuledBackground = mMainWindow->actionRuledDarkBackground;
                else
                    actionRuledBackground = mMainWindow->actionRuledLightBackground;
            if(actionRuledBackground)
                actionRuledBackground->setChecked(true);
        }
        break;

        default:
            if (isDark)
                mMainWindow->actionPlainDarkBackground->setChecked(true);
            else
                mMainWindow->actionPlainLightBackground->setChecked(true);
        break;
    }
}


void UBBoardController::addItem()
{
    QString defaultPath = UBSettings::settings()->lastImportToLibraryPath->get().toString();

    QString extensions;
    foreach(QString ext, UBSettings::imageFileExtensions)
    {
        extensions += " *.";
        extensions += ext;
    }

    QString filename = QFileDialog::getOpenFileName(mControlContainer, tr("Add Item"),
                                                    defaultPath,
                                                    tr("All Supported (%1)").arg(extensions), NULL, QFileDialog::DontUseNativeDialog);

    if (filename.length() > 0)
    {
        mPaletteManager->addItem(QUrl::fromLocalFile(filename));
        QFileInfo source(filename);
        UBSettings::settings()->lastImportToLibraryPath->set(QVariant(source.absolutePath()));
    }
}

void UBBoardController::importPage()
{
    int pageCount = selectedDocument()->pageCount();
    if (UBApplication::documentController->addFileToDocument(selectedDocument()))
    {
        setActiveDocumentScene(selectedDocument(), pageCount, true);
    }
}

void UBBoardController::notifyPageChanged()
{
    emit activeSceneChanged();
}

void UBBoardController::onDownloadModalFinished()
{

}

void UBBoardController::displayMetaData(QMap<QString, QString> metadatas)
{
    emit displayMetadata(metadatas);
}

void UBBoardController::freezeW3CWidgets(bool freeze)
{
    if (mActiveSceneIndex >= 0)
    {
        QList<QGraphicsItem *> list = UBApplication::boardController->activeScene()->items();
        foreach(QGraphicsItem *item, list)
        {
            freezeW3CWidget(item, freeze);
        }
    }
}

void UBBoardController::freezeW3CWidget(QGraphicsItem *item, bool freeze)
{
    if (item->type() == UBGraphicsW3CWidgetItem::Type)
    {
        UBGraphicsWidgetItem* widget = qgraphicsitem_cast<UBGraphicsWidgetItem*>(item);
        widget->setWebActive(!freeze);
    }
}
