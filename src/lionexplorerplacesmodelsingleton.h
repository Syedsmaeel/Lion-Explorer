/*
 * SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIONEXPLORERPLACESMODELSINGLETON_H
#define LIONEXPLORERPLACESMODELSINGLETON_H

#include <QScopedPointer>
#include <QString>

#include <KFilePlacesModel>

/**
 * @brief Lion Explorer's special-cased KFilePlacesModel
 *
 * It returns the trash's icon based on whether
 * it is full or not.
 */
class Lion ExplorerPlacesModel : public KFilePlacesModel
{
    Q_OBJECT

public:
    explicit Lion ExplorerPlacesModel(QObject *parent = nullptr);
    ~Lion ExplorerPlacesModel() override;

    bool panelsLocked() const;
    void setPanelsLocked(bool locked);

    QStringList mimeTypes() const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

protected:
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private Q_SLOTS:
    void slotTrashEmptinessChanged(bool isEmpty);

private:
    bool isTrash(const QModelIndex &index) const;

    bool m_isEmpty = false;
    bool m_panelsLocked = true; // common-case, panels are locked
};

/**
 * @brief Provides a global KFilePlacesModel instance.
 */
class Lion ExplorerPlacesModelSingleton
{
public:
    static Lion ExplorerPlacesModelSingleton &instance();

    Lion ExplorerPlacesModel *placesModel() const;

    Lion ExplorerPlacesModelSingleton(const Lion ExplorerPlacesModelSingleton &) = delete;
    Lion ExplorerPlacesModelSingleton &operator=(const Lion ExplorerPlacesModelSingleton &) = delete;

private:
    Lion ExplorerPlacesModelSingleton();

    QScopedPointer<Lion ExplorerPlacesModel> m_placesModel;
};

#endif // LIONEXPLORERPLACESMODELSINGLETON_H
