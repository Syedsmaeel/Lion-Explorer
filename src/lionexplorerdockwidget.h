/*
 * SPDX-FileCopyrightText: 2010 Peter Penz <peter.penz19@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIONEXPLORER_DOCK_WIDGET_H
#define LIONEXPLORER_DOCK_WIDGET_H

#include <QDockWidget>

/**
 * @brief Extends QDockWidget to be able to get locked.
 */
class Lion ExplorerDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit Lion ExplorerDockWidget(const QString &title = QString(), QWidget *parent = nullptr, Qt::WindowFlags flags = {});
    ~Lion ExplorerDockWidget() override;

    /**
     * @param lock If \a lock is true, the title bar of the dock-widget will get hidden so
     *             that it is not possible for the user anymore to move or undock the dock-widget.
     */
    void setLocked(bool lock);
    bool isLocked() const;

protected:
    /**
     * Make sure we do not emit QDockWidget::visibilityChanged() signals whenever Lion Explorer's window is minimized or restored.
     */
    bool event(QEvent *event) override;

private:
    bool m_locked;
    QWidget *m_dockTitleBar;
};

#endif
