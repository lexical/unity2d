/*
 * Copyright (C) 2010-2011 Canonical, Ltd.
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

#include "launcherapplication.h"
#include "launcherapplicationslist.h"
#include "webfavorite.h"
#include "launcherapplicationslistdbus.h"

#include "bamf-matcher.h"
#include "bamf-application.h"
#include "gconfitem-qml-wrapper.h"

#include <QStringList>
#include <QDir>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFileInfo>
#include <QX11Info>

extern "C" {
#include <libsn/sn.h>
}


#define FAVORITES_KEY QString("/desktop/unity-2d/launcher/favorites")
#define DBUS_SERVICE_UNITY "com.canonical.Unity"
#define DBUS_SERVICE_LAUNCHER_ENTRY "com.canonical.Unity.LauncherEntry"
#define DBUS_SERVICE_LAUNCHER "com.canonical.Unity.Launcher"
#define DBUS_OBJECT_LAUNCHER "/com/canonical/Unity/Launcher"

/* List of executables that are too generic to be matched against a single application. */
static const QStringList EXECUTABLES_BLACKLIST = (QStringList() << "xdg-open");

LauncherApplicationsList::LauncherApplicationsList(QObject *parent) :
    QAbstractListModel(parent)
{
    m_favorites_list = new GConfItemQmlWrapper();
    m_favorites_list->setKey(FAVORITES_KEY);

    QDBusConnection session = QDBusConnection::sessionBus();
    /* FIXME: libunity will send out the Update signal for LauncherEntries
       only if it finds com.canonical.Unity on the bus, so let's just quickly
       register ourselves as Unity here. Should be moved somewhere else more proper */
    if (!session.registerService(DBUS_SERVICE_UNITY)) {
        qWarning() << "The name" << DBUS_SERVICE_UNITY << "is already taken on DBUS";
    } else {
        /* Set ourselves up to receive any Update signal coming from any
           LauncherEntry */
        session.connect(QString(), QString(),
                        DBUS_SERVICE_LAUNCHER_ENTRY, "Update",
                        this, SLOT(onRemoteEntryUpdated(QString,QMap<QString,QVariant>)));
    }

    if (!session.registerService(DBUS_SERVICE_LAUNCHER)) {
        qWarning() << "The name" << DBUS_SERVICE_LAUNCHER << "is already taken on DBUS";
    } else {
        /* Set ourselves up to receive a method call from Software Center asking us to add
           to favorites an application that is being installed and that the user requested
           to be added. */
        LauncherApplicationsListDBUS *dbusAdapter = new LauncherApplicationsListDBUS(this);
        if (!session.registerObject(DBUS_OBJECT_LAUNCHER, dbusAdapter,
                                    QDBusConnection::ExportAllSlots)) {
            qWarning() << "The object" << DBUS_OBJECT_LAUNCHER << "on" << DBUS_SERVICE_LAUNCHER
                       << "is already present on DBUS.";
        }
    }

    /* Register the display to receive startup notifications */
    Display *xdisplay = QX11Info::display();
    m_snDisplay = sn_display_new(xdisplay, NULL, NULL);
    m_snContext = sn_monitor_context_new(m_snDisplay, QX11Info::appScreen(),
                                          LauncherApplicationsList::snEventHandler,
                                          this, NULL);
    Unity2dApplication::instance()->installX11EventFilter(this);

    load();
}

void
LauncherApplicationsList::snEventHandler(SnMonitorEvent *event, void *user_data)
{
    /* This method is static and only forwards the event to a non static method. */
    ((LauncherApplicationsList*)user_data)->onSnMonitorEventReceived(event);
}

void
LauncherApplicationsList::onSnMonitorEventReceived(SnMonitorEvent *event)
{
    SnStartupSequence *sequence = sn_monitor_event_get_startup_sequence(event);

    switch (sn_monitor_event_get_type (event)) {
        case SN_MONITOR_EVENT_INITIATED:
            insertSnStartupSequence(sequence);
            break;
        case SN_MONITOR_EVENT_CHANGED:
        case SN_MONITOR_EVENT_COMPLETED:
        case SN_MONITOR_EVENT_CANCELED:
            /* These events are ignored for now. This is acceptable since the
               case of a failed application startup is handled by
               LauncherApplication::launching being automatically reset to
               false after a timeout. */
            break;
    }
}


bool
LauncherApplicationsList::x11EventFilter(XEvent* xevent)
{
    /* libsn specifies that all events need to be forwarded to
       sn_display_process_event but it is not actually necessary.
       Forwarding only the events of type ClientMessage.
     */
    if (xevent->type == ClientMessage) {
        sn_display_process_event(m_snDisplay, xevent);
    }
    return false;
}

void
LauncherApplicationsList::onRemoteEntryUpdated(QString applicationURI, QMap<QString, QVariant> properties)
{
    QString sender = message().service();
    QString desktopFile;
    if (applicationURI.indexOf("application://") == 0) {
        desktopFile = applicationURI.mid(14);
    } else {
        qWarning() << "Ignoring update that didn't come from an application:// URI but from:" << applicationURI;
        return;
    }

    Q_FOREACH(LauncherApplication *application, m_applications) {
        if (QFileInfo(application->desktop_file()).fileName() == desktopFile) {
            application->updateOverlaysState(sender, properties);
            return;
        }
    }

    qWarning() << "Application sent an update but we don't seem to have it in the launcher:" << applicationURI;
}

LauncherApplicationsList::~LauncherApplicationsList()
{
    Unity2dApplication::instance()->removeX11EventFilter(this);
    sn_monitor_context_unref(m_snContext);
    sn_display_unref(m_snDisplay);

    qDeleteAll(m_applications);
    delete m_favorites_list;
}

QString
LauncherApplicationsList::favoriteFromDesktopFilePath(QString desktop_file)
{
    return QDir(desktop_file).dirName();
}

void
LauncherApplicationsList::insertApplication(LauncherApplication* application)
{
    /* Insert at the end of the list. */
    int index = m_applications.size();

    beginInsertRows(QModelIndex(), index, index);
    m_applications.insert(index, application);

    if (!application->desktop_file().isEmpty()) {
        m_applicationForDesktopFile.insert(application->desktop_file(), application);
    }
    QString executable = application->executable();
    if (!executable.isEmpty() && !EXECUTABLES_BLACKLIST.contains(executable)) {
        m_applicationForExecutable.insert(executable, application);
    }
    endInsertRows();

    QObject::connect(application, SIGNAL(closed()), this, SLOT(onApplicationClosed()));
    QObject::connect(application, SIGNAL(stickyChanged(bool)), this, SLOT(onApplicationStickyChanged(bool)));
    QObject::connect(application, SIGNAL(launchingChanged(bool)), this, SLOT(onApplicationLaunchingChanged(bool)));
    QObject::connect(application, SIGNAL(urgentChanged(bool)), this, SLOT(onApplicationUrgentChanged(bool)));
}

void
LauncherApplicationsList::removeApplication(LauncherApplication* application)
{
    int index = m_applications.indexOf(application);

    if (index == -1) {
        /* application is not present in m_applications */
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_applications.removeAt(index);
    m_applicationForDesktopFile.remove(application->desktop_file());
    m_applicationForExecutable.remove(application->executable());
    endRemoveRows();

    /* LauncherApplicationsList::removeApplication might have been called in
       response to a signal emitted by application itself. Do not delete
       immediately to cater for this case.
    */
    application->deleteLater();
}

void LauncherApplicationsList::insertBamfApplication(BamfApplication* bamf_application)
{
    if (!bamf_application->user_visible()) {
        return;
    }

    LauncherApplication* matchingApplication = NULL;
    LauncherApplication* newApplication = new LauncherApplication;
    newApplication->setBamfApplication(bamf_application);

    QString executable = newApplication->executable();
    QString desktop_file = newApplication->desktop_file();
    if (m_applicationForDesktopFile.contains(desktop_file)) {
        /* A LauncherApplication with the same desktop file already exists */
        matchingApplication = m_applicationForDesktopFile[desktop_file];
    } else if (m_applicationForExecutable.contains(executable)) {
        /* A LauncherApplication with the same executable already exists */
        matchingApplication = m_applicationForExecutable[executable];
        /* If the application already registered for that executable has a
           desktop file assigned then make sure that the one to be inserted
           has the same desktop file.
        */
        QString matchingDesktopFile = matchingApplication->desktop_file();
        if (!matchingDesktopFile.isEmpty() && !desktop_file.isEmpty() &&
            matchingDesktopFile != desktop_file) {
                matchingApplication = NULL;
        }
    }

    if (matchingApplication != NULL) {
        /* A LauncherApplication that corresponds to bamf_application already exists */
        /* FIXME: this deletion blocks for a long time (around 100ms here) and
           leads to a visual glitch in the launcher when an application finished
           starting up. This is due to the deletion of the QFileSystemWatcher
           belonging to the LauncherApplication. */
        delete newApplication;
        matchingApplication->setBamfApplication(bamf_application);
    } else {
        insertApplication(newApplication);
    }
}

void
LauncherApplicationsList::insertFavoriteApplication(QString desktop_file)
{
    if (m_applicationForDesktopFile.contains(desktop_file)) {
        return;
    }

    /* Create a new LauncherApplication */
    LauncherApplication* application = new LauncherApplication;
    application->setDesktopFile(desktop_file);

    /* If the desktop_file property is empty after setting it, it
       means glib couldn't load the desktop file (probably corrupted) */
    if (application->desktop_file().isEmpty()) {
        qWarning() << "Favorite application not added due to desktop file missing or corrupted ("
                   << desktop_file << ")";
        delete application;
    } else {
        /* Register favorite desktop file into BAMF: applications with the same
           executable file will match with the given desktop file. This replicates
           the behaviour of Unity that does it automatically when calling libbamf's
           bamf_matcher_get_application_for_desktop_file.
           It fixes bug https://bugs.launchpad.net/unity-2d/+bug/739454
           The need for that API call is odd and causes at least one bug:
           https://bugs.launchpad.net/unity/+bug/762898
        */
        BamfMatcher& matcher = BamfMatcher::get_default();
        matcher.register_favorites(QStringList(application->desktop_file()));

        insertApplication(application);
        application->setSticky(true);
    }
}

void
LauncherApplicationsList::insertWebFavorite(const QUrl& url)
{
    if (!url.isValid() || url.isRelative()) {
        qWarning() << "Invalid URL:" << url;
        return;
    }

    LauncherApplication* application = new LauncherApplication;
    WebFavorite* webfav = new WebFavorite(url, application);

    application->setDesktopFile(webfav->desktopFile());
    insertApplication(application);
    application->setSticky(true);
}

void
LauncherApplicationsList::insertSnStartupSequence(SnStartupSequence* sequence)
{
    if (sequence == NULL) {
        return;
    }

    QString executable = sn_startup_sequence_get_binary_name(sequence);
    if (EXECUTABLES_BLACKLIST.contains(executable)) {
        return;
    }

    if (m_applicationForExecutable.contains(executable)) {
        /* A LauncherApplication with the same executable already exists */
        m_applicationForExecutable[executable]->setSnStartupSequence(sequence);
    } else {
        /* Create a new LauncherApplication and append it to the list */
        LauncherApplication* newApplication = new LauncherApplication;
        newApplication->setSnStartupSequence(sequence);
        insertApplication(newApplication);
    }
}

void
LauncherApplicationsList::load()
{
    /* Insert favorites */
    /* FIXME: migrate to GSettings, like unity. */
    QString desktop_file;
    QStringList favorites = m_favorites_list->getValue().toStringList();

    Q_FOREACH(QString favorite, favorites) {
       insertFavoriteApplication(favorite);
    }

    /* Insert running applications from Bamf */
    BamfMatcher& matcher = BamfMatcher::get_default();
    QScopedPointer<BamfApplicationList> running_applications(matcher.running_applications());
    BamfApplication* bamf_application;

    for(int i=0; i<running_applications->size(); i++) {
        bamf_application = running_applications->at(i);
        insertBamfApplication(bamf_application);
    }

    QObject::connect(&matcher, SIGNAL(ViewOpened(BamfView*)), SLOT(onBamfViewOpened(BamfView*)));
}

void
LauncherApplicationsList::onBamfViewOpened(BamfView* bamf_view)
{
    /* Make sure bamf_view is in fact a BamfApplication */
    BamfApplication* bamf_application;
    bamf_application = dynamic_cast<BamfApplication*>(bamf_view);

    if (bamf_application == NULL) {
        return;
    }

    insertBamfApplication(bamf_application);
}

void LauncherApplicationsList::onApplicationClosed()
{
    LauncherApplication* application = static_cast<LauncherApplication*>(sender());

    if (!application->sticky() && !application->running()) {
        removeApplication(application);
    }
}

void
LauncherApplicationsList::onApplicationStickyChanged(bool sticky)
{
    LauncherApplication* application = static_cast<LauncherApplication*>(sender());

    writeFavoritesToGConf();

    if (!sticky && !application->running()) {
        removeApplication(application);
    }
}

void
LauncherApplicationsList::onApplicationLaunchingChanged(bool launching)
{
    LauncherApplication* application = static_cast<LauncherApplication*>(sender());

    if (!application->sticky() && !application->running() && !application->launching()) {
        removeApplication(application);
    }
}

void
LauncherApplicationsList::onApplicationUrgentChanged(bool urgent)
{
    LauncherApplication* application = static_cast<LauncherApplication*>(sender());
    if (urgent) {
        Q_EMIT applicationBecameUrgent(m_applications.indexOf(application));
    }
}

void
LauncherApplicationsList::writeFavoritesToGConf()
{
    QStringList favorites;

    Q_FOREACH(LauncherApplication *application, m_applications) {
        QString desktop_file = application->desktop_file();
        if (application->sticky()) {
            favorites.append(favoriteFromDesktopFilePath(desktop_file));
        }
    }

    m_favorites_list->blockSignals(true);
    m_favorites_list->setValue(QVariant(favorites));
    m_favorites_list->blockSignals(false);
}

int
LauncherApplicationsList::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return m_applications.size();
}

QVariant
LauncherApplicationsList::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(role);

    if (!index.isValid()) {
        return QVariant();
    }

    return QVariant::fromValue(m_applications.at(index.row()));
}

void
LauncherApplicationsList::move(int from, int to)
{
    QModelIndex parent;
    /* When moving an item down, the destination index needs to be incremented
       by one, as explained in the documentation:
       http://doc.qt.nokia.com/qabstractitemmodel.html#beginMoveRows */
    beginMoveRows(parent, from, from, parent, to + (to > from ? 1 : 0));
    m_applications.move(from, to);
    endMoveRows();

    if (m_applications[from]->sticky() || m_applications[to]->sticky()) {
        /* Update favorites only if at least one of the applications is a favorite */
        writeFavoritesToGConf();
    }
}
