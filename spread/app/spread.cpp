/*
 * Copyright (C) 2010 Canonical, Ltd.
 *
 * Authors:
 *  Ugo Riboni <ugo.riboni@canonical.com>
 *  Florian Boucault <florian.boucault@canonical.com>
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

#include <gtk/gtk.h>
#include <QApplication>
#include <QDesktopWidget>
#include <QDeclarativeEngine>
#include <QDeclarativeContext>

#include "spreadview.h"
#include "spreadcontrol.h"

#include "config.h"

int main(int argc, char *argv[])
{
    /* UnityApplications plugin uses GTK APIs to retrieve theme icons
       (gtk_icon_theme_get_default) and requires a call to gtk_init */
    gtk_init(&argc, &argv);

    /* Forcing graphics system to 'raster' instead of the default 'native'
       which on X11 is 'XRender'.
       'XRender' defaults to using a TrueColor visual. We mimick that behaviour
       with 'raster' by calling QApplication::setColorSpec.

       Reference: https://bugs.launchpad.net/upicek/+bug/674484
    */
    QApplication::setGraphicsSystem("raster");
    QApplication::setColorSpec(QApplication::ManyColor);
    QApplication application(argc, argv);

    SpreadView view;

    /* The spread window is borderless and not moveable by the user, yet not
       fullscreen */
    view.setAttribute(Qt::WA_X11NetWmWindowTypeDock, true);

    /* Performance tricks */
    view.setAttribute(Qt::WA_OpaquePaintEvent);
    view.setAttribute(Qt::WA_NoSystemBackground);

    view.engine()->addImportPath(unity2dImportPath());
    /* Note: baseUrl seems to be picky: if it does not end with a slash,
       setSource() will fail */
    view.engine()->setBaseUrl(QUrl::fromLocalFile(unity2dDirectory() + "/spread/"));

    if (!isRunningInstalled()) {
        /* Spread.qml imports UnityApplications, which is part of the launcher
           component */
        view.engine()->addImportPath(unity2dDirectory() + "/launcher/");
        /* Spread.qml imports UnityPlaces, which is part of the places
           component */
        view.engine()->addImportPath(unity2dDirectory() + "/places/");
    }

    /* This is needed by GnomeBackground.qml (see explanation in there)
       The dash provides already the symlink needed to for the default
       wallpaper to work, so we just re-use it instead of having our own here
       in the spread too. FIXME: this should be removed when spread and dash
       are merged together. */
    view.rootContext()->setContextProperty("engineBaseUrl", unity2dDirectory() + "/places/");

    /* Add a SpreadControl instance to the QML context */
    /* FIXME: the SpreadControl class should be exposed to QML by a plugin and
              instantiated on the QML side */
    SpreadControl control;
    control.connectToBus();
    view.rootContext()->setContextProperty("control", &control);

    /* Load the QML UI, focus and show the window */
    view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
    view.rootContext()->setContextProperty("spreadView", &view);
    view.setSource(QUrl("./Spread.qml"));

    /* Always match the size of the desktop */
    int current_screen = QApplication::desktop()->screenNumber(&view);
    view.fitToAvailableSpace(current_screen);
    QObject::connect(QApplication::desktop(), SIGNAL(workAreaResized(int)), &view, SLOT(fitToAvailableSpace(int)));

    return application.exec();
}