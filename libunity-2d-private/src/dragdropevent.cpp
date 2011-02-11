/*
 * Copyright (C) 2011 Canonical, Ltd.
 *
 * Authors:
 *  Olivier Tilloy <olivier.tilloy@canonical.com>
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

/* This code is largely inspired by Grégory Schlomoff’s qml-drag-drop project
   (https://bitbucket.org/gregschlom/qml-drag-drop/).
   FIXME: what is the license of the original source code? */

#include "dragdropevent.h"

DeclarativeDragDropEvent::DeclarativeDragDropEvent(QGraphicsSceneDragDropEvent* event, QObject* parent)
    : QObject(parent)
    , m_event(event)
    , m_mimeData(event->mimeData())
{
    /* By default the event is not accepted, let the receiver decide. */
    m_event->setAccepted(false);
}

Qt::DropAction
DeclarativeDragDropEvent::dropAction() const
{
    return m_event->dropAction();
}

void
DeclarativeDragDropEvent::setDropAction(Qt::DropAction action)
{
    m_event->setDropAction(action);
}

void
DeclarativeDragDropEvent::accept()
{
    m_event->accept();
}

#include "dragdropevent.moc"

