/*
 * SPDX-FileCopyrightText: 2014 Emmanuel Pescosta <emmanuelpescosta099@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIONEXPLORER_RECENT_TABS_MENU_H
#define LIONEXPLORER_RECENT_TABS_MENU_H

#include <KActionMenu>

#include <QUrl>

class QAction;

class Lion ExplorerRecentTabsMenu : public KActionMenu
{
    Q_OBJECT

public:
    explicit Lion ExplorerRecentTabsMenu(QObject *parent);

public Q_SLOTS:
    void rememberClosedTab(const QUrl &url, const QByteArray &state);
    void undoCloseTab();

Q_SIGNALS:
    void restoreClosedTab(const QByteArray &state);
    void closedTabsCountChanged(unsigned int count);

private Q_SLOTS:
    void handleAction(QAction *action);

private:
    QAction *m_clearListAction;
};

#endif
