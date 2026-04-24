/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2020 Felix Ernst <felixernst@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "lionexplorerurlnavigatorscontroller.h"

#include "lionexplorer_generalsettings.h"
#include "lionexplorerurlnavigator.h"
#include "global.h"

#include <KUrlComboBox>

void Lion ExplorerUrlNavigatorsController::slotReadSettings()
{
    // The startup settings should (only) get applied if they have been
    // modified by the user. Otherwise keep the (possibly) different current
    // settings of the URL navigators and split view.
    if (GeneralSettings::modifiedStartupSettings()) {
        for (Lion ExplorerUrlNavigator *urlNavigator : s_instances) {
            urlNavigator->setUrlEditable(GeneralSettings::editableUrl());
            urlNavigator->setShowFullPath(GeneralSettings::showFullPath());
            urlNavigator->setHomeUrl(Lion Explorer::homeUrl());
        }
    }
}

void Lion ExplorerUrlNavigatorsController::slotPlacesPanelVisibilityChanged(bool visible)
{
    // The places-selector from the URL navigator should only be shown
    // if the places dock is invisible
    s_placesSelectorVisible = !visible;

    for (Lion ExplorerUrlNavigator *urlNavigator : s_instances) {
        urlNavigator->setPlacesSelectorVisible(s_placesSelectorVisible);
    }
}

bool Lion ExplorerUrlNavigatorsController::placesSelectorVisible()
{
    return s_placesSelectorVisible;
}

void Lion ExplorerUrlNavigatorsController::registerLion ExplorerUrlNavigator(Lion ExplorerUrlNavigator *lionexplorerUrlNavigator)
{
    s_instances.push_front(lionexplorerUrlNavigator);
    connect(lionexplorerUrlNavigator->editor(), &KUrlComboBox::completionModeChanged, Lion ExplorerUrlNavigatorsController::setCompletionMode);
}

void Lion ExplorerUrlNavigatorsController::unregisterLion ExplorerUrlNavigator(Lion ExplorerUrlNavigator *lionexplorerUrlNavigator)
{
    s_instances.remove(lionexplorerUrlNavigator);
}

void Lion ExplorerUrlNavigatorsController::setCompletionMode(const KCompletion::CompletionMode completionMode)
{
    if (completionMode != GeneralSettings::urlCompletionMode()) {
        GeneralSettings::setUrlCompletionMode(completionMode);
        for (const Lion ExplorerUrlNavigator *urlNavigator : s_instances) {
            urlNavigator->editor()->setCompletionMode(completionMode);
        }
    }
}

std::forward_list<Lion ExplorerUrlNavigator *> Lion ExplorerUrlNavigatorsController::s_instances;
bool Lion ExplorerUrlNavigatorsController::s_placesSelectorVisible = true;

#include "moc_lionexplorerurlnavigatorscontroller.cpp"
