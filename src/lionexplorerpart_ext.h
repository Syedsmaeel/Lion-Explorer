/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2012 Dawit Alemayehu <adawit@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef LIONEXPLORERPART_EXT_H
#define LIONEXPLORERPART_EXT_H

#include <KParts/FileInfoExtension>
#include <KParts/ListingFilterExtension>
#include <KParts/ListingNotificationExtension>
#include <KParts/NavigationExtension>

#include <QUrl>

class Lion ExplorerPart;

class Lion ExplorerPartBrowserExtension : public KParts::NavigationExtension
{
    Q_OBJECT
public:
    explicit Lion ExplorerPartBrowserExtension(Lion ExplorerPart *part);
    void restoreState(QDataStream &stream) override;
    void saveState(QDataStream &stream) override;

public Q_SLOTS:
    void cut();
    void copy();
    void paste();
    void pasteTo(const QUrl &);
    void reparseConfiguration();

private:
    Lion ExplorerPart *m_part;
};

class Lion ExplorerPartFileInfoExtension : public KParts::FileInfoExtension
{
    Q_OBJECT

public:
    explicit Lion ExplorerPartFileInfoExtension(Lion ExplorerPart *part);

    QueryModes supportedQueryModes() const override;
    bool hasSelection() const override;

    KFileItemList queryFor(QueryMode mode) const override;

private:
    Lion ExplorerPart *m_part;
};

class Lion ExplorerPartListingFilterExtension : public KParts::ListingFilterExtension
{
    Q_OBJECT

public:
    explicit Lion ExplorerPartListingFilterExtension(Lion ExplorerPart *part);
    FilterModes supportedFilterModes() const override;
    bool supportsMultipleFilters(FilterMode mode) const override;
    QVariant filter(FilterMode mode) const override;
    void setFilter(FilterMode mode, const QVariant &filter) override;

private:
    Lion ExplorerPart *m_part;
};

class Lion ExplorerPartListingNotificationExtension : public KParts::ListingNotificationExtension
{
    Q_OBJECT

public:
    explicit Lion ExplorerPartListingNotificationExtension(Lion ExplorerPart *part);
    NotificationEventTypes supportedNotificationEventTypes() const override;

public Q_SLOTS:
    void slotNewItems(const KFileItemList &);
    void slotItemsDeleted(const KFileItemList &);
};

#endif
