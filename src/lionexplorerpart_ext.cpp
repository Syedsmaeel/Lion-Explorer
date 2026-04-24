/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2012 Dawit Alemayehu <adawit@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "lionexplorerpart_ext.h"

#include "lionexplorerpart.h"
#include "views/lionexplorerview.h"

Lion ExplorerPartBrowserExtension::Lion ExplorerPartBrowserExtension(Lion ExplorerPart *part)
    : KParts::NavigationExtension(part)
    , m_part(part)
{
}

void Lion ExplorerPartBrowserExtension::restoreState(QDataStream &stream)
{
    KParts::NavigationExtension::restoreState(stream);
    m_part->view()->restoreState(stream);
}

void Lion ExplorerPartBrowserExtension::saveState(QDataStream &stream)
{
    KParts::NavigationExtension::saveState(stream);
    m_part->view()->saveState(stream);
}

void Lion ExplorerPartBrowserExtension::cut()
{
    m_part->view()->cutSelectedItemsToClipboard();
}

void Lion ExplorerPartBrowserExtension::copy()
{
    m_part->view()->copySelectedItemsToClipboard();
}

void Lion ExplorerPartBrowserExtension::paste()
{
    m_part->view()->paste();
}

void Lion ExplorerPartBrowserExtension::pasteTo(const QUrl &)
{
    m_part->view()->pasteIntoFolder();
}

void Lion ExplorerPartBrowserExtension::reparseConfiguration()
{
    m_part->view()->readSettings();
}

Lion ExplorerPartFileInfoExtension::Lion ExplorerPartFileInfoExtension(Lion ExplorerPart *part)
    : KParts::FileInfoExtension(part)
    , m_part(part)
{
}

bool Lion ExplorerPartFileInfoExtension::hasSelection() const
{
    return m_part->view()->selectedItemsCount() > 0;
}

KParts::FileInfoExtension::QueryModes Lion ExplorerPartFileInfoExtension::supportedQueryModes() const
{
    return (KParts::FileInfoExtension::AllItems | KParts::FileInfoExtension::SelectedItems);
}

KFileItemList Lion ExplorerPartFileInfoExtension::queryFor(KParts::FileInfoExtension::QueryMode mode) const
{
    KFileItemList list;

    if (mode == KParts::FileInfoExtension::None)
        return list;

    if (!(supportedQueryModes() & mode))
        return list;

    switch (mode) {
    case KParts::FileInfoExtension::SelectedItems:
        if (hasSelection())
            return m_part->view()->selectedItems();
        break;
    case KParts::FileInfoExtension::AllItems:
        return m_part->view()->items();
    default:
        break;
    }

    return list;
}

Lion ExplorerPartListingFilterExtension::Lion ExplorerPartListingFilterExtension(Lion ExplorerPart *part)
    : KParts::ListingFilterExtension(part)
    , m_part(part)
{
}

KParts::ListingFilterExtension::FilterModes Lion ExplorerPartListingFilterExtension::supportedFilterModes() const
{
    return (KParts::ListingFilterExtension::MimeType | KParts::ListingFilterExtension::SubString | KParts::ListingFilterExtension::WildCard);
}

bool Lion ExplorerPartListingFilterExtension::supportsMultipleFilters(KParts::ListingFilterExtension::FilterMode mode) const
{
    if (mode == KParts::ListingFilterExtension::MimeType)
        return true;

    return false;
}

QVariant Lion ExplorerPartListingFilterExtension::filter(KParts::ListingFilterExtension::FilterMode mode) const
{
    QVariant result;

    switch (mode) {
    case KParts::ListingFilterExtension::MimeType:
        result = m_part->view()->mimeTypeFilters();
        break;
    case KParts::ListingFilterExtension::SubString:
    case KParts::ListingFilterExtension::WildCard:
        result = m_part->view()->nameFilter();
        break;
    default:
        break;
    }

    return result;
}

void Lion ExplorerPartListingFilterExtension::setFilter(KParts::ListingFilterExtension::FilterMode mode, const QVariant &filter)
{
    switch (mode) {
    case KParts::ListingFilterExtension::MimeType:
        m_part->view()->setMimeTypeFilters(filter.toStringList());
        break;
    case KParts::ListingFilterExtension::SubString:
    case KParts::ListingFilterExtension::WildCard:
        m_part->view()->setNameFilter(filter.toString());
        break;
    default:
        break;
    }
}

////

Lion ExplorerPartListingNotificationExtension::Lion ExplorerPartListingNotificationExtension(Lion ExplorerPart *part)
    : KParts::ListingNotificationExtension(part)
{
}

KParts::ListingNotificationExtension::NotificationEventTypes Lion ExplorerPartListingNotificationExtension::supportedNotificationEventTypes() const
{
    return (KParts::ListingNotificationExtension::ItemsAdded | KParts::ListingNotificationExtension::ItemsDeleted);
}

void Lion ExplorerPartListingNotificationExtension::slotNewItems(const KFileItemList &items)
{
    Q_EMIT listingEvent(KParts::ListingNotificationExtension::ItemsAdded, items);
}

void Lion ExplorerPartListingNotificationExtension::slotItemsDeleted(const KFileItemList &items)
{
    Q_EMIT listingEvent(KParts::ListingNotificationExtension::ItemsDeleted, items);
}

#include "moc_lionexplorerpart_ext.cpp"
