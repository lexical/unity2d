/*
 * Copyright (C) 2010 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qsortfilterproxymodelqml.h"
#include <QDebug>

QSortFilterProxyModelQML::QSortFilterProxyModelQML(QObject *parent) :
    QSortFilterProxyModel(parent)
{
}

void
QSortFilterProxyModelQML::updateRoleNames()
{
    setRoleNames(((QAbstractItemModel*)sourceModel())->roleNames());
}

QObject*
QSortFilterProxyModelQML::sourceModelQObject() const
{
    return (QAbstractItemModel*)sourceModel();
}

void
QSortFilterProxyModelQML::setSourceModelQObject(QObject *model)
{
    if (model == NULL) {
        return;
    }

    QAbstractItemModel* itemModel = qobject_cast<QAbstractItemModel*>(model);
    if (itemModel == NULL) {
        qWarning() << "QSortFilterProxyModelQML only accepts objects of type QAbstractItemModel as its model";
        return;
    }

    if (sourceModel() != NULL) {
        sourceModel()->disconnect(this);
    }

    setSourceModel(itemModel);

    connect(itemModel, SIGNAL(modelAboutToBeReset()), SLOT(updateRoleNames()));
    connect(itemModel, SIGNAL(modelReset()), SLOT(updateRoleNames()));
}

QVariantMap
QSortFilterProxyModelQML::get(int row)
{
    if (sourceModel() == NULL) {
        return QVariantMap();
    }

    QVariantMap result;
    QHashIterator<int, QByteArray> i(roleNames());
    while (i.hasNext()) {
        i.next();
        QModelIndex modelIndex = index(row, 0);
        QVariant data = modelIndex.data(i.key());
        result[i.value()] = data;
     }
     return result;
}

int
QSortFilterProxyModelQML::count()
{
    return rowCount();
}
