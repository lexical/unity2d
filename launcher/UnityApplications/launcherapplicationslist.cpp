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

#include "bamf-matcher.h"
#include "bamf-application.h"
#include "gconfitem-qml-wrapper.h"

#include <QStringList>
#include <QDir>
#include <QDebug>

#define FAVORITES_KEY QString("/desktop/unity/launcher/favorites/")

LauncherApplicationsList::LauncherApplicationsList(QObject *parent) :
    QAbstractListModel(parent)
{
    m_favorites_list = new GConfItemQmlWrapper();
    m_favorites_list->setKey(FAVORITES_KEY + "favorites_list");

    load();
}

LauncherApplicationsList::~LauncherApplicationsList()
{
    qDeleteAll(m_applications);
    delete m_favorites_list;
}

QString
LauncherApplicationsList::desktopFilePathFromFavorite(QString favorite_id)
{
    GConfItemQmlWrapper favorite;
    favorite.setKey(FAVORITES_KEY + favorite_id + "/desktop_file");
    return favorite.getValue().toString();
}

QString
LauncherApplicationsList::favoriteFromDesktopFilePath(QString desktop_file)
{
    return QString("app-") + QDir(desktop_file).dirName();
}


void
LauncherApplicationsList::insertApplication(LauncherApplication* application)
{
    int index = 0;
    int priority = application->priority();
    if (priority == -1) {
        /* Undefined priority, insert at the end of the list. */
        index = m_applications.size();
    } else {
        /* Compute where to insert based on other applications’ priorities. */
        Q_FOREACH(LauncherApplication* app, m_applications) {
            if ((app->priority() == -1) || (app->priority() > priority)) {
                break;
            }
            ++index;
        }
    }

    beginInsertRows(QModelIndex(), index, index);
    m_applications.insert(index, application);

    if (!application->desktop_file().isEmpty()) {
        m_applicationForDesktopFile.insert(application->desktop_file(), application);
    }
    endInsertRows();

    QObject::connect(application, SIGNAL(closed()), this, SLOT(onApplicationClosed()));
    QObject::connect(application, SIGNAL(stickyChanged(bool)), this, SLOT(onApplicationStickyChanged(bool)));
}

void
LauncherApplicationsList::removeApplication(LauncherApplication* application)
{
    int index = m_applications.indexOf(application);

    beginRemoveRows(QModelIndex(), index, index);
    m_applications.removeAt(index);
    m_applicationForDesktopFile.remove(application->desktop_file());
    endRemoveRows();

    delete application;
}


void LauncherApplicationsList::insertBamfApplication(BamfApplication* bamf_application)
{
    if (!bamf_application->user_visible())
        return;

    LauncherApplication* application;

    QString desktop_file = bamf_application->desktop_file();
    if (m_applicationForDesktopFile.contains(desktop_file)) {
        /* A LauncherApplication with the same desktop file already exists */
        application = m_applicationForDesktopFile[desktop_file];
        application->setBamfApplication(bamf_application);
    } else {
        /* Create a new LauncherApplication and append it to the list */
        application = new LauncherApplication;
        application->setBamfApplication(bamf_application);
        insertApplication(application);
    }
}

void
LauncherApplicationsList::insertFavoriteApplication(QString desktop_file)
{
    if (m_applicationForDesktopFile.contains(desktop_file))
        return;

    /* Create a new LauncherApplication */
    LauncherApplication* application = new LauncherApplication;
    application->setDesktopFile(desktop_file);

    /* Does the application have a priority defined? */
    /* Note: when migrating to GSettings for the storage of favorites,
       priorities will go away. Favorite applications will simply be sorted in
       the order they are found in the list in GSettings. */
    GConfItemQmlWrapper priority;
    priority.setKey(FAVORITES_KEY + favoriteFromDesktopFilePath(desktop_file) + "/priority");
    QVariant value = priority.getValue();
    if (value.isValid()) {
        application->setPriority(value.toInt());
    }

    /* If the desktop_file property is empty after setting it, it
       means glib couldn't load the desktop file (probably corrupted) */
    if (application->desktop_file().isEmpty()) {
        qWarning() << "Favorite application not added due to desktop file missing or corrupted ("
                   << desktop_file << ")";
        delete application;
    } else {
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
LauncherApplicationsList::load()
{
    /* Insert favorites */
    /* FIXME: migrate to GSettings, like unity. */
    QString desktop_file;
    QStringList favorites = m_favorites_list->getValue().toStringList();

    for(QStringList::iterator iter=favorites.begin(); iter!=favorites.end(); iter++)
    {
        desktop_file = desktopFilePathFromFavorite(*iter);
        insertFavoriteApplication(desktop_file);
    }

    /* Insert running applications from Bamf */
    BamfMatcher& matcher = BamfMatcher::get_default();
    QScopedPointer<BamfApplicationList> running_applications(matcher.running_applications());
    BamfApplication* bamf_application;

    for(int i=0; i<running_applications->size(); i++)
    {
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

    if(bamf_application == NULL)
        return;

    insertBamfApplication(bamf_application);
}

void LauncherApplicationsList::onApplicationClosed()
{
    LauncherApplication* application = static_cast<LauncherApplication*>(sender());

    if (!application->sticky() && !application->running())
        removeApplication(application);
}

void
LauncherApplicationsList::onApplicationStickyChanged(bool sticky)
{
    LauncherApplication* application = static_cast<LauncherApplication*>(sender());

    if (sticky)
    {
        addApplicationToFavorites(application);
    }
    else
    {
        removeApplicationFromFavorites(application);
        if (!application->running())
            removeApplication(application);
    }
}

void
LauncherApplicationsList::addApplicationToFavorites(LauncherApplication* application)
{
    QString desktop_file = application->desktop_file();
    QString favorite_id = favoriteFromDesktopFilePath(desktop_file);

    /* Add the favorite id to the GConf list of favorites */
    QStringList favorites = m_favorites_list->getValue().toStringList();
    if (favorites.contains(favorite_id))
        return;
    favorites << favorite_id;
    m_favorites_list->blockSignals(true);
    m_favorites_list->setValue(QVariant(favorites));
    m_favorites_list->blockSignals(false);

    /* FIXME: storing these attributes in GConf should not be tied to adding
              application to the list of favorites but instead should happen
              in the LauncherApplication itself whenever these values change.
    */
    /* Set GConf values corresponding to the favorite id for:
        - desktop file
        - type
        - priority
    */
    GConfItemQmlWrapper gconf_desktop_file;
    gconf_desktop_file.setKey(FAVORITES_KEY + favorite_id + "/desktop_file");
    gconf_desktop_file.setValue(QVariant(desktop_file));

    GConfItemQmlWrapper gconf_type;
    gconf_type.setKey(FAVORITES_KEY + favorite_id + "/type");
    gconf_type.setValue(QVariant(application->application_type()));

    int priority = application->priority();
    if (priority != -1) {
        GConfItemQmlWrapper gconf_priority;
        gconf_priority.setKey(FAVORITES_KEY + favorite_id + "/priority");
        /* FIXME: unity expects floats and not ints; it crashes at startup
                  otherwise */
        gconf_priority.setValue(QVariant(double(priority)));
    }
}

void
LauncherApplicationsList::removeApplicationFromFavorites(LauncherApplication* application)
{
    QString desktop_file = application->desktop_file();
    QStringList favorites = m_favorites_list->getValue().toStringList();

    for (QStringList::iterator i = favorites.begin(); i != favorites.end(); i++ )
    {
        QString current_desktop_file = desktopFilePathFromFavorite(*i);
        if (current_desktop_file == desktop_file)
        {
            favorites.erase(i);
            m_favorites_list->blockSignals(true);
            m_favorites_list->setValue(QVariant(favorites));
            m_favorites_list->blockSignals(false);
            /* The iterator 'i' is invalid but since we break off the loop
               nothing nasty happens. */
            break;
        }
    }
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

    if (!index.isValid())
        return QVariant();

    return QVariant::fromValue(m_applications.at(index.row()));
}

