/*
 * SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lionexplorerplacesmodelsingleton.h"
#include "trash/lionexplorertrash.h"
#include "views/draganddrophelper.h"

#include <KAboutData>

#include <QIcon>
#include <QMimeData>

Lion ExplorerPlacesModel::Lion ExplorerPlacesModel(QObject *parent)
    : KFilePlacesModel(parent)
{
    connect(&Trash::instance(), &Trash::emptinessChanged, this, &Lion ExplorerPlacesModel::slotTrashEmptinessChanged);
}

Lion ExplorerPlacesModel::~Lion ExplorerPlacesModel() = default;

bool Lion ExplorerPlacesModel::panelsLocked() const
{
    return m_panelsLocked;
}

void Lion ExplorerPlacesModel::setPanelsLocked(bool locked)
{
    if (m_panelsLocked == locked) {
        return;
    }

    m_panelsLocked = locked;

    if (rowCount() > 0) {
        int lastPlace = rowCount() - 1;

        for (int i = 0; i < rowCount(); ++i) {
            if (KFilePlacesModel::groupType(index(i, 0)) != KFilePlacesModel::PlacesType) {
                lastPlace = i - 1;
                break;
            }
        }

        Q_EMIT dataChanged(index(0, 0), index(lastPlace, 0), {KFilePlacesModel::GroupRole});
    }
}

QStringList Lion ExplorerPlacesModel::mimeTypes() const
{
    QStringList types = KFilePlacesModel::mimeTypes();
    types << DragAndDropHelper::arkDndServiceMimeType() << DragAndDropHelper::arkDndPathMimeType();
    return types;
}

bool Lion ExplorerPlacesModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    // We make the view accept the drag by returning them from mimeTypes()
    // but the drop should be handled exclusively by PlacesPanel::slotUrlsDropped
    if (DragAndDropHelper::isArkDndMimeType(data)) {
        return false;
    }

    return KFilePlacesModel::dropMimeData(data, action, row, column, parent);
}

QVariant Lion ExplorerPlacesModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
    case Qt::DecorationRole:
        if (isTrash(index)) {
            if (m_isEmpty) {
                return QIcon::fromTheme(QStringLiteral("user-trash"));
            } else {
                return QIcon::fromTheme(QStringLiteral("user-trash-full"));
            }
        }
        break;
    case KFilePlacesModel::GroupRole: {
        // When panels are unlocked, avoid a double "Places" heading,
        // one from the panel title bar, one from the places view section.
        if (!m_panelsLocked) {
            const auto groupType = KFilePlacesModel::groupType(index);
            if (groupType == KFilePlacesModel::PlacesType) {
                return QString();
            }
        }
        break;
    }
    }

    return KFilePlacesModel::data(index, role);
}

void Lion ExplorerPlacesModel::slotTrashEmptinessChanged(bool isEmpty)
{
    if (m_isEmpty == isEmpty) {
        return;
    }

    // NOTE Trash::isEmpty() reads the config file whereas emptinessChanged is
    // hooked up to whether a dirlister in trash:/ has any files and they disagree...
    m_isEmpty = isEmpty;

    for (int i = 0; i < rowCount(); ++i) {
        const QModelIndex index = this->index(i, 0);
        if (isTrash(index)) {
            Q_EMIT dataChanged(index, index, {Qt::DecorationRole});
        }
    }
}

bool Lion ExplorerPlacesModel::isTrash(const QModelIndex &index) const
{
    return url(index) == QUrl(QStringLiteral("trash:/"));
}

Lion ExplorerPlacesModelSingleton::Lion ExplorerPlacesModelSingleton()
    : m_placesModel(new Lion ExplorerPlacesModel())
{
}

Lion ExplorerPlacesModelSingleton &Lion ExplorerPlacesModelSingleton::instance()
{
    static Lion ExplorerPlacesModelSingleton s_self;
    return s_self;
}

Lion ExplorerPlacesModel *Lion ExplorerPlacesModelSingleton::placesModel() const
{
    return m_placesModel.data();
}

#include "moc_lionexplorerplacesmodelsingleton.cpp"
