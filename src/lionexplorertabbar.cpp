/*
 * SPDX-FileCopyrightText: 2014 Emmanuel Pescosta <emmanuelpescosta099@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lionexplorertabbar.h"
#include "lionexplorer_generalsettings.h"
#include <KLocalizedString>

#include <QDragEnterEvent>
#include <QInputDialog>
#include <QMenu>
#include <QMimeData>
#include <QTimer>

class PreventFocusWhileHidden : public QObject
{
public:
    PreventFocusWhileHidden(QObject *parent)
        : QObject(parent){};

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        switch (ev->type()) {
        case QEvent::Hide:
            static_cast<QWidget *>(obj)->setFocusPolicy(Qt::NoFocus);
            return false;
        case QEvent::Show:
            static_cast<QWidget *>(obj)->setFocusPolicy(Qt::TabFocus);
            return false;
        default:
            return false;
        }
    };
};

Lion ExplorerTabBar::Lion ExplorerTabBar(QWidget *parent)
    : QTabBar(parent)
    , m_autoActivationIndex(-1)
    , m_tabToBeClosedOnMiddleMouseButtonRelease(-1)
{
    setAcceptDrops(true);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    setMovable(true);
    setTabsClosable(true);

    setFocusPolicy(Qt::NoFocus);
    installEventFilter(new PreventFocusWhileHidden(this));

    m_autoActivationTimer = new QTimer(this);
    m_autoActivationTimer->setSingleShot(true);
    m_autoActivationTimer->setInterval(800);
    connect(m_autoActivationTimer, &QTimer::timeout, this, &Lion ExplorerTabBar::slotAutoActivationTimeout);
    connect(GeneralSettings::self(), &GeneralSettings::tabBarChanged, this, &Lion ExplorerTabBar::slotTabBarChanged);

    QTimer::singleShot(0, this, &Lion ExplorerTabBar::slotTabBarChanged);
}

QSize Lion ExplorerTabBar::tabSizeHint(int index) const
{
    if (GeneralSettings::tabStyle() == GeneralSettings::EnumTabStyle::FixedSize) {
        QSize defaultSize = QTabBar::tabSizeHint(index);
        defaultSize.setWidth(225);
        return defaultSize;
    } else if (GeneralSettings::tabStyle() == GeneralSettings::EnumTabStyle::FullWidth && count() > 0) {
        QSize defaultSize = QTabBar::tabSizeHint(index);
        defaultSize.setWidth(qMax(25, width() / count()));
        return defaultSize;
    }
    return QTabBar::tabSizeHint(index);
}

QSize Lion ExplorerTabBar::minimumSizeHint() const
{
    QSize s = QTabBar::minimumSizeHint();

    if (GeneralSettings::tabStyle() == GeneralSettings::EnumTabStyle::FullWidth) {
        s.setWidth(0); // allow shrinking
    }

    return s;
}

void Lion ExplorerTabBar::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    const int index = tabAt(event->position().toPoint());

    if (mimeData->hasUrls()) {
        event->acceptProposedAction();
        updateAutoActivationTimer(index);
    }

    QTabBar::dragEnterEvent(event);
}

void Lion ExplorerTabBar::dragLeaveEvent(QDragLeaveEvent *event)
{
    updateAutoActivationTimer(-1);

    QTabBar::dragLeaveEvent(event);
}

void Lion ExplorerTabBar::dragMoveEvent(QDragMoveEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    const int index = tabAt(event->position().toPoint());

    if (mimeData->hasUrls()) {
        Q_EMIT tabDragMoveEvent(index, event);
        updateAutoActivationTimer(index);
    }

    QTabBar::dragMoveEvent(event);
}

void Lion ExplorerTabBar::dropEvent(QDropEvent *event)
{
    // Disable the auto activation timer
    updateAutoActivationTimer(-1);

    const QMimeData *mimeData = event->mimeData();
    const int index = tabAt(event->position().toPoint());

    if (mimeData->hasUrls()) {
        Q_EMIT tabDropEvent(index, event);
    }

    QTabBar::dropEvent(event);
}

void Lion ExplorerTabBar::mousePressEvent(QMouseEvent *event)
{
    const int index = tabAt(event->pos());

    if (index >= 0 && event->button() == Qt::MiddleButton) {
        m_tabToBeClosedOnMiddleMouseButtonRelease = index;
        return;
    }

    QTabBar::mousePressEvent(event);
}

void Lion ExplorerTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    const int index = tabAt(event->pos());

    if (index >= 0 && index == m_tabToBeClosedOnMiddleMouseButtonRelease && event->button() == Qt::MiddleButton) {
        // Mouse middle click on a tab closes this tab.
        Q_EMIT tabCloseRequested(index);
        return;
    }

    QTabBar::mouseReleaseEvent(event);
}

void Lion ExplorerTabBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        int index = tabAt(event->pos());

        if (index < 0) {
            // empty tabbar area case
            index = currentIndex();
        }
        // Double left click on the tabbar opens a new activated tab
        // with the url from the doubleclicked tab or currentTab otherwise.
        Q_EMIT openNewActivatedTab(index);
    }

    QTabBar::mouseDoubleClickEvent(event);
}

void Lion ExplorerTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    const int index = tabAt(event->pos());

    if (index >= 0) {
        // Tab context menu
        QMenu menu(this);

        QAction *newTabAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "New Tab"));
        QAction *detachTabAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-detach")), i18nc("@action:inmenu", "Detach Tab"));
        QAction *closeOtherTabsAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-close-other")), i18nc("@action:inmenu", "Close Other Tabs"));
        QAction *closeTabAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-close")), i18nc("@action:inmenu", "Close Tab"));

        QAction *renameTabAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), i18nc("@action:inmenu", "Rename Tab"));

        QAction *selectedAction = menu.exec(event->globalPos());
        if (selectedAction == newTabAction) {
            Q_EMIT openNewActivatedTab(index);
        } else if (selectedAction == detachTabAction) {
            Q_EMIT tabDetachRequested(index);
        } else if (selectedAction == closeOtherTabsAction) {
            const int tabCount = count();
            for (int i = 0; i < index; i++) {
                Q_EMIT tabCloseRequested(0);
            }
            for (int i = index + 1; i < tabCount; i++) {
                Q_EMIT tabCloseRequested(1);
            }
        } else if (selectedAction == closeTabAction) {
            Q_EMIT tabCloseRequested(index);
        } else if (selectedAction == renameTabAction) {
            bool renamed = false;
            const QString tabNewName = QInputDialog::getText(this, i18nc("@title:window for text input", "Rename Tab"), i18n("New tab name:"), QLineEdit::Normal, tabText(index), &renamed);

            if (renamed) {
                Q_EMIT tabRenamed(index, tabNewName);
            }
        }

        return;
    }

    QTabBar::contextMenuEvent(event);
}

void Lion ExplorerTabBar::slotTabBarChanged()
{
    if (GeneralSettings::tabStyle() == GeneralSettings::EnumTabStyle::FixedSize) {
        setExpanding(false);
        setUsesScrollButtons(true);
    } else if (GeneralSettings::tabStyle() == GeneralSettings::EnumTabStyle::FullWidth) {
        setExpanding(true);
        setUsesScrollButtons(false);
    } else {
        setExpanding(false);
        setUsesScrollButtons(true);
    }

    updateGeometry();
}

void Lion ExplorerTabBar::slotAutoActivationTimeout()
{
    if (m_autoActivationIndex >= 0) {
        setCurrentIndex(m_autoActivationIndex);
        updateAutoActivationTimer(-1);
    }
}

void Lion ExplorerTabBar::updateAutoActivationTimer(const int index)
{
    if (m_autoActivationIndex != index) {
        m_autoActivationIndex = index;

        if (m_autoActivationIndex < 0) {
            m_autoActivationTimer->stop();
        } else {
            m_autoActivationTimer->start();
        }
    }
}

#include "moc_lionexplorertabbar.cpp"
