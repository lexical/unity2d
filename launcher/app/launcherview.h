/*
 * Copyright (C) 2010 Canonical, Ltd.
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

#ifndef LAUNCHERVIEW
#define LAUNCHERVIEW

#include <QDeclarativeView>
#include <QUrl>
#include <QList>
#include <QDragEnterEvent>

class QGraphicsObject;

class LauncherView : public QDeclarativeView
{
    Q_OBJECT

public:
    explicit LauncherView();
    Q_INVOKABLE QList<QVariant> getColorsFromIcon(QUrl source, QSize size) const;

Q_SIGNALS:
    void desktopFileDropped(QString path);
    void webpageUrlDropped(const QUrl& url);
    void keyboardShortcutPressed(int itemNumber);

private Q_SLOTS:
    void setHotkeysForModifiers(Qt::KeyboardModifiers modifiers);
    void forwardHotkey();

private:
    QList<QUrl> getEventUrls(QDropEvent*);

    /* Whether the launcher is already being resized */
    bool m_resizing;

    /* Whether space at the left of the screen has already been reserved */
    bool m_reserved;

    /* Custom drag’n’drop handling */
    void dragEnterEvent(QDragEnterEvent*);
    void dragMoveEvent(QDragMoveEvent*);
    void dropEvent(QDropEvent*);

    QGraphicsObject* launcherItemAt(const QPoint&) const;
    void delegateDragEventHandlingToItem(QDropEvent*, QGraphicsObject*);
    bool acceptDndEvent(QDragEnterEvent*);

    /* The launcher item currently under the mouse cursor during a dnd event */
    QGraphicsObject* m_dndCurrentLauncherItem;
    /* Whether it accepted the event */
    bool m_dndCurrentLauncherItemAccepted;
    /* Whether the launcher itself handles the current dnd event */
    bool m_dndAccepted;

    bool m_keyboardShortcutsActive;
};

#endif // LAUNCHERVIEW

