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

#ifndef APPLICATION_H
#define APPLICATION_H

#include <gio/gdesktopappinfo.h>
#include <libwnck/libwnck.h>

// libindicator
#include <libindicator/indicator-desktop-shortcuts.h>

#include "launcheritem.h"

// libunity-2d
#include <gscopedpointer.h>

// Qt
#include <QObject>
#include <QUrl>
#include <QMetaType>
#include <QString>
#include <QTimer>
#include <QHash>
#include <QPointer>
#include <QScopedPointer>

#include "bamf-application.h"

struct SnStartupSequence;
extern "C" {
void sn_startup_sequence_unref(struct SnStartupSequence*);
}

class DBusMenuImporter;
class QFileSystemWatcher;
class QDBusServiceWatcher;

typedef GObjectScopedPointer<GAppInfo> GAppInfoPointer;
typedef GObjectScopedPointer<GDesktopAppInfo> GDesktopAppInfoPointer;
typedef GScopedPointer<SnStartupSequence, sn_startup_sequence_unref> SnStartupSequencePointer;
typedef GObjectScopedPointer<IndicatorDesktopShortcuts> IndicatorDesktopShortcutsPointer;
class Application : public LauncherItem
{
    Q_OBJECT
    friend class ApplicationsListDBUS;

    Q_PROPERTY(bool sticky READ sticky WRITE setSticky NOTIFY stickyChanged)
    Q_PROPERTY(QString application_type READ application_type NOTIFY applicationTypeChanged)
    Q_PROPERTY(QString desktop_file READ desktop_file WRITE setDesktopFile NOTIFY desktopFileChanged)
    Q_PROPERTY(QString executable READ executable NOTIFY executableChanged)
    Q_PROPERTY(bool has_visible_window READ has_visible_window NOTIFY hasVisibleWindowChanged)

public:
    Application();
    Application(const Application& other);
    ~Application();

    /* getters */
    virtual bool active() const;
    virtual bool running() const;
    virtual int windowCount() const;
    virtual bool urgent() const;
    bool sticky() const;
    virtual QString name() const;
    virtual QString icon() const;
    QString application_type() const;
    QString desktop_file() const;
    QString executable() const;
    virtual bool launching() const;
    bool has_visible_window() const;

    virtual bool progressBarVisible() const;
    virtual float progress() const;
    virtual bool counterVisible() const;
    virtual int counter() const;
    virtual bool emblemVisible() const;
    virtual QString emblem() const;

    /* setters */
    void setDesktopFile(const QString& desktop_file);
    void setSticky(bool sticky);
    void setBamfApplication(BamfApplication *application);
    void setSnStartupSequence(SnStartupSequence* sequence);

    /* methods */
    Q_INVOKABLE virtual void activate();
    Q_INVOKABLE void close();
    Q_INVOKABLE void spread(bool showAllWorkspaces = false);
    Q_INVOKABLE void setIconGeometry(int x, int y, int width, int height, uint xid=0);
    Q_INVOKABLE virtual void launchNewInstance();

    Q_INVOKABLE virtual void createMenuActions();
    Q_INVOKABLE virtual bool belongsToDifferentWorkspace();
    Q_INVOKABLE virtual bool belongsToDifferentScreen(int screen);
    Q_INVOKABLE void connectWindowSignals();
    void disconnectWindowSignals();

    void updateOverlaysState(const QString& sender, const QMap<QString, QVariant>& properties);

Q_SIGNALS:
    void stickyChanged(bool);
    void applicationTypeChanged(QString);
    void desktopFileChanged(QString);
    void executableChanged(QString);
    void hasVisibleWindowChanged(bool);

    void closed();

    void windowAdded(uint xid);

private Q_SLOTS:
    void onBamfApplicationClosed(bool running);
    void onLaunchingTimeouted();
    void updateHasVisibleWindow();
    void updateWindowCount();
    void updateCounterVisible();

    bool launch();
    void show();

    /* Contextual menu callbacks */
    void onStaticShortcutTriggered();
    void onKeepTriggered();
    void onQuitTriggered();

    void onWindowAdded(BamfWindow*);

    void slotChildAdded(BamfView*);
    void slotChildRemoved(BamfView*);
    void onIndicatorMenuUpdated();

    void onDesktopFileChanged(const QString&);
    void checkDesktopFileReallyRemoved();
    void beginForceUrgent(int duration);
    void endForceUrgent();

    void dynamicQuicklistImporterServiceOwnerChanged(const QString& serviceName, const QString& oldOwner, const QString& newOwner);

    void onDragEnter(DeclarativeDragDropEvent*);
    void onDrop(DeclarativeDragDropEvent*);

    void onWindowGeometryChanged();

private:
    QPointer<BamfApplication> m_application;
    QFileSystemWatcher *m_desktopFileWatcher;
    GAppInfoPointer m_appInfo;
    SnStartupSequencePointer m_snStartupSequence;
    bool m_sticky;
    QTimer m_launching_timer;
    bool m_has_visible_window;
    QHash<QString, DBusMenuImporter*> m_indicatorMenus;
    int m_indicatorMenusReady;
    float m_progress;
    bool m_progressBarVisible;
    int m_counter;
    bool m_counterVisible;
    QString m_emblem;
    bool m_emblemVisible;
    bool m_forceUrgent;

    void updateBamfApplicationDependentProperties();
    void monitorDesktopFile(const QString&);
    void fetchIndicatorMenus();
    void createDynamicMenuActions();
    void createStaticMenuActions();
    int windowCountOnCurrentWorkspace();
    template<typename T>
    bool updateOverlayState(const QMap<QString, QVariant>& properties,
                            const QString& propertyName, T* member);
    QList<QUrl> validateUrisForLaunch(DeclarativeMimeData* mimedata);
    QStringList supportedTypes();

    static void onWindowWorkspaceChanged(WnckWindow *window, gpointer user_data);

    QString m_dynamicQuicklistPath;
    QScopedPointer<DBusMenuImporter> m_dynamicQuicklistImporter;
    QDBusServiceWatcher* m_dynamicQuicklistServiceWatcher;
    void setDynamicQuicklistImporter(const QString& service);
    IndicatorDesktopShortcutsPointer m_staticShortcuts;
    QTimer m_geometryChangedTimer;
};

Q_DECLARE_METATYPE(Application*)

#endif // APPLICATION_H

