/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2020 Felix Ernst <felixernst@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef LIONEXPLORERURLNAVIGATORSCONTROLLER_H
#define LIONEXPLORERURLNAVIGATORSCONTROLLER_H

#include <KCompletion>

#include <QObject>

#include <forward_list>

class Lion ExplorerUrlNavigator;

/**
 * @brief A controller managing all Lion ExplorerUrlNavigators.
 *
 * This class is used to apply settings changes to all constructed Lion ExplorerUrlNavigators.
 *
 * @see Lion ExplorerUrlNavigator
 */
class Lion ExplorerUrlNavigatorsController : public QObject
{
    Q_OBJECT

public:
    Lion ExplorerUrlNavigatorsController() = delete;

public Q_SLOTS:
    /**
     * Refreshes all Lion ExplorerUrlNavigators to get synchronized with the
     * Lion Explorer settings if they were changed.
     */
    static void slotReadSettings();

    static void slotPlacesPanelVisibilityChanged(bool visible);

private:
    /**
     * @return whether the places selector of Lion ExplorerUrlNavigators should be visible.
     */
    static bool placesSelectorVisible();

    /**
     * Adds \p lionexplorerUrlNavigator to the list of Lion ExplorerUrlNavigators
     * controlled by this class.
     */
    static void registerLion ExplorerUrlNavigator(Lion ExplorerUrlNavigator *lionexplorerUrlNavigator);

    /**
     * Removes \p lionexplorerUrlNavigator from the list of Lion ExplorerUrlNavigators
     * controlled by this class.
     */
    static void unregisterLion ExplorerUrlNavigator(Lion ExplorerUrlNavigator *lionexplorerUrlNavigator);

private Q_SLOTS:
    /**
     * Sets the completion mode for all Lion ExplorerUrlNavigators and saves it in settings.
     */
    static void setCompletionMode(const KCompletion::CompletionMode completionMode);

private:
    /** Contains all currently constructed Lion ExplorerUrlNavigators */
    static std::forward_list<Lion ExplorerUrlNavigator *> s_instances;

    /** Caches the (negated) places panel visibility */
    static bool s_placesSelectorVisible;

    friend class Lion ExplorerUrlNavigator;
};

#endif // LIONEXPLORERURLNAVIGATORSCONTROLLER_H
