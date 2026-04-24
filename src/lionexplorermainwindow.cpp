/*
 * SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz19@gmail.com>
 * SPDX-FileCopyrightText: 2006 Stefan Monov <logixoul@gmail.com>
 * SPDX-FileCopyrightText: 2006 Cvetoslav Ludmiloff <ludmiloff@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lionexplorermainwindow.h"

#include "admin/workerintegration.h"
#include "lionexplorer_generalsettings.h"
#include "lionexplorerbookmarkhandler.h"
#include "lionexplorercontextmenu.h"
#include "lionexplorerdockwidget.h"
#include "lionexplorermainwindowadaptor.h"
#include "lionexplorernavigatorswidgetaction.h"
#include "lionexplorernewfilemenu.h"
#include "lionexplorerplacesmodelsingleton.h"
#include "lionexplorerrecenttabsmenu.h"
#include "lionexplorertabpage.h"
#include "lionexplorerurlnavigatorscontroller.h"
#include "lionexplorerviewcontainer.h"
#include "global.h"
#include "middleclickactioneventfilter.h"
#include "panels/folders/folderspanel.h"
#include "panels/places/placespanel.h"
#include "panels/terminal/terminalpanel.h"
#include "search/lionexplorerquery.h"
#include "selectionmode/actiontexthelper.h"
#if KIO_VERSION >= QT_VERSION_CHECK(6, 24, 0)
#include "servicemenushortcutmanager.h"
#endif
#include "settings/lionexplorersettingsdialog.h"
#include "statusbar/diskspaceusagemenu.h"
#include "statusbar/lionexplorerstatusbar.h"
#include "views/lionexplorernewfilemenuobserver.h"
#include "views/lionexplorerremoteencoding.h"
#include "views/lionexplorerviewactionhandler.h"
#include "views/draganddrophelper.h"
#include "views/viewproperties.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KAuthorized>
#include <KColorSchemeManager>
#include <KColorSchemeMenu>
#include <KConfig>
#include <KConfigGui>
#include <KDesktopFile>
#include <KDialogJobUiDelegate>
#include <KDualAction>
#include <KFileItemListProperties>
#include <KIO/CommandLauncherJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenFileManagerWindowJob>
#include <KIO/OpenUrlJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KProtocolInfo>
#include <KProtocolManager>
#include <KRecentFilesAction>
#include <KRuntimePlatform>
#include <KShell>
#include <KShortcutsDialog>
#include <KStandardAction>
#include <KSycoca>
#include <KTerminalLauncherJob>
#include <KToggleAction>
#include <KToolBar>
#include <KToolBarPopupAction>
#include <KUrlComboBox>
#include <KUrlNavigator>
#include <KWindowSystem>
#include <KXMLGUIFactory>

#include <kwidgetsaddons_version.h>

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDomDocument>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenuBar>
#include <QPushButton>
#include <QShowEvent>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QtConcurrentRun>
#include <lionexplorerdebug.h>

#include <algorithm>

#if HAVE_X11
#include <KStartupInfo>
#endif

namespace
{
// Used for GeneralSettings::version() to determine whether
// an updated version of Lion Explorer is running, so as to migrate
// removed/renamed ...etc config entries; increment it in such
// cases
const int CurrentLion ExplorerVersion = 202;
// The maximum number of entries in the back/forward popup menu
const int MaxNumberOfNavigationentries = 12;
// The maximum number of "Go to Tab" shortcuts
const int MaxActivateTabShortcuts = 9;
}

Lion ExplorerMainWindow::Lion ExplorerMainWindow()
    : KXmlGuiWindow(nullptr)
    , m_newFileMenu(nullptr)
    , m_tabWidget(nullptr)
    , m_activeViewContainer(nullptr)
    , m_actionHandler(nullptr)
    , m_remoteEncoding(nullptr)
    , m_settingsDialog()
    , m_bookmarkHandler(nullptr)
    , m_disabledActionNotifier(nullptr)
    , m_lastHandleUrlOpenJob(nullptr)
    , m_terminalPanel(nullptr)
    , m_placesPanel(nullptr)
    , m_tearDownFromPlacesRequested(false)
    , m_backAction(nullptr)
    , m_forwardAction(nullptr)
    , m_splitViewAction(nullptr)
    , m_splitViewMenuAction(nullptr)
    , m_diskSpaceUsageMenu(nullptr)
    , m_sessionSaveTimer(nullptr)
    , m_sessionSaveWatcher(nullptr)
    , m_sessionSaveScheduled(false)
{
    Q_INIT_RESOURCE(lionexplorer);

    new MainWindowAdaptor(this);

#ifndef Q_OS_WIN
    setWindowFlags(Qt::WindowContextHelpButtonHint);
#endif
    setComponentName(QStringLiteral("lionexplorer"), QGuiApplication::applicationDisplayName());
    setObjectName(QStringLiteral("Lion Explorer#"));

    setStateConfigGroup("State");

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    new KColorSchemeManager(this); // Sets a sensible color scheme which fixes unreadable icons and text on Windows.
#endif

    connect(&Lion ExplorerNewFileMenuObserver::instance(), &Lion ExplorerNewFileMenuObserver::errorMessage, this, &Lion ExplorerMainWindow::showErrorMessage);

    KIO::FileUndoManager *undoManager = KIO::FileUndoManager::self();
    undoManager->setUiInterface(new UndoUiInterface());

    connect(undoManager, &KIO::FileUndoManager::undoAvailable, this, &Lion ExplorerMainWindow::slotUndoAvailable);
    connect(undoManager, &KIO::FileUndoManager::undoTextChanged, this, &Lion ExplorerMainWindow::slotUndoTextChanged);
#if KIO_VERSION >= QT_VERSION_CHECK(6, 17, 0)
    connect(undoManager, &KIO::FileUndoManager::redoAvailable, this, &Lion ExplorerMainWindow::slotRedoAvailable);
    connect(undoManager, &KIO::FileUndoManager::redoTextChanged, this, &Lion ExplorerMainWindow::slotRedoTextChanged);
#endif
    connect(undoManager, &KIO::FileUndoManager::jobRecordingStarted, this, &Lion ExplorerMainWindow::clearStatusBar);
    connect(undoManager, &KIO::FileUndoManager::jobRecordingFinished, this, &Lion ExplorerMainWindow::showCommand);

    const bool firstRun = (GeneralSettings::version() < 200);
    if (firstRun) {
        GeneralSettings::setViewPropsTimestamp(QDateTime::currentDateTime());
    }

    setAcceptDrops(true);

    auto *navigatorsWidgetAction = new Lion ExplorerNavigatorsWidgetAction(this);
    actionCollection()->addAction(QStringLiteral("url_navigators"), navigatorsWidgetAction);
    m_tabWidget = new Lion ExplorerTabWidget(navigatorsWidgetAction, this);
    m_tabWidget->setObjectName("tabWidget");
    connect(m_tabWidget, &Lion ExplorerTabWidget::activeViewChanged, this, &Lion ExplorerMainWindow::activeViewChanged);
    connect(m_tabWidget, &Lion ExplorerTabWidget::tabCountChanged, this, &Lion ExplorerMainWindow::tabCountChanged);
    connect(m_tabWidget, &Lion ExplorerTabWidget::currentUrlChanged, this, &Lion ExplorerMainWindow::updateWindowTitle);
    setCentralWidget(m_tabWidget);

    m_actionTextHelper = new SelectionMode::ActionTextHelper(this);
    setupActions();

    m_actionHandler = new Lion ExplorerViewActionHandler(actionCollection(), m_actionTextHelper, this);
    connect(m_actionHandler, &Lion ExplorerViewActionHandler::actionBeingHandled, this, &Lion ExplorerMainWindow::clearStatusBar);
    connect(m_actionHandler, &Lion ExplorerViewActionHandler::createDirectoryTriggered, this, &Lion ExplorerMainWindow::createDirectory);
    connect(m_actionHandler, &Lion ExplorerViewActionHandler::createFileTriggered, this, &Lion ExplorerMainWindow::createFile);
    connect(m_actionHandler, &Lion ExplorerViewActionHandler::selectionModeChangeTriggered, this, &Lion ExplorerMainWindow::slotSetSelectionMode);

    QAction *newDirAction = actionCollection()->action(QStringLiteral("create_dir"));
    Q_ASSERT(newDirAction);
    m_newFileMenu->setNewFolderShortcutAction(newDirAction);

    QAction *newFileAction = actionCollection()->action(QStringLiteral("create_file"));
    Q_ASSERT(newFileAction);
    m_newFileMenu->setNewFileShortcutAction(newFileAction);

    m_remoteEncoding = new Lion ExplorerRemoteEncoding(this, m_actionHandler);
    connect(this, &Lion ExplorerMainWindow::urlChanged, m_remoteEncoding, &Lion ExplorerRemoteEncoding::slotAboutToOpenUrl);

    m_disabledActionNotifier = new DisabledActionNotifier(this);
    connect(m_disabledActionNotifier, &DisabledActionNotifier::disabledActionTriggered, this, [this](const QAction *, const QString &reason) {
        m_activeViewContainer->showMessage(reason, KMessageWidget::Warning);
    });

    setupDockWidgets();

#if KIO_VERSION >= QT_VERSION_CHECK(6, 24, 0)
    m_serviceMenuShortcutManager = new ServiceMenuShortcutManager(actionCollection(), this);
#endif
    setupFileItemActions();

    const bool usePhoneUi{KRuntimePlatform::runtimePlatform().contains(QLatin1String("phone"))};
    setupGUI(Save | Create | ToolBar, usePhoneUi ? QStringLiteral("lionexploreruiforphones.rc") : QString() /* load the default lionexplorerui.rc file */);
    stateChanged(QStringLiteral("new_file"));

    QClipboard *clipboard = QApplication::clipboard();
    connect(clipboard, &QClipboard::dataChanged, this, &Lion ExplorerMainWindow::updatePasteAction);

    QAction *toggleFilterBarAction = actionCollection()->action(QStringLiteral("toggle_filter"));
    toggleFilterBarAction->setChecked(GeneralSettings::filterBar());

    if (firstRun) {
        menuBar()->setVisible(false);

        if (usePhoneUi) {
            Q_ASSERT(qobject_cast<QDockWidget *>(m_placesPanel->parent()));
            m_placesPanel->parentWidget()->hide();
            auto settings = GeneralSettings::self();
            settings->setShowZoomSlider(false); // Zooming can be done with pinch gestures instead and we are short on horizontal space.
            settings->setRenameInline(false); // This works around inline renaming currently not working well with virtual keyboards.
            settings->save(); // Otherwise the RenameInline setting is not picked up for the first time Lion Explorer is used.
        }
    }

    QAction *showMenuBarAction = actionCollection()->action(KStandardAction::name(KStandardAction::ShowMenubar));
    menuBar()->setVisible(showMenuBarAction->isChecked());

    auto hamburgerMenu = static_cast<KHamburgerMenu *>(actionCollection()->action(KStandardAction::name(KStandardAction::HamburgerMenu)));
    hamburgerMenu->setMenuBar(menuBar());
    hamburgerMenu->setShowMenuBarAction(showMenuBarAction);
    connect(hamburgerMenu, &KHamburgerMenu::aboutToShowMenu, this, &Lion ExplorerMainWindow::updateHamburgerMenu);
    hamburgerMenu->hideActionsOf(toolBar());
    if (GeneralSettings::version() < 201 && !toolBar()->actions().contains(hamburgerMenu)) {
        addHamburgerMenuToToolbar();
    }

    updateAllowedToolbarAreas();
    updateNavigatorsBackground();

    // enable middle-click on back/forward/up to open in a new tab
    auto *middleClickEventFilter = new MiddleClickActionEventFilter(this);
    connect(middleClickEventFilter, &MiddleClickActionEventFilter::actionMiddleClicked, this, &Lion ExplorerMainWindow::slotToolBarActionMiddleClicked);
    toolBar()->installEventFilter(middleClickEventFilter);

    setupWhatsThis();

    connect(KSycoca::self(), &KSycoca::databaseChanged, this, &Lion ExplorerMainWindow::updateOpenPreferredSearchToolAction);

    QTimer::singleShot(0, this, &Lion ExplorerMainWindow::updateOpenPreferredSearchToolAction);

    m_serviceMenuConfigWatcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kservicemenurc")));
    connect(m_serviceMenuConfigWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup & /*group*/, const QByteArrayList & /*names*/) {
        setupFileItemActions();
    });
    connect(GeneralSettings::self(), &GeneralSettings::splitViewChanged, this, &Lion ExplorerMainWindow::slotSplitViewChanged);
    connect(GeneralSettings::self(), &GeneralSettings::tabBarChanged, this, &Lion ExplorerMainWindow::slotTabBarChanged);
}

Lion ExplorerMainWindow::~Lion ExplorerMainWindow()
{
    // This fixes a crash on Wayland when closing the mainwindow while another dialog is open.
    disconnect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &Lion ExplorerMainWindow::updatePasteAction);

    // This fixes a crash in lionexplorermainwindowtest where the connection below fires even though the KMainWindow destructor of this object is already running.
    Q_ASSERT(qobject_cast<Lion ExplorerDockWidget *>(m_placesPanel->parent()));
    disconnect(static_cast<Lion ExplorerDockWidget *>(m_placesPanel->parent()),
               &Lion ExplorerDockWidget::visibilityChanged,
               this,
               &Lion ExplorerMainWindow::slotPlacesPanelVisibilityChanged);
}

QVector<Lion ExplorerViewContainer *> Lion ExplorerMainWindow::viewContainers() const
{
    QVector<Lion ExplorerViewContainer *> viewContainers;

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        Lion ExplorerTabPage *tabPage = m_tabWidget->tabPageAt(i);

        viewContainers << tabPage->primaryViewContainer();
        if (tabPage->splitViewEnabled()) {
            viewContainers << tabPage->secondaryViewContainer();
        }
    }
    return viewContainers;
}

void Lion ExplorerMainWindow::openDirectories(const QList<QUrl> &dirs, bool splitView)
{
    m_tabWidget->openDirectories(dirs, splitView);
}

void Lion ExplorerMainWindow::openDirectories(const QStringList &dirs, bool splitView)
{
    openDirectories(QUrl::fromStringList(dirs), splitView);
}

void Lion ExplorerMainWindow::openFiles(const QList<QUrl> &files, bool splitView)
{
    m_tabWidget->openFiles(files, splitView);
}

bool Lion ExplorerMainWindow::isFoldersPanelEnabled() const
{
    return actionCollection()->action(QStringLiteral("show_folders_panel"))->isChecked();
}

bool Lion ExplorerMainWindow::isInformationPanelEnabled() const
{
#if HAVE_BALOO
    return actionCollection()->action(QStringLiteral("show_information_panel"))->isChecked();
#else
    return false;
#endif
}

bool Lion ExplorerMainWindow::isSplitViewEnabledInCurrentTab() const
{
    return m_tabWidget->currentTabPage()->splitViewEnabled();
}

void Lion ExplorerMainWindow::openFiles(const QStringList &files, bool splitView)
{
    openFiles(QUrl::fromStringList(files), splitView);
}

void Lion ExplorerMainWindow::activateWindow(const QString &activationToken)
{
    window()->setAttribute(Qt::WA_NativeWindow, true);

    if (KWindowSystem::isPlatformWayland()) {
        KWindowSystem::setCurrentXdgActivationToken(activationToken);
    } else if (KWindowSystem::isPlatformX11()) {
#if HAVE_X11
        KStartupInfo::setNewStartupId(window()->windowHandle(), activationToken.toUtf8());
#endif
    }

    KWindowSystem::activateWindow(window()->windowHandle());
}

bool Lion ExplorerMainWindow::isActiveWindow()
{
    return window()->isActiveWindow();
}

void Lion ExplorerMainWindow::showCommand(CommandType command)
{
    Lion ExplorerStatusBar *statusBar = m_activeViewContainer->statusBar();
    switch (command) {
    case KIO::FileUndoManager::Copy:
        statusBar->setText(i18nc("@info:status", "Successfully copied."));
        break;
    case KIO::FileUndoManager::Move:
        statusBar->setText(i18nc("@info:status", "Successfully moved."));
        break;
    case KIO::FileUndoManager::Link:
        statusBar->setText(i18nc("@info:status", "Successfully linked."));
        break;
    case KIO::FileUndoManager::Trash:
        statusBar->setText(i18nc("@info:status", "Successfully moved to trash."));
        break;
    case KIO::FileUndoManager::Rename:
        statusBar->setText(i18nc("@info:status", "Successfully renamed."));
        break;

    case KIO::FileUndoManager::Mkdir:
        statusBar->setText(i18nc("@info:status", "Created folder."));
        break;

    default:
        break;
    }
}

void Lion ExplorerMainWindow::pasteIntoFolder()
{
    m_activeViewContainer->view()->pasteIntoFolder();
}

void Lion ExplorerMainWindow::changeUrl(const QUrl &url)
{
    if (!KProtocolManager::supportsListing(url)) {
        // The URL navigator only checks for validity, not
        // if the URL can be listed. An error message is
        // shown due to Lion ExplorerViewContainer::restoreView().
        return;
    }

    m_activeViewContainer->setUrl(url);
    updateFileAndEditActions();
    updatePasteAction();
    updateViewActions();
    updateGoActions();
    m_diskSpaceUsageMenu->setUrl(url);

    // will signal used urls to activities manager, too
    m_recentFiles->addUrl(url, QString(), "inode/directory");

    Q_EMIT urlChanged(url);
}

void Lion ExplorerMainWindow::slotTerminalDirectoryChanged(const QUrl &url)
{
    if (m_tearDownFromPlacesRequested && url == QUrl::fromLocalFile(QDir::homePath())) {
        m_placesPanel->proceedWithTearDown();
        m_tearDownFromPlacesRequested = false;
    }

    m_activeViewContainer->setGrabFocusOnUrlChange(false);
    changeUrl(url);
    m_activeViewContainer->setGrabFocusOnUrlChange(true);
}

void Lion ExplorerMainWindow::slotEditableStateChanged(bool editable)
{
    KToggleAction *editableLocationAction = static_cast<KToggleAction *>(actionCollection()->action(QStringLiteral("editable_location")));
    editableLocationAction->setChecked(editable);
}

void Lion ExplorerMainWindow::slotSelectionChanged(const KFileItemList &selection)
{
    updateFileAndEditActions();

    if (m_fileItemActions) {
        m_fileItemActions->setItemListProperties(KFileItemListProperties(selection));
    }

    const int selectedUrlsCount = m_tabWidget->currentTabPage()->selectedItemsCount();

    QAction *compareFilesAction = actionCollection()->action(QStringLiteral("compare_files"));
    if (selectedUrlsCount == 2) {
        compareFilesAction->setEnabled(isKompareInstalled());
    } else {
        compareFilesAction->setEnabled(false);
    }

    Q_EMIT selectionChanged(selection);
}

void Lion ExplorerMainWindow::updateHistory()
{
    const KUrlNavigator *urlNavigator = m_activeViewContainer->urlNavigatorInternalWithHistory();
    const int index = urlNavigator->historyIndex();

    QAction *backAction = actionCollection()->action(KStandardAction::name(KStandardAction::Back));
    if (backAction) {
        backAction->setToolTip(i18nc("@info", "Go back"));
        backAction->setWhatsThis(i18nc("@info:whatsthis go back", "Return to the previously viewed folder."));
        backAction->setEnabled(index < urlNavigator->historySize() - 1);
    }

    QAction *forwardAction = actionCollection()->action(KStandardAction::name(KStandardAction::Forward));
    if (forwardAction) {
        forwardAction->setToolTip(i18nc("@info", "Go forward"));
        forwardAction->setWhatsThis(xi18nc("@info:whatsthis go forward", "This undoes a <interface>Go|Back</interface> action."));
        forwardAction->setEnabled(index > 0);
    }
}

void Lion ExplorerMainWindow::updateFilterBarAction(bool show)
{
    QAction *toggleFilterBarAction = actionCollection()->action(QStringLiteral("toggle_filter"));
    toggleFilterBarAction->setChecked(show);
}

void Lion ExplorerMainWindow::openNewMainWindow()
{
    Lion Explorer::openNewWindow({m_activeViewContainer->url()}, this);
}

void Lion ExplorerMainWindow::openNewActivatedTab()
{
    // keep browsers compatibility, new tab is always after last one
    auto openNewTabAfterLastTabConfigured = GeneralSettings::openNewTabAfterLastTab();
    GeneralSettings::setOpenNewTabAfterLastTab(true);
    m_tabWidget->openNewActivatedTab();
    GeneralSettings::setOpenNewTabAfterLastTab(openNewTabAfterLastTabConfigured);
}

void Lion ExplorerMainWindow::addToPlaces()
{
    QUrl url;
    QString name;

    // If nothing is selected, act on the current dir
    if (m_activeViewContainer->view()->selectedItems().isEmpty()) {
        url = m_activeViewContainer->url();
        name = m_activeViewContainer->placesText();
    } else {
        const auto dirToAdd = m_activeViewContainer->view()->selectedItems().first();
        url = dirToAdd.url();
        name = dirToAdd.name();
    }
    if (url.isValid()) {
        QString icon;
        if (isSearchUrl(url)) {
            icon = QStringLiteral("folder-saved-search-symbolic");
        } else {
            icon = KIO::iconNameForUrl(url);
        }
        Lion ExplorerPlacesModelSingleton::instance().placesModel()->addPlace(name, url, icon);
    }
}

Lion ExplorerTabPage *Lion ExplorerMainWindow::openNewTab(const QUrl &url)
{
    return m_tabWidget->openNewTab(url, QUrl());
}

void Lion ExplorerMainWindow::openNewTabAndActivate(const QUrl &url)
{
    m_tabWidget->openNewActivatedTab(url, QUrl());
}

void Lion ExplorerMainWindow::openNewWindow(const QUrl &url)
{
    Lion Explorer::openNewWindow({url}, this);
}

void Lion ExplorerMainWindow::slotSplitViewChanged()
{
    m_tabWidget->currentTabPage()->setSplitViewEnabled(GeneralSettings::splitView(), WithAnimation);
    updateSplitActions();
}

void Lion ExplorerMainWindow::slotTabBarChanged()
{
    m_tabWidget->setTabBarAutoHide(!GeneralSettings::alwaysShowTabBar());
    m_tabWidget->tabBar()->setTabsClosable(GeneralSettings::showCloseButtonOnTabs());
}

void Lion ExplorerMainWindow::openInNewTab()
{
    const KFileItemList &list = m_activeViewContainer->view()->selectedItems();
    bool tabCreated = false;

    for (const KFileItem &item : list) {
        const QUrl &url = Lion ExplorerView::openItemAsFolderUrl(item);
        if (!url.isEmpty()) {
            openNewTab(url);
            tabCreated = true;
        }
    }

    // if no new tab has been created from the selection
    // open the current directory in a new tab
    if (!tabCreated) {
        openNewTab(m_activeViewContainer->url());
    }
}

void Lion ExplorerMainWindow::openInNewWindow()
{
    QUrl newWindowUrl;

    const KFileItemList list = m_activeViewContainer->view()->selectedItems();
    if (list.isEmpty()) {
        newWindowUrl = m_activeViewContainer->url();
    } else if (list.count() == 1) {
        const KFileItem &item = list.first();
        newWindowUrl = Lion ExplorerView::openItemAsFolderUrl(item);
    }

    if (!newWindowUrl.isEmpty()) {
        Lion Explorer::openNewWindow({newWindowUrl}, this);
    }
}

void Lion ExplorerMainWindow::openInSplitView(const QUrl &url)
{
    QUrl newSplitViewUrl = url;

    if (newSplitViewUrl.isEmpty()) {
        const KFileItemList list = m_activeViewContainer->view()->selectedItems();
        if (list.count() == 1) {
            const KFileItem &item = list.first();
            newSplitViewUrl = Lion ExplorerView::openItemAsFolderUrl(item);
        }
    }

    if (newSplitViewUrl.isEmpty()) {
        return;
    }

    Lion ExplorerTabPage *tabPage = m_tabWidget->currentTabPage();
    if (tabPage->splitViewEnabled()) {
        tabPage->switchActiveView();
        tabPage->activeViewContainer()->setUrl(newSplitViewUrl);
    } else {
        tabPage->setSplitViewEnabled(true, WithAnimation, newSplitViewUrl);
        updateViewActions();
    }
}

void Lion ExplorerMainWindow::showTarget()
{
    const KFileItem link = m_activeViewContainer->view()->selectedItems().at(0);
    const QUrl destinationUrl = link.url().resolved(QUrl(link.linkDest()));

    auto job = KIO::stat(destinationUrl, KIO::StatJob::SourceSide, KIO::StatNoDetails);

    connect(job, &KJob::finished, this, [this, destinationUrl](KJob *job) {
        KIO::StatJob *statJob = static_cast<KIO::StatJob *>(job);

        if (statJob->error()) {
            m_activeViewContainer->showMessage(job->errorString(), KMessageWidget::Error);
        } else {
            KIO::highlightInFileManager({destinationUrl});
        }
    });
}

bool Lion ExplorerMainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        const QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Space && m_activeViewContainer->view()->handleSpaceAsNormalKey()) {
            event->accept();
            return true;
        }
    }

    return KXmlGuiWindow::event(event);
}

void Lion ExplorerMainWindow::showEvent(QShowEvent *event)
{
    KXmlGuiWindow::showEvent(event);

    if (!event->spontaneous() && m_activeViewContainer) {
        m_activeViewContainer->view()->setFocus();
    }
}

void Lion ExplorerMainWindow::closeEvent(QCloseEvent *event)
{
    // Find out if Lion Explorer is closed directly by the user or
    // by the session manager because the session is closed
    bool closedByUser = true;
    if (qApp->isSavingSession()) {
        closedByUser = false;
    }

    if (m_tabWidget->count() > 1 && GeneralSettings::confirmClosingMultipleTabs() && !GeneralSettings::rememberOpenedTabs() && closedByUser) {
        // Ask the user if he really wants to quit and close all tabs.
        // Open a confirmation dialog with 3 buttons:
        // QDialogButtonBox::Yes    -> Quit
        // QDialogButtonBox::No     -> Close only the current tab
        // QDialogButtonBox::Cancel -> do nothing
        QDialog *dialog = new QDialog(this, Qt::Dialog);
        dialog->setWindowTitle(i18nc("@title:window", "Confirmation"));
        dialog->setModal(true);
        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
        KGuiItem::assign(buttons->button(QDialogButtonBox::Yes),
                         KGuiItem(i18nc("@action:button 'Quit Lion Explorer' button", "&Quit %1", QGuiApplication::applicationDisplayName()),
                                  QIcon::fromTheme(QStringLiteral("application-exit"))));
        KGuiItem::assign(buttons->button(QDialogButtonBox::No), KGuiItem(i18n("C&lose Current Tab"), QIcon::fromTheme(QStringLiteral("tab-close"))));
        KGuiItem::assign(buttons->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());
        buttons->button(QDialogButtonBox::Yes)->setDefault(true);

        bool doNotAskAgainCheckboxResult = false;

        const auto result = KMessageBox::createKMessageBox(dialog,
                                                           buttons,
                                                           QMessageBox::Warning,
                                                           i18n("You have multiple tabs open in this window, are you sure you want to quit?"),
                                                           QStringList(),
                                                           i18n("Do not ask again"),
                                                           &doNotAskAgainCheckboxResult,
                                                           KMessageBox::Notify);

        if (doNotAskAgainCheckboxResult) {
            GeneralSettings::setConfirmClosingMultipleTabs(false);
        }

        switch (result) {
        case QDialogButtonBox::Yes:
            // Quit
            break;
        case QDialogButtonBox::No:
            // Close only the current tab
            m_tabWidget->closeTab();
            Q_FALLTHROUGH();
        default:
            event->ignore();
            return;
        }
    }

    if (m_terminalPanel && m_terminalPanel->hasProgramRunning() && GeneralSettings::confirmClosingTerminalRunningProgram() && closedByUser) {
        // Ask if the user really wants to quit Lion Explorer with a program that is still running in the Terminal panel
        // Open a confirmation dialog with 3 buttons:
        // QDialogButtonBox::Yes    -> Quit
        // QDialogButtonBox::No     -> Show Terminal Panel
        // QDialogButtonBox::Cancel -> do nothing
        QDialog *dialog = new QDialog(this, Qt::Dialog);
        dialog->setWindowTitle(i18nc("@title:window", "Confirmation"));
        dialog->setModal(true);
        auto standardButtons = QDialogButtonBox::Yes | QDialogButtonBox::Cancel;
        if (!m_terminalPanel->isVisible()) {
            standardButtons |= QDialogButtonBox::No;
        }
        QDialogButtonBox *buttons = new QDialogButtonBox(standardButtons);
        KGuiItem::assign(buttons->button(QDialogButtonBox::Yes), KStandardGuiItem::quit());
        if (!m_terminalPanel->isVisible()) {
            KGuiItem::assign(buttons->button(QDialogButtonBox::No), KGuiItem(i18n("Show &Terminal Panel"), QIcon::fromTheme(QStringLiteral("dialog-scripts"))));
        }
        KGuiItem::assign(buttons->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());

        bool doNotAskAgainCheckboxResult = false;

        const auto result = KMessageBox::createKMessageBox(
            dialog,
            buttons,
            QMessageBox::Warning,
            i18n("The program '%1' is still running in the Terminal panel. Are you sure you want to quit?", m_terminalPanel->runningProgramName()),
            QStringList(),
            i18n("Do not ask again"),
            &doNotAskAgainCheckboxResult,
            KMessageBox::Notify | KMessageBox::Dangerous);

        if (doNotAskAgainCheckboxResult) {
            GeneralSettings::setConfirmClosingTerminalRunningProgram(false);
        }

        switch (result) {
        case QDialogButtonBox::Yes:
            // Quit
            break;
        case QDialogButtonBox::No:
            actionCollection()->action("show_terminal_panel")->trigger();
            // Do not quit, ignore quit event
            Q_FALLTHROUGH();
        default:
            event->ignore();
            return;
        }
    }

    if (m_sessionSaveTimer && (m_sessionSaveTimer->isActive() || m_sessionSaveWatcher->isRunning())) {
        const bool sessionSaveTimerActive = m_sessionSaveTimer->isActive();

        m_sessionSaveTimer->stop();
        m_sessionSaveWatcher->disconnect();
        m_sessionSaveWatcher->waitForFinished();

        if (sessionSaveTimerActive || m_sessionSaveScheduled) {
            slotSaveSession();
        }
    }

    GeneralSettings::setVersion(CurrentLion ExplorerVersion);
    GeneralSettings::self()->save();

    KXmlGuiWindow::closeEvent(event);
}

void Lion ExplorerMainWindow::slotSaveSession()
{
    m_sessionSaveScheduled = false;

    if (m_sessionSaveWatcher->isRunning()) {
        // The previous session is still being saved - schedule another save.
        m_sessionSaveWatcher->disconnect();
        connect(m_sessionSaveWatcher, &QFutureWatcher<void>::finished, this, &Lion ExplorerMainWindow::slotSaveSession, Qt::SingleShotConnection);
        m_sessionSaveScheduled = true;
    } else if (!m_sessionSaveTimer->isActive()) {
        // No point in saving the session if the timer is running (since it will save the session again when it times out).
        KConfigGui::setSessionConfig(QStringLiteral("lionexplorer"), QStringLiteral("lionexplorer"));
        KConfig *config = KConfigGui::sessionConfig();
        saveGlobalProperties(config);
        savePropertiesInternal(config, 1);
        KConfigGroup group = config->group(QStringLiteral("Number"));
        group.writeEntry("NumberOfWindows", 1); // Makes session restore aware that there is a window to restore.

        auto future = QtConcurrent::run([config]() {
            config->sync();
        });
        m_sessionSaveWatcher->setFuture(future);
    }
}

void Lion ExplorerMainWindow::setSessionAutoSaveEnabled(bool enable)
{
    if (enable) {
        if (!m_sessionSaveTimer) {
            m_sessionSaveTimer = new QTimer(this);
            m_sessionSaveWatcher = new QFutureWatcher<void>(this);
            m_sessionSaveTimer->setSingleShot(true);
            m_sessionSaveTimer->setInterval(22000);

            connect(m_sessionSaveTimer, &QTimer::timeout, this, &Lion ExplorerMainWindow::slotSaveSession);
        }

        connect(m_tabWidget, &Lion ExplorerTabWidget::urlChanged, m_sessionSaveTimer, qOverload<>(&QTimer::start), Qt::UniqueConnection);
        connect(m_tabWidget, &Lion ExplorerTabWidget::tabCountChanged, m_sessionSaveTimer, qOverload<>(&QTimer::start), Qt::UniqueConnection);
        connect(m_tabWidget, &Lion ExplorerTabWidget::activeViewChanged, m_sessionSaveTimer, qOverload<>(&QTimer::start), Qt::UniqueConnection);
    } else if (m_sessionSaveTimer) {
        m_sessionSaveTimer->stop();
        m_sessionSaveWatcher->disconnect();
        m_sessionSaveScheduled = false;

        m_sessionSaveWatcher->waitForFinished();

        m_sessionSaveTimer->deleteLater();
        m_sessionSaveWatcher->deleteLater();
        m_sessionSaveTimer = nullptr;
        m_sessionSaveWatcher = nullptr;
    }
}

void Lion ExplorerMainWindow::saveProperties(KConfigGroup &group)
{
    m_tabWidget->saveProperties(group);
}

void Lion ExplorerMainWindow::readProperties(const KConfigGroup &group)
{
    m_tabWidget->readProperties(group);
}

void Lion ExplorerMainWindow::updateNewMenu()
{
    m_newFileMenu->checkUpToDate();
    m_newFileMenu->setWorkingDirectory(activeViewContainer()->url());
}

void Lion ExplorerMainWindow::createDirectory()
{
    // When creating directory, namejob is being run. In network folders,
    // this job can take long time, so instead of starting multiple namejobs,
    // just check if we are already running one. This prevents opening multiple
    // dialogs. BUG:481401
    if (!m_newFileMenu->isCreateDirectoryRunning()) {
        m_newFileMenu->setWorkingDirectory(activeViewContainer()->url());
        m_newFileMenu->createDirectory();
    }
}

void Lion ExplorerMainWindow::createFile()
{
    // Use the same logic as in createDirectory()
    if (!m_newFileMenu->isCreateFileRunning()) {
        m_newFileMenu->setWorkingDirectory(activeViewContainer()->url());
        m_newFileMenu->createFile();
    }
}

void Lion ExplorerMainWindow::quit()
{
    close();
}

void Lion ExplorerMainWindow::showErrorMessage(const QString &message)
{
    m_activeViewContainer->showMessage(message, KMessageWidget::Error);
}

void Lion ExplorerMainWindow::slotUndoAvailable(bool available)
{
    QAction *undoAction = actionCollection()->action(KStandardAction::name(KStandardAction::Undo));
    if (undoAction) {
        undoAction->setEnabled(available);
    }
}

void Lion ExplorerMainWindow::slotUndoTextChanged(const QString &text)
{
    QAction *undoAction = actionCollection()->action(KStandardAction::name(KStandardAction::Undo));
    if (undoAction) {
        undoAction->setText(text);
    }
}

void Lion ExplorerMainWindow::undo()
{
    clearStatusBar();
    KIO::FileUndoManager::self()->uiInterface()->setParentWidget(this);
    KIO::FileUndoManager::self()->undo();
}

#if KIO_VERSION >= QT_VERSION_CHECK(6, 17, 0)
void Lion ExplorerMainWindow::slotRedoAvailable(bool available)
{
    QAction *redoAction = actionCollection()->action(KStandardAction::name(KStandardAction::Redo));
    if (redoAction) {
        redoAction->setEnabled(available);
    }
}

void Lion ExplorerMainWindow::slotRedoTextChanged(const QString &text)
{
    QAction *redoAction = actionCollection()->action(KStandardAction::name(KStandardAction::Redo));
    if (redoAction) {
        redoAction->setText(text);
    }
}

void Lion ExplorerMainWindow::redo()
{
    clearStatusBar();
    KIO::FileUndoManager::self()->uiInterface()->setParentWidget(this);
    KIO::FileUndoManager::self()->redo();
}
#endif

void Lion ExplorerMainWindow::cut()
{
    if (m_activeViewContainer->view()->selectedItems().isEmpty()) {
        m_activeViewContainer->setSelectionModeEnabled(true, actionCollection(), SelectionMode::BottomBar::Contents::CutContents);
    } else {
        m_activeViewContainer->view()->cutSelectedItemsToClipboard();
        m_activeViewContainer->setSelectionModeEnabled(false);
    }
}

void Lion ExplorerMainWindow::copy()
{
    if (m_activeViewContainer->view()->selectedItems().isEmpty()) {
        m_activeViewContainer->setSelectionModeEnabled(true, actionCollection(), SelectionMode::BottomBar::Contents::CopyContents);
    } else {
        m_activeViewContainer->view()->copySelectedItemsToClipboard();
        m_activeViewContainer->setSelectionModeEnabled(false);
    }
}

void Lion ExplorerMainWindow::paste()
{
    m_activeViewContainer->view()->paste();
}

void Lion ExplorerMainWindow::find()
{
    m_activeViewContainer->setSearchBarVisible(true);
    m_activeViewContainer->setFocusToSearchBar();
}

void Lion ExplorerMainWindow::updateSearchAction()
{
    QAction *toggleSearchAction = actionCollection()->action(QStringLiteral("toggle_search"));
    toggleSearchAction->setChecked(m_activeViewContainer->isSearchBarVisible());
}

void Lion ExplorerMainWindow::updatePasteAction()
{
    QAction *pasteAction = actionCollection()->action(KStandardAction::name(KStandardAction::Paste));
    QPair<bool, QString> pasteInfo = m_activeViewContainer->view()->pasteInfo();
    pasteAction->setEnabled(pasteInfo.first);
    m_disabledActionNotifier->setDisabledReason(pasteAction,
                                                m_activeViewContainer->rootItem().isWritable()
                                                    ? i18nc("@info", "Cannot paste: The clipboard is empty.")
                                                    : i18nc("@info", "Cannot paste: You do not have permission to write into this folder."));
    pasteAction->setText(pasteInfo.second);
}

void Lion ExplorerMainWindow::slotDirectoryLoadingCompleted()
{
    updatePasteAction();
}

void Lion ExplorerMainWindow::slotToolBarActionMiddleClicked(QAction *action)
{
    if (action == actionCollection()->action(KStandardAction::name(KStandardAction::Back))) {
        goBackInNewTab();
    } else if (action == actionCollection()->action(KStandardAction::name(KStandardAction::Forward))) {
        goForwardInNewTab();
    } else if (action == actionCollection()->action(QStringLiteral("go_up"))) {
        goUpInNewTab();
    } else if (action == actionCollection()->action(QStringLiteral("go_home"))) {
        goHomeInNewTab();
    }
}

QAction *Lion ExplorerMainWindow::urlNavigatorHistoryAction(const KUrlNavigator *urlNavigator, int historyIndex, QObject *parent)
{
    const QUrl url = urlNavigator->locationUrl(historyIndex);

    QString text;

    if (isSearchUrl(url)) {
        text = Search::Lion ExplorerQuery(url, QUrl{}).title();
    } else if (urlNavigator->showFullPath()) {
        text = url.toDisplayString(QUrl::PreferLocalFile);
    } else {
        const KFilePlacesModel *placesModel = Lion ExplorerPlacesModelSingleton::instance().placesModel();

        const QModelIndex closestIdx = placesModel->closestItem(url);
        if (closestIdx.isValid()) {
            const QUrl placeUrl = placesModel->url(closestIdx);

            text = placesModel->text(closestIdx);

            QString pathInsidePlace = url.path().mid(placeUrl.path().length());

            if (!pathInsidePlace.isEmpty() && !pathInsidePlace.startsWith(QLatin1Char('/'))) {
                pathInsidePlace.prepend(QLatin1Char('/'));
            }

            if (pathInsidePlace != QLatin1Char('/')) {
                text.append(pathInsidePlace);
            }
        }
    }

    QAction *action = new QAction(QIcon::fromTheme(KIO::iconNameForUrl(url)), text, parent);
    action->setData(historyIndex);
    return action;
}

void Lion ExplorerMainWindow::slotAboutToShowBackPopupMenu()
{
    const KUrlNavigator *urlNavigator = m_activeViewContainer->urlNavigatorInternalWithHistory();
    int entries = 0;
    QMenu *menu = m_backAction->popupMenu();
    menu->clear();
    for (int i = urlNavigator->historyIndex() + 1; i < urlNavigator->historySize() && entries < MaxNumberOfNavigationentries; ++i, ++entries) {
        QAction *action = urlNavigatorHistoryAction(urlNavigator, i, menu);
        menu->addAction(action);
    }
}

void Lion ExplorerMainWindow::slotGoBack(QAction *action)
{
    int gotoIndex = action->data().value<int>();
    const KUrlNavigator *urlNavigator = m_activeViewContainer->urlNavigatorInternalWithHistory();
    for (int i = gotoIndex - urlNavigator->historyIndex(); i > 0; --i) {
        goBack();
    }
}

void Lion ExplorerMainWindow::slotBackForwardActionMiddleClicked(QAction *action)
{
    if (action) {
        const KUrlNavigator *urlNavigator = activeViewContainer()->urlNavigatorInternalWithHistory();
        openNewTab(urlNavigator->locationUrl(action->data().value<int>()));
    }
}

void Lion ExplorerMainWindow::slotAboutToShowForwardPopupMenu()
{
    const KUrlNavigator *urlNavigator = m_activeViewContainer->urlNavigatorInternalWithHistory();
    int entries = 0;
    QMenu *menu = m_forwardAction->popupMenu();
    menu->clear();
    for (int i = urlNavigator->historyIndex() - 1; i >= 0 && entries < MaxNumberOfNavigationentries; --i, ++entries) {
        QAction *action = urlNavigatorHistoryAction(urlNavigator, i, menu);
        menu->addAction(action);
    }
}

void Lion ExplorerMainWindow::slotGoForward(QAction *action)
{
    int gotoIndex = action->data().value<int>();
    const KUrlNavigator *urlNavigator = m_activeViewContainer->urlNavigatorInternalWithHistory();
    for (int i = urlNavigator->historyIndex() - gotoIndex; i > 0; --i) {
        goForward();
    }
}

void Lion ExplorerMainWindow::slotSetSelectionMode(bool enabled, SelectionMode::BottomBar::Contents bottomBarContents)
{
    m_activeViewContainer->setSelectionModeEnabled(enabled, actionCollection(), bottomBarContents);
}

void Lion ExplorerMainWindow::selectAll()
{
    clearStatusBar();

    // if the URL navigator is editable and focused, select the whole
    // URL instead of all items of the view

    KUrlNavigator *urlNavigator = m_activeViewContainer->urlNavigator();
    QLineEdit *lineEdit = urlNavigator->editor()->lineEdit();
    const bool selectUrl = urlNavigator->isUrlEditable() && lineEdit->hasFocus();
    if (selectUrl) {
        lineEdit->selectAll();
    } else {
        m_activeViewContainer->view()->selectAll();
    }
}

void Lion ExplorerMainWindow::invertSelection()
{
    clearStatusBar();
    m_activeViewContainer->view()->invertSelection();
}

void Lion ExplorerMainWindow::toggleSplitView()
{
    QUrl newSplitViewUrl;
    const KFileItemList list = m_activeViewContainer->view()->selectedItems();
    if (list.count() == 1) {
        const KFileItem &item = list.first();
        newSplitViewUrl = Lion ExplorerView::openItemAsFolderUrl(item);
    }

    Lion ExplorerTabPage *tabPage = m_tabWidget->currentTabPage();
    tabPage->setSplitViewEnabled(!tabPage->splitViewEnabled(), WithAnimation, newSplitViewUrl);
    m_tabWidget->updateTabName(m_tabWidget->indexOf(tabPage));
    updateViewActions();
}

void Lion ExplorerMainWindow::popoutSplitView()
{
    Lion ExplorerTabPage *tabPage = m_tabWidget->currentTabPage();
    if (!tabPage->splitViewEnabled())
        return;
    using Choice = GeneralSettings::EnumCloseSplitViewChoice;
    switch (GeneralSettings::closeSplitViewChoice()) {
    case Choice::ActiveView:
        openNewWindow(tabPage->activeViewContainer()->url());
        break;
    case Choice::InactiveView:
        openNewWindow(tabPage->inactiveViewContainer()->url());
        break;
    case Choice::RightView:
        openNewWindow(tabPage->secondaryViewContainer()->url());
        break;
    default:
        Q_UNREACHABLE();
    }

    tabPage->setSplitViewEnabled(false, WithAnimation);
    updateSplitActions();
}

void Lion ExplorerMainWindow::toggleSplitStash()
{
    Lion ExplorerTabPage *tabPage = m_tabWidget->currentTabPage();
    tabPage->setSplitViewEnabled(false, WithAnimation);
    tabPage->setSplitViewEnabled(true, WithAnimation, QUrl("stash:/"));
}

void Lion ExplorerMainWindow::copyToInactiveSplitView()
{
    if (m_activeViewContainer->view()->selectedItems().isEmpty()) {
        m_activeViewContainer->setSelectionModeEnabled(true, actionCollection(), SelectionMode::BottomBar::Contents::CopyToOtherViewContents);
    } else {
        m_tabWidget->copyToInactiveSplitView();
        m_activeViewContainer->setSelectionModeEnabled(false);
    }
}

void Lion ExplorerMainWindow::moveToInactiveSplitView()
{
    if (m_activeViewContainer->view()->selectedItems().isEmpty()) {
        m_activeViewContainer->setSelectionModeEnabled(true, actionCollection(), SelectionMode::BottomBar::Contents::MoveToOtherViewContents);
    } else {
        m_tabWidget->moveToInactiveSplitView();
        m_activeViewContainer->setSelectionModeEnabled(false);
    }
}

void Lion ExplorerMainWindow::reloadView()
{
    clearStatusBar();
    m_activeViewContainer->reload();
    m_activeViewContainer->statusBar()->updateSpaceInfo();
    Q_EMIT urlRefreshed(m_activeViewContainer->url());
}

void Lion ExplorerMainWindow::stopLoading()
{
    m_activeViewContainer->view()->stopLoading();
}

void Lion ExplorerMainWindow::enableStopAction()
{
    actionCollection()->action(QStringLiteral("stop"))->setEnabled(true);
}

void Lion ExplorerMainWindow::disableStopAction()
{
    actionCollection()->action(QStringLiteral("stop"))->setEnabled(false);
}

void Lion ExplorerMainWindow::toggleSelectionMode()
{
    const bool checked = !m_activeViewContainer->isSelectionModeEnabled();

    m_activeViewContainer->setSelectionModeEnabled(checked, actionCollection(), SelectionMode::BottomBar::Contents::GeneralContents);
    actionCollection()->action(QStringLiteral("toggle_selection_mode"))->setChecked(checked);
}

void Lion ExplorerMainWindow::showFilterBar()
{
    m_activeViewContainer->setFilterBarVisible(true);
}

void Lion ExplorerMainWindow::toggleFilterBar()
{
    const bool checked = !m_activeViewContainer->isFilterBarVisible();
    m_activeViewContainer->setFilterBarVisible(checked);

    QAction *toggleFilterBarAction = actionCollection()->action(QStringLiteral("toggle_filter"));
    toggleFilterBarAction->setChecked(checked);
}

void Lion ExplorerMainWindow::toggleEditLocation()
{
    clearStatusBar();

    QAction *action = actionCollection()->action(QStringLiteral("editable_location"));
    KUrlNavigator *urlNavigator = m_activeViewContainer->urlNavigator();
    urlNavigator->setUrlEditable(action->isChecked());
}

void Lion ExplorerMainWindow::replaceLocation()
{
    KUrlNavigator *navigator = m_activeViewContainer->urlNavigator();
    QLineEdit *lineEdit = navigator->editor()->lineEdit();

    // If the text field currently has focus and everything is selected,
    // pressing the keyboard shortcut returns the whole thing to breadcrumb mode
    // and goes back to the view, just like how it was before this action was triggered the first time.
    if (navigator->isUrlEditable() && lineEdit->hasFocus() && lineEdit->selectedText() == lineEdit->text()) {
        navigator->setUrlEditable(false);
        m_activeViewContainer->view()->setFocus();
    } else {
        navigator->setUrlEditable(true);
        navigator->setFocus();
        lineEdit->selectAll();
    }
}

void Lion ExplorerMainWindow::togglePanelLockState()
{
    const bool newLockState = !GeneralSettings::lockPanels();
    const auto childrenObjects = children();
    for (QObject *child : childrenObjects) {
        Lion ExplorerDockWidget *dock = qobject_cast<Lion ExplorerDockWidget *>(child);
        if (dock) {
            dock->setLocked(newLockState);
        }
    }

    Lion ExplorerPlacesModelSingleton::instance().placesModel()->setPanelsLocked(newLockState);

    GeneralSettings::setLockPanels(newLockState);
}

void Lion ExplorerMainWindow::slotTerminalPanelVisibilityChanged(bool visible)
{
    if (!visible && m_activeViewContainer) {
        m_activeViewContainer->view()->setFocus();
    }
    // Putting focus to the Terminal is not handled here but in TerminalPanel::showEvent().
}

void Lion ExplorerMainWindow::slotPlacesPanelVisibilityChanged(bool visible)
{
    if (!visible && m_activeViewContainer) {
        m_activeViewContainer->view()->setFocus();
        return;
    }
    m_placesPanel->setFocus();
}

void Lion ExplorerMainWindow::goBack()
{
    Lion ExplorerUrlNavigator *urlNavigator = m_activeViewContainer->urlNavigatorInternalWithHistory();
    urlNavigator->goBack();

    if (urlNavigator->locationState().isEmpty()) {
        // An empty location state indicates a redirection URL,
        // which must be skipped too
        urlNavigator->goBack();
    }
}

void Lion ExplorerMainWindow::goForward()
{
    m_activeViewContainer->urlNavigatorInternalWithHistory()->goForward();
}

void Lion ExplorerMainWindow::goUp()
{
    m_activeViewContainer->urlNavigatorInternalWithHistory()->goUp();
}

void Lion ExplorerMainWindow::goHome()
{
    m_activeViewContainer->urlNavigatorInternalWithHistory()->goHome();
}

void Lion ExplorerMainWindow::goBackInNewTab()
{
    const KUrlNavigator *urlNavigator = activeViewContainer()->urlNavigatorInternalWithHistory();
    const int index = urlNavigator->historyIndex() + 1;
    openNewTab(urlNavigator->locationUrl(index));
}

void Lion ExplorerMainWindow::goForwardInNewTab()
{
    const KUrlNavigator *urlNavigator = activeViewContainer()->urlNavigatorInternalWithHistory();
    const int index = urlNavigator->historyIndex() - 1;
    openNewTab(urlNavigator->locationUrl(index));
}

void Lion ExplorerMainWindow::goUpInNewTab()
{
    const QUrl currentUrl = activeViewContainer()->urlNavigator()->locationUrl();
    openNewTab(KIO::upUrl(currentUrl));
}

void Lion ExplorerMainWindow::goHomeInNewTab()
{
    openNewTab(Lion Explorer::homeUrl());
}

void Lion ExplorerMainWindow::compareFiles()
{
    const KFileItemList items = m_tabWidget->currentTabPage()->selectedItems();
    if (items.count() != 2) {
        // The action is disabled in this case, but it could have been triggered
        // via D-Bus, see https://bugs.kde.org/show_bug.cgi?id=325517
        return;
    }

    QUrl urlA = items.at(0).url();
    QUrl urlB = items.at(1).url();

    QString command(QStringLiteral("kompare -c \""));
    command.append(urlA.toDisplayString(QUrl::PreferLocalFile));
    command.append("\" \"");
    command.append(urlB.toDisplayString(QUrl::PreferLocalFile));
    command.append('\"');

    KIO::CommandLauncherJob *job = new KIO::CommandLauncherJob(command, this);
    job->setDesktopName(QStringLiteral("org.kde.kompare"));
    job->start();
}

void Lion ExplorerMainWindow::toggleShowMenuBar()
{
    QAction *showMenuBarAction = actionCollection()->action(KStandardAction::name(KStandardAction::ShowMenubar));
    menuBar()->setVisible(showMenuBarAction->isChecked());
}

QPointer<QAction> Lion ExplorerMainWindow::preferredSearchTool()
{
    m_searchTools.clear();

    KService::Ptr kfind = KService::serviceByDesktopName(QStringLiteral("org.kde.kfind"));

    if (!kfind) {
        return nullptr;
    }

    auto *action = new QAction(QIcon::fromTheme(kfind->icon()), kfind->name(), this);

    connect(action, &QAction::triggered, this, [this, kfind] {
        auto *job = new KIO::ApplicationLauncherJob(kfind);
        job->setUrls({m_activeViewContainer->url()});
        job->start();
    });

    return action;
}

void Lion ExplorerMainWindow::updateOpenPreferredSearchToolAction()
{
    QAction *openPreferredSearchTool = actionCollection()->action(QStringLiteral("open_preferred_search_tool"));
    if (!openPreferredSearchTool) {
        return;
    }
    QPointer<QAction> tool = preferredSearchTool();
    if (tool) {
        openPreferredSearchTool->setVisible(true);
        openPreferredSearchTool->setText(i18nc("@action:inmenu Tools", "Open %1", tool->text()));
        // Only override with the app icon if it is the default, i.e. the user hasn't configured one manually
        // https://bugs.kde.org/show_bug.cgi?id=442815
        if (openPreferredSearchTool->icon().name() == QLatin1String("search")) {
            openPreferredSearchTool->setIcon(tool->icon());
        }
    } else {
        openPreferredSearchTool->setVisible(false);
        // still visible in Shortcuts configuration window
        openPreferredSearchTool->setText(i18nc("@action:inmenu Tools", "Open Preferred Search Tool"));
        openPreferredSearchTool->setIcon(QIcon::fromTheme(QStringLiteral("search")));
    }
}

void Lion ExplorerMainWindow::openPreferredSearchTool()
{
    QPointer<QAction> tool = preferredSearchTool();
    if (tool) {
        tool->trigger();
    }
}

void Lion ExplorerMainWindow::openTerminal()
{
    openTerminalJob(m_activeViewContainer->url());
}

void Lion ExplorerMainWindow::openTerminalHere()
{
    QList<QUrl> urls = {};

    const auto selectedItems = m_activeViewContainer->view()->selectedItems();
    for (const KFileItem &item : selectedItems) {
        QUrl url = item.targetUrl();
        if (item.isFile()) {
            url.setPath(QFileInfo(url.path()).absolutePath());
        }
        if (!urls.contains(url)) {
            urls << url;
        }
    }

    // No items are selected. Open a terminal window for the current location.
    if (urls.count() == 0) {
        openTerminal();
        return;
    }

    if (urls.count() > 5) {
        QString question = i18np("Are you sure you want to open 1 terminal window?", "Are you sure you want to open %1 terminal windows?", urls.count());
        const int answer = KMessageBox::warningContinueCancel(
            this,
            question,
            {},
            KGuiItem(i18ncp("@action:button", "Open %1 Terminal", "Open %1 Terminals", urls.count()), QStringLiteral("utilities-terminal")),
            KStandardGuiItem::cancel(),
            QStringLiteral("ConfirmOpenManyTerminals"));
        if (answer != KMessageBox::PrimaryAction && answer != KMessageBox::Continue) {
            return;
        }
    }

    for (const QUrl &url : std::as_const(urls)) {
        openTerminalJob(url);
    }
}

void Lion ExplorerMainWindow::openTerminalJob(const QUrl &url)
{
    if (url.isLocalFile()) {
        auto job = new KTerminalLauncherJob(QString());
        job->setWorkingDirectory(url.toLocalFile());
        job->start();
        return;
    }

    // Not a local file, with protocol Class ":local", try stat'ing
    if (KProtocolInfo::protocolClass(url.scheme()) == QLatin1String(":local")) {
        KIO::StatJob *job = KIO::mostLocalUrl(url);
        KJobWidgets::setWindow(job, this);
        connect(job, &KJob::result, this, [job]() {
            QUrl statUrl;
            if (!job->error()) {
                statUrl = job->mostLocalUrl();
            }

            auto job = new KTerminalLauncherJob(QString());
            job->setWorkingDirectory(statUrl.isLocalFile() ? statUrl.toLocalFile() : QDir::homePath());
            job->start();
        });

        return;
    }

    // Nothing worked, just use $HOME
    auto job = new KTerminalLauncherJob(QString());
    job->setWorkingDirectory(QDir::homePath());
    job->start();
}

void Lion ExplorerMainWindow::editSettings()
{
    if (!m_settingsDialog) {
        Lion ExplorerViewContainer *container = activeViewContainer();
        container->view()->writeSettings();

        const QUrl url = container->url();
        Lion ExplorerSettingsDialog *settingsDialog = new Lion ExplorerSettingsDialog(url, this, actionCollection());
        connect(settingsDialog, &Lion ExplorerSettingsDialog::settingsChanged, this, &Lion ExplorerMainWindow::refreshViews);
        connect(settingsDialog, &Lion ExplorerSettingsDialog::settingsChanged, &Lion ExplorerUrlNavigatorsController::slotReadSettings);
        settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
        settingsDialog->show();
        m_settingsDialog = settingsDialog;
    } else {
        m_settingsDialog.data()->raise();
    }
}

void Lion ExplorerMainWindow::handleUrl(const QUrl &url)
{
    delete m_lastHandleUrlOpenJob;
    m_lastHandleUrlOpenJob = nullptr;

    if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir()) {
        activeViewContainer()->setUrl(url);
    } else {
        m_lastHandleUrlOpenJob = new KIO::OpenUrlJob(url);
        m_lastHandleUrlOpenJob->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        m_lastHandleUrlOpenJob->setShowOpenOrExecuteDialog(true);

        connect(m_lastHandleUrlOpenJob, &KIO::OpenUrlJob::mimeTypeFound, this, [this, url](const QString &mimetype) {
            if (mimetype == QLatin1String("inode/directory")) {
                // If it's a dir, we'll take it from here
                m_lastHandleUrlOpenJob->kill();
                m_lastHandleUrlOpenJob = nullptr;
                activeViewContainer()->setUrl(url);
            }
        });

        connect(m_lastHandleUrlOpenJob, &KIO::OpenUrlJob::result, this, [this]() {
            m_lastHandleUrlOpenJob = nullptr;
        });

        m_lastHandleUrlOpenJob->start();
    }
}

void Lion ExplorerMainWindow::slotWriteStateChanged(bool isFolderWritable)
{
    // trash:/ is writable but we don't want to create new items in it.
    // TODO: remove the trash check once https://phabricator.kde.org/T8234 is implemented
    newFileMenu()->setEnabled(isFolderWritable && m_activeViewContainer->url().scheme() != QLatin1String("trash"));
    // When the menu is disabled, actions in it are disabled later in the event loop, and we need to set the disabled reason after that.
    QTimer::singleShot(0, this, [this]() {
        m_disabledActionNotifier->setDisabledReason(actionCollection()->action(QStringLiteral("create_file")),
                                                    i18nc("@info", "Cannot create new file: You do not have permission to create items in this folder."));
        m_disabledActionNotifier->setDisabledReason(actionCollection()->action(QStringLiteral("create_dir")),
                                                    i18nc("@info", "Cannot create new folder: You do not have permission to create items in this folder."));
    });
}

void Lion ExplorerMainWindow::openContextMenu(const QPoint &pos, const KFileItem &item, const KFileItemList &selectedItems, const QUrl &url)
{
    QPointer<Lion ExplorerContextMenu> contextMenu = new Lion ExplorerContextMenu(this, item, selectedItems, url, m_fileItemActions);
    contextMenu->exec(pos);

    // Delete the menu, unless it has been deleted in its own nested event loop already.
    if (contextMenu) {
        contextMenu->deleteLater();
    }
}

QMenu *Lion ExplorerMainWindow::createPopupMenu()
{
    QMenu *menu = KXmlGuiWindow::createPopupMenu();

    menu->addSeparator();
    menu->addAction(actionCollection()->action(QStringLiteral("lock_panels")));

    return menu;
}

void Lion ExplorerMainWindow::updateHamburgerMenu()
{
    KActionCollection *ac = actionCollection();
    auto hamburgerMenu = static_cast<KHamburgerMenu *>(ac->action(KStandardAction::name(KStandardAction::HamburgerMenu)));
    auto menu = hamburgerMenu->menu();
    if (!menu) {
        menu = new QMenu(this);
        hamburgerMenu->setMenu(menu);
        hamburgerMenu->hideActionsOf(ac->action(QStringLiteral("basic_actions"))->menu());
        hamburgerMenu->hideActionsOf(qobject_cast<KToolBarPopupAction *>(ac->action(QStringLiteral("zoom")))->popupMenu());
    } else {
        menu->clear();
    }
    const QList<QAction *> toolbarActions = toolBar()->actions();

    if (!toolBar()->isVisible()) {
        // If neither the menu bar nor the toolbar are visible, these actions should be available.
        menu->addAction(ac->action(KStandardAction::name(KStandardAction::ShowMenubar)));
        menu->addAction(toolBarMenuAction());
        menu->addSeparator();
    }

    // This group of actions (until the next separator) contains all the most basic actions
    // necessary to use Lion Explorer effectively.
    menu->addAction(ac->action(QStringLiteral("go_back")));
    menu->addAction(ac->action(QStringLiteral("go_forward")));

    menu->addMenu(m_newFileMenu->menu());
    if (!toolBar()->isVisible() || !toolbarActions.contains(ac->action(QStringLiteral("toggle_selection_mode_tool_bar")))) {
        menu->addAction(ac->action(QStringLiteral("toggle_selection_mode")));
    }
    menu->addAction(ac->action(QStringLiteral("basic_actions")));
    menu->addAction(ac->action(KStandardAction::name(KStandardAction::Undo)));
#if KIO_VERSION >= QT_VERSION_CHECK(6, 17, 0)
    menu->addAction(ac->action(KStandardAction::name(KStandardAction::Redo)));
#endif
    if (!toolBar()->isVisible()
        || (!toolbarActions.contains(ac->action(QStringLiteral("toggle_search")))
            && !toolbarActions.contains(ac->action(QStringLiteral("open_preferred_search_tool"))))) {
        menu->addAction(ac->action(KStandardAction::name(KStandardAction::Find)));
        // This way a search action will only be added if none of the three available
        // search actions is present on the toolbar.
    }
    if (!toolBar()->isVisible() || !toolbarActions.contains(ac->action(QStringLiteral("toggle_filter")))) {
        menu->addAction(ac->action(QStringLiteral("show_filter_bar")));
        // This way a filter action will only be added if none of the two available
        // filter actions is present on the toolbar.
    }
    menu->addSeparator();

    // The second group of actions (up until the next separator) contains actions for opening
    // additional views to interact with the file system.
    menu->addAction(ac->action(QStringLiteral("file_new")));
    menu->addAction(ac->action(QStringLiteral("new_tab")));
    if (ac->action(QStringLiteral("undo_close_tab"))->isEnabled()) {
        menu->addAction(ac->action(QStringLiteral("closed_tabs")));
    }
    menu->addAction(ac->action(QStringLiteral("open_terminal")));
    menu->addSeparator();

    // The third group contains actions to change what one sees in the view
    // and to change the more general UI.
    if (!toolBar()->isVisible()
        || ((!toolbarActions.contains(ac->action(QStringLiteral("icons"))) && !toolbarActions.contains(ac->action(QStringLiteral("compact")))
             && !toolbarActions.contains(ac->action(QStringLiteral("details"))) && !toolbarActions.contains(ac->action(QStringLiteral("view_mode"))))
            && !toolbarActions.contains(ac->action(QStringLiteral("view_settings"))))) {
        menu->addAction(ac->action(QStringLiteral("view_mode")));
    }
    if (!toolBar()->isVisible() || !toolbarActions.contains(ac->action(QStringLiteral("view_settings")))) {
        menu->addAction(ac->action(QStringLiteral("show_hidden_files")));
        menu->addAction(ac->action(QStringLiteral("sort")));
        menu->addAction(ac->action(QStringLiteral("additional_info")));
        if (!GeneralSettings::showStatusBar() || !GeneralSettings::showZoomSlider()) {
            menu->addAction(ac->action(QStringLiteral("zoom")));
        }
    }
    menu->addAction(ac->action(QStringLiteral("panels")));

    // The "Configure" menu is not added to the actionCollection() because there is hardly
    // a good reason for users to put it on their toolbar.
    auto configureMenu = menu->addMenu(QIcon::fromTheme(QStringLiteral("configure")), i18nc("@action:inmenu menu for configure actions", "Configure"));
    configureMenu->addAction(actionCollection()->action(QStringLiteral("window_color_sheme")));
    configureMenu->addSeparator();
    configureMenu->addAction(ac->action(KStandardAction::name(KStandardAction::SwitchApplicationLanguage)));
    configureMenu->addAction(ac->action(KStandardAction::name(KStandardAction::KeyBindings)));
    configureMenu->addAction(ac->action(KStandardAction::name(KStandardAction::ConfigureToolbars)));
    configureMenu->addAction(ac->action(KStandardAction::name(KStandardAction::Preferences)));
    hamburgerMenu->hideActionsOf(configureMenu);
}

void Lion ExplorerMainWindow::slotPlaceActivated(const QUrl &url)
{
    Lion ExplorerViewContainer *view = activeViewContainer();

    if (view->url() == url) {
        view->clearFilterBar(); // Fixes bug 259382.

        // We can end up here if the user clicked a device in the Places Panel
        // which had been unmounted earlier, see https://bugs.kde.org/show_bug.cgi?id=161385.
        reloadView();

        m_activeViewContainer->view()->setFocus(); // We always want the focus on the view after activating a place.
    } else {
        view->disableUrlNavigatorSelectionRequests();
        changeUrl(url);
        view->enableUrlNavigatorSelectionRequests();
    }
}

void Lion ExplorerMainWindow::closedTabsCountChanged(unsigned int count)
{
    actionCollection()->action(QStringLiteral("undo_close_tab"))->setEnabled(count > 0);
}

void Lion ExplorerMainWindow::activeViewChanged(Lion ExplorerViewContainer *viewContainer)
{
    Lion ExplorerViewContainer *oldViewContainer = m_activeViewContainer;
    Q_ASSERT(viewContainer);

    m_activeViewContainer = viewContainer;

    if (oldViewContainer) {
        // Disconnect all signals between the old view container (container,
        // view and url navigator) and main window.
        oldViewContainer->disconnect(this);
        oldViewContainer->view()->disconnect(this);
        oldViewContainer->urlNavigatorInternalWithHistory()->disconnect(this);
        auto navigators = static_cast<Lion ExplorerNavigatorsWidgetAction *>(actionCollection()->action(QStringLiteral("url_navigators")));
        navigators->primaryUrlNavigator()->disconnect(this);
        if (auto secondaryUrlNavigator = navigators->secondaryUrlNavigator()) {
            secondaryUrlNavigator->disconnect(this);
        }
        oldViewContainer->disconnect(m_diskSpaceUsageMenu);

        // except the requestItemInfo so that on hover the information panel can still be updated
        connect(oldViewContainer->view(), &Lion ExplorerView::requestItemInfo, this, &Lion ExplorerMainWindow::requestItemInfo);

        // Disconnect other slots.
        disconnect(oldViewContainer,
                   &Lion ExplorerViewContainer::selectionModeChanged,
                   actionCollection()->action(QStringLiteral("toggle_selection_mode")),
                   &QAction::setChecked);
    }

    connectViewSignals(viewContainer);

    m_actionHandler->setCurrentView(viewContainer->view());

    updateHistory();
    updateFileAndEditActions();
    updatePasteAction();
    updateViewActions();
    updateGoActions();
    updateSearchAction();
    connect(m_diskSpaceUsageMenu,
            &DiskSpaceUsageMenu::showMessage,
            viewContainer,
            [viewContainer](const QString &message, KMessageWidget::MessageType messageType) {
                viewContainer->showMessage(message, messageType);
            });
    connect(m_diskSpaceUsageMenu, &DiskSpaceUsageMenu::showInstallationProgress, viewContainer, &Lion ExplorerViewContainer::showProgress);

    const QUrl url = viewContainer->url();
    Q_EMIT urlChanged(url);
    Q_EMIT selectionChanged(m_activeViewContainer->view()->selectedItems());
}

void Lion ExplorerMainWindow::tabCountChanged(int count)
{
    const bool enableTabActions = (count > 1);
    for (int i = 0; i < MaxActivateTabShortcuts; ++i) {
        actionCollection()->action(QStringLiteral("activate_tab_%1").arg(i))->setEnabled(enableTabActions);
    }
    actionCollection()->action(QStringLiteral("activate_last_tab"))->setEnabled(enableTabActions);
    actionCollection()->action(QStringLiteral("activate_next_tab"))->setEnabled(enableTabActions);
    actionCollection()->action(QStringLiteral("activate_prev_tab"))->setEnabled(enableTabActions);
}

void Lion ExplorerMainWindow::updateWindowTitle()
{
    const QString newTitle = m_activeViewContainer->captionWindowTitle();
    if (windowTitle() != newTitle) {
        setWindowTitle(newTitle);
    }
}

void Lion ExplorerMainWindow::slotStorageTearDownFromPlacesRequested(const QString &mountPath)
{
    connect(m_placesPanel, &PlacesPanel::storageTearDownSuccessful, this, [this, mountPath]() {
        setViewsToHomeIfMountPathOpen(mountPath);
    });

    if (m_terminalPanel && m_terminalPanel->currentWorkingDirectoryIsChildOf(mountPath)) {
        m_tearDownFromPlacesRequested = true;
        m_terminalPanel->goHome();
        // m_placesPanel->proceedWithTearDown() will be called in slotTerminalDirectoryChanged
    } else {
        m_placesPanel->proceedWithTearDown();
    }
}

void Lion ExplorerMainWindow::slotStorageTearDownExternallyRequested(const QString &mountPath)
{
    connect(m_placesPanel, &PlacesPanel::storageTearDownSuccessful, this, [this, mountPath]() {
        setViewsToHomeIfMountPathOpen(mountPath);
    });

    if (m_terminalPanel && m_terminalPanel->currentWorkingDirectoryIsChildOf(mountPath)) {
        m_tearDownFromPlacesRequested = false;
        m_terminalPanel->goHome();
    }
}

void Lion ExplorerMainWindow::slotKeyBindings()
{
#if KIO_VERSION >= QT_VERSION_CHECK(6, 24, 0)
    m_serviceMenuShortcutManager->cleanupStaleShortcuts(this);
#endif

    KShortcutsDialog dialog(KShortcutsEditor::AllActions, KShortcutsEditor::LetterShortcutsAllowed, this);
    dialog.addCollection(actionCollection());
    if (m_terminalPanel) {
        KActionCollection *konsolePartActionCollection = m_terminalPanel->actionCollection();
        if (konsolePartActionCollection) {
            dialog.addCollection(konsolePartActionCollection, QStringLiteral("KonsolePart"));
        }
    }
    dialog.configure();
}

void Lion ExplorerMainWindow::setViewsToHomeIfMountPathOpen(const QString &mountPath)
{
    const QVector<Lion ExplorerViewContainer *> theViewContainers = viewContainers();
    for (Lion ExplorerViewContainer *viewContainer : theViewContainers) {
        if (!viewContainer) {
            continue;
        }
        const auto viewPath = viewContainer->url().toLocalFile();
        if (viewPath.startsWith(mountPath + QLatin1String("/")) || viewPath == mountPath) {
            viewContainer->setUrl(QUrl::fromLocalFile(QDir::homePath()));
        }
    }
    disconnect(m_placesPanel, &PlacesPanel::storageTearDownSuccessful, nullptr, nullptr);
}

void Lion ExplorerMainWindow::setupActions()
{
    auto hamburgerMenuAction = KStandardAction::hamburgerMenu(nullptr, nullptr, actionCollection());

    // setup 'File' menu
    m_newFileMenu = new Lion ExplorerNewFileMenu(nullptr, nullptr, this);
    actionCollection()->addAction(QStringLiteral("new_menu"), m_newFileMenu);
    QMenu *menu = m_newFileMenu->menu();
    menu->setTitle(i18nc("@title:menu Create new folder, file, link, etc.", "Create New"));
    menu->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_newFileMenu->setPopupMode(QToolButton::InstantPopup);
    connect(menu, &QMenu::aboutToShow, this, &Lion ExplorerMainWindow::updateNewMenu);
    connect(m_newFileMenu, &KNewFileMenu::directoryCreated, this, [this](const QUrl &createdDirectory) {
        activeViewContainer()->view()->expandToUrl(createdDirectory);
    });

    QAction *newWindow = KStandardAction::openNew(this, &Lion ExplorerMainWindow::openNewMainWindow, actionCollection());
    newWindow->setText(i18nc("@action:inmenu File", "New &Window"));
    newWindow->setToolTip(i18nc("@info", "Open a new Lion Explorer window"));
    newWindow->setWhatsThis(xi18nc("@info:whatsthis",
                                   "This opens a new "
                                   "window just like this one with the current location."
                                   "<nl/>You can drag and drop items between windows."));
    newWindow->setIcon(QIcon::fromTheme(QStringLiteral("window-new")));

    QAction *newTab = actionCollection()->addAction(QStringLiteral("new_tab"));
    newTab->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    newTab->setText(i18nc("@action:inmenu File", "New Tab"));
    newTab->setWhatsThis(xi18nc("@info:whatsthis",
                                "This opens a new "
                                "<emphasis>Tab</emphasis> with the current location."
                                "<nl/>Tabs allow you to quickly switch between multiple locations and views within this window. "
                                "You can drag and drop items between tabs."));
    actionCollection()->setDefaultShortcut(newTab, Qt::CTRL | Qt::Key_T);
    connect(newTab, &QAction::triggered, this, &Lion ExplorerMainWindow::openNewActivatedTab);

    QAction *addToPlaces = actionCollection()->addAction(QStringLiteral("add_to_places"));
    addToPlaces->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new")));
    addToPlaces->setText(i18nc("@action:inmenu Add current folder to places", "Add to Places"));
    addToPlaces->setWhatsThis(xi18nc("@info:whatsthis",
                                     "This adds the selected folder "
                                     "to the Places panel."));
    connect(addToPlaces, &QAction::triggered, this, &Lion ExplorerMainWindow::addToPlaces);

    QAction *closeTab = KStandardAction::close(m_tabWidget, QOverload<>::of(&Lion ExplorerTabWidget::closeTab), actionCollection());
    closeTab->setText(i18nc("@action:inmenu File", "Close Tab"));
    closeTab->setToolTip(i18nc("@info", "Close Tab"));
    closeTab->setWhatsThis(i18nc("@info:whatsthis",
                                 "This closes the "
                                 "currently viewed tab. If no more tabs are left, this closes "
                                 "the whole window instead."));

    QAction *quitAction = KStandardAction::quit(this, &Lion ExplorerMainWindow::quit, actionCollection());
    quitAction->setWhatsThis(i18nc("@info:whatsthis quit", "This closes this window."));

    // setup 'Edit' menu
    KStandardAction::undo(this, &Lion ExplorerMainWindow::undo, actionCollection());
#if KIO_VERSION >= QT_VERSION_CHECK(6, 17, 0)
    KStandardAction::redo(this, &Lion ExplorerMainWindow::redo, actionCollection());
#endif

    // i18n: This will be the last paragraph for the whatsthis for all three:
    // Cut, Copy and Paste
    const QString cutCopyPastePara = xi18nc("@info:whatsthis",
                                            "<para><emphasis>Cut, "
                                            "Copy</emphasis> and <emphasis>Paste</emphasis> work between many "
                                            "applications and are among the most used commands. That's why their "
                                            "<emphasis>keyboard shortcuts</emphasis> are prominently placed right "
                                            "next to each other on the keyboard: <shortcut>Ctrl+X</shortcut>, "
                                            "<shortcut>Ctrl+C</shortcut> and <shortcut>Ctrl+V</shortcut>.</para>");
    QAction *cutAction = KStandardAction::cut(this, &Lion ExplorerMainWindow::cut, actionCollection());
    m_actionTextHelper->registerTextWhenNothingIsSelected(cutAction, i18nc("@action", "Cut…"));
    cutAction->setWhatsThis(xi18nc("@info:whatsthis cut",
                                   "This copies the items "
                                   "in your current selection to the <emphasis>clipboard</emphasis>.<nl/>"
                                   "Use the <emphasis>Paste</emphasis> action afterwards to copy them from "
                                   "the clipboard to a new location. The items will be removed from their "
                                   "initial location.")
                            + cutCopyPastePara);
    QAction *copyAction = KStandardAction::copy(this, &Lion ExplorerMainWindow::copy, actionCollection());
    m_actionTextHelper->registerTextWhenNothingIsSelected(copyAction, i18nc("@action", "Copy…"));
    copyAction->setWhatsThis(xi18nc("@info:whatsthis copy",
                                    "This copies the "
                                    "items in your current selection to the <emphasis>clipboard</emphasis>."
                                    "<nl/>Use the <emphasis>Paste</emphasis> action afterwards to copy them "
                                    "from the clipboard to a new location.")
                             + cutCopyPastePara);
    QAction *paste = KStandardAction::paste(this, &Lion ExplorerMainWindow::paste, actionCollection());
    // The text of the paste-action is modified dynamically by Lion Explorer
    // (e. g. to "Paste One Folder"). To prevent that the size of the toolbar changes
    // due to the long text, the text "Paste" is used:
    paste->setIconText(i18nc("@action:inmenu Edit", "Paste"));
    paste->setWhatsThis(xi18nc("@info:whatsthis paste",
                               "This copies the items from "
                               "your <emphasis>clipboard</emphasis> to the currently viewed folder.<nl/>"
                               "If the items were added to the clipboard by the <emphasis>Cut</emphasis> "
                               "action they are removed from their old location.")
                        + cutCopyPastePara);

    QAction *copyToOtherViewAction = actionCollection()->addAction(QStringLiteral("copy_to_inactive_split_view"));
    copyToOtherViewAction->setText(i18nc("@action:inmenu", "Copy to Other View"));
    m_actionTextHelper->registerTextWhenNothingIsSelected(copyToOtherViewAction, i18nc("@action:inmenu", "Copy to Other View…"));
    copyToOtherViewAction->setWhatsThis(xi18nc("@info:whatsthis Copy",
                                               "This copies the selected items from "
                                               "the view in focus to the other view. "
                                               "(Only available while in Split View mode.)"));
    copyToOtherViewAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    copyToOtherViewAction->setIconText(i18nc("@action:inmenu Edit", "Copy to Other View"));
    actionCollection()->setDefaultShortcut(copyToOtherViewAction, Qt::SHIFT | Qt::Key_F5);
    connect(copyToOtherViewAction, &QAction::triggered, this, &Lion ExplorerMainWindow::copyToInactiveSplitView);

    QAction *moveToOtherViewAction = actionCollection()->addAction(QStringLiteral("move_to_inactive_split_view"));
    moveToOtherViewAction->setText(i18nc("@action:inmenu", "Move to Other View"));
    m_actionTextHelper->registerTextWhenNothingIsSelected(moveToOtherViewAction, i18nc("@action:inmenu", "Move to Other View…"));
    moveToOtherViewAction->setWhatsThis(xi18nc("@info:whatsthis Move",
                                               "This moves the selected items from "
                                               "the view in focus to the other view. "
                                               "(Only available while in Split View mode.)"));
    moveToOtherViewAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-cut")));
    moveToOtherViewAction->setIconText(i18nc("@action:inmenu Edit", "Move to Other View"));
    actionCollection()->setDefaultShortcut(moveToOtherViewAction, Qt::SHIFT | Qt::Key_F6);
    connect(moveToOtherViewAction, &QAction::triggered, this, &Lion ExplorerMainWindow::moveToInactiveSplitView);

    QAction *showFilterBar = actionCollection()->addAction(QStringLiteral("show_filter_bar"));
    showFilterBar->setText(i18nc("@action:inmenu Tools", "Filter…"));
    showFilterBar->setToolTip(i18nc("@info:tooltip", "Show Filter Bar"));
    showFilterBar->setWhatsThis(xi18nc("@info:whatsthis",
                                       "This opens the "
                                       "<emphasis>Filter Bar</emphasis> at the bottom of the window.<nl/> "
                                       "There you can enter text to filter the files and folders currently displayed. "
                                       "Only those that contain the text in their name will be kept in view."));
    showFilterBar->setIcon(QIcon::fromTheme(QStringLiteral("view-filter")));
    actionCollection()->setDefaultShortcuts(showFilterBar, {Qt::CTRL | Qt::Key_I, Qt::Key_Slash});
    connect(showFilterBar, &QAction::triggered, this, &Lion ExplorerMainWindow::showFilterBar);

    // toggle_filter acts as a copy of the main showFilterBar to be used mainly
    // in the toolbar, with no default shortcut attached, to avoid messing with
    // existing workflows (filter bar always open and Ctrl-I to focus)
    QAction *toggleFilter = actionCollection()->addAction(QStringLiteral("toggle_filter"));
    toggleFilter->setText(i18nc("@action:inmenu", "Toggle Filter Bar"));
    toggleFilter->setIconText(i18nc("@action:intoolbar", "Filter"));
    toggleFilter->setIcon(showFilterBar->icon());
    toggleFilter->setToolTip(showFilterBar->toolTip());
    toggleFilter->setWhatsThis(showFilterBar->whatsThis());
    toggleFilter->setCheckable(true);
    connect(toggleFilter, &QAction::triggered, this, &Lion ExplorerMainWindow::toggleFilterBar);

    QAction *searchAction = KStandardAction::find(this, &Lion ExplorerMainWindow::find, actionCollection());
    searchAction->setText(i18n("Search…"));
    searchAction->setToolTip(i18nc("@info:tooltip", "Search for files and folders"));
    searchAction->setWhatsThis(xi18nc("@info:whatsthis find",
                                      "<para>This helps you "
                                      "find files and folders by opening a <emphasis>search bar</emphasis>. "
                                      "There you can enter search terms and specify settings to find the "
                                      "items you are looking for.</para>"));

    // toggle_search acts as a copy of the main searchAction to be used mainly
    // in the toolbar, with no default shortcut attached, to avoid messing with
    // existing workflows (search bar always open and Ctrl-F to focus)
    QAction *toggleSearchAction = actionCollection()->addAction(QStringLiteral("toggle_search"));
    toggleSearchAction->setText(i18nc("@action:inmenu", "Toggle Search Bar"));
    toggleSearchAction->setIconText(i18nc("@action:intoolbar", "Search"));
    toggleSearchAction->setIcon(searchAction->icon());
    toggleSearchAction->setToolTip(searchAction->toolTip());
    toggleSearchAction->setWhatsThis(searchAction->whatsThis());
    toggleSearchAction->setCheckable(true);
    connect(toggleSearchAction, &QAction::triggered, this, [this](bool checked) {
        if (checked) {
            find();
        } else {
            m_activeViewContainer->setSearchBarVisible(false);
        }
    });

    QAction *toggleSelectionModeAction = actionCollection()->addAction(QStringLiteral("toggle_selection_mode"));
    // i18n: This action toggles a selection mode.
    toggleSelectionModeAction->setText(i18nc("@action:inmenu", "Select Files and Folders"));
    // i18n: Opens a selection mode for selecting files/folders.
    // The text is kept so unspecific because it will be shown on the toolbar where space is at a premium.
    toggleSelectionModeAction->setIconText(i18nc("@action:intoolbar", "Select"));
    toggleSelectionModeAction->setWhatsThis(xi18nc(
        "@info:whatsthis",
        "<para>This application only knows which files or folders should be acted on if they are"
        " <emphasis>selected</emphasis> first. Press this to toggle a <emphasis>Selection Mode</emphasis> which makes selecting and deselecting as easy as "
        "pressing an item once.</para><para>While in this mode, a quick access bar at the bottom shows available actions for the currently selected items."
        "</para>"));
    toggleSelectionModeAction->setIcon(QIcon::fromTheme(QStringLiteral("quickwizard")));
    toggleSelectionModeAction->setCheckable(true);
    actionCollection()->setDefaultShortcut(toggleSelectionModeAction, Qt::Key_Space);
    connect(toggleSelectionModeAction, &QAction::triggered, this, &Lion ExplorerMainWindow::toggleSelectionMode);

    // A special version of the toggleSelectionModeAction for the toolbar that also contains a menu
    // with the selectAllAction and invertSelectionAction.
    auto *toggleSelectionModeToolBarAction =
        new KToolBarPopupAction(toggleSelectionModeAction->icon(), toggleSelectionModeAction->iconText(), actionCollection());
    toggleSelectionModeToolBarAction->setToolTip(toggleSelectionModeAction->text());
    toggleSelectionModeToolBarAction->setWhatsThis(toggleSelectionModeAction->whatsThis());
    actionCollection()->addAction(QStringLiteral("toggle_selection_mode_tool_bar"), toggleSelectionModeToolBarAction);
    toggleSelectionModeToolBarAction->setCheckable(true);
    toggleSelectionModeToolBarAction->setPopupMode(KToolBarPopupAction::DelayedPopup);
    connect(toggleSelectionModeToolBarAction, &QAction::triggered, toggleSelectionModeAction, &QAction::trigger);
    connect(toggleSelectionModeAction, &QAction::toggled, toggleSelectionModeToolBarAction, &QAction::setChecked);

    QAction *selectAllAction = KStandardAction::selectAll(this, &Lion ExplorerMainWindow::selectAll, actionCollection());
    selectAllAction->setWhatsThis(xi18nc("@info:whatsthis",
                                         "This selects all "
                                         "files and folders in the current location."));

    QAction *invertSelection = actionCollection()->addAction(QStringLiteral("invert_selection"));
    invertSelection->setText(i18nc("@action:inmenu Edit", "Invert Selection"));
    invertSelection->setWhatsThis(xi18nc("@info:whatsthis invert",
                                         "This selects all "
                                         "items that you have currently <emphasis>not</emphasis> selected instead."));
    invertSelection->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-invert")));
    actionCollection()->setDefaultShortcut(invertSelection, Qt::CTRL | Qt::SHIFT | Qt::Key_A);
    connect(invertSelection, &QAction::triggered, this, &Lion ExplorerMainWindow::invertSelection);

    QMenu *toggleSelectionModeActionMenu = new QMenu(this);
    toggleSelectionModeActionMenu->addAction(selectAllAction);
    toggleSelectionModeActionMenu->addAction(invertSelection);
    toggleSelectionModeToolBarAction->setMenu(toggleSelectionModeActionMenu);

    // setup 'View' menu
    // (note that most of it is set up in Lion ExplorerViewActionHandler)

    Admin::WorkerIntegration::createActAsAdminAction(actionCollection(), this);

    m_splitViewAction = actionCollection()->add<KActionMenu>(QStringLiteral("split_view"));
    m_splitViewMenuAction = actionCollection()->addAction(QStringLiteral("split_view_menu"));

    m_splitViewAction->setWhatsThis(xi18nc("@info:whatsthis split",
                                           "<para>This presents "
                                           "a second view side-by-side with the current view, so you can see "
                                           "the contents of two folders at once and easily move items between "
                                           "them.</para><para>The view that is not \"in focus\" will be dimmed. "
                                           "</para>Click this button again to close one of the views."));
    m_splitViewMenuAction->setWhatsThis(m_splitViewAction->whatsThis());

    // only set it for the menu version
    actionCollection()->setDefaultShortcut(m_splitViewMenuAction, Qt::Key_F3);

    connect(m_splitViewAction, &QAction::triggered, this, &Lion ExplorerMainWindow::toggleSplitView);
    connect(m_splitViewMenuAction, &QAction::triggered, this, &Lion ExplorerMainWindow::toggleSplitView);

    QAction *popoutSplit = actionCollection()->addAction(QStringLiteral("popout_split_view"));
    popoutSplit->setWhatsThis(xi18nc("@info:whatsthis",
                                     "If the view has been split, this will pop the view in focus "
                                     "out into a new window."));
    popoutSplit->setIcon(QIcon::fromTheme(QStringLiteral("window-new")));
    actionCollection()->setDefaultShortcut(popoutSplit, Qt::SHIFT | Qt::Key_F3);
    connect(popoutSplit, &QAction::triggered, this, &Lion ExplorerMainWindow::popoutSplitView);

    QAction *stashSplit = actionCollection()->addAction(QStringLiteral("split_stash"));
    actionCollection()->setDefaultShortcut(stashSplit, Qt::CTRL | Qt::Key_S);
    stashSplit->setText(i18nc("@action:intoolbar Stash", "Stash"));
    stashSplit->setToolTip(i18nc("@info", "Opens the stash virtual directory in a split window"));
    stashSplit->setIcon(QIcon::fromTheme(QStringLiteral("folder-stash")));
    stashSplit->setCheckable(false);
    QDBusConnectionInterface *sessionInterface = QDBusConnection::sessionBus().interface();
    stashSplit->setVisible(sessionInterface && sessionInterface->isServiceRegistered(QStringLiteral("org.kde.kio.StashNotifier")));
    connect(stashSplit, &QAction::triggered, this, &Lion ExplorerMainWindow::toggleSplitStash);

    QAction *redisplay = KStandardAction::redisplay(this, &Lion ExplorerMainWindow::reloadView, actionCollection());
    redisplay->setAutoRepeat(false);
    redisplay->setToolTip(i18nc("@info:tooltip", "Refresh view"));
    redisplay->setWhatsThis(xi18nc("@info:whatsthis refresh",
                                   "<para>This refreshes "
                                   "the folder view.</para>"
                                   "<para>If the contents of this folder have changed, refreshing will re-scan this folder "
                                   "and show you a newly-updated view of the files and folders contained here.</para>"
                                   "<para>If the view is split, this refreshes the one that is currently in focus.</para>"));

    QAction *stop = actionCollection()->addAction(QStringLiteral("stop"));
    stop->setText(i18nc("@action:inmenu View", "Stop"));
    stop->setToolTip(i18nc("@info", "Stop loading"));
    stop->setWhatsThis(i18nc("@info", "This stops the loading of the contents of the current folder."));
    stop->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
    connect(stop, &QAction::triggered, this, &Lion ExplorerMainWindow::stopLoading);

    KToggleAction *editableLocation = actionCollection()->add<KToggleAction>(QStringLiteral("editable_location"));
    editableLocation->setText(i18nc("@action:inmenu Navigation Bar", "Editable Location"));
    editableLocation->setWhatsThis(xi18nc("@info:whatsthis",
                                          "This toggles the <emphasis>Location Bar</emphasis> to be "
                                          "editable so you can directly enter a location you want to go to.<nl/>"
                                          "You can also switch to editing by clicking to the right of the "
                                          "location and switch back by confirming the edited location."));
    actionCollection()->setDefaultShortcut(editableLocation, Qt::Key_F6);
    connect(editableLocation, &KToggleAction::triggered, this, &Lion ExplorerMainWindow::toggleEditLocation);

    QAction *replaceLocation = actionCollection()->addAction(QStringLiteral("replace_location"));
    replaceLocation->setText(i18nc("@action:inmenu Navigation Bar", "Replace Location"));
    // i18n: "enter" is used both in the meaning of "writing" and "going to" a new location here.
    // Both meanings are useful but not necessary to understand the use of "Replace Location".
    // So you might want to be more verbose in your language to convey the meaning but it's up to you.
    replaceLocation->setWhatsThis(xi18nc("@info:whatsthis",
                                         "This switches to editing the location and selects it "
                                         "so you can quickly enter a different location."));
    actionCollection()->setDefaultShortcuts(replaceLocation, {Qt::CTRL | Qt::Key_L, Qt::ALT | Qt::Key_D});
    connect(replaceLocation, &QAction::triggered, this, &Lion ExplorerMainWindow::replaceLocation);

    // setup 'Go' menu
    {
        QScopedPointer<QAction> backAction(KStandardAction::back(nullptr, nullptr, nullptr));
        m_backAction = new KToolBarPopupAction(backAction->icon(), backAction->text(), actionCollection());
        m_backAction->setObjectName(backAction->objectName());
        m_backAction->setShortcuts(backAction->shortcuts());
    }
    m_backAction->setPopupMode(KToolBarPopupAction::DelayedPopup);
    connect(m_backAction, &QAction::triggered, this, &Lion ExplorerMainWindow::goBack);
    connect(m_backAction->popupMenu(), &QMenu::aboutToShow, this, &Lion ExplorerMainWindow::slotAboutToShowBackPopupMenu);
    connect(m_backAction->popupMenu(), &QMenu::triggered, this, &Lion ExplorerMainWindow::slotGoBack);
    actionCollection()->addAction(m_backAction->objectName(), m_backAction);

    auto backShortcuts = m_backAction->shortcuts();
    // Prepend this shortcut, to avoid being hidden by the two-slot UI (#371130)
    backShortcuts.prepend(QKeySequence(Qt::Key_Backspace));
    actionCollection()->setDefaultShortcuts(m_backAction, backShortcuts);

    Lion ExplorerRecentTabsMenu *recentTabsMenu = new Lion ExplorerRecentTabsMenu(this);
    actionCollection()->addAction(QStringLiteral("closed_tabs"), recentTabsMenu);
    connect(m_tabWidget, &Lion ExplorerTabWidget::rememberClosedTab, recentTabsMenu, &Lion ExplorerRecentTabsMenu::rememberClosedTab);
    connect(recentTabsMenu, &Lion ExplorerRecentTabsMenu::restoreClosedTab, m_tabWidget, &Lion ExplorerTabWidget::restoreClosedTab);
    connect(recentTabsMenu, &Lion ExplorerRecentTabsMenu::closedTabsCountChanged, this, &Lion ExplorerMainWindow::closedTabsCountChanged);

    QAction *undoCloseTab = actionCollection()->addAction(QStringLiteral("undo_close_tab"));
    undoCloseTab->setText(i18nc("@action:inmenu File", "Undo close tab"));
    undoCloseTab->setWhatsThis(i18nc("@info:whatsthis undo close tab", "This returns you to the previously closed tab."));
    actionCollection()->setDefaultShortcut(undoCloseTab, Qt::CTRL | Qt::SHIFT | Qt::Key_T);
    undoCloseTab->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
    undoCloseTab->setEnabled(false);
    connect(undoCloseTab, &QAction::triggered, recentTabsMenu, &Lion ExplorerRecentTabsMenu::undoCloseTab);

    auto undoAction = actionCollection()->action(KStandardAction::name(KStandardAction::Undo));
    undoAction->setWhatsThis(xi18nc("@info:whatsthis",
                                    "This undoes "
                                    "the last change you made to files or folders.<nl/>"
                                    "Such changes include <interface>creating</interface>, <interface>renaming</interface> "
                                    "and <interface>moving</interface> them to a different location "
                                    "or to the <filename>Trash</filename>. <nl/>Any changes that cannot be undone "
                                    "will ask for your confirmation beforehand."));
    undoAction->setEnabled(false); // undo should be disabled by default

#if KIO_VERSION >= QT_VERSION_CHECK(6, 17, 0)
    auto redoAction = actionCollection()->action(KStandardAction::name(KStandardAction::Redo));
    redoAction->setWhatsThis(xi18nc("@info:whatsthis",
                                    "This redoes "
                                    "the last change you undid.<nl/>"
                                    "Such changes include <interface>creating</interface>, <interface>renaming</interface> "
                                    "and <interface>moving</interface> files or folders to a different location "
                                    "or to the <filename>Trash</filename>.<nl/>Any changes that cannot be undone "
                                    "will ask for your confirmation beforehand."));
    redoAction->setEnabled(false); // redo should be disabled by default
#endif

    {
        QScopedPointer<QAction> forwardAction(KStandardAction::forward(nullptr, nullptr, nullptr));
        m_forwardAction = new KToolBarPopupAction(forwardAction->icon(), forwardAction->text(), actionCollection());
        m_forwardAction->setObjectName(forwardAction->objectName());
        m_forwardAction->setShortcuts(forwardAction->shortcuts());
    }
    m_forwardAction->setPopupMode(KToolBarPopupAction::DelayedPopup);
    connect(m_forwardAction, &QAction::triggered, this, &Lion ExplorerMainWindow::goForward);
    connect(m_forwardAction->popupMenu(), &QMenu::aboutToShow, this, &Lion ExplorerMainWindow::slotAboutToShowForwardPopupMenu);
    connect(m_forwardAction->popupMenu(), &QMenu::triggered, this, &Lion ExplorerMainWindow::slotGoForward);
    actionCollection()->addAction(m_forwardAction->objectName(), m_forwardAction);
    actionCollection()->setDefaultShortcuts(m_forwardAction, m_forwardAction->shortcuts());

    // enable middle-click to open in a new tab
    auto *middleClickEventFilter = new MiddleClickActionEventFilter(this);
    connect(middleClickEventFilter, &MiddleClickActionEventFilter::actionMiddleClicked, this, &Lion ExplorerMainWindow::slotBackForwardActionMiddleClicked);
    m_backAction->popupMenu()->installEventFilter(middleClickEventFilter);
    m_forwardAction->popupMenu()->installEventFilter(middleClickEventFilter);
    KStandardAction::up(this, &Lion ExplorerMainWindow::goUp, actionCollection());
    QAction *homeAction = KStandardAction::home(this, &Lion ExplorerMainWindow::goHome, actionCollection());
    homeAction->setWhatsThis(xi18nc("@info:whatsthis",
                                    "Go to your "
                                    "<filename>Home</filename> folder.<nl/>Every user account "
                                    "has their own <filename>Home</filename> that contains their personal files, "
                                    "as well as hidden folders for their applications' data and configuration files."));

    // setup 'Tools' menu
    QAction *compareFiles = actionCollection()->addAction(QStringLiteral("compare_files"));
    compareFiles->setText(i18nc("@action:inmenu Tools", "Compare Files"));
    compareFiles->setIcon(QIcon::fromTheme(QStringLiteral("kompare")));
    compareFiles->setEnabled(false);
    connect(compareFiles, &QAction::triggered, this, &Lion ExplorerMainWindow::compareFiles);

    QAction *manageDiskSpaceUsage = actionCollection()->addAction(QStringLiteral("manage_disk_space"));
    manageDiskSpaceUsage->setText(i18nc("@action:inmenu Tools", "Manage Disk Space Usage"));
    manageDiskSpaceUsage->setIcon(QIcon::fromTheme(QStringLiteral("filelight")));
    m_diskSpaceUsageMenu = new DiskSpaceUsageMenu{this};
    manageDiskSpaceUsage->setMenu(m_diskSpaceUsageMenu);

    QAction *openPreferredSearchTool = actionCollection()->addAction(QStringLiteral("open_preferred_search_tool"));
    openPreferredSearchTool->setText(i18nc("@action:inmenu Tools", "Open Preferred Search Tool"));
    openPreferredSearchTool->setWhatsThis(xi18nc("@info:whatsthis",
                                                 "<para>This opens a preferred search tool for the viewed location.</para>"
                                                 "<para>Use <emphasis>More Search Tools</emphasis> menu to configure it.</para>"));
    openPreferredSearchTool->setIcon(QIcon::fromTheme(QStringLiteral("search")));
    actionCollection()->setDefaultShortcut(openPreferredSearchTool, Qt::CTRL | Qt::SHIFT | Qt::Key_F);
    connect(openPreferredSearchTool, &QAction::triggered, this, &Lion ExplorerMainWindow::openPreferredSearchTool);

    if (KAuthorized::authorize(QStringLiteral("shell_access"))) {
        // Get icon of user default terminal emulator application
        const KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kdeglobals"), KConfig::SimpleConfig), QStringLiteral("General"));
        const QString terminalDesktopFilename = group.readEntry("TerminalService");
        // Use utilities-terminal icon from theme if readEntry() has failed
        const QString terminalIcon = terminalDesktopFilename.isEmpty() ? "utilities-terminal" : KDesktopFile(terminalDesktopFilename).readIcon();

        QAction *openTerminal = actionCollection()->addAction(QStringLiteral("open_terminal"));
        openTerminal->setText(i18nc("@action:inmenu Tools", "Open Terminal"));
        openTerminal->setWhatsThis(xi18nc("@info:whatsthis",
                                          "<para>This opens a <emphasis>terminal</emphasis> application for the viewed location.</para>"
                                          "<para>To learn more about terminals use the help features in the terminal application.</para>"));
        openTerminal->setIcon(QIcon::fromTheme(terminalIcon));
        actionCollection()->setDefaultShortcut(openTerminal, Qt::SHIFT | Qt::Key_F4);
        connect(openTerminal, &QAction::triggered, this, &Lion ExplorerMainWindow::openTerminal);

        QAction *openTerminalHere = actionCollection()->addAction(QStringLiteral("open_terminal_here"));
        // i18n: "Here" refers to the location(s) of the currently selected item(s) or the currently viewed location if nothing is selected.
        openTerminalHere->setText(i18nc("@action:inmenu Tools", "Open Terminal Here"));
        openTerminalHere->setWhatsThis(xi18nc("@info:whatsthis",
                                              "<para>This opens <emphasis>terminal</emphasis> applications for the selected items' locations.</para>"
                                              "<para>To learn more about terminals use the help features in the terminal application.</para>"));
        openTerminalHere->setIcon(QIcon::fromTheme(terminalIcon));
        actionCollection()->setDefaultShortcut(openTerminalHere, Qt::SHIFT | Qt::ALT | Qt::Key_F4);
        connect(openTerminalHere, &QAction::triggered, this, &Lion ExplorerMainWindow::openTerminalHere);
    }

    // setup 'Bookmarks' menu
    KActionMenu *bookmarkMenu = new KActionMenu(i18nc("@title:menu", "&Bookmarks"), this);
    bookmarkMenu->setIcon(QIcon::fromTheme(QStringLiteral("bookmarks")));
    // Make the toolbar button version work properly on click
    bookmarkMenu->setPopupMode(QToolButton::InstantPopup);
    m_bookmarkHandler = new Lion ExplorerBookmarkHandler(this, actionCollection(), bookmarkMenu->menu(), this);
    actionCollection()->addAction(QStringLiteral("bookmarks"), bookmarkMenu);

    // setup 'Settings' menu
    KToggleAction *showMenuBar = KStandardAction::showMenubar(nullptr, nullptr, actionCollection());
    showMenuBar->setWhatsThis(xi18nc("@info:whatsthis",
                                     "<para>This switches between having a <emphasis>Menubar</emphasis> "
                                     "and having an <interface>%1</interface> button. Both "
                                     "contain mostly the same actions and configuration options.</para>"
                                     "<para>The Menubar takes up more space but allows for fast and organized access to all "
                                     "actions an application has to offer.</para><para>The %1 button "
                                     "is simpler and small which makes triggering advanced actions more time consuming.</para>",
                                     hamburgerMenuAction->text().replace('&', "")));
    connect(showMenuBar,
            &KToggleAction::triggered, // Fixes #286822
            this,
            &Lion ExplorerMainWindow::toggleShowMenuBar,
            Qt::QueuedConnection);

    KStandardAction::keyBindings(this, &Lion ExplorerMainWindow::slotKeyBindings, actionCollection());
    KStandardAction::preferences(this, &Lion ExplorerMainWindow::editSettings, actionCollection());

    // not in menu actions
    QList<QKeySequence> nextTabKeys = KStandardShortcut::tabNext();
    nextTabKeys.append(QKeySequence(Qt::CTRL | Qt::Key_Tab));

    QList<QKeySequence> prevTabKeys = KStandardShortcut::tabPrev();
    prevTabKeys.append(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));

    for (int i = 0; i < MaxActivateTabShortcuts; ++i) {
        QAction *activateTab = actionCollection()->addAction(QStringLiteral("activate_tab_%1").arg(i));
        activateTab->setText(i18nc("@action:inmenu", "Go to Tab %1", i + 1));
        activateTab->setEnabled(false);
        connect(activateTab, &QAction::triggered, this, [this, i]() {
            m_tabWidget->activateTab(i);
        });

        // only add default shortcuts for the first 9 tabs regardless of MaxActivateTabShortcuts
        if (i < 9) {
            actionCollection()->setDefaultShortcut(activateTab, QStringLiteral("Alt+%1").arg(i + 1));
        }
    }

    QAction *activateLastTab = actionCollection()->addAction(QStringLiteral("activate_last_tab"));
    activateLastTab->setIconText(i18nc("@action:inmenu", "Last Tab"));
    activateLastTab->setText(i18nc("@action:inmenu", "Go to Last Tab"));
    activateLastTab->setEnabled(false);
    connect(activateLastTab, &QAction::triggered, m_tabWidget, &Lion ExplorerTabWidget::activateLastTab);
    actionCollection()->setDefaultShortcut(activateLastTab, Qt::ALT | Qt::Key_0);

    QAction *activateNextTab = actionCollection()->addAction(QStringLiteral("activate_next_tab"));
    activateNextTab->setIconText(i18nc("@action:inmenu", "Next Tab"));
    activateNextTab->setText(i18nc("@action:inmenu", "Go to Next Tab"));
    activateNextTab->setEnabled(false);
    connect(activateNextTab, &QAction::triggered, m_tabWidget, &Lion ExplorerTabWidget::activateNextTab);
    actionCollection()->setDefaultShortcuts(activateNextTab, nextTabKeys);

    QAction *activatePrevTab = actionCollection()->addAction(QStringLiteral("activate_prev_tab"));
    activatePrevTab->setIconText(i18nc("@action:inmenu", "Previous Tab"));
    activatePrevTab->setText(i18nc("@action:inmenu", "Go to Previous Tab"));
    activatePrevTab->setEnabled(false);
    connect(activatePrevTab, &QAction::triggered, m_tabWidget, &Lion ExplorerTabWidget::activatePrevTab);
    actionCollection()->setDefaultShortcuts(activatePrevTab, prevTabKeys);

    // for context menu
    QAction *showTarget = actionCollection()->addAction(QStringLiteral("show_target"));
    showTarget->setText(i18nc("@action:inmenu", "Show Target"));
    showTarget->setIcon(QIcon::fromTheme(QStringLiteral("document-open-folder")));
    showTarget->setEnabled(false);
    connect(showTarget, &QAction::triggered, this, &Lion ExplorerMainWindow::showTarget);

    QAction *openInNewTab = actionCollection()->addAction(QStringLiteral("open_in_new_tab"));
    openInNewTab->setText(i18nc("@action:inmenu", "Open in New Tab"));
    openInNewTab->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    connect(openInNewTab, &QAction::triggered, this, &Lion ExplorerMainWindow::openInNewTab);

    QAction *openInNewTabs = actionCollection()->addAction(QStringLiteral("open_in_new_tabs"));
    openInNewTabs->setText(i18nc("@action:inmenu", "Open in New Tabs"));
    openInNewTabs->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    connect(openInNewTabs, &QAction::triggered, this, &Lion ExplorerMainWindow::openInNewTab);

    QAction *openInNewWindow = actionCollection()->addAction(QStringLiteral("open_in_new_window"));
    openInNewWindow->setText(i18nc("@action:inmenu", "Open in New Window"));
    openInNewWindow->setIcon(QIcon::fromTheme(QStringLiteral("window-new")));
    connect(openInNewWindow, &QAction::triggered, this, &Lion ExplorerMainWindow::openInNewWindow);

    QAction *openInSplitViewAction = actionCollection()->addAction(QStringLiteral("open_in_split_view"));
    openInSplitViewAction->setText(i18nc("@action:inmenu", "Open in Split View"));
    openInSplitViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    connect(openInSplitViewAction, &QAction::triggered, this, [this]() {
        openInSplitView(QUrl());
    });

    // Window color scheme menu
    auto *manager = KColorSchemeManager::instance();
    KActionMenu *selectionMenu = KColorSchemeMenu::createMenu(manager, this);
    auto windowColorSchemeMenu = new QAction(this);
    windowColorSchemeMenu->setMenu(selectionMenu->menu());
    windowColorSchemeMenu->menu()->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-color")));
    windowColorSchemeMenu->menu()->setTitle(i18n("&Window Color Scheme"));
    actionCollection()->addAction(QStringLiteral("window_color_sheme"), windowColorSchemeMenu);

    m_recentFiles = new KRecentFilesAction(this);
}

void Lion ExplorerMainWindow::setupDockWidgets()
{
    const bool lock = GeneralSettings::lockPanels();

    Lion ExplorerPlacesModelSingleton::instance().placesModel()->setPanelsLocked(lock);

    KDualAction *lockLayoutAction = actionCollection()->add<KDualAction>(QStringLiteral("lock_panels"));
    lockLayoutAction->setActiveText(i18nc("@action:inmenu Panels", "Unlock Panels"));
    lockLayoutAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("object-unlocked")));
    lockLayoutAction->setInactiveText(i18nc("@action:inmenu Panels", "Lock Panels"));
    lockLayoutAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("object-locked")));
    lockLayoutAction->setWhatsThis(xi18nc("@info:whatsthis",
                                          "This "
                                          "switches between having panels <emphasis>locked</emphasis> or "
                                          "<emphasis>unlocked</emphasis>.<nl/>Unlocked panels can be "
                                          "dragged to the other side of the window and have a close "
                                          "button.<nl/>Locked panels are embedded more cleanly."));
    lockLayoutAction->setActive(lock);
    connect(lockLayoutAction, &KDualAction::triggered, this, &Lion ExplorerMainWindow::togglePanelLockState);

    // Setup "Information"
    Lion ExplorerDockWidget *infoDock = new Lion ExplorerDockWidget(i18nc("@title:window", "Information"), this);
    infoDock->setLocked(lock);
    infoDock->setObjectName(QStringLiteral("infoDock"));
    infoDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

#if HAVE_BALOO
    InformationPanel *infoPanel = new InformationPanel(infoDock);
    infoPanel->setCustomContextMenuActions({lockLayoutAction});
    connect(infoPanel, &InformationPanel::urlActivated, this, &Lion ExplorerMainWindow::handleUrl);
    infoDock->setWidget(infoPanel);

    createPanelAction(QIcon::fromTheme(QStringLiteral("documentinfo")), Qt::Key_F11, infoDock, QStringLiteral("show_information_panel"));

    addDockWidget(Qt::RightDockWidgetArea, infoDock);
    connect(this, &Lion ExplorerMainWindow::urlChanged, infoPanel, &InformationPanel::setUrl);
    connect(this, &Lion ExplorerMainWindow::selectionChanged, infoPanel, &InformationPanel::setSelection);
    connect(this, &Lion ExplorerMainWindow::requestItemInfo, infoPanel, &InformationPanel::requestDelayedItemInfo);
    connect(this, &Lion ExplorerMainWindow::fileItemsChanged, infoPanel, &InformationPanel::slotFilesItemChanged);
    connect(this, &Lion ExplorerMainWindow::settingsChanged, infoPanel, &InformationPanel::readSettings);
#endif

    // i18n: This is the last paragraph for the "What's This"-texts of all four panels.
    const QString panelWhatsThis = xi18nc("@info:whatsthis",
                                          "<para>To show or "
                                          "hide panels like this go to <interface>Menu|Panels</interface> "
                                          "or <interface>View|Panels</interface>.</para>");
#if HAVE_BALOO
    actionCollection()
        ->action(QStringLiteral("show_information_panel"))
        ->setWhatsThis(xi18nc("@info:whatsthis",
                              "<para> This toggles the "
                              "<emphasis>information</emphasis> panel at the right side of the "
                              "window.</para><para>The panel provides in-depth information "
                              "about the items your mouse is hovering over or about the selected "
                              "items. Otherwise it informs you about the currently viewed folder.<nl/>"
                              "For single items a preview of their contents is provided.</para>"));
#endif
    infoDock->setWhatsThis(xi18nc("@info:whatsthis",
                                  "<para>This panel "
                                  "provides in-depth information about the items your mouse is "
                                  "hovering over or about the selected items. Otherwise it informs "
                                  "you about the currently viewed folder.<nl/>For single items a "
                                  "preview of their contents is provided.</para><para>You can configure "
                                  "which and how details are given here by right-clicking.</para>")
                           + panelWhatsThis);

    // Setup "Folders"
    Lion ExplorerDockWidget *foldersDock = new Lion ExplorerDockWidget(i18nc("@title:window", "Folders"));
    foldersDock->setLocked(lock);
    foldersDock->setObjectName(QStringLiteral("foldersDock"));
    foldersDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    FoldersPanel *foldersPanel = new FoldersPanel(foldersDock);
    foldersPanel->setCustomContextMenuActions({lockLayoutAction});
    foldersDock->setWidget(foldersPanel);

    createPanelAction(QIcon::fromTheme(QStringLiteral("folder")), Qt::Key_F7, foldersDock, QStringLiteral("show_folders_panel"));

    addDockWidget(Qt::LeftDockWidgetArea, foldersDock);
    connect(this, &Lion ExplorerMainWindow::urlChanged, foldersPanel, &FoldersPanel::setUrl);
    connect(foldersPanel, &FoldersPanel::folderActivated, this, &Lion ExplorerMainWindow::changeUrl);
    connect(foldersPanel, &FoldersPanel::folderInNewTab, this, &Lion ExplorerMainWindow::openNewTab);
    connect(foldersPanel, &FoldersPanel::folderInNewActiveTab, this, &Lion ExplorerMainWindow::openNewTabAndActivate);
    connect(foldersPanel, &FoldersPanel::errorMessage, this, &Lion ExplorerMainWindow::showErrorMessage);

    actionCollection()
        ->action(QStringLiteral("show_folders_panel"))
        ->setWhatsThis(xi18nc("@info:whatsthis",
                              "This toggles the "
                              "<emphasis>folders</emphasis> panel at the left side of the window."
                              "<nl/><nl/>It shows the folders of the <emphasis>file system"
                              "</emphasis> in a <emphasis>tree view</emphasis>."));
    foldersDock->setWhatsThis(xi18nc("@info:whatsthis",
                                     "<para>This panel "
                                     "shows the folders of the <emphasis>file system</emphasis> in a "
                                     "<emphasis>tree view</emphasis>.</para><para>Click a folder to go "
                                     "there. Click the arrow to the left of a folder to see its subfolders. "
                                     "This allows quick switching between any folders.</para>")
                              + panelWhatsThis);

    // Setup "Terminal"
#if HAVE_TERMINAL
    if (KAuthorized::authorize(QStringLiteral("shell_access"))) {
        Lion ExplorerDockWidget *terminalDock = new Lion ExplorerDockWidget(i18nc("@title:window Shell terminal", "Terminal"));
        terminalDock->setLocked(lock);
        terminalDock->setObjectName(QStringLiteral("terminalDock"));
        terminalDock->setContentsMargins(0, 0, 0, 0);
        m_terminalPanel = new TerminalPanel(terminalDock);
        m_terminalPanel->setCustomContextMenuActions({lockLayoutAction});
        terminalDock->setWidget(m_terminalPanel);

        connect(m_terminalPanel, &TerminalPanel::hideTerminalPanel, terminalDock, &Lion ExplorerDockWidget::hide);
        connect(m_terminalPanel, &TerminalPanel::changeUrl, this, &Lion ExplorerMainWindow::slotTerminalDirectoryChanged);
        connect(terminalDock, &Lion ExplorerDockWidget::visibilityChanged, m_terminalPanel, &TerminalPanel::dockVisibilityChanged);
        connect(terminalDock, &Lion ExplorerDockWidget::visibilityChanged, this, &Lion ExplorerMainWindow::slotTerminalPanelVisibilityChanged);

        createPanelAction(QIcon::fromTheme(QStringLiteral("dialog-scripts")), Qt::Key_F4, terminalDock, QStringLiteral("show_terminal_panel"));

        addDockWidget(Qt::BottomDockWidgetArea, terminalDock);
        connect(this, &Lion ExplorerMainWindow::urlChanged, m_terminalPanel, &TerminalPanel::setUrl);
        connect(this, &Lion ExplorerMainWindow::urlRefreshed, m_terminalPanel, &TerminalPanel::refreshUrl);

        if (GeneralSettings::version() < 200) {
            terminalDock->hide();
        }

        actionCollection()
            ->action(QStringLiteral("show_terminal_panel"))
            ->setWhatsThis(xi18nc("@info:whatsthis",
                                  "<para>This toggles the "
                                  "<emphasis>terminal</emphasis> panel at the bottom of the window."
                                  "<nl/>The location in the terminal will always match the folder "
                                  "view so you can navigate using either.</para><para>The terminal "
                                  "panel is not needed for basic computer usage but can be useful "
                                  "for advanced tasks. To learn more about terminals use the help features "
                                  "in a standalone terminal application like Konsole.</para>"));
        terminalDock->setWhatsThis(xi18nc("@info:whatsthis",
                                          "<para>This is "
                                          "the <emphasis>terminal</emphasis> panel. It behaves like a "
                                          "normal terminal but will match the location of the folder view "
                                          "so you can navigate using either.</para><para>The terminal panel "
                                          "is not needed for basic computer usage but can be useful for "
                                          "advanced tasks. To learn more about terminals use the help features in a "
                                          "standalone terminal application like Konsole.</para>")
                                   + panelWhatsThis);

        QAction *focusTerminalPanel = actionCollection()->addAction(QStringLiteral("focus_terminal_panel"));
        focusTerminalPanel->setText(i18nc("@action:inmenu Tools", "Focus Terminal Panel"));
        focusTerminalPanel->setToolTip(i18nc("@info:tooltip", "Move keyboard focus to and from the Terminal panel."));
        focusTerminalPanel->setIcon(QIcon::fromTheme(QStringLiteral("swap-panels")));
        actionCollection()->setDefaultShortcut(focusTerminalPanel, Qt::CTRL | Qt::SHIFT | Qt::Key_F4);
        connect(focusTerminalPanel, &QAction::triggered, this, &Lion ExplorerMainWindow::toggleTerminalPanelFocus);

        QAction *switchTerminalUrlSync = actionCollection()->addAction(QStringLiteral("switch_terminal_url_sync"));
        switchTerminalUrlSync->setText(i18nc("@action:inmenu", "Follow Directory Switch"));
        switchTerminalUrlSync->setToolTip(
            i18nc("@info:tooltip", "Determines if current working directory must be kept in sync with terminal whenever directory is changed."));
        switchTerminalUrlSync->setCheckable(true);
        connect(switchTerminalUrlSync, &QAction::toggled, m_terminalPanel, &TerminalPanel::switchSync);
        switchTerminalUrlSync->setChecked(true);

        m_terminalPanel->setSwitchTerminalUrlSyncAction(switchTerminalUrlSync);

    } // endif "shell_access" allowed
#endif // HAVE_TERMINAL

    if (GeneralSettings::version() < 200) {
        infoDock->hide();
        foldersDock->hide();
    }

    // Setup "Places"
    Lion ExplorerDockWidget *placesDock = new Lion ExplorerDockWidget(i18nc("@title:window", "Places"));
    placesDock->setLocked(lock);
    placesDock->setObjectName(QStringLiteral("placesDock"));
    placesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_placesPanel = new PlacesPanel(placesDock);
    m_placesPanel->setCustomContextMenuActions({lockLayoutAction});
    placesDock->setWidget(m_placesPanel);

    createPanelAction(QIcon::fromTheme(QStringLiteral("compass")), Qt::Key_F9, placesDock, QStringLiteral("show_places_panel"));

    addDockWidget(Qt::LeftDockWidgetArea, placesDock);
    connect(m_placesPanel, &PlacesPanel::placeActivated, this, &Lion ExplorerMainWindow::slotPlaceActivated);
    connect(m_placesPanel, &PlacesPanel::tabRequested, this, &Lion ExplorerMainWindow::openNewTab);
    connect(m_placesPanel, &PlacesPanel::activeTabRequested, this, &Lion ExplorerMainWindow::openNewTabAndActivate);
    connect(m_placesPanel, &PlacesPanel::newWindowRequested, this, [this](const QUrl &url) {
        Lion Explorer::openNewWindow({url}, this);
    });
    connect(m_placesPanel, &PlacesPanel::openInSplitViewRequested, this, &Lion ExplorerMainWindow::openInSplitView);
    connect(m_placesPanel, &PlacesPanel::errorMessage, this, &Lion ExplorerMainWindow::showErrorMessage);
    connect(this, &Lion ExplorerMainWindow::urlChanged, m_placesPanel, &PlacesPanel::setUrl);
    connect(placesDock, &Lion ExplorerDockWidget::visibilityChanged, &Lion ExplorerUrlNavigatorsController::slotPlacesPanelVisibilityChanged);
    connect(placesDock, &Lion ExplorerDockWidget::visibilityChanged, this, &Lion ExplorerMainWindow::slotPlacesPanelVisibilityChanged);
    connect(this, &Lion ExplorerMainWindow::settingsChanged, m_placesPanel, &PlacesPanel::readSettings);
    connect(m_placesPanel, &PlacesPanel::storageTearDownRequested, this, &Lion ExplorerMainWindow::slotStorageTearDownFromPlacesRequested);
    connect(m_placesPanel, &PlacesPanel::storageTearDownExternallyRequested, this, &Lion ExplorerMainWindow::slotStorageTearDownExternallyRequested);
    Lion ExplorerUrlNavigatorsController::slotPlacesPanelVisibilityChanged(m_placesPanel->isVisible());

    auto actionShowAllPlaces = new QAction(QIcon::fromTheme(QStringLiteral("view-hidden")), i18nc("@item:inmenu", "Show Hidden Places"), this);
    actionShowAllPlaces->setCheckable(true);
    actionShowAllPlaces->setDisabled(true);
    actionShowAllPlaces->setWhatsThis(i18nc("@info:whatsthis",
                                            "This displays "
                                            "all places in the places panel that have been hidden. They will "
                                            "appear semi-transparent and allow you to uncheck their \"Hide\" property."));

    connect(actionShowAllPlaces, &QAction::triggered, this, [this](bool checked) {
        m_placesPanel->setShowAll(checked);
    });
    connect(m_placesPanel, &PlacesPanel::allPlacesShownChanged, actionShowAllPlaces, &QAction::setChecked);

    actionCollection()
        ->action(QStringLiteral("show_places_panel"))
        ->setWhatsThis(xi18nc("@info:whatsthis",
                              "<para>This toggles the "
                              "<emphasis>places</emphasis> panel at the left side of the window."
                              "</para><para>It allows you to go to locations you have "
                              "bookmarked and to access disk or media attached to the computer "
                              "or to the network. It also contains sections to find recently "
                              "saved files or files of a certain type.</para>"));
    placesDock->setWhatsThis(xi18nc("@info:whatsthis",
                                    "<para>This is the "
                                    "<emphasis>Places</emphasis> panel. It allows you to go to locations "
                                    "you have bookmarked and to access disk or media attached to the "
                                    "computer or to the network. It also contains sections to find "
                                    "recently saved files or files of a certain type.</para><para>"
                                    "Click on an entry to go there. Click with the right mouse button "
                                    "instead to open any entry in a new tab or new window.</para>"
                                    "<para>New entries can be added by dragging folders onto this panel. "
                                    "Right-click any section or entry to hide it. Right-click an empty "
                                    "space on this panel and select <interface>Show Hidden Places"
                                    "</interface> to display it again.</para>")
                             + panelWhatsThis);

    QAction *focusPlacesPanel = actionCollection()->addAction(QStringLiteral("focus_places_panel"));
    focusPlacesPanel->setText(i18nc("@action:inmenu View", "Focus Places Panel"));
    focusPlacesPanel->setToolTip(i18nc("@info:tooltip", "Move keyboard focus to and from the Places panel."));
    focusPlacesPanel->setIcon(QIcon::fromTheme(QStringLiteral("swap-panels")));
    actionCollection()->setDefaultShortcut(focusPlacesPanel, Qt::CTRL | Qt::Key_P);
    connect(focusPlacesPanel, &QAction::triggered, this, &Lion ExplorerMainWindow::togglePlacesPanelFocus);

    // Add actions into the "Panels" menu
    KActionMenu *panelsMenu = new KActionMenu(i18nc("@action:inmenu View", "Show Panels"), this);
    actionCollection()->addAction(QStringLiteral("panels"), panelsMenu);
    panelsMenu->setIcon(QIcon::fromTheme(QStringLiteral("view-sidetree")));
    panelsMenu->setPopupMode(QToolButton::InstantPopup);
    const KActionCollection *ac = actionCollection();
    panelsMenu->addAction(ac->action(QStringLiteral("show_places_panel")));
#if HAVE_BALOO
    panelsMenu->addAction(ac->action(QStringLiteral("show_information_panel")));
#endif
    panelsMenu->addAction(ac->action(QStringLiteral("show_folders_panel")));
    panelsMenu->addAction(ac->action(QStringLiteral("show_terminal_panel")));
    panelsMenu->addSeparator();
    panelsMenu->addAction(lockLayoutAction);
    panelsMenu->addSeparator();
    panelsMenu->addAction(actionShowAllPlaces);
    panelsMenu->addAction(focusPlacesPanel);
    panelsMenu->addAction(ac->action(QStringLiteral("focus_terminal_panel")));

    connect(panelsMenu->menu(), &QMenu::aboutToShow, this, [actionShowAllPlaces] {
        actionShowAllPlaces->setEnabled(Lion ExplorerPlacesModelSingleton::instance().placesModel()->hiddenCount());
    });
}

void Lion ExplorerMainWindow::setupFileItemActions()
{
    if (m_fileItemActions) {
        delete m_fileItemActions;
    }

    m_fileItemActions = new KFileItemActions(this);
    m_fileItemActions->setParentWidget(this);
    connect(m_fileItemActions, &KFileItemActions::error, this, [this](const QString &errorMessage) {
        showErrorMessage(errorMessage);
    });

#if KIO_VERSION >= QT_VERSION_CHECK(6, 24, 0)
    m_serviceMenuShortcutManager->refresh(m_fileItemActions);
#endif
}

void Lion ExplorerMainWindow::updateFileAndEditActions()
{
    const KFileItemList list = m_activeViewContainer->view()->selectedItems();
    const KActionCollection *col = actionCollection();
    KFileItemListProperties capabilitiesSource(list);

    QAction *renameAction = col->action(KStandardAction::name(KStandardAction::RenameFile));
    QAction *moveToTrashAction = col->action(KStandardAction::name(KStandardAction::MoveToTrash));
    QAction *deleteAction = col->action(KStandardAction::name(KStandardAction::DeleteFile));
    QAction *cutAction = col->action(KStandardAction::name(KStandardAction::Cut));
    QAction *duplicateAction = col->action(QStringLiteral("duplicate")); // see Lion ExplorerViewActionHandler
    QAction *addToPlacesAction = col->action(QStringLiteral("add_to_places"));
    QAction *copyToOtherViewAction = col->action(QStringLiteral("copy_to_inactive_split_view"));
    QAction *moveToOtherViewAction = col->action(QStringLiteral("move_to_inactive_split_view"));
    QAction *copyLocation = col->action(QStringLiteral("copy_location"));

    if (list.isEmpty()) {
        stateChanged(QStringLiteral("has_no_selection"));

        // All actions that need a selection to function can be enabled because they should trigger selection mode.
        renameAction->setEnabled(true);
        moveToTrashAction->setEnabled(true);
        deleteAction->setEnabled(true);
        cutAction->setEnabled(true);
        duplicateAction->setEnabled(true);
        addToPlacesAction->setEnabled(true);
        copyLocation->setEnabled(true);
        // Them triggering selection mode and not directly acting on selected items is signified by adding "…" to their text.
        m_actionTextHelper->textsWhenNothingIsSelectedEnabled(true);

    } else {
        m_actionTextHelper->textsWhenNothingIsSelectedEnabled(false);
        stateChanged(QStringLiteral("has_selection"));

        QAction *deleteWithTrashShortcut = col->action(QStringLiteral("delete_shortcut")); // see Lion ExplorerViewActionHandler
        QAction *showTarget = col->action(QStringLiteral("show_target"));

        if (list.length() == 1 && list.first().isDir()) {
            addToPlacesAction->setEnabled(true);
        } else {
            addToPlacesAction->setEnabled(false);
        }

        const bool enableMoveToTrash = capabilitiesSource.isLocal() && capabilitiesSource.supportsMoving();

        renameAction->setEnabled(capabilitiesSource.supportsMoving());
        m_disabledActionNotifier->setDisabledReason(renameAction, i18nc("@info", "Cannot rename: You do not have permission to rename items in this folder."));
        deleteAction->setEnabled(capabilitiesSource.supportsDeleting());
        m_disabledActionNotifier->setDisabledReason(deleteAction,
                                                    i18nc("@info", "Cannot delete: You do not have permission to remove items from this folder."));
        cutAction->setEnabled(capabilitiesSource.supportsMoving());
        m_disabledActionNotifier->setDisabledReason(cutAction, i18nc("@info", "Cannot cut: You do not have permission to move items from this folder."));
        copyLocation->setEnabled(list.length() == 1);
        showTarget->setEnabled(list.length() == 1 && list.at(0).isLink());
        duplicateAction->setEnabled(capabilitiesSource.supportsWriting());
        m_disabledActionNotifier->setDisabledReason(duplicateAction,
                                                    i18nc("@info", "Cannot duplicate here: You do not have permission to create items in this folder."));

        if (enableMoveToTrash) {
            moveToTrashAction->setEnabled(true);
            deleteWithTrashShortcut->setEnabled(false);
            m_disabledActionNotifier->clearDisabledReason(deleteWithTrashShortcut);
        } else {
            moveToTrashAction->setEnabled(false);
            deleteWithTrashShortcut->setEnabled(capabilitiesSource.supportsDeleting());
            m_disabledActionNotifier->setDisabledReason(deleteWithTrashShortcut,
                                                        i18nc("@info", "Cannot delete: You do not have permission to remove items from this folder."));
        }
    }

    if (!m_tabWidget->currentTabPage()->splitViewEnabled()) {
        // No need to set the disabled reason here, as it's obvious to the user that the reason is the split view being disabled.
        copyToOtherViewAction->setEnabled(false);
        m_disabledActionNotifier->clearDisabledReason(copyToOtherViewAction);
        moveToOtherViewAction->setEnabled(false);
        m_disabledActionNotifier->clearDisabledReason(moveToOtherViewAction);
    } else if (list.isEmpty()) {
        copyToOtherViewAction->setEnabled(false);
        m_disabledActionNotifier->setDisabledReason(copyToOtherViewAction, i18nc("@info", "Cannot copy to other view: No files selected."));
        moveToOtherViewAction->setEnabled(false);
        m_disabledActionNotifier->setDisabledReason(moveToOtherViewAction, i18nc("@info", "Cannot move to other view: No files selected."));
    } else {
        Lion ExplorerTabPage *tabPage = m_tabWidget->currentTabPage();
        KFileItem capabilitiesDestination;

        if (tabPage->primaryViewActive()) {
            capabilitiesDestination = tabPage->secondaryViewContainer()->rootItem();
        } else {
            capabilitiesDestination = tabPage->primaryViewContainer()->rootItem();
        }

        const auto destUrl = capabilitiesDestination.url();
        const bool allNotTargetOrigin = std::all_of(list.cbegin(), list.cend(), [destUrl](const KFileItem &item) {
            return item.url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash) != destUrl;
        });

        if (!allNotTargetOrigin) {
            copyToOtherViewAction->setEnabled(false);
            m_disabledActionNotifier->setDisabledReason(copyToOtherViewAction,
                                                        i18nc("@info", "Cannot copy to other view: The other view already contains these items."));
            moveToOtherViewAction->setEnabled(false);
            m_disabledActionNotifier->setDisabledReason(moveToOtherViewAction,
                                                        i18nc("@info", "Cannot move to other view: The other view already contains these items."));
        } else if (!capabilitiesDestination.isWritable()) {
            copyToOtherViewAction->setEnabled(false);
            m_disabledActionNotifier->setDisabledReason(
                copyToOtherViewAction,
                i18nc("@info", "Cannot copy to other view: You do not have permission to write into the destination folder."));
            moveToOtherViewAction->setEnabled(false);
            m_disabledActionNotifier->setDisabledReason(
                moveToOtherViewAction,
                i18nc("@info", "Cannot move to other view: You do not have permission to write into the destination folder."));
        } else {
            copyToOtherViewAction->setEnabled(true);
            moveToOtherViewAction->setEnabled(capabilitiesSource.supportsMoving());
            m_disabledActionNotifier->setDisabledReason(
                moveToOtherViewAction,
                i18nc("@info", "Cannot move to other view: You do not have permission to move items from this folder."));
        }
    }
}

void Lion ExplorerMainWindow::updateViewActions()
{
    m_actionHandler->updateViewActions();

    QAction *toggleFilterBarAction = actionCollection()->action(QStringLiteral("toggle_filter"));
    toggleFilterBarAction->setChecked(m_activeViewContainer->isFilterBarVisible());

    updateSplitActions();
}

void Lion ExplorerMainWindow::updateGoActions()
{
    QAction *goUpAction = actionCollection()->action(KStandardAction::name(KStandardAction::Up));
    const QUrl currentUrl = m_activeViewContainer->url();
    // I think this is one of the best places to firstly be confronted
    // with a file system and its hierarchy. Talking about the root
    // directory might seem too much here but it is the question that
    // naturally arises in this context.
    goUpAction->setWhatsThis(xi18nc("@info:whatsthis",
                                    "<para>Go to "
                                    "the folder that contains the currently viewed one.</para>"
                                    "<para>All files and folders are organized in a hierarchical "
                                    "<emphasis>file system</emphasis>. At the top of this hierarchy is "
                                    "a directory that contains all data connected to this computer"
                                    "—the <emphasis>root directory</emphasis>.</para>"));
    goUpAction->setEnabled(KIO::upUrl(currentUrl) != currentUrl);
}

void Lion ExplorerMainWindow::refreshViews()
{
    setupFileItemActions();
    m_tabWidget->refreshViews();

    if (GeneralSettings::modifiedStartupSettings()) {
        updateWindowTitle();
    }

    updateSplitActions();

    Q_EMIT settingsChanged();
}

void Lion ExplorerMainWindow::clearStatusBar()
{
    m_activeViewContainer->statusBar()->resetToDefaultText();
}

void Lion ExplorerMainWindow::connectViewSignals(Lion ExplorerViewContainer *container)
{
    connect(container, &Lion ExplorerViewContainer::showFilterBarChanged, this, &Lion ExplorerMainWindow::updateFilterBarAction);
    connect(container, &Lion ExplorerViewContainer::writeStateChanged, this, &Lion ExplorerMainWindow::slotWriteStateChanged);
    slotWriteStateChanged(container->view()->isFolderWritable());
    connect(container, &Lion ExplorerViewContainer::searchBarVisibilityChanged, this, &Lion ExplorerMainWindow::updateSearchAction);
    connect(container, &Lion ExplorerViewContainer::captionChanged, this, &Lion ExplorerMainWindow::updateWindowTitle);
    connect(container, &Lion ExplorerViewContainer::tabRequested, this, &Lion ExplorerMainWindow::openNewTab);
    connect(container, &Lion ExplorerViewContainer::activeTabRequested, this, &Lion ExplorerMainWindow::openNewTabAndActivate);

    // Make the toggled state of the selection mode actions visually follow the selection mode state of the view.
    auto toggleSelectionModeAction = actionCollection()->action(QStringLiteral("toggle_selection_mode"));
    toggleSelectionModeAction->setChecked(m_activeViewContainer->isSelectionModeEnabled());
    connect(m_activeViewContainer, &Lion ExplorerViewContainer::selectionModeChanged, toggleSelectionModeAction, &QAction::setChecked);

    const Lion ExplorerView *view = container->view();
    connect(view, &Lion ExplorerView::selectionChanged, this, &Lion ExplorerMainWindow::slotSelectionChanged);
    connect(view, &Lion ExplorerView::requestItemInfo, this, &Lion ExplorerMainWindow::requestItemInfo);
    connect(view, &Lion ExplorerView::fileItemsChanged, this, &Lion ExplorerMainWindow::fileItemsChanged);
    connect(view, &Lion ExplorerView::tabRequested, this, &Lion ExplorerMainWindow::openNewTab);
    connect(view, &Lion ExplorerView::activeTabRequested, this, &Lion ExplorerMainWindow::openNewTabAndActivate);
    connect(view, &Lion ExplorerView::windowRequested, this, &Lion ExplorerMainWindow::openNewWindow);
    connect(view, &Lion ExplorerView::requestContextMenu, this, &Lion ExplorerMainWindow::openContextMenu);
    connect(view, &Lion ExplorerView::directoryLoadingStarted, this, &Lion ExplorerMainWindow::enableStopAction);
    connect(view, &Lion ExplorerView::directoryLoadingCompleted, this, &Lion ExplorerMainWindow::disableStopAction);
    connect(view, &Lion ExplorerView::directoryLoadingCompleted, this, &Lion ExplorerMainWindow::slotDirectoryLoadingCompleted);
    connect(view, &Lion ExplorerView::goBackRequested, this, &Lion ExplorerMainWindow::goBack);
    connect(view, &Lion ExplorerView::goForwardRequested, this, &Lion ExplorerMainWindow::goForward);
    connect(view, &Lion ExplorerView::urlActivated, this, &Lion ExplorerMainWindow::handleUrl);
    connect(view, &Lion ExplorerView::goUpRequested, this, &Lion ExplorerMainWindow::goUp);
    connect(view, &Lion ExplorerView::doubleClickViewBackground, this, &Lion ExplorerMainWindow::slotDoubleClickViewBackground);

    connect(container->urlNavigatorInternalWithHistory(), &KUrlNavigator::urlChanged, this, &Lion ExplorerMainWindow::changeUrl);
    connect(container->urlNavigatorInternalWithHistory(), &KUrlNavigator::historyChanged, this, &Lion ExplorerMainWindow::updateHistory);

    auto navigators = static_cast<Lion ExplorerNavigatorsWidgetAction *>(actionCollection()->action(QStringLiteral("url_navigators")));
    const KUrlNavigator *navigator =
        m_tabWidget->currentTabPage()->primaryViewActive() ? navigators->primaryUrlNavigator() : navigators->secondaryUrlNavigator();

    QAction *editableLocactionAction = actionCollection()->action(QStringLiteral("editable_location"));
    editableLocactionAction->setChecked(navigator->isUrlEditable());
    connect(navigator, &KUrlNavigator::editableStateChanged, this, &Lion ExplorerMainWindow::slotEditableStateChanged);
    connect(navigator, &KUrlNavigator::tabRequested, this, &Lion ExplorerMainWindow::openNewTab);
    connect(navigator, &KUrlNavigator::activeTabRequested, this, &Lion ExplorerMainWindow::openNewTabAndActivate);
    connect(navigator, &KUrlNavigator::newWindowRequested, this, &Lion ExplorerMainWindow::openNewWindow);
}

void Lion ExplorerMainWindow::updateSplitActions()
{
    QAction *popoutSplitAction = actionCollection()->action(QStringLiteral("popout_split_view"));

    auto setActionPopupMode = [this](KActionMenu *action, QToolButton::ToolButtonPopupMode popupMode) {
        action->setPopupMode(popupMode);
        if (auto *buttonForAction = qobject_cast<QToolButton *>(toolBar()->widgetForAction(action))) {
            buttonForAction->setPopupMode(popupMode);
        }
    };

    const Lion ExplorerTabPage *tabPage = m_tabWidget->currentTabPage();
    if (tabPage->splitViewEnabled()) {
        using Choice = GeneralSettings::EnumCloseSplitViewChoice;
        switch (GeneralSettings::closeSplitViewChoice()) {
        case Choice::ActiveView:
            if (tabPage->primaryViewActive()) {
                m_splitViewAction->setText(i18nc("@action:intoolbar Close left view", "Close"));
                m_splitViewAction->setToolTip(i18nc("@info View refer here to split view", "Close Left View"));
                m_splitViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-left-close")));
                m_splitViewMenuAction->setText(i18nc("@action:inmenu View refer here to split view", "Close Left View"));

                popoutSplitAction->setText(i18nc("@action:intoolbar Move left split view to a new window", "Pop out Left View"));
                popoutSplitAction->setToolTip(i18nc("@info View refer here to split view", "Move left split view to a new window"));
            } else {
                m_splitViewAction->setText(i18nc("@action:intoolbar Close right view", "Close"));
                m_splitViewAction->setToolTip(i18nc("@info View refer here to split view", "Close Right View"));
                m_splitViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-right-close")));
                m_splitViewMenuAction->setText(i18nc("@action:inmenu View refer here to split view", "Close Right View"));

                popoutSplitAction->setText(i18nc("@action:intoolbar Move right split view to a new window", "Pop out Right View"));
                popoutSplitAction->setToolTip(i18nc("@info View refer here to split view", "Move right split view to a new window"));
            }
            break;
        case Choice::InactiveView:
            if (!tabPage->primaryViewActive()) {
                m_splitViewAction->setText(i18nc("@action:intoolbar Close left view", "Close"));
                m_splitViewAction->setToolTip(i18nc("@info View refer here to split view", "Close Left View"));
                m_splitViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-left-close")));
                m_splitViewMenuAction->setText(i18nc("@action:inmenu View refer here to split view", "Close Left View"));

                popoutSplitAction->setText(i18nc("@action:intoolbar Move left split view to a new window", "Pop out Left View"));
                popoutSplitAction->setToolTip(i18nc("@info View refer here to split view", "Move left split view to a new window"));
            } else {
                m_splitViewAction->setText(i18nc("@action:intoolbar Close right view", "Close"));
                m_splitViewAction->setToolTip(i18nc("@info View refer here to split view", "Close Right View"));
                m_splitViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-right-close")));
                m_splitViewMenuAction->setText(i18nc("@action:inmenu View refer here to split view", "Close Right View"));

                popoutSplitAction->setText(i18nc("@action:intoolbar Move right split view to a new window", "Pop out Right View"));
                popoutSplitAction->setToolTip(i18nc("@info View refer here to split view", "Move right split view to a new window"));
            }
            break;
        case Choice::RightView:
            m_splitViewAction->setText(i18nc("@action:intoolbar Close right view", "Close"));
            m_splitViewAction->setToolTip(i18nc("@info View refer here to split view", "Close Right View"));
            m_splitViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-right-close")));
            m_splitViewMenuAction->setText(i18nc("@action:inmenu View refer here to split view", "Close Right View"));

            popoutSplitAction->setText(i18nc("@action:intoolbar Move right split view to a new window", "Pop out Right View"));
            popoutSplitAction->setToolTip(i18nc("@info View refer here to split view", "Move right split view to a new window"));
            break;
        default:
            Q_UNREACHABLE();
        }
        popoutSplitAction->setEnabled(true);
        if (!m_splitViewAction->menu()) {
            setActionPopupMode(m_splitViewAction, QToolButton::MenuButtonPopup);
            m_splitViewAction->setMenu(new QMenu);
            m_splitViewAction->addAction(popoutSplitAction);
        }
    } else {
        m_splitViewAction->setText(i18nc("@action:intoolbar Split view", "Split"));
        m_splitViewMenuAction->setText(m_splitViewAction->text());
        m_splitViewAction->setToolTip(i18nc("@info", "Split view"));
        m_splitViewAction->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
        popoutSplitAction->setText(i18nc("@action:intoolbar Move view in focus to a new window", "Pop out"));
        popoutSplitAction->setEnabled(false);
        if (m_splitViewAction->menu()) {
            m_splitViewAction->removeAction(popoutSplitAction);
            m_splitViewAction->menu()->deleteLater();
            m_splitViewAction->setMenu(nullptr);
            setActionPopupMode(m_splitViewAction, QToolButton::DelayedPopup);
        }
    }

    // Update state from toolbar action
    m_splitViewMenuAction->setToolTip(m_splitViewAction->toolTip());
    m_splitViewMenuAction->setIcon(m_splitViewAction->icon());
}

void Lion ExplorerMainWindow::updateAllowedToolbarAreas()
{
    auto navigators = static_cast<Lion ExplorerNavigatorsWidgetAction *>(actionCollection()->action(QStringLiteral("url_navigators")));
    if (toolBar()->actions().contains(navigators)) {
        toolBar()->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
        if (toolBarArea(toolBar()) == Qt::LeftToolBarArea || toolBarArea(toolBar()) == Qt::RightToolBarArea) {
            addToolBar(Qt::TopToolBarArea, toolBar());
        }
    } else {
        toolBar()->setAllowedAreas(Qt::AllToolBarAreas);
    }
}

void Lion ExplorerMainWindow::updateNavigatorsBackground()
{
    auto navigators = static_cast<Lion ExplorerNavigatorsWidgetAction *>(actionCollection()->action(QStringLiteral("url_navigators")));
    navigators->setBackgroundEnabled(navigators->isInToolbar());
}

bool Lion ExplorerMainWindow::isKompareInstalled() const
{
    static bool initialized = false;
    static bool installed = false;
    if (!initialized) {
        // TODO: maybe replace this approach later by using a menu
        // plugin like kdiff3plugin.cpp
        installed = !QStandardPaths::findExecutable(QStringLiteral("kompare")).isEmpty();
        initialized = true;
    }
    return installed;
}

void Lion ExplorerMainWindow::createPanelAction(const QIcon &icon, const QKeySequence &shortcut, QDockWidget *dockWidget, const QString &actionName)
{
    auto dockAction = dockWidget->toggleViewAction();
    dockAction->setIcon(icon);
    dockAction->setEnabled(true);

    QAction *panelAction = actionCollection()->addAction(actionName, dockAction);
    actionCollection()->setDefaultShortcut(panelAction, shortcut);
}
// clang-format off
void Lion ExplorerMainWindow::setupWhatsThis()
{
    // main widgets
    menuBar()->setWhatsThis(xi18nc("@info:whatsthis", "<para>This is the "
        "<emphasis>Menubar</emphasis>. It provides access to commands and "
        "configuration options. Left-click on any of the menus on this "
        "bar to see its contents.</para><para>The Menubar can be hidden "
        "by unchecking <interface>Settings|Show Menubar</interface>. Then "
        "most of its contents become available through a <interface>Menu"
        "</interface> button on the <emphasis>Toolbar</emphasis>.</para>"));
    toolBar()->setWhatsThis(xi18nc("@info:whatsthis", "<para>This is the "
        "<emphasis>Toolbar</emphasis>. It allows quick access to "
        "frequently used actions.</para><para>It is highly customizable. "
        "All items you see in the <interface>Menu</interface> or "
        "in the <interface>Menubar</interface> can be placed on the "
        "Toolbar. Just right-click on it and select <interface>Configure "
        "Toolbars…</interface> or find this action within the <interface>"
        "menu</interface>."
        "</para><para>The location of the bar and the style of its "
        "buttons can also be changed in the right-click menu. Right-click "
        "a button if you want to show or hide its text.</para>"));
    m_tabWidget->setWhatsThis(xi18nc("@info:whatsthis main view",
        "<para>Here you can see the <emphasis>folders</emphasis> and "
        "<emphasis>files</emphasis> that are at the location described in "
        "the <interface>Location Bar</interface> above. This area is the "
        "central part of this application where you navigate to the files "
        "you want to use.</para><para>For an elaborate and general "
        "introduction to this application <link "
        "url='https://userbase.kde.org/Lion Explorer/File_Management#Introduction_to_Lion Explorer'>"
        "click here</link>. This will open an introductory article from "
        "the <emphasis>KDE UserBase Wiki</emphasis>.</para><para>For brief "
        "explanations of all the features of this <emphasis>view</emphasis> "
        "<link url='help:/lionexplorer/lionexplorer-view.html'>click here</link> "
        "instead. This will open a page from the <emphasis>Handbook"
        "</emphasis> that covers the basics.</para>"));

    // Settings menu
    actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings))
        ->setWhatsThis(xi18nc("@info:whatsthis","<para>This opens a window "
        "that lists the <emphasis>keyboard shortcuts</emphasis>.<nl/>"
        "There you can set up key combinations to trigger an action when "
        "they are pressed simultaneously. All commands in this application can "
        "be triggered this way.</para>"));
    actionCollection()->action(KStandardAction::name(KStandardAction::ConfigureToolbars))
        ->setWhatsThis(xi18nc("@info:whatsthis","<para>This opens a window in which "
        "you can change which buttons appear on the <emphasis>Toolbar</emphasis>.</para>"
        "<para>All items you see in the <interface>Menu</interface> can also be placed on the Toolbar.</para>"));
    actionCollection()->action(KStandardAction::name(KStandardAction::Preferences))
        ->setWhatsThis(xi18nc("@info:whatsthis","This opens a window where you can "
        "change a multitude of settings for this application. For an explanation "
        "of the various settings go to the chapter <emphasis>Configuring Lion Explorer"
        "</emphasis> in <interface>Help|Lion Explorer Handbook</interface>."));

    // Help menu

    auto setStandardActionWhatsThis = [this](KStandardAction::StandardAction actionId,
                                             const QString &whatsThis) {
        // Check for the existence of an action since it can be restricted through the Kiosk system
        if (auto *action = actionCollection()->action(KStandardAction::name(actionId))) {
            action->setWhatsThis(whatsThis);
        }
    };

    // i18n: If the external link isn't available in your language it might make
    // sense to state the external link's language in brackets to not
    // frustrate the user. If there are multiple languages that the user might
    // know with a reasonable chance you might want to have 2 external links.
    // The same might be true for any external link you translate.
    setStandardActionWhatsThis(KStandardAction::HelpContents, xi18nc("@info:whatsthis handbook", "<para>This opens the Handbook for this application. It provides explanations for every part of <emphasis>Lion Explorer</emphasis>.</para><para>If you want more elaborate introductions to the different features of <emphasis>Lion Explorer</emphasis> <link url='https://userbase.kde.org/Lion Explorer/File_Management'>click here</link>. It will open the dedicated page in the KDE UserBase Wiki.</para>"));
    // (The i18n call should be completely in the line following the i18n: comment without any line breaks within the i18n call or the comment might not be correctly extracted. See: https://commits.kde.org/kxmlgui/a31135046e1b3335b5d7bbbe6aa9a883ce3284c1 )

    setStandardActionWhatsThis(KStandardAction::WhatsThis,
        xi18nc("@info:whatsthis whatsthis button",
        "<para>This is the button that invokes the help feature you are "
        "using right now! Click it, then click any component of this "
        "application to ask \"What's this?\" about it. The mouse cursor "
        "will change appearance if no help is available for a spot.</para>"
        "<para>There are two other ways to get help: "
        "The <link url='help:/lionexplorer/index.html'>Lion Explorer Handbook</link> and "
        "the <link url='https://userbase.kde.org/Lion Explorer/File_Management'>KDE "
        "UserBase Wiki</link>.</para><para>The \"What's this?\" help is "
        "missing in most other windows so don't get too used to this.</para>"));

    setStandardActionWhatsThis(KStandardAction::ReportBug,
        xi18nc("@info:whatsthis","<para>This opens a "
        "window that will guide you through reporting errors or flaws "
        "in this application or in other KDE software.</para>"
        "<para>High-quality bug reports are much appreciated. To learn "
        "how to make your bug report as effective as possible "
        "<link url='https://community.kde.org/Get_Involved/Bug_Reporting'>"
        "click here</link>.</para>"));

    setStandardActionWhatsThis(KStandardAction::Donate,
        xi18nc("@info:whatsthis", "<para>This opens a "
        "<emphasis>web page</emphasis> where you can donate to "
        "support the continued work on this application and many "
        "other projects by the <emphasis>KDE</emphasis> community.</para>"
        "<para>Donating is the easiest and fastest way to efficiently "
        "support KDE and its projects. KDE projects are available for "
        "free therefore your donation is needed to cover things that "
        "require money like servers, contributor meetings, etc.</para>"
        "<para><emphasis>KDE e.V.</emphasis> is the non-profit "
        "organization behind the KDE community.</para>"));

    setStandardActionWhatsThis(KStandardAction::SwitchApplicationLanguage,
        xi18nc("@info:whatsthis",
        "With this you can change the language this application uses."
        "<nl/>You can even set secondary languages which will be used "
        "if texts are not available in your preferred language."));

    setStandardActionWhatsThis(KStandardAction::AboutApp,
        xi18nc("@info:whatsthis","This opens a "
        "window that informs you about the version, license, "
        "used libraries and maintainers of this application."));

    setStandardActionWhatsThis(KStandardAction::AboutKDE,
        xi18nc("@info:whatsthis","This opens a "
        "window with information about <emphasis>KDE</emphasis>. "
        "The KDE community are the people behind this free software."
        "<nl/>If you like using this application but don't know "
        "about KDE or want to see a cute dragon have a look!"));
}
// clang-format on

bool Lion ExplorerMainWindow::addHamburgerMenuToToolbar()
{
    QDomDocument domDocument = KXMLGUIClient::domDocument();
    if (domDocument.isNull()) {
        return false;
    }
    QDomNode toolbar = domDocument.elementsByTagName(QStringLiteral("ToolBar")).at(0);
    if (toolbar.isNull()) {
        return false;
    }

    QDomElement hamburgerMenuElement = domDocument.createElement(QStringLiteral("Action"));
    hamburgerMenuElement.setAttribute(QStringLiteral("name"), QStringLiteral("hamburger_menu"));
    toolbar.appendChild(hamburgerMenuElement);

    KXMLGUIFactory::saveConfigFile(domDocument, xmlFile());
    reloadXML();
    createGUI();
    return true;
    // Make sure to also remove the <KXMLGUIFactory> and <QDomDocument> include
    // whenever this method is removed (maybe in the year ~2026).
}

// Set a sane initial window size
QSize Lion ExplorerMainWindow::sizeHint() const
{
    return KXmlGuiWindow::sizeHint().expandedTo(QSize(760, 550));
}

void Lion ExplorerMainWindow::saveNewToolbarConfig()
{
    KXmlGuiWindow::saveNewToolbarConfig(); // Applies the new config. This has to be called first
                                           // because the rest of this method decides things
                                           // based on the new config.
    auto navigators = static_cast<Lion ExplorerNavigatorsWidgetAction *>(actionCollection()->action(QStringLiteral("url_navigators")));

    m_tabWidget->currentTabPage()->insertNavigatorsWidget(navigators);

    updateAllowedToolbarAreas();
    updateNavigatorsBackground();
    (static_cast<KHamburgerMenu *>(actionCollection()->action(KStandardAction::name(KStandardAction::HamburgerMenu))))->hideActionsOf(toolBar());
}

void Lion ExplorerMainWindow::toggleTerminalPanelFocus()
{
    if (!m_terminalPanel->isVisible()) {
        actionCollection()->action(QStringLiteral("show_terminal_panel"))->trigger(); // Also moves focus to the panel.
        actionCollection()->action(QStringLiteral("focus_terminal_panel"))->setText(i18nc("@action:inmenu Tools", "Defocus Terminal Panel"));
        return;
    }

    if (m_terminalPanel->terminalHasFocus()) {
        m_activeViewContainer->view()->setFocus(Qt::FocusReason::ShortcutFocusReason);
        actionCollection()->action(QStringLiteral("focus_terminal_panel"))->setText(i18nc("@action:inmenu Tools", "Focus Terminal Panel"));
        return;
    }

    m_terminalPanel->setFocus(Qt::FocusReason::ShortcutFocusReason);
    actionCollection()->action(QStringLiteral("focus_terminal_panel"))->setText(i18nc("@action:inmenu Tools", "Defocus Terminal Panel"));
}

void Lion ExplorerMainWindow::togglePlacesPanelFocus()
{
    if (!m_placesPanel->isVisible()) {
        actionCollection()->action(QStringLiteral("show_places_panel"))->trigger(); // Also moves focus to the panel.
        actionCollection()->action(QStringLiteral("focus_places_panel"))->setText(i18nc("@action:inmenu View", "Defocus Terminal Panel"));
        return;
    }

    if (m_placesPanel->hasFocus()) {
        m_activeViewContainer->view()->setFocus(Qt::FocusReason::ShortcutFocusReason);
        actionCollection()->action(QStringLiteral("focus_places_panel"))->setText(i18nc("@action:inmenu View", "Focus Places Panel"));
        return;
    }

    m_placesPanel->setFocus(Qt::FocusReason::ShortcutFocusReason);
    actionCollection()->action(QStringLiteral("focus_places_panel"))->setText(i18nc("@action:inmenu View", "Defocus Places Panel"));
}

Lion ExplorerMainWindow::UndoUiInterface::UndoUiInterface()
    : KIO::FileUndoManager::UiInterface()
{
}

Lion ExplorerMainWindow::UndoUiInterface::~UndoUiInterface() = default;

void Lion ExplorerMainWindow::UndoUiInterface::jobError(KIO::Job *job)
{
    Lion ExplorerMainWindow *mainWin = qobject_cast<Lion ExplorerMainWindow *>(parentWidget());
    if (mainWin) {
        Lion ExplorerViewContainer *container = mainWin->activeViewContainer();
        container->showMessage(job->errorString(), KMessageWidget::Error);
    } else {
        KIO::FileUndoManager::UiInterface::jobError(job);
    }
}

bool Lion ExplorerMainWindow::isUrlOpen(const QString &url)
{
    return m_tabWidget->isUrlOpen(QUrl::fromUserInput(url));
}

bool Lion ExplorerMainWindow::isItemVisibleInAnyView(const QString &urlOfItem)
{
    return m_tabWidget->isItemVisibleInAnyView(QUrl::fromUserInput(urlOfItem));
}

void Lion ExplorerMainWindow::slotDoubleClickViewBackground(Qt::MouseButton button)
{
    if (button != Qt::MouseButton::LeftButton) {
        // only handle left mouse button for now
        return;
    }

    GeneralSettings *settings = GeneralSettings::self();
    QString clickAction = settings->doubleClickViewAction();

    Lion ExplorerView *view = activeViewContainer()->view();
    if (view == nullptr || clickAction == "none") {
        return;
    }

    if (clickAction == customCommand) {
        // run custom command set by the user
        QString path = view->url().toLocalFile();
        QString clickCustomAction = settings->doubleClickViewCustomAction();
        clickCustomAction.replace("{path}", path.prepend('"').append('"'));

        m_job = new KIO::CommandLauncherJob(clickCustomAction);
        m_job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        m_job->start();

    } else {
        // get the action set by the user and trigger it
        const KActionCollection *actions = actionCollection();
        QAction *action = actions->action(clickAction);
        if (action == nullptr) {
            qCWarning(Lion ExplorerDebug) << QStringLiteral("Double-click view: action `%1` was not found").arg(clickAction);
            return;
        }
        action->trigger();
    }
}

#include "moc_lionexplorermainwindow.cpp"
