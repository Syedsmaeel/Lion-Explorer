/*
 * SPDX-FileCopyrightText: 2019 David Hallas <david@davidhallas.dk>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIONEXPLORERBOOKMARKHANDLER_H
#define LIONEXPLORERBOOKMARKHANDLER_H

#include <KBookmarkManager>
#include <KBookmarkOwner>
#include <QObject>

class Lion ExplorerMainWindow;
class Lion ExplorerViewContainer;
class KActionCollection;
class KBookmarkMenu;
class QMenu;

class Lion ExplorerBookmarkHandler : public QObject, public KBookmarkOwner
{
    Q_OBJECT
public:
    Lion ExplorerBookmarkHandler(Lion ExplorerMainWindow *mainWindow, KActionCollection *collection, QMenu *menu, QObject *parent);
    ~Lion ExplorerBookmarkHandler() override;

private:
    QString currentTitle() const override;
    QUrl currentUrl() const override;
    QString currentIcon() const override;
    bool supportsTabs() const override;
    QList<FutureBookmark> currentBookmarkList() const override;
    bool enableOption(BookmarkOption option) const override;
    void openBookmark(const KBookmark &bookmark, Qt::MouseButtons, Qt::KeyboardModifiers) override;
    void openFolderinTabs(const KBookmarkGroup &bookmarkGroup) override;
    void openInNewTab(const KBookmark &bookmark) override;
    void openInNewWindow(const KBookmark &bookmark) override;
    static QString title(Lion ExplorerViewContainer *viewContainer);
    static QUrl url(Lion ExplorerViewContainer *viewContainer);
    static QString icon(Lion ExplorerViewContainer *viewContainer);

private:
    Lion ExplorerMainWindow *m_mainWindow;
    std::unique_ptr<KBookmarkManager> m_bookmarkManager;
    QScopedPointer<KBookmarkMenu> m_bookmarkMenu;
};

#endif // LIONEXPLORERBOOKMARKHANDLER_H
