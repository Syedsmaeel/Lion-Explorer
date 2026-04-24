/*
 * SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>
 * SPDX-FileCopyrightText: 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lionexplorerremoveaction.h"

#include <QApplication>

Lion ExplorerRemoveAction::Lion ExplorerRemoveAction(QObject *parent, KActionCollection *collection)
    : QAction(parent)
    , m_collection(collection)
{
    update();
    connect(this, &Lion ExplorerRemoveAction::triggered, this, &Lion ExplorerRemoveAction::slotRemoveActionTriggered);
}

void Lion ExplorerRemoveAction::slotRemoveActionTriggered()
{
    if (m_action) {
        m_action->trigger();
    }
}

void Lion ExplorerRemoveAction::update(ShiftState shiftState)
{
    if (!m_collection) {
        m_action = nullptr;
        return;
    }

    if (shiftState == ShiftState::Unknown) {
        shiftState = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier ? ShiftState::Pressed : ShiftState::Released;
    }

    switch (shiftState) {
    case ShiftState::Pressed: {
        m_action = m_collection->action(KStandardAction::name(KStandardAction::DeleteFile));
        // Make sure we show Shift+Del in the context menu.
        auto deleteShortcuts = m_action->shortcuts();
        deleteShortcuts.removeAll(Qt::SHIFT | Qt::Key_Delete);
        deleteShortcuts.prepend(Qt::SHIFT | Qt::Key_Delete);
        m_collection->setDefaultShortcuts(this, deleteShortcuts);
        break;
    }
    case ShiftState::Released: {
        m_action = m_collection->action(KStandardAction::name(KStandardAction::MoveToTrash));
        // Make sure we show Del in the context menu.
        auto trashShortcuts = m_action->shortcuts();
        trashShortcuts.removeAll(QKeySequence::Delete);
        trashShortcuts.prepend(QKeySequence::Delete);
        m_collection->setDefaultShortcuts(this, trashShortcuts);
        break;
    }
    case ShiftState::Unknown:
        Q_UNREACHABLE();
        break;
    }

    if (m_action) {
        setText(m_action->text());
        setIcon(m_action->icon());
        setEnabled(m_action->isEnabled());
    }
}

#include "moc_lionexplorerremoveaction.cpp"
