/*
 * This file is part of unity-2d
 *
 * Copyright 2011 Canonical Ltd.
 *
 * Authors:
 * - Aurélien Gâteau <aurelien.gateau@canonical.com>
 *
 * License: GPL v3
 */
// Self
#include "intellihidebehavior.h"

// Local

// libunity-2d
#include <debug_p.h>
#include <unity2dpanel.h>

// Qt
#include <QEvent>
#include <QTimer>

// libwnck
#undef signals
extern "C" {
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>
}

// Handy macros to declare GObject callbacks. The 'n' in CALLBACKn refers to
// the number of dummy arguments the callback returns
#define GOBJECT_CALLBACK0(callbackName, slot) \
static void \
callbackName(GObject* src, QObject* dst) \
{ \
    QMetaObject::invokeMethod(dst, slot); \
}

#define GOBJECT_CALLBACK1(callbackName, slot) \
static void \
callbackName(GObject* src, void* dummy1, QObject* dst) \
{ \
    QMetaObject::invokeMethod(dst, slot); \
}

#define GOBJECT_CALLBACK2(callbackName, slot) \
static void \
callbackName(GObject* src, void* dummy1, void* dummy2, QObject* dst) \
{ \
    QMetaObject::invokeMethod(dst, slot); \
}

// Screen callbacks
GOBJECT_CALLBACK1(activeWindowChangedCB, "updateActiveWindowConnections");
GOBJECT_CALLBACK1(activeWorkspaceChangedCB, "updateVisibility");

// Window callbacks
GOBJECT_CALLBACK2(stateChangedCB, "updateVisibility");
GOBJECT_CALLBACK0(geometryChangedCB, "updateVisibility");
GOBJECT_CALLBACK0(workspaceChangedCB, "updateVisibility");

IntelliHideBehavior::IntelliHideBehavior(Unity2dPanel* panel)
: AbstractVisibilityBehavior(panel)
, m_activeWindow(0)
{
    m_panel->installEventFilter(this);

    WnckScreen* screen = wnck_screen_get_default();
    g_signal_connect(G_OBJECT(screen), "active-window-changed", G_CALLBACK(activeWindowChangedCB), this);
    g_signal_connect(G_OBJECT(screen), "active-workspace-changed", G_CALLBACK(activeWorkspaceChangedCB), this);

    /* Delay monitoring the active window giving time to the user to reach
       for the panel before it disappears */
    QTimer::singleShot(1000, this, SLOT(updateActiveWindowConnections()));
}

IntelliHideBehavior::~IntelliHideBehavior()
{
    disconnectFromGSignals();
    WnckScreen* screen = wnck_screen_get_default();
    g_signal_handlers_disconnect_by_func(G_OBJECT(screen), gpointer(activeWindowChangedCB), this);
    g_signal_handlers_disconnect_by_func(G_OBJECT(screen), gpointer(activeWorkspaceChangedCB), this);
}

void IntelliHideBehavior::disconnectFromGSignals()
{
    if (m_activeWindow) {
        g_signal_handlers_disconnect_by_func(m_activeWindow, gpointer(stateChangedCB), this);
        g_signal_handlers_disconnect_by_func(m_activeWindow, gpointer(geometryChangedCB), this);
        g_signal_handlers_disconnect_by_func(m_activeWindow, gpointer(workspaceChangedCB), this);
    }
}

void IntelliHideBehavior::updateActiveWindowConnections()
{
    WnckScreen* screen = wnck_screen_get_default();

    disconnectFromGSignals();
    m_activeWindow = 0;

    WnckWindow* window = wnck_screen_get_active_window(screen);
    if (window) {
        m_activeWindow = window;
        g_signal_connect(G_OBJECT(window), "state-changed", G_CALLBACK(stateChangedCB), this);
        g_signal_connect(G_OBJECT(window), "geometry-changed", G_CALLBACK(geometryChangedCB), this);
        g_signal_connect(G_OBJECT(window), "workspace-changed", G_CALLBACK(workspaceChangedCB), this);
    }

    updateVisibility();
}

void IntelliHideBehavior::updateVisibility()
{
    if (isMouseForcingVisibility()) {
        return;
    }
    int launcherPid = getpid();

    // Compute launcherRect, adjust "left" to the position where the launcher
    // is fully visible.
    QRect launcherRect = m_panel->geometry();
    launcherRect.moveLeft(0);

    WnckScreen* screen = wnck_screen_get_default();
    WnckWorkspace* workspace = wnck_screen_get_active_workspace(screen);

    // Check whether a window is crossing our launcher rect
    bool crossWindow = false;
    GList* list = wnck_screen_get_windows(screen);
    for (; list; list = g_list_next(list)) {
        WnckWindow* window = WNCK_WINDOW(list->data);
        if (wnck_window_is_on_workspace(window, workspace) && wnck_window_get_pid(window) != launcherPid) {
            WnckWindowType type = wnck_window_get_window_type(window);

            // Only take into account typical application windows
            if (type != WNCK_WINDOW_NORMAL  &&
                type != WNCK_WINDOW_DIALOG  &&
                type != WNCK_WINDOW_TOOLBAR &&
                type != WNCK_WINDOW_MENU    &&
                type != WNCK_WINDOW_UTILITY) {
                continue;
            }

            WnckWindowState state = wnck_window_get_state(window);

            // Skip hidden (==minimized and other states) windows
            // Note: check this *before* checking if window is maximized
            // because a window can be both minimized and maximized
            if (state & WNCK_WINDOW_STATE_HIDDEN) {
                continue;
            }

            // Maximized window should always be considered as crossing the
            // window
            if (state & WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY) {
                crossWindow = true;
                break;
            }

            // Not maximized => really check the window rect
            int x, y, width, height;
            wnck_window_get_geometry(window, &x, &y, &width, &height);
            QRect rect(x, y, width, height);
            if (rect.intersects(launcherRect)) {
                crossWindow = true;
                break;
            }
        }
    }

    if (crossWindow) {
        m_panel->slideOut();
    } else {
        m_panel->slideIn();
    }
}

bool IntelliHideBehavior::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::Leave && !isMouseForcingVisibility()) {
        updateVisibility();
    } else if (event->type() == QEvent::Resize) {
        updateVisibility();
    }
    return false;
}

bool IntelliHideBehavior::isMouseForcingVisibility() const
{
    return m_panel->underMouse();
}
