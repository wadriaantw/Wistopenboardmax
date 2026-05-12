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
#include <functional>
#include <memory>
#include <QMenu>
#include <QToolButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QClipboard>
#include <QApplication>
#include <QWebEngineCookieStore>
#include <QWebEngineHistory>
#include <QWebEngineHistoryItem>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QtWebEngineWidgetsVersion>

#include "frameworks/UBPlatformUtils.h"

#include "UBWebController.h"
#include "UBEmbedController.h"
#include "UBEmbedParser.h"

#include "web/simplebrowser/browserwindow.h"
#include "web/simplebrowser/webview.h"
#include "web/simplebrowser/tabwidget.h"

#include "network/UBCookieJar.h"
#include "network/UBNetworkAccessManager.h"

#include "gui/UBMainWindow.h"
#include "gui/UBWebToolsPalette.h"
#include "gui/UBKeyboardPalette.h"
#include "gui/UBStartupHintsPalette.h"

#include "core/UBSettings.h"
#include "core/UBSetting.h"
#include "core/UBApplication.h"
#include "core/UBApplicationController.h"
#include "core/UBDisplayManager.h"

#include "board/UBBoardController.h"
#include "board/UBDrawingController.h"

#include "domain/UBGraphicsScene.h"

#include "desktop/UBCustomCaptureWindow.h"
#include "board/UBBoardPaletteManager.h"

#include "core/memcheck.h"

UBWebController::UBWebController(UBMainWindow* mainWindow)
    : QObject(mainWindow->centralWidget())
    , mMainWindow(mainWindow)
    , mCurrentWebBrowser(nullptr)
    , mBrowserWidget(nullptr)
    , mEmbedController(nullptr)
    , mToolsCurrentPalette(nullptr)
    , mToolsPalettePositionned(false)
    , mDownloadViewIsVisible(false)
{
    connect(mMainWindow->actionOpenTutorial, SIGNAL(triggered()), this, SLOT(onOpenTutorial()));
    connect(mMainWindow->actionHintsAndTips, SIGNAL(triggered()), this, SLOT(onHintsAndTips()));

    bool privateBrowsing = UBSettings::settings()->webPrivateBrowsing->get().toBool();
    qDebug() << "Private browsing" << privateBrowsing;

    if (privateBrowsing)
    {
        // create off-the-record profile that leaves no record on the local machine, and has no persistent data or cache
        mWebProfile = new QWebEngineProfile();
    }
    else
    {
        mWebProfile = new QWebEngineProfile("Default");
        mWebProfile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    }

    // compute a system specific user agent string
    QString originalUserAgent = mWebProfile->httpUserAgent();
    static const QRegularExpression exp("\\(([^;]*);([^)]*)\\)");

    QString p1;
    QString p2;

    QRegularExpressionMatch match = exp.match(originalUserAgent);

    if (match.hasMatch())
    {
        p1 = match.captured(1);
        p2 = match.captured(2);
    }

    QString userAgent = UBSettings::settings()->alternativeUserAgent->get().toString();
    userAgent = userAgent.arg(p1).arg(p2);

    mInterceptor = new UBUserAgentInterceptor(userAgent.toUtf8(), mWebProfile);

#if QTWEBENGINEWIDGETS_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    mWebProfile->setUrlRequestInterceptor(mInterceptor);
#else
    mWebProfile->setRequestInterceptor(mInterceptor);
#endif

    QWebEngineSettings* settings = mWebProfile->settings();
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    settings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);

    // ---------- Web-mode annotation overlay --------------------------------
    // Inject a canvas at the top of every loaded page that scrolls with the
    // document. While the user has Pen/Marker/Eraser selected, the canvas
    // captures input and draws strokes; with Selector active, pointer-events
    // pass through to the page. Strokes are persisted to the page's
    // localStorage keyed by URL — so reloading or revisiting the same URL
    // restores the ink.
    {
        QWebEngineScript ann;
        ann.setName("WistOpenboardAnnotationOverlay");
        ann.setInjectionPoint(QWebEngineScript::DocumentReady);
        ann.setRunsOnSubFrames(false);
        ann.setWorldId(QWebEngineScript::MainWorld);
        ann.setSourceCode(QStringLiteral(R"JS(
(function(){
  if (window.__obAnnLoaded) return;
  window.__obAnnLoaded = true;

  var ratio = window.devicePixelRatio || 1;
  var canvas = document.createElement('canvas');
  canvas.id = '__obAnnotationCanvas';
  canvas.style.cssText =
    'position:absolute;top:0;left:0;margin:0;padding:0;'
    + 'z-index:2147483647;pointer-events:none;';
  if (document.documentElement) document.documentElement.appendChild(canvas);

  // Eraser preview circle (dashed) — only shown while the eraser tool is
  // active and the cursor is over the canvas.
  var eraserCircle = document.createElement('div');
  eraserCircle.id = '__obEraserCircle';
  eraserCircle.style.cssText =
    'position:fixed;top:0;left:0;border:1.5px dashed rgba(0,0,0,0.7);'
    + 'border-radius:50%;pointer-events:none;'
    + 'z-index:2147483647;display:none;'
    + 'box-sizing:border-box;background:transparent;';
  if (document.documentElement) document.documentElement.appendChild(eraserCircle);
  function setEraserCircle(clientX, clientY, r){
    if (tool !== 'eraser') { eraserCircle.style.display = 'none'; return; }
    eraserCircle.style.display = 'block';
    eraserCircle.style.width  = (r * 2) + 'px';
    eraserCircle.style.height = (r * 2) + 'px';
    eraserCircle.style.left = (clientX - r) + 'px';
    eraserCircle.style.top  = (clientY - r) + 'px';
  }

  var ctx = canvas.getContext('2d');
  var tool = 'none', color = '#000000', width = 3;
  var drawing = false, currentStroke = null, allStrokes = [];

  // ---- Adaptive eraser state (matches the board's speed-based eraser) ----
  // Eraser radius grows when you drag fast and shrinks when you slow down.
  var eraserSamples = [];     // recent {x, y, t}
  var eraserSmoothed = 0;     // smoothed radius
  function adaptiveEraserRadius(x, y, baseW){
    // Idle/minimum radius matches the visible hover circle so the eraser
    // never appears to SHRINK on click — only grow when dragged fast.
    var BASE_MIN     = Math.max(baseW, 12);
    var BASE_MAX     = Math.max(baseW * 8.0, 96);
    var WINDOW_MS    = 120;
    var SPEED_SLOW   = 0.05;   // px/ms below this -> minimum radius
    var SPEED_FAST   = 4.0;    // px/ms above this -> maximum radius
    var SMOOTHING    = 0.7;
    var now = performance.now();
    eraserSamples.push({x:x, y:y, t:now});
    while (eraserSamples.length > 1 && (now - eraserSamples[0].t) > WINDOW_MS)
      eraserSamples.shift();
    var dist = 0, dt = 0;
    for (var i = 1; i < eraserSamples.length; i++){
      var a = eraserSamples[i-1], b = eraserSamples[i];
      dist += Math.hypot(b.x - a.x, b.y - a.y);
      dt   += (b.t - a.t);
    }
    var speed = (dt > 0) ? (dist / dt) : 0;
    var k = (speed - SPEED_SLOW) / (SPEED_FAST - SPEED_SLOW);
    if (k < 0) k = 0; if (k > 1) k = 1;
    var target = BASE_MIN + (BASE_MAX - BASE_MIN) * k;
    if (eraserSmoothed <= 0) eraserSmoothed = target;
    eraserSmoothed = SMOOTHING * eraserSmoothed + (1 - SMOOTHING) * target;
    return eraserSmoothed;
  }
  function resetEraserState(){
    eraserSamples = []; eraserSmoothed = 0;
  }

  function key(){ return '__obStrokes:' + location.href.split('#')[0]; }

  function save(){
    try { localStorage.setItem(key(), JSON.stringify(allStrokes)); } catch(e){}
  }

  function restore(){
    try {
      var d = localStorage.getItem(key());
      if (d) { allStrokes = JSON.parse(d) || []; redrawAll(); }
    } catch(e){ allStrokes = []; }
  }

  function fitCanvas(){
    var w = Math.max(document.documentElement.scrollWidth, window.innerWidth);
    var h = Math.max(document.documentElement.scrollHeight, window.innerHeight);
    var px = w * ratio, py = h * ratio;
    if (canvas.width === px && canvas.height === py
        && canvas.style.width === w + 'px') return;
    canvas.style.width = w + 'px';
    canvas.style.height = h + 'px';
    canvas.width = px; canvas.height = py;
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    redrawAll();
  }

  function redrawAll(){
    ctx.clearRect(0, 0, canvas.width / ratio, canvas.height / ratio);
    for (var i = 0; i < allStrokes.length; i++) drawStroke(allStrokes[i], false);
  }

  function drawStroke(s, incremental){
    ctx.save();
    if (s.tool === 'eraser'){
      ctx.globalCompositeOperation = 'destination-out';
      ctx.strokeStyle = 'rgba(0,0,0,1)';
    } else {
      ctx.globalCompositeOperation = 'source-over';
      ctx.strokeStyle = s.color;
      if (s.tool === 'marker') ctx.globalAlpha = 0.4;
    }
    ctx.lineWidth = s.width;
    ctx.lineCap = 'round'; ctx.lineJoin = 'round';
    ctx.beginPath();
    var p = s.pts;
    if (!p || p.length === 0){ ctx.restore(); return; }
    ctx.moveTo(p[0][0], p[0][1]);
    for (var i = 1; i < p.length; i++) ctx.lineTo(p[i][0], p[i][1]);
    ctx.stroke();
    ctx.restore();
  }

  function pos(e){
    var r = canvas.getBoundingClientRect();
    return [e.clientX - r.left, e.clientY - r.top];
  }

  canvas.addEventListener('mousedown', function(e){
    if (tool === 'none') return;
    drawing = true;
    resetEraserState();
    var p = pos(e);
    if (tool === 'eraser'){
      // Erase immediately under the cursor on press (zero-distance erase).
      var r = adaptiveEraserRadius(p[0], p[1], width);
      eraseDot(p[0], p[1], r);
      removeStrokesNear(p[0], p[1], r);
      setEraserCircle(e.clientX, e.clientY, r);
    } else {
      currentStroke = { tool: tool, color: color, width: width, pts: [p] };
    }
    e.preventDefault();
  });
  canvas.addEventListener('mousemove', function(e){
    if (drawing && tool === 'eraser'){
      var pE = pos(e);
      var r = adaptiveEraserRadius(pE[0], pE[1], width);
      eraseDot(pE[0], pE[1], r);
      removeStrokesNear(pE[0], pE[1], r);
      setEraserCircle(e.clientX, e.clientY, r);
      e.preventDefault();
      return;
    }
    if (drawing && currentStroke){
      var p = pos(e); var pts = currentStroke.pts;
      pts.push(p);
      drawStroke({ tool: currentStroke.tool, color: currentStroke.color,
                   width: currentStroke.width,
                   pts: [pts[pts.length - 2], p] }, true);
      e.preventDefault();
      return;
    }
    // Hover (button not pressed): keep the eraser preview circle following
    // the cursor at the configured idle radius.
    if (tool === 'eraser'){
      setEraserCircle(e.clientX, e.clientY, Math.max(width, 12));
    }
  });
  function endStroke(e){
    if (!drawing) return;
    drawing = false;
    if (tool === 'eraser'){
      save();        // persist the post-erase stroke list
    } else if (currentStroke && currentStroke.pts.length > 1){
      allStrokes.push(currentStroke); save();
    }
    currentStroke = null;
    resetEraserState();
    if (e) e.preventDefault();
  }
  canvas.addEventListener('mouseup', endStroke);
  canvas.addEventListener('mouseleave', function(e){
    endStroke(e);
    eraserCircle.style.display = 'none';
  });

  function eraseDot(x, y, r){
    ctx.save();
    ctx.globalCompositeOperation = 'destination-out';
    ctx.beginPath();
    ctx.arc(x, y, r, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
  }

  function pointNearStroke(s, x, y, r){
    var pts = s.pts;
    var rs = (s.width || 1) * 0.5 + r;
    for (var i = 0; i < pts.length; i++){
      var dx = pts[i][0] - x, dy = pts[i][1] - y;
      if (dx*dx + dy*dy <= rs * rs) return true;
    }
    return false;
  }
  function removeStrokesNear(x, y, r){
    var kept = [];
    for (var i = 0; i < allStrokes.length; i++){
      var s = allStrokes[i];
      if (s.tool === 'eraser') { kept.push(s); continue; }
      if (!pointNearStroke(s, x, y, r)) kept.push(s);
    }
    allStrokes = kept;
  }

  window.__obSetTool = function(t, c, w){
    tool = t || 'none';
    if (c) color = c;
    if (typeof w === 'number' && w > 0) width = w;
    canvas.style.pointerEvents = (tool === 'none') ? 'none' : 'auto';
    // Hide the system mouse cursor over the canvas while the eraser is
    // active — the dashed preview circle is the cursor.
    if (tool === 'eraser') {
      canvas.style.cursor = 'none';
    } else {
      canvas.style.cursor = (tool === 'none') ? 'auto' : 'crosshair';
      eraserCircle.style.display = 'none';
    }
  };
  window.__obClear = function(){
    allStrokes = []; redrawAll();
    try { localStorage.removeItem(key()); } catch(e){}
  };

  window.addEventListener('resize', fitCanvas);
  if (window.MutationObserver){
    var mo = new MutationObserver(fitCanvas);
    mo.observe(document.documentElement, { childList: true, subtree: true });
  }
  fitCanvas();
  restore();
})();
)JS"));
        mWebProfile->scripts()->insert(ann);
    }
    // ----------------------------------------------------------------------

    // install cookie filter
    QWebEngineCookieStore* cookieStore = mWebProfile->cookieStore();

    QByteArray value = UBSettings::settings()->webCookiePolicy->get().toByteArray();
    QMetaEnum cookiePolicyEnum = staticMetaObject.enumerator(staticMetaObject.indexOfEnumerator("CookiePolicy"));
    int enumOrdinal = cookiePolicyEnum.keyToValue(value);
    CookiePolicy cookiePolicy = enumOrdinal == -1 ?
                DenyThirdParty :
                static_cast<CookiePolicy>(enumOrdinal);

    qDebug() << "Using cookie policy" << value;
    cookieStore->setCookieFilter(
                [cookiePolicy](const QWebEngineCookieStore::FilterRequest &request)
    {
        switch (cookiePolicy) {
        case DenyAll:
            return false;

        case DenyThirdParty:
            return !request.thirdParty;

        case AcceptAll:
            return true;
        }

        return false;
    }
    );

    // synchronize with QNetworkAccessManager
    QNetworkCookieJar* jar = UBNetworkAccessManager::defaultAccessManager()->cookieJar();

    connect(cookieStore, &QWebEngineCookieStore::cookieAdded, jar, [jar](const QNetworkCookie &cookie){
        jar->insertCookie(cookie);
    });
    connect(cookieStore, &QWebEngineCookieStore::cookieRemoved, jar, [jar](const QNetworkCookie &cookie){
        jar->deleteCookie(cookie);
    });

    // remember settings for cleanup
    cookieAutoDelete = UBSettings::settings()->webCookieAutoDelete->get().toBool();
    cookieKeepDomains = UBSettings::settings()->webCookieKeepDomains->get().toStringList();
}

UBWebController::~UBWebController()
{
    if (cookieAutoDelete)
    {
        QWebEngineCookieStore* cookieStore = mWebProfile->cookieStore();

        if (cookieKeepDomains.empty())
        {
            cookieStore->deleteAllCookies();
        }
        else
        {
            UBCookieJar* jar = dynamic_cast<UBCookieJar*>(UBNetworkAccessManager::defaultAccessManager()->cookieJar());

            if (jar)
            {
                for (const QNetworkCookie& cookie : jar->cookieList())
                {
                    QString domain = cookie.domain();
                    bool keep = false;

                    for (QString keepDomain : cookieKeepDomains)
                    {
                        if (keepDomain.startsWith('.'))
                        {
                            // check for suffix match
                            keep = domain.endsWith(keepDomain);
                        }
                        else
                        {
                            // check for exact match
                            keep = domain == keepDomain;
                        }

                        if (keep)
                        {
                            break;
                        }
                    }

                    if (!keep)
                    {
                        cookieStore->deleteCookie(cookie);
                    }
                }
            }
        }
    }
}

void UBWebController::webBrowserInstance()
{
    QString webHomePage = UBSettings::settings()->webHomePage->get().toString();
    QUrl currentUrl = guessUrlFromString(webHomePage);

    if (UBSettings::settings()->webUseExternalBrowser->get().toBool())
    {
        QDesktopServices::openUrl(currentUrl);
    }
    else
    {
        if (!mCurrentWebBrowser)
        {
            mCurrentWebBrowser = new BrowserWindow(nullptr, mWebProfile);

            mMainWindow->addWebWidget(mCurrentWebBrowser);

            connect(mCurrentWebBrowser, SIGNAL(activeViewChange(QWidget*)), this, SLOT(setSourceWidget(QWidget*)));

            mDownloadManagerWidget.setParent(mCurrentWebBrowser, Qt::Tool);

            UBApplication::app()->insertSpaceToToolbarBeforeAction(mMainWindow->webToolBar, mMainWindow->actionBoard, 32);
            QToolBar* navigationBar = mCurrentWebBrowser->createToolBar(mMainWindow->webToolBar);
            mMainWindow->webToolBar->insertWidget(mMainWindow->actionBoard, navigationBar);
            UBApplication::app()->decorateActionMenu(mMainWindow->actionMenu);

            showTabAtTop(UBSettings::settings()->appToolBarPositionedAtTop->get().toBool());

            adaptToolBar();

            mEmbedController = new UBEmbedController(mCurrentWebBrowser);

            connect(mCurrentWebBrowser, SIGNAL(activeViewPageChanged()), this, SLOT(activePageChanged()));
            connect(mCurrentWebBrowser->tabWidget(), &TabWidget::tabCreated, this, &UBWebController::tabCreated);

            // initialize the browser
            mCurrentWebBrowser->init();

            TabWidget* tabWidget = mCurrentWebBrowser->tabWidget();

            // signals are not emitted for first tab, so call explicitly
            setSourceWidget(mCurrentWebBrowser->currentTab());
            tabCreated(mCurrentWebBrowser->currentTab());

            // connect buttons
            connect(mMainWindow->actionWebBack, &QAction::triggered, tabWidget, [tabWidget]() {
                tabWidget->triggerWebPageAction(QWebEnginePage::Back);
            });

            connect(mMainWindow->actionWebForward, &QAction::triggered, tabWidget, [tabWidget]() {
                tabWidget->triggerWebPageAction(QWebEnginePage::Forward);
            });

            connect(mMainWindow->actionWebReload, &QAction::triggered, tabWidget, [tabWidget]() {
                tabWidget->triggerWebPageAction(QWebEnginePage::Reload);
            });

            connect(mMainWindow->actionStopLoading, &QAction::triggered, tabWidget, [tabWidget]() {
                tabWidget->triggerWebPageAction(QWebEnginePage::Stop);
            });

            connect(mMainWindow->actionHome, &QAction::triggered, this, [this, currentUrl](){
                mCurrentWebBrowser->currentTab()->load(currentUrl);
            });

            connect(mMainWindow->actionWebBigger, SIGNAL(triggered()), mCurrentWebBrowser, SLOT(zoomIn()));
            connect(mMainWindow->actionWebSmaller, SIGNAL(triggered()), mCurrentWebBrowser, SLOT(zoomOut()));

            //--------------- Annotation overlay glue (WistOpenboard fork) ----//
            // 1. Sync the current stylus tool / color / width into the
            //    injected JS canvas whenever the user changes drawing tool or
            //    when they switch tabs.
            // 2. Add a "Clear ink" toolbar button that wipes the active tab's
            //    annotation canvas + its stored strokes for that URL.
            {
                auto pushToolToPage = [this]() {
                    if (!mCurrentWebBrowser || !mCurrentWebBrowser->currentTab()) return;
                    UBDrawingController *dc = UBDrawingController::drawingController();
                    if (!dc) return;
                    UBStylusTool::Enum t = (UBStylusTool::Enum)dc->stylusTool();
                    QString jsTool = "none";
                    if (t == UBStylusTool::Pen)         jsTool = "pen";
                    else if (t == UBStylusTool::Marker) jsTool = "marker";
                    else if (t == UBStylusTool::Eraser) jsTool = "eraser";
                    QColor c = dc->currentToolColor();
                    qreal  w = dc->currentToolWidth();
                    QString js = QString("if(window.__obSetTool)__obSetTool('%1','%2',%3);")
                                    .arg(jsTool, c.name(QColor::HexRgb))
                                    .arg(w);
                    mCurrentWebBrowser->currentTab()->page()->runJavaScript(js);
                };

                connect(UBDrawingController::drawingController(),
                        &UBDrawingController::stylusToolChanged,
                        this, [pushToolToPage](int){ pushToolToPage(); });

                connect(UBDrawingController::drawingController(),
                        &UBDrawingController::colorPaletteChanged,
                        this, [pushToolToPage](){ pushToolToPage(); });

                connect(mCurrentWebBrowser->tabWidget(), &TabWidget::currentChanged,
                        this, [pushToolToPage](int){ pushToolToPage(); });

                // Clear-ink button on the web toolbar.
                QToolButton *clearBtn = new QToolButton(mMainWindow->webToolBar);
                clearBtn->setText(tr("Clear Ink"));
                clearBtn->setIcon(QIcon(":/images/toolbar/clearPage.png"));
                clearBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                clearBtn->setAutoRaise(true);
                clearBtn->setToolTip(tr("Erase all ink on this page"));
                connect(clearBtn, &QToolButton::clicked, this, [this](){
                    if (mCurrentWebBrowser && mCurrentWebBrowser->currentTab())
                        mCurrentWebBrowser->currentTab()->page()->runJavaScript(
                            "if(window.__obClear)__obClear();");
                });
                mMainWindow->webToolBar->insertWidget(mMainWindow->actionWebTools, clearBtn);
            }
            //-----------------------------------------------------------------

            //--------------- Shortcuts dropdown (WistOpenboard fork) ---------------//
            // User-pinnable list of websites. Stored in
            //   <userDataDirectory>/WebShortcuts.ini  under [Shortcuts]/list as
            //   QStringList items "Title|||URL". Add via "Add current page",
            //   "Paste URL...", or "Manage..." to edit/remove.
            {
                QToolButton *shortcutsBtn = new QToolButton(mMainWindow->webToolBar);
                shortcutsBtn->setText(tr("Shortcuts"));
                shortcutsBtn->setIcon(QIcon(":/images/toolbar/bookmarks.png"));
                shortcutsBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                shortcutsBtn->setPopupMode(QToolButton::InstantPopup);
                shortcutsBtn->setAutoRaise(true);
                shortcutsBtn->setStyleSheet("QToolButton::menu-indicator { image: none; }");

                QMenu *shortcutsMenu = new QMenu(shortcutsBtn);
                // WistOpenboard: force readable colors so the menu stays legible
                // when Windows is in dark mode (Fusion otherwise picks up the
                // system palette and renders white text on a near-white menu).
                shortcutsMenu->setStyleSheet(
                    "QMenu { background-color: #f5f5f5; color: #202020;"
                    " border: 1px solid #888; }"
                    "QMenu::item { padding: 4px 24px 4px 16px; }"
                    "QMenu::item:selected { background-color: #2a82da; color: white; }"
                    "QMenu::separator { height: 1px; background: #cccccc;"
                    " margin: 4px 8px; }");
                shortcutsBtn->setMenu(shortcutsMenu);

                auto shortcutsFilePath = []() -> QString {
                    return UBSettings::userDataDirectory() + "/WebShortcuts.ini";
                };

                auto loadShortcuts = [shortcutsFilePath]() -> QStringList {
                    QSettings s(shortcutsFilePath(), QSettings::IniFormat);
                    QStringList list = s.value("Shortcuts/list").toStringList();
                    if (list.isEmpty() && !s.contains("Shortcuts/list")) {
                        // First run defaults so the user sees something useful.
                        list << "YouTube|||https://www.youtube.com/"
                             << "Google|||https://www.google.com/"
                             << "Wikipedia|||https://en.wikipedia.org/"
                             << "GeoGebra|||https://www.geogebra.org/classic"
                             << "Desmos|||https://www.desmos.com/calculator"
                             << "PhET Sims|||https://phet.colorado.edu/en/simulations/browse"
                             << "Periodic Table|||https://en.wikipedia.org/wiki/Periodic_table"
                             << "Khan Academy|||https://www.khanacademy.org/";
                        s.setValue("Shortcuts/list", list);
                    }
                    return list;
                };

                auto saveShortcuts = [shortcutsFilePath](const QStringList& list) {
                    QSettings s(shortcutsFilePath(), QSettings::IniFormat);
                    s.setValue("Shortcuts/list", list);
                    s.sync();
                };

                // Forward declaration so lambdas can recursively rebuild the menu.
                auto rebuildMenu = std::make_shared<std::function<void()>>();

                auto loadInTab = [this](const QString& url) {
                    if (mCurrentWebBrowser && mCurrentWebBrowser->currentTab())
                        mCurrentWebBrowser->currentTab()->load(QUrl(url));
                };

                *rebuildMenu = [this, shortcutsMenu, loadShortcuts, saveShortcuts, loadInTab, rebuildMenu]() {
                    shortcutsMenu->clear();
                    QStringList list = loadShortcuts();

                    for (const QString& entry : list) {
                        QStringList parts = entry.split("|||");
                        if (parts.size() < 2) continue;
                        const QString title = parts[0];
                        const QString url   = parts[1];
                        QAction *act = shortcutsMenu->addAction(title);
                        act->setToolTip(url);
                        QObject::connect(act, &QAction::triggered, this, [loadInTab, url](){
                            loadInTab(url);
                        });
                    }
                    if (!list.isEmpty())
                        shortcutsMenu->addSeparator();

                    QAction *addCurrent = shortcutsMenu->addAction(tr("Add current page"));
                    QObject::connect(addCurrent, &QAction::triggered, this, [this, loadShortcuts, saveShortcuts, rebuildMenu](){
                        if (!mCurrentWebBrowser || !mCurrentWebBrowser->currentTab()) return;
                        QString url = mCurrentWebBrowser->currentTab()->url().toString();
                        QString title = mCurrentWebBrowser->currentTab()->title();
                        if (url.isEmpty()) return;
                        bool ok = false;
                        title = QInputDialog::getText(mCurrentWebBrowser, tr("Add shortcut"),
                                                     tr("Name:"), QLineEdit::Normal, title, &ok);
                        if (!ok || title.trimmed().isEmpty()) return;
                        QStringList list = loadShortcuts();
                        list << (title.trimmed() + "|||" + url);
                        saveShortcuts(list);
                        (*rebuildMenu)();
                    });

                    QAction *pasteUrl = shortcutsMenu->addAction(tr("Paste URL..."));
                    QObject::connect(pasteUrl, &QAction::triggered, this, [this, loadShortcuts, saveShortcuts, rebuildMenu](){
                        QString clip = QApplication::clipboard()->text().trimmed();
                        bool ok = false;
                        QString url = QInputDialog::getText(mCurrentWebBrowser, tr("Add shortcut"),
                                                            tr("URL:"), QLineEdit::Normal, clip, &ok);
                        if (!ok || url.trimmed().isEmpty()) return;
                        if (!url.contains("://")) url = "https://" + url;
                        QString title = QInputDialog::getText(mCurrentWebBrowser, tr("Add shortcut"),
                                                               tr("Name:"), QLineEdit::Normal,
                                                               QUrl(url).host(), &ok);
                        if (!ok || title.trimmed().isEmpty()) return;
                        QStringList list = loadShortcuts();
                        list << (title.trimmed() + "|||" + url.trimmed());
                        saveShortcuts(list);
                        (*rebuildMenu)();
                    });

                    QAction *manage = shortcutsMenu->addAction(tr("Manage..."));
                    QObject::connect(manage, &QAction::triggered, this, [this, loadShortcuts, saveShortcuts, rebuildMenu](){
                        QDialog dlg(mCurrentWebBrowser);
                        dlg.setWindowTitle(tr("Manage Shortcuts"));
                        dlg.resize(420, 320);
                        QVBoxLayout *vl = new QVBoxLayout(&dlg);
                        QListWidget *lw = new QListWidget(&dlg);
                        QStringList list = loadShortcuts();
                        for (const QString& entry : list) {
                            QStringList parts = entry.split("|||");
                            if (parts.size() < 2) continue;
                            QListWidgetItem *it = new QListWidgetItem(parts[0] + "  —  " + parts[1], lw);
                            it->setData(Qt::UserRole, entry);
                        }
                        vl->addWidget(lw);
                        QHBoxLayout *hl = new QHBoxLayout();
                        QPushButton *removeBtn = new QPushButton(tr("Remove"), &dlg);
                        QPushButton *upBtn     = new QPushButton(tr("Move up"), &dlg);
                        QPushButton *downBtn   = new QPushButton(tr("Move down"), &dlg);
                        QPushButton *closeBtn  = new QPushButton(tr("Close"), &dlg);
                        hl->addWidget(removeBtn); hl->addWidget(upBtn);
                        hl->addWidget(downBtn); hl->addStretch(); hl->addWidget(closeBtn);
                        vl->addLayout(hl);

                        QObject::connect(removeBtn, &QPushButton::clicked, &dlg, [lw](){
                            delete lw->currentItem();
                        });
                        QObject::connect(upBtn, &QPushButton::clicked, &dlg, [lw](){
                            int r = lw->currentRow();
                            if (r > 0) {
                                QListWidgetItem *it = lw->takeItem(r);
                                lw->insertItem(r - 1, it);
                                lw->setCurrentRow(r - 1);
                            }
                        });
                        QObject::connect(downBtn, &QPushButton::clicked, &dlg, [lw](){
                            int r = lw->currentRow();
                            if (r >= 0 && r < lw->count() - 1) {
                                QListWidgetItem *it = lw->takeItem(r);
                                lw->insertItem(r + 1, it);
                                lw->setCurrentRow(r + 1);
                            }
                        });
                        QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

                        if (dlg.exec() == QDialog::Accepted) {
                            QStringList newList;
                            for (int i = 0; i < lw->count(); ++i)
                                newList << lw->item(i)->data(Qt::UserRole).toString();
                            saveShortcuts(newList);
                            (*rebuildMenu)();
                        }
                    });
                };

                (*rebuildMenu)();

                mMainWindow->webToolBar->insertWidget(mMainWindow->actionWebTools, shortcutsBtn);
            }
            //--------------- end Shortcuts dropdown ---------------//


            mHistoryBackMenu = new QMenu(mMainWindow);
            connect(mHistoryBackMenu, SIGNAL(aboutToShow()),this, SLOT(aboutToShowBackMenu()));
            connect(mHistoryBackMenu, SIGNAL(triggered(QAction *)), this, SLOT(openActionUrl(QAction *)));

            // setup history drop down menus
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            for (QObject* menuWidget : mMainWindow->actionWebBack->associatedObjects())
#else
            for (QWidget* menuWidget : mMainWindow->actionWebBack->associatedWidgets())
#endif
            {
                QToolButton *tb = qobject_cast<QToolButton*>(menuWidget);

                if (tb && !tb->menu())
                {
                    tb->setMenu(mHistoryBackMenu);
                    tb->setStyleSheet("QToolButton::menu-indicator { subcontrol-position: bottom left; }");
                }
            }

            mHistoryForwardMenu = new QMenu(mMainWindow);
            connect(mHistoryForwardMenu, SIGNAL(aboutToShow()), this, SLOT(aboutToShowForwardMenu()));
            connect(mHistoryForwardMenu, SIGNAL(triggered(QAction *)), this, SLOT(openActionUrl(QAction *)));

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            for (QObject* menuWidget : mMainWindow->actionWebForward->associatedObjects())
#else
            for (QWidget* menuWidget : mMainWindow->actionWebForward->associatedWidgets())
#endif
            {
                QToolButton *tb = qobject_cast<QToolButton*>(menuWidget);

                if (tb && !tb->menu())
                {
                    tb->setMenu(mHistoryForwardMenu);
                    tb->setStyleSheet("QToolButton { padding-right: 8px; }");
                }
            }

            mCurrentWebBrowser->currentTab()->load(currentUrl);
            mCurrentWebBrowser->tabWidget()->tabBar()->show();

            QObject::connect(
                mWebProfile, &QWebEngineProfile::downloadRequested,
                &mDownloadManagerWidget, &DownloadManagerWidget::downloadRequested);

            connect(mMainWindow->actionWebTools, &QAction::triggered, this, [this](){
                mToolsCurrentPalette->setVisible(mMainWindow->actionWebTools->isChecked());
            });
        }

        UBApplication::applicationController->setMirrorSourceWidget(mCurrentWebBrowser->currentTab());
        mMainWindow->switchToWebWidget();

        setupPalettes();
        screenLayoutChanged();

        bool mirroring = UBSettings::settings()->webShowPageImmediatelyOnMirroredScreen->get().toBool();
        UBApplication::mainWindow->actionWebShowHideOnDisplay->setChecked(mirroring);
        mToolsCurrentPalette->show();
    }

    if (mDownloadViewIsVisible)
    {
        mDownloadManagerWidget.show();
    }
}

UBEmbedParser *UBWebController::embedParser(const QWebEngineView* view) const
{
    return view->findChild<UBEmbedParser*>("UBEmbedParser");
}

void UBWebController::updateEmbeddableContent(const QWebEngineView *view) const
{
    QList<UBEmbedContent> list = getEmbeddedContent(view);

    if (mEmbedController)
    {
        mEmbedController->updateListOfEmbeddableContent(list);
    }
}

void UBWebController::show()
{
    webBrowserInstance();
}

QWidget *UBWebController::controlView() const
{
    return mBrowserWidget;
}

QWebEngineProfile *UBWebController::webProfile() const
{
    return mWebProfile;
}

QList<UBEmbedContent> UBWebController::getEmbeddedContent(const QWebEngineView *view) const
{
    UBEmbedParser* parser = embedParser(view);

    if (parser)
    {
        return parser->embeddedContent();
    }

    return QList<UBEmbedContent>();
}

BrowserWindow* UBWebController::browserWindow() const
{
    return mCurrentWebBrowser;
}

QWebEnginePage::PermissionPolicy UBWebController::hasFeaturePermission(const QUrl &securityOrigin, QWebEnginePage::Feature feature)
{
    QPair<QUrl,QWebEnginePage::Feature> featurePermission(securityOrigin, feature);

    if (mFeaturePermissions.contains(featurePermission))
    {
        return mFeaturePermissions[featurePermission];
    }

    return QWebEnginePage::PermissionUnknown;
}

void UBWebController::setFeaturePermission(const QUrl &securityOrigin, QWebEnginePage::Feature feature, QWebEnginePage::PermissionPolicy policy)
{
    QPair<QUrl,QWebEnginePage::Feature> featurePermission(securityOrigin, feature);
    mFeaturePermissions[featurePermission] = policy;
}

void UBWebController::injectScripts(QWebEngineView *view)
{
    // inject the QWebChannel interface and initialization script
    QFile js(":/qtwebchannel/qwebchannel.js");

    if (js.open(QIODevice::ReadOnly))
    {
        qDebug() << "Injecting qwebchannel.js";
        QString src = js.readAll();

        QFile asyncwrapper(UBPlatformUtils::applicationTemplateDirectory() + "/asyncAPI.js");

        if (asyncwrapper.open(QIODevice::ReadOnly))
        {
            src += asyncwrapper.readAll();
        }

        QWebEngineScript script;
        script.setName("qwebchannel");
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setSourceCode(src);
        view->page()->scripts().insert(script);
    }
}

void UBWebController::setSourceWidget(QWidget* pWidget)
{
    mBrowserWidget = pWidget;
    UBApplication::applicationController->setMirrorSourceWidget(pWidget);
}


void UBWebController::trap()
{
    mEmbedController->showEmbedDialog();
    activePageChanged();
}

void UBWebController::activePageChanged()
{
    if (mCurrentWebBrowser && mCurrentWebBrowser->currentTab())
    {
        WebView* view = mCurrentWebBrowser->currentTab();

        updateEmbeddableContent(view);

        if (mEmbedController)
        {
            mEmbedController->pageUrlChanged(view->url());
            mEmbedController->pageTitleChanged(view->title());

            connect(view, &QWebEngineView::urlChanged, mEmbedController, &UBEmbedController::pageUrlChanged);
            connect(view, &QWebEngineView::titleChanged, mEmbedController, &UBEmbedController::pageTitleChanged);
        }

        emit activeWebPageChanged(mCurrentWebBrowser->currentTab());
    }
}

void UBWebController::captureCurrentPage()
{
    QPixmap* pix;

    if (mCurrentWebBrowser
            && mCurrentWebBrowser->currentTab()
            && mCurrentWebBrowser->currentTab()->page())
    {
        WebView* view = mCurrentWebBrowser->currentTab();
        QWebEnginePage* page = view->page();
        QSize size = page->contentsSize().toSize();
        QPoint scrollPosition = page->scrollPosition().toPoint();
        QSize viewportSize = view->size();

        // pix is deleted at the final run of captureStripe
        pix = new QPixmap(size);
        QPointF pos(0, 0);

        // capture complete web page in stripes, starting at top left
        captureStripe(pos, viewportSize, pix, scrollPosition);
    }
}

void UBWebController::setupPalettes()
{
    if(!mToolsCurrentPalette)
    {
        mToolsCurrentPalette = new UBWebToolsPalette(UBApplication::mainWindow);
        UBApplication::boardController->paletteManager()->setCurrentWebToolsPalette(mToolsCurrentPalette);
#ifndef Q_OS_WIN
        if (UBPlatformUtils::hasVirtualKeyboard() && UBApplication::boardController->paletteManager()->mKeyboardPalette)
            connect(UBApplication::boardController->paletteManager()->mKeyboardPalette, SIGNAL(closed()),
                    UBApplication::boardController->paletteManager()->mKeyboardPalette, SLOT(onDeactivated()));
#endif

        connect(mMainWindow->actionCaptureWebContent, SIGNAL(triggered()), this, SLOT(trap()));
        connect(mMainWindow->actionWebCustomCapture, SIGNAL(triggered()), this, SLOT(customCapture()));
        connect(mMainWindow->actionWebWindowCapture, SIGNAL(triggered()), this, SLOT(captureWindow()));
        connect(mMainWindow->actionWebShowHideOnDisplay, SIGNAL(toggled(bool)), this, SLOT(toogleMirroring(bool)));

        mToolsCurrentPalette->hide();
        mToolsCurrentPalette->adjustSizeAndPosition();

        if (controlView())
        {
            int left = controlView()->width() - 20 - mToolsCurrentPalette->width();
            int top = (controlView()->height() - mToolsCurrentPalette->height()) / 2;
            mToolsCurrentPalette->setCustomPosition(true);
            mToolsCurrentPalette->move(left, top);
        }

        mMainWindow->actionWebTools->trigger();
    }
}


void UBWebController::captureWindow()
{
    captureCurrentPage();
}


void UBWebController::customCapture()
{
    mToolsCurrentPalette->setVisible(false);
    qApp->processEvents();

    UBCustomCaptureWindow customCaptureWindow(mCurrentWebBrowser);

    customCaptureWindow.show();

    if (customCaptureWindow.execute(getScreenPixmap()) == QDialog::Accepted)
    {
        QPixmap selectedPixmap = customCaptureWindow.getSelectedPixmap();
        emit imageCaptured(selectedPixmap, false, mCurrentWebBrowser->currentTab()->url());
    }

    mToolsCurrentPalette->setVisible(true);
}


void UBWebController::toogleMirroring(bool checked)
{
    UBApplication::applicationController->mirroringEnabled(checked);
}


QPixmap UBWebController::getScreenPixmap()
{
    return UBApplication::displayManager->grab(ScreenRole::Control);
}


void UBWebController::screenLayoutChanged()
{
    bool hasDisplay = (UBApplication::applicationController &&
                       UBApplication::displayManager &&
                       UBApplication::displayManager->hasDisplay());

    UBApplication::mainWindow->actionWebShowHideOnDisplay->setVisible(hasDisplay);
}


void UBWebController::closing()
{
    //NOOP
}


void UBWebController::adaptToolBar()
{
    bool highResolution = mMainWindow->width() > 1024;

    mMainWindow->actionWebReload->setVisible(highResolution);
    mMainWindow->actionStopLoading->setVisible(highResolution);
}


void UBWebController::showTabAtTop(bool attop)
{
    if (mCurrentWebBrowser)
        mCurrentWebBrowser->tabWidget()->setTabPosition(attop ? QTabWidget::North : QTabWidget::South);
}


QUrl UBWebController::guessUrlFromString(const QString &string)
{
    QString urlStr = string.trimmed();
    static const QRegularExpression test(QRegularExpression::anchoredPattern("^[a-zA-Z]+\\:.*"));

    // Check if it looks like a qualified URL. Try parsing it and see.
    QRegularExpressionMatch match = test.match(urlStr);
    bool hasSchema = match.hasMatch();
    if (hasSchema)
    {
        int dotCount = urlStr.count(".");

        if (dotCount == 0 && !urlStr.contains(".com"))
        {
            urlStr += ".com";
        }

        QUrl url = QUrl::fromEncoded(urlStr.toUtf8(), QUrl::TolerantMode);
        if (url.isValid())
        {
            return url;
        }
    }

    // Might be a file.
    if (QFile::exists(urlStr))
    {
        QFileInfo info(urlStr);
        return QUrl::fromLocalFile(info.absoluteFilePath());
    }

    // Might be a shorturl - try to detect the schema.
    if (!hasSchema)
    {
        QString schema = "http";

        QString guessed = schema + "://" + urlStr;

        int dotCount = guessed.count(".");

        if (dotCount == 0 && !urlStr.contains(".com"))
        {
            guessed += ".com";
        }

        QUrl url = QUrl::fromEncoded(guessed.toUtf8(), QUrl::TolerantMode);

        if (url.isValid())
            return url;
    }

    // Fall back to QUrl's own tolerant parser.
    QUrl url = QUrl::fromUserInput(string);

    // finally for cases where the user just types in a hostname add http
    if (url.scheme().isEmpty())
        url = QUrl::fromEncoded("http://" + string.toUtf8(), QUrl::TolerantMode);

    return url;
}

void UBWebController::tabCreated(WebView *webView)
{
    // create and attach an UBEmbedParser to the view
    if (!embedParser(webView))
    {
        UBEmbedParser* parser = new UBEmbedParser(webView);
        connect(webView, &QWebEngineView::loadProgress, this, [parser,webView](int progress){
            // Note: The loadFinished signal is not always emitted, but progress = 100 is.
            if (progress == 100)
            {
                qDebug() << "loadFinished";

                webView->page()->toHtml([parser](const QString &html) {
                    parser->parse(html);
                });

                // WistOpenboard fork: push the current OpenBoard stylus tool
                // into the just-loaded page's annotation overlay so drawing
                // works immediately without the user having to toggle tools.
                UBDrawingController *dc = UBDrawingController::drawingController();
                if (dc) {
                    UBStylusTool::Enum t = (UBStylusTool::Enum)dc->stylusTool();
                    QString jsTool = "none";
                    if (t == UBStylusTool::Pen)         jsTool = "pen";
                    else if (t == UBStylusTool::Marker) jsTool = "marker";
                    else if (t == UBStylusTool::Eraser) jsTool = "eraser";
                    QColor c = dc->currentToolColor();
                    qreal w = dc->currentToolWidth();
                    QString js = QString("if(window.__obSetTool)__obSetTool('%1','%2',%3);")
                                    .arg(jsTool, c.name(QColor::HexRgb))
                                    .arg(w);
                    webView->page()->runJavaScript(js);
                }
            }
        });

        connect(parser, &UBEmbedParser::parseResult, this, [this,webView](bool hasEmbeddedContent){
            onEmbedParsed(webView, hasEmbeddedContent);
        });
    }
}


void UBWebController::loadUrl(const QUrl& url)
{
    UBApplication::applicationController->showInternet();
    if (UBSettings::settings()->webUseExternalBrowser->get().toBool())
    {
        QDesktopServices::openUrl(url);
    }
    else
    {
        WebView* view = mCurrentWebBrowser->tabWidget()->createTab();
        view->load(url);
    }
}


WebView* UBWebController::createNewTab()
{
    if (mCurrentWebBrowser)
    {
        UBApplication::applicationController->showInternet();
        return mCurrentWebBrowser->tabWidget()->createTab();
    }

    return nullptr;
}


void UBWebController::copy()
{
    if (mCurrentWebBrowser && mCurrentWebBrowser->currentTab())
    {
        WebView* webView = mCurrentWebBrowser->currentTab();
        QAction *act = webView->pageAction(QWebEnginePage::Copy);
        if(act)
            act->trigger();
    }
}


void UBWebController::paste()
{
    if (mCurrentWebBrowser && mCurrentWebBrowser->currentTab())
    {
        WebView* webView = mCurrentWebBrowser->currentTab();
        QAction *act = webView->pageAction(QWebEnginePage::Paste);
        if(act)
            act->trigger();
    }
}


void UBWebController::cut()
{
    if (mCurrentWebBrowser && mCurrentWebBrowser->currentTab())
    {
        WebView* webView = mCurrentWebBrowser->currentTab();
        QAction *act = webView->pageAction(QWebEnginePage::Cut);
        if(act)
            act->trigger();
    }
}

void UBWebController::aboutToShowBackMenu()
{
    mHistoryBackMenu->clear();

    if (!mCurrentWebBrowser->currentTab())
        return;

    QWebEngineHistory *history = mCurrentWebBrowser->currentTab()->history();

    int historyCount = history->count();
    int historyLimit = history->backItems(historyCount).count() - UBSettings::settings()->historyLimit->get().toReal();
    if (historyLimit < 0)
        historyLimit = 0;

    for (int i = history->backItems(historyCount).count() - 1; i >= historyLimit; --i)
    {
        QWebEngineHistoryItem item = history->backItems(historyCount).at(i);

        QAction *action = new QAction(this);
        action->setData(-1*(historyCount-i-1));

        // TODO fetch icon or keep a cache somewhere
//        if (!item.iconUrl().isEmpty())
//            action->setIcon(item.icon());
        action->setText(item.title().isEmpty() ? item.url().toString() : item.title());
        mHistoryBackMenu->addAction(action);
    }
}

void UBWebController::aboutToShowForwardMenu()
{
    mHistoryForwardMenu->clear();

    if (!mCurrentWebBrowser->currentTab())
        return;

    QWebEngineHistory *history = mCurrentWebBrowser->currentTab()->history();
    int historyCount = history->count();

    int historyLimit = history->forwardItems(historyCount).count();
    if (historyLimit > UBSettings::settings()->historyLimit->get().toReal())
        historyLimit = UBSettings::settings()->historyLimit->get().toReal();

    for (int i = 0; i < historyLimit; ++i)
    {
        QWebEngineHistoryItem item = history->forwardItems(historyCount).at(i);

        QAction *action = new QAction(this);
        action->setData(historyCount-i);

        // TODO fetch icon or keep a cache somewhere
//        if (!item.iconUrl().isEmpty())
//            action->setIcon(item.icon());
        action->setText(item.title().isEmpty() ? item.url().toString() : item.title());
        mHistoryForwardMenu->addAction(action);
    }
}

void UBWebController::openActionUrl(QAction *action)
{
    QWebEngineHistory *history = mCurrentWebBrowser->currentTab()->history();

    int offset = action->data().toInt();

    if (offset < 0)
        history->goToItem(history->backItems(-1*offset).first());
    else if (offset > 0)
        history->goToItem(history->forwardItems(history->count() - offset + 1).back());
 }

void UBWebController::onEmbedParsed(QWebEngineView *view, bool hasEmbeddedContent)
{
    // check: is this for current tab?
    if (view == mBrowserWidget)
    {
        // enable/disable embed button
        UBApplication::mainWindow->actionWebOEmbed->setEnabled(hasEmbeddedContent);

        updateEmbeddableContent(view);
    }
}

void UBWebController::onOpenTutorial()
{
    loadUrl(QUrl(UBSettings::settings()->tutorialUrl->get().toString()));
}

void UBWebController::onHintsAndTips()
{
    UBApplication::boardController->paletteManager()->tipsPalette()->show();
}

void UBWebController::captureStripe(QPointF pos, QSize size, QPixmap* pix, QPointF scrollPosition)
{
    WebView* view = mCurrentWebBrowser->currentTab();
    QString scrollto = QString("window.scrollTo(%1,%2)").arg(pos.x()).arg(pos.y());
    view->page()->runJavaScript(scrollto, [this,pos,size,pix,scrollPosition](const QVariant&){
        // we need some time for rendering - there is no signal when finished, so just wait
        QTimer::singleShot(100, this, [this,pos,size,pix,scrollPosition](){
            WebView* view = mCurrentWebBrowser->currentTab();
            QPixmap stripe(size);

            {
                // render stripe, local block to release QPainter
                QPainter p(&stripe);
                view->render(&p);
            }
            {
                // copy rendered stripe to pix
                QPointF actualPos = view->page()->scrollPosition();
                QRectF target(actualPos, size);
                QPainter p(pix);
                p.drawPixmap(target.toRect(), stripe);
            }

            QPointF nextPos = pos + QPointF(0, size.height() / view->zoomFactor());

            if (nextPos.y() < pix->height() / view->zoomFactor())
            {
                // capture next stripe
                captureStripe(nextPos, size, pix, scrollPosition);
            }
            else
            {
                QPixmap captured = *pix;
                // allocated at captureCurrentPage()
                delete pix;
                emit imageCaptured(captured, true, view->url());

                // scroll back to initial position
                QString scrollto = QString("window.scrollTo(%1,%2)").arg(scrollPosition.x()).arg(scrollPosition.y());
                view->page()->runJavaScript(scrollto);
            }
        });
    });
}

UBUserAgentInterceptor::UBUserAgentInterceptor(const QByteArray &alternativeUserAgent, QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent), mAlternativeUserAgent(alternativeUserAgent)
{
    QStringList userAgentDomains = UBSettings::settings()->alternativeUserAgentDomains->get().toStringList();

    // convert patterns to regular expressions
    for (QString& pattern : userAgentDomains) {
        // escape dots
        pattern.replace(".", "\\.");

        // replace wildcards
        pattern.replace("*", "\\w*");
    }

    // set patterns in brachets, join with | and anchor at end of string with $
    QString domains = "(" + userAgentDomains.join(")|(") + ")$";

    mDomainMatcher.setPattern(domains);

    // handle invalid pattern
    if (!mDomainMatcher.isValid())
    {
        // this works!
        qDebug() << "Wrong pattern syntax " << domains << "fallback to google.*";
        mDomainMatcher.setPattern("google\\.\\w*$");
    }

    mDomainMatcher.optimize();
}

void UBUserAgentInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    QUrl url = info.requestUrl();

    if (mDomainMatcher.match(url.host()).hasMatch())
    {
        info.setHttpHeader("User-Agent", mAlternativeUserAgent);
    }
}
