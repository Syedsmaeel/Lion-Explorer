/*
 * SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lionexplorernewfilemenu.h"

#include "views/lionexplorernewfilemenuobserver.h"

#include <KIO/Global>

#include <QAction>

Lion ExplorerNewFileMenu::Lion ExplorerNewFileMenu(QAction *createDirAction, QAction *createFileAction, QObject *parent)
    : KNewFileMenu(parent)
{
    setNewFolderShortcutAction(createDirAction);
    setNewFileShortcutAction(createFileAction);
    Lion ExplorerNewFileMenuObserver::instance().attach(this);
}

Lion ExplorerNewFileMenu::~Lion ExplorerNewFileMenu()
{
    Lion ExplorerNewFileMenuObserver::instance().detach(this);
}

void Lion ExplorerNewFileMenu::slotResult(KJob *job)
{
    if (job->error() && job->error() != KIO::ERR_USER_CANCELED) {
        Q_EMIT errorMessage(job->errorString());
    } else {
        KNewFileMenu::slotResult(job);
    }
}

#include "moc_lionexplorernewfilemenu.cpp"
