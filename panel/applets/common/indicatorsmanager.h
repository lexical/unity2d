/*
 * This file is part of unity-2d
 *
 * Copyright 2011 Canonical Ltd.
 *
 * Authors:
 * - Aurélien Gâteau <aurelien.gateau@canonical.com>
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
#ifndef INDICATORSMANAGER_H
#define INDICATORSMANAGER_H

// Local

// Qt
#include <QMap>
#include <QObject>

// libunity-core
#include <UnityCore/UnityCore.h>

class IndicatorEntryWidget;

/**
 * Instantiates DBusIndicators and implement common behavior
 */
class IndicatorsManager : public QObject, public sigc::trackable
{
    Q_OBJECT
public:
    IndicatorsManager(QObject* parent);

    unity::indicator::DBusIndicators::Ptr indicators() const;

    void addIndicatorEntryWidget(IndicatorEntryWidget* widget);

private:
    Q_DISABLE_COPY(IndicatorsManager)
    unity::indicator::DBusIndicators::Ptr m_indicators;

    QMap<std::string, IndicatorEntryWidget*> m_widgetForEntryId;

    void onEntryShowMenu(const std::string&, int x, int y, int timestamp, int button);
    void onMenuPointerMoved(int x, int y);
    void onEntryActivateRequest(const std::string& entryId);
};

#endif /* INDICATORSMANAGER_H */
