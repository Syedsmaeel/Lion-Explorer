/*
 * SPDX-FileCopyrightText: 2019 David Hallas <david@davidhallas.dk>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lionexplorerbookmarkhandler.h"
#include "lionexplorermainwindow.h"
#include "lionexplorerviewcontainer.h"
#include "global.h"
#include <KActionCollection>
#include <KBookmarkMenu>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

Lion ExplorerBookmarkHandler::Lion ExplorerBookmarkHandler(Lion ExplorerMainWindow *mainWindow, KActionCollection *collection, QMenu *menu, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    QString bookmarksFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kfile/bookmarks.xml"));
    if (bookmarksFile.isEmpty()) {
        QString genericDataLocation = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (genericDataLocation.isEmpty()) {
            qWarning() << "GenericDataLocation is empty! Bookmarks will not be saved correctly.";
        }
        bookmarksFile = QStringLiteral("%1/lionexplorer").arg(genericDataLocation);
        QDir().mkpath(bookmarksFile);
        bookmarksFile += QLatin1String("/bookmarks.xml");
    }
    m_bookmarkManager = std::make_unique<KBookmarkManager>(bookmarksFile);
    m_bookmarkMenu.reset(new KBookmarkMenu(m_bookmarkManager.get(), this, menu));

    collection->addAction(QStringLiteral("add_bookmark"), m_bookmarkMenu->addBookmarkAction());
    collection->addAction(QStringLiteral("edit_bookmarks"), m_bookmarkMenu->editBookmarksAction());
    collection->addAction(QStringLiteral("add_bookmarks_list"), m_bookmarkMenu->bookmarkTabsAsFolderAction());
}

Lion ExplorerBookmarkHandler::~Lion ExplorerBookmarkHandler() = default;

QString Lion ExplorerBookmarkHandler::currentTitle() const
{
    return title(m_mainWindow->activeViewContainer());
}

QUrl Lion ExplorerBookmarkHandler::currentUrl() const
{
    return url(m_mainWindow->activeViewContainer());
}

QString Lion ExplorerBookmarkHandler::currentIcon() const
{
    return icon(m_mainWindow->activeViewContainer());
}

bool Lion ExplorerBookmarkHandler::supportsTabs() const
{
    return true;
}

QList<KBookmarkOwner::FutureBookmark> Lion ExplorerBookmarkHandler::currentBookmarkList() const
{
    const auto viewContainers = m_mainWindow->viewContainers();
    QList<FutureBookmark> bookmarks;
    bookmarks.reserve(viewContainers.size());
    for (const auto viewContainer : viewContainers) {
        bookmarks << FutureBookmark(title(viewContainer), url(viewContainer), icon(viewContainer));
    }
    return bookmarks;
}

bool Lion ExplorerBookmarkHandler::enableOption(KBookmarkOwner::BookmarkOption option) const
{
    switch (option) {
    case BookmarkOption::ShowAddBookmark:
        return true;
    case BookmarkOption::ShowEditBookmark:
        return true;
    }
    return false;
}

void Lion ExplorerBookmarkHandler::openBookmark(const KBookmark &bookmark, Qt::MouseButtons, Qt::KeyboardModifiers)
{
    m_mainWindow->changeUrl(bookmark.url());
}

void Lion ExplorerBookmarkHandler::openFolderinTabs(const KBookmarkGroup &bookmarkGroup)
{
    m_mainWindow->openDirectories(bookmarkGroup.groupUrlList(), false);
}

void Lion ExplorerBookmarkHandler::openInNewTab(const KBookmark &bookmark)
{
    m_mainWindow->openNewTab(bookmark.url());
}

void Lion ExplorerBookmarkHandler::openInNewWindow(const KBookmark &bookmark)
{
    Lion Explorer::openNewWindow({bookmark.url()}, m_mainWindow);
}

QString Lion ExplorerBookmarkHandler::title(Lion ExplorerViewContainer *viewContainer)
{
    return viewContainer->caption();
}

QUrl Lion ExplorerBookmarkHandler::url(Lion ExplorerViewContainer *viewContainer)
{
    return viewContainer->url();
}

QString Lion ExplorerBookmarkHandler::icon(Lion ExplorerViewContainer *viewContainer)
{
    return KIO::iconNameForUrl(viewContainer->url());
}

#include "moc_lionexplorerbookmarkhandler.cpp"
