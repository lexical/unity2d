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

/* Those have to be included before any QObject-style header to avoid
   compilation errors. */
#include <gdk/gdk.h>

/* Required otherwise using wnck_set_client_type breaks linking with error:
   undefined reference to `wnck_set_client_type(WnckClientType)'
*/
extern "C" {
#include <libwnck/libwnck.h>
#include <libwnck/util.h>
}

#include "launcherapplication.h"
#include "launchermenu.h"
#include "bamf-matcher.h"
#include "bamf-indicator.h"

#include "dbusmenuimporter.h"

#include <X11/X.h>

#include <QDebug>
#include <QAction>
#include <QDBusInterface>
#include <QDBusReply>

LauncherApplication::LauncherApplication() :
    m_application(NULL), m_appInfo(NULL), m_sticky(false), m_has_visible_window(false)
{
    /* Make sure wnck_set_client_type is called only once */
    static bool client_type_set = false;
    if(!client_type_set)
    {
        /* Critically important to set the client type to pager because wnck
           will pass that type over to the window manager through XEvents.
           Window managers tend to respect orders from pagers to the letter by
           for example bypassing focus stealing prevention.
           Compiz does exactly that in src/event.c:handleEvent(...) in the
           ClientMessage case (line 1702).
           Metacity has a similar policy in src/core/window.c:window_activate(...)
           (line 2951).
        */
        wnck_set_client_type(WNCK_CLIENT_TYPE_PAGER);
        client_type_set = true;
    }

    QObject::connect(&m_launching_timer, SIGNAL(timeout()), this, SLOT(onLaunchingTimeouted()));
}

LauncherApplication::LauncherApplication(const LauncherApplication& other) :
    m_application(NULL), m_appInfo(NULL)
{
    /* FIXME: a number of members are not copied over */
    QObject::connect(&m_launching_timer, SIGNAL(timeout()), this, SLOT(onLaunchingTimeouted()));
    if (other.m_application != NULL)
        setBamfApplication(other.m_application);
    m_priority = other.m_priority;
}

LauncherApplication::~LauncherApplication()
{
    if(m_application != NULL)
    {
        m_application = NULL;
    }
    if(m_appInfo != NULL)
    {
        m_appInfo = NULL;
    }
}

bool
LauncherApplication::active() const
{
    if(m_application != NULL)
        return m_application->active();

    return false;
}

bool
LauncherApplication::running() const
{
    if(m_application != NULL)
        return m_application->running();

    return false;
}

bool
LauncherApplication::urgent() const
{
    if(m_application != NULL)
        return m_application->urgent();

    return false;
}

bool
LauncherApplication::sticky() const
{
    return m_sticky;
}

QString
LauncherApplication::name() const
{
    if(m_application != NULL)
        return m_application->name();

    if(m_appInfo != NULL)
        return QString::fromUtf8(g_app_info_get_name((GAppInfo*)m_appInfo));

    return QString("");
}

QString
LauncherApplication::icon() const
{
    if(m_application != NULL)
        return m_application->icon();

    if(m_appInfo != NULL)
        return QString::fromUtf8(g_icon_to_string(g_app_info_get_icon((GAppInfo*)m_appInfo)));

    return QString("");
}

QString
LauncherApplication::application_type() const
{
    if(m_application != NULL)
        return m_application->application_type();

    return QString("");
}

QString
LauncherApplication::desktop_file() const
{
    if(m_application != NULL)
        return m_application->desktop_file();

    if(m_appInfo != NULL)
        return QString::fromUtf8(g_desktop_app_info_get_filename(m_appInfo));

    return QString("");
}

void
LauncherApplication::setSticky(bool sticky)
{
    if (sticky == m_sticky)
        return;

    m_sticky = sticky;
    emit stickyChanged(sticky);
}

void
LauncherApplication::setDesktopFile(QString desktop_file)
{
    QByteArray byte_array = desktop_file.toUtf8();
    gchar *file = byte_array.data();

    if(desktop_file.startsWith("/"))
    {
        /* It looks like a full path to a desktop file */
        m_appInfo = g_desktop_app_info_new_from_filename(file);
    }
    else
    {
        /* It might just be a desktop file name; let GIO look for the actual
           desktop file for us */
        m_appInfo = g_desktop_app_info_new(file);
    }

    /* Emit the Changed signal on all properties that can depend on m_appInfo
       m_application's properties take precedence over m_appInfo's
    */
    if(m_application == NULL && m_appInfo != NULL)
    {
        emit desktopFileChanged(desktop_file);
        emit nameChanged(name());
        emit iconChanged(icon());
    }
}

void
LauncherApplication::setBamfApplication(BamfApplication *application)
{
    if (application == NULL) {
        return;
    }

    m_application = application;
    setDesktopFile(application->desktop_file());

    QObject::connect(application, SIGNAL(ActiveChanged(bool)), this, SIGNAL(activeChanged(bool)));

    /* FIXME: a bug somewhere makes connecting to the Closed() signal not work even though
              the emit Closed() in bamf-view.cpp is reached. */
    /* Connect first the onBamfApplicationClosed slot, then the runningChanged
       signal, to avoid a race condition when an application is closed.
       See https://launchpad.net/bugs/634057 */
    QObject::connect(application, SIGNAL(RunningChanged(bool)), this, SLOT(onBamfApplicationClosed(bool)));
    QObject::connect(application, SIGNAL(RunningChanged(bool)), this, SIGNAL(runningChanged(bool)));
    QObject::connect(application, SIGNAL(UrgentChanged(bool)), this, SIGNAL(urgentChanged(bool)));
    QObject::connect(application, SIGNAL(WindowAdded(BamfWindow*)), this, SLOT(updateHasVisibleWindow()));
    QObject::connect(application, SIGNAL(WindowRemoved(BamfWindow*)), this, SLOT(updateHasVisibleWindow()));
    connect(application, SIGNAL(ChildAdded(BamfView*)), SLOT(slotChildAdded(BamfView*)));
    connect(application, SIGNAL(ChildRemoved(BamfView*)), SLOT(slotChildRemoved(BamfView*)));

    connect(application, SIGNAL(WindowAdded(BamfWindow*)), SLOT(onWindowAdded(BamfWindow*)));

    updateBamfApplicationDependentProperties();
}

void
LauncherApplication::updateBamfApplicationDependentProperties()
{
    emit activeChanged(active());
    emit runningChanged(running());
    emit urgentChanged(urgent());
    emit nameChanged(name());
    emit iconChanged(icon());
    emit applicationTypeChanged(application_type());
    emit desktopFileChanged(desktop_file());
    m_launching_timer.stop();
    emit launchingChanged(launching());
    updateHasVisibleWindow();
    fetchIndicatorMenus();
}

void
LauncherApplication::onBamfApplicationClosed(bool running)
{
    if(running)
       return;

    m_application->disconnect(this);
    m_application = NULL;
    updateBamfApplicationDependentProperties();
    emit closed();
}

void
LauncherApplication::setIconGeometry(int x, int y, int width, int height, uint xid)
{
    if (m_application == NULL) {
        return;
    }

    QScopedPointer<BamfUintList> xids;
    if (xid == 0) {
        xids.reset(m_application->xids());
    } else {
        QList<uint> list;
        list.append(xid);
        xids.reset(new BamfUintList(list));
    }
    int size = xids->size();
    if (size < 1) {
        return;
    }

    WnckScreen* screen = wnck_screen_get_default();
    wnck_screen_force_update(screen);

    for (int i = 0; i < size; ++i) {
        WnckWindow* window = wnck_window_get(xids->at(i));
        wnck_window_set_icon_geometry(window, x, y, width, height);
    }
}

void
LauncherApplication::onWindowAdded(BamfWindow* window)
{
    if (window != NULL) {
        windowAdded(window->xid());
    }
}

int
LauncherApplication::priority() const
{
    return m_priority;
}

bool
LauncherApplication::launching() const
{
    return m_launching_timer.isActive();
}

void
LauncherApplication::updateHasVisibleWindow()
{
    bool prev = m_has_visible_window;

    if (m_application != NULL) {
        m_has_visible_window = QScopedPointer<BamfUintList>(m_application->xids())->size() > 0;
    } else {
        m_has_visible_window = false;
    }
    if (m_has_visible_window != prev)
        emit hasVisibleWindowChanged(m_has_visible_window);
}

bool
LauncherApplication::has_visible_window() const
{
    return m_has_visible_window;
}

/* Returns the number of window for this application that reside on the
   current workspace */
int
LauncherApplication::windowCountOnCurrentWorkspace()
{
    int windowCount = 0;
    WnckWorkspace *current = wnck_screen_get_active_workspace(wnck_screen_get_default());

    for (int i = 0; i < m_application->windows()->size(); i++) {
        BamfWindow *window = m_application->windows()->at(i);
        if (window == NULL) {
            continue;
        }

        /* When geting the wnck window, it's possible we get a NULL
           return value because wnck hasn't updated its internal list yet,
           so we need to force it once to be sure */
        WnckWindow *wnck_window = wnck_window_get(window->xid());
        if (wnck_window == NULL) {
            wnck_screen_force_update(wnck_screen_get_default());
            wnck_window = wnck_window_get(window->xid());
            if (wnck_window == NULL) {
                continue;
            }
        }

        WnckWorkspace *workspace = wnck_window_get_workspace(wnck_window);
        if (workspace == current) {
            windowCount++;
        }
    }
    return windowCount;
}

void
LauncherApplication::activate()
{
    if (active())
    {
        if (windowCountOnCurrentWorkspace() > 1) {
            spread();
        }
    }
    else if (running() && has_visible_window())
    {
        show();
    }
    else
    {
        launch();
    }
}

bool
LauncherApplication::launch()
{
    if(m_appInfo == NULL) return false;

    GError* error = NULL;
    GdkAppLaunchContext *context;
    GTimeVal timeval;

    g_get_current_time (&timeval);
    context = gdk_app_launch_context_new();
    /* Using GDK_CURRENT_TIME doesn’t seem to work, launched windows
       sometimes don’t get focus (see https://launchpad.net/bugs/643616). */
    gdk_app_launch_context_set_timestamp(context, timeval.tv_sec);

    g_app_info_launch((GAppInfo*)m_appInfo, NULL, (GAppLaunchContext*)context, &error);
    g_object_unref(context);

    if (error != NULL)
    {
        qWarning() << "Failed to launch application:" << error->message;
        g_error_free(error);
        return false;
    }

    /* 'launching' property becomes true for a maximum of 8 seconds and becomes
       false as soon as the application is launched */
    m_launching_timer.setSingleShot(true);
    m_launching_timer.start(8000);
    emit launchingChanged(true);

    return true;
}

void
LauncherApplication::onLaunchingTimeouted()
{
    emit launchingChanged(false);
}

void
LauncherApplication::close()
{
    if (m_application == NULL)
        return;

    QScopedPointer<BamfUintList> xids(m_application->xids());
    int size = xids->size();
    if (size < 1)
        return;

    WnckScreen* screen = wnck_screen_get_default();
    wnck_screen_force_update(screen);

    for (int i = 0; i < size; ++i) {
        WnckWindow* window = wnck_window_get(xids->at(i));
        wnck_window_close(window, CurrentTime);
    }
}

void
LauncherApplication::show()
{
    if(m_application == NULL) {
        return;
    }

    QScopedPointer<BamfWindowList> windows(m_application->windows());
    int size = windows->size();
    if (size < 1) {
        return;
    }

    /* Pick the most important window.
       The primary criterion to determine the most important window is urgency.
       The secondary criterion is the last_active timestamp (the last time the
       window was activated). */
    BamfWindow* important = windows->at(0);
    for (int i = 0; i < size; ++i) {
        BamfWindow* current = windows->at(i);
        if (current->urgent() && !important->urgent()) {
            important = current;
        }
        else if (current->urgent() || !important->urgent()) {
            if (current->last_active() > important->last_active()) {
                important = current;
            }
        }
    }

    WnckScreen* screen = wnck_screen_get_default();
    wnck_screen_force_update(screen);

    WnckWindow* window = wnck_window_get(important->xid());
    showWindow(window);
}

void
LauncherApplication::showWindow(WnckWindow* window)
{
    WnckWorkspace* workspace = wnck_window_get_workspace(window);

    /* Using X.h's CurrentTime (= 0) */
    wnck_workspace_activate(workspace, CurrentTime);

    /* If the workspace contains a viewport then move the viewport so
       that the window is visible.
       Compiz for example uses only one workspace with a desktop larger
       than the screen size which means that a viewport is used to
       determine what part of the desktop is visible.

       Reference:
       http://standards.freedesktop.org/wm-spec/wm-spec-latest.html#largedesks
    */
    if (wnck_workspace_is_virtual(workspace)) {
        moveViewportToWindow(window);
    }

    /* Using X.h's CurrentTime (= 0) */
    wnck_window_activate(window, CurrentTime);
}

void
LauncherApplication::moveViewportToWindow(WnckWindow* window)
{
    WnckWorkspace* workspace = wnck_window_get_workspace(window);
    WnckScreen* screen = wnck_window_get_screen(window);

    int screen_width = wnck_screen_get_width(screen);
    int screen_height = wnck_screen_get_height(screen);
    int viewport_x = wnck_workspace_get_viewport_x(workspace);
    int viewport_y = wnck_workspace_get_viewport_y(workspace);

    int window_x, window_y, window_width, window_height;
    wnck_window_get_geometry(window, &window_x, &window_y,
                                     &window_width, &window_height);

    /* Compute the row and column of the "virtual workspace" that contains
       the window. A "virtual workspace" is a portion of the desktop of the
       size of the screen.
    */
    int viewport_column = (viewport_x + window_x) / screen_width;
    int viewport_row = (viewport_y + window_y) / screen_height;

    wnck_screen_move_viewport(screen, viewport_column * screen_width,
                                      viewport_row * screen_height);
}

void
LauncherApplication::spread()
{
    QDBusInterface iface("com.canonical.Unity2d.Spread", "/Spread",
                         "com.canonical.Unity2d.Spread");
    QDBusReply<bool> isShown = iface.call("IsShown");
    if (isShown.value() == true) {
        iface.call("FilterByApplication", m_application->desktop_file());
    } else {
        iface.call("ShowCurrentWorkspace", m_application->desktop_file());
    }
}

void
LauncherApplication::slotChildAdded(BamfView* child)
{
    BamfIndicator* indicator = qobject_cast<BamfIndicator*>(child);
    if (indicator != NULL) {
        QString path = indicator->dbus_menu_path();
        if (!m_indicatorMenus.contains(path)) {
            DBusMenuImporter* importer = new DBusMenuImporter(indicator->address(), path, this);
            connect(importer, SIGNAL(menuUpdated()), SLOT(onIndicatorMenuUpdated()));
            m_indicatorMenus[path] = importer;
        }
    }
}

void
LauncherApplication::slotChildRemoved(BamfView* child)
{
    BamfIndicator* indicator = qobject_cast<BamfIndicator*>(child);
    if (indicator != NULL) {
        QString path = indicator->dbus_menu_path();
        if (m_indicatorMenus.contains(path)) {
            m_indicatorMenus.take(path)->deleteLater();
        }
    }
}

void
LauncherApplication::fetchIndicatorMenus()
{
    Q_FOREACH(QString path, m_indicatorMenus.keys()) {
        m_indicatorMenus.take(path)->deleteLater();
    }
    if (m_application != NULL) {
        QScopedPointer<BamfViewList> children(m_application->children());
        for (int i = 0; i < children->size(); ++i) {
            slotChildAdded(children->at(i));
        }
    }
}

void
LauncherApplication::createMenuActions()
{
    if (m_application != NULL && !m_indicatorMenus.isEmpty()) {
        /* Request indicator menus to be updated: this is asynchronous
           and the corresponding actions are added to the menu in
           SLOT(onIndicatorMenuUpdated()).
           Static menu actions are appended after all indicator menus
           have been updated.*/
        m_indicatorMenusReady = 0;
        Q_FOREACH(DBusMenuImporter* importer, m_indicatorMenus) {
            importer->updateMenu();
        }
    } else {
        createStaticMenuActions();
    }
}

void
LauncherApplication::createStaticMenuActions()
{
    m_menu->addSeparator();
    QList<QAction*> actions;

    bool is_running = running();

    /* Only applications with a corresponding desktop file can be kept in the launcher */
    if (!desktop_file().isEmpty()) {
        QAction* keep = new QAction(m_menu);
        keep->setCheckable(is_running);
        keep->setChecked(sticky());
        keep->setText(is_running ? tr("Keep In Launcher") : tr("Remove From Launcher"));
        actions.append(keep);
        QObject::connect(keep, SIGNAL(triggered()), this, SLOT(onKeepTriggered()));
    }

    if (is_running)
    {
        QAction* quit = new QAction(m_menu);
        quit->setText(tr("Quit"));
        actions.append(quit);
        QObject::connect(quit, SIGNAL(triggered()), this, SLOT(onQuitTriggered()));
    }

    /* Filter out duplicate actions. This typically happens with indicator
       menus that contain a "Quit" action: we don’t want two "Quit" actions in
       our menu. */
    Q_FOREACH(QAction* pending, actions) {
        bool duplicate = false;
        Q_FOREACH(QAction* action, m_menu->actions()) {
            /* The filtering is done on the text of the action. This will
               obviously break if for example only one of the two actions is
               localized ("Quit" != "Quitter"), but we don’t have a better way
               to identify duplicate actions. */
            if (pending->text() == action->text()) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            m_menu->addAction(pending);
        } else {
            delete pending;
        }
    } 
}

void
LauncherApplication::onIndicatorMenuUpdated()
{
    if (!m_menu->isVisible()) {
        return;
    }

    DBusMenuImporter* importer = static_cast<DBusMenuImporter*>(sender());
    QList<QAction*> actions = importer->menu()->actions();
    Q_FOREACH(QAction* action, actions) {
        if (action->isSeparator()) {
            m_menu->addSeparator();
        } else {
            /* Copy the action so we can override the slot triggered on
               activation. */
            QAction* copy = new QAction(action->icon(), action->text(), action);
            copy->setVisible(action->isVisible());
            copy->setEnabled(action->isEnabled());
            if (action->isCheckable()) {
                copy->setCheckable(true);
                if (action->isChecked()) {
                    copy->setChecked(true);
                }
            }
            connect(copy, SIGNAL(triggered()), SLOT(onIndicatorMenuActionTriggered()));
            m_menu->addAction(copy);
        }
    }

    if (++m_indicatorMenusReady == m_indicatorMenus.size()) {
        /* All indicator menus have been updated. */
        createStaticMenuActions();
    }
}

void
LauncherApplication::onKeepTriggered()
{
    QAction* keep = static_cast<QAction*>(sender());
    bool sticky = keep->isChecked();
    m_menu->hide();
    setSticky(sticky);
}

void
LauncherApplication::onQuitTriggered()
{
    m_menu->hide();
    close();
}

void LauncherApplication::onIndicatorMenuActionTriggered()
{
    QAction* action = static_cast<QAction*>(sender());
    QAction* parent = qobject_cast<QAction*>(action->parent());
    m_menu->hide();
    if (parent != NULL) {
        parent->trigger();
    }
}

