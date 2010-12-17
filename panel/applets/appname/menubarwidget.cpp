/*
 * This file is part of unity-qt
 *
 * Copyright 2010 Canonical Ltd.
 *
 * Authors:
 * - Aurélien Gâteau <aurelien.gateau@canonical.com>
 *
 * License: GPL v3
 */
// Self
#include "menubarwidget.h"

// Local
#include "config.h"
#include "debug_p.h"
#include "registrar.h"

// dbusmenu-qt
#include <dbusmenuimporter.h>

// bamf
#include <bamf-matcher.h>
#include <bamf-window.h>

// Qt
#include <QActionEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QTimer>

class MyDBusMenuImporter : public DBusMenuImporter
{
public:
    MyDBusMenuImporter(const QString &service, const QString &path, QObject *parent)
    : DBusMenuImporter(service, path, parent)
    , m_service(service)
    , m_path(path)
    {}

    QString service() const { return m_service; }
    QString path() const { return m_path; }

private:
    QString m_service;
    QString m_path;
};

MenuBarWidget::MenuBarWidget(QMenu* windowMenu, QWidget* parent)
: QWidget(parent)
, m_windowMenu(windowMenu)
{
    m_activeWinId = 0;
    setupRegistrar();
    setupMenuBar();

    connect(&BamfMatcher::get_default(), SIGNAL(ActiveWindowChanged(BamfWindow*, BamfWindow*)),
        SLOT(slotActiveWindowChanged(BamfWindow*, BamfWindow*)));
    updateActiveWinId(BamfMatcher::get_default().active_window());
}

void MenuBarWidget::setupRegistrar()
{
    m_registrar = new Registrar(this);
    if (!m_registrar->connectToBus()) {
        UQ_WARNING << "could not connect registrar to DBus";
    }

    connect(m_registrar, SIGNAL(WindowRegistered(WId, const QString&, const QDBusObjectPath&)),
        SLOT(slotWindowRegistered(WId, const QString&, const QDBusObjectPath&)));
}

void MenuBarWidget::setupMenuBar()
{
    QLabel* separatorLabel = new QLabel;
    QPixmap pix(unityQtDirectory() + "/panel/artwork/divider.png");
    separatorLabel->setPixmap(pix);
    separatorLabel->setFixedSize(pix.size());

    m_menuBar = new QMenuBar;
    new MenuBarClosedHelper(this);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(separatorLabel);
    layout->addWidget(m_menuBar);
    m_menuBar->setNativeMenuBar(false);

    m_updateMenuBarTimer = new QTimer(this);
    m_updateMenuBarTimer->setSingleShot(true);
    m_updateMenuBarTimer->setInterval(0);
    connect(m_updateMenuBarTimer, SIGNAL(timeout()),
        SLOT(updateMenuBar()));
}

void MenuBarWidget::slotActiveWindowChanged(BamfWindow* /*former*/, BamfWindow* current)
{
    if (current) {
        updateActiveWinId(current);
    }
}

void MenuBarWidget::slotWindowRegistered(WId wid, const QString& service, const QDBusObjectPath& menuObjectPath)
{
    MyDBusMenuImporter* importer = new MyDBusMenuImporter(service, menuObjectPath.path(), this);
    delete m_importers.take(wid);
    m_importers.insert(wid, importer);
    connect(importer, SIGNAL(menuUpdated()), SLOT(slotMenuUpdated()));
    connect(importer, SIGNAL(actionActivationRequested(QAction*)), SLOT(slotActionActivationRequested(QAction*)));
    QMetaObject::invokeMethod(importer, "updateMenu", Qt::QueuedConnection);
}

void MenuBarWidget::slotMenuUpdated()
{
    DBusMenuImporter* importer = static_cast<DBusMenuImporter*>(sender());

    if (m_importers.value(m_activeWinId) == importer) {
        updateMenuBar();
    }
}

void MenuBarWidget::slotActionActivationRequested(QAction* action)
{
    DBusMenuImporter* importer = static_cast<DBusMenuImporter*>(sender());

    if (m_importers.value(m_activeWinId) == importer) {
        m_menuBar->setActiveAction(action);
    }
}

QMenu* MenuBarWidget::menuForWinId(WId wid) const
{
    MyDBusMenuImporter* importer = m_importers.value(wid);
    return importer ? importer->menu() : 0;
}

void MenuBarWidget::updateActiveWinId(BamfWindow* bamfWindow)
{
    WId id = bamfWindow ? bamfWindow->xid() : 0;
    if (id == m_activeWinId) {
        return;
    }
    if (id == window()->winId()) {
        // Do not update id if the active window is the one hosting this applet
        return;
    }
    m_activeWinId = id;
    updateMenuBar();
}

void MenuBarWidget::updateMenuBar()
{
    WId winId = m_activeWinId;
    QMenu* menu = menuForWinId(winId);

    if (!menu) {
        if (winId) {
            menu = m_windowMenu;
        } else {
            // No active window, show a desktop menubar
            // FIXME: Empty menu
            /*
            menu = mEmptyMenu;
            */
        }
    }

    m_menuBar->clear();
    // FIXME: Empty menu
    if (!menu) {
        return;
    }
    menu->installEventFilter(this);
    Q_FOREACH(QAction* action, menu->actions()) {
        if (action->isSeparator()) {
            continue;
        }
        m_menuBar->addAction(action);
    }
}

bool MenuBarWidget::eventFilter(QObject* object, QEvent* event)
{
    switch (event->type()) {
    case QEvent::ActionAdded:
    case QEvent::ActionRemoved:
    case QEvent::ActionChanged:
        m_updateMenuBarTimer->start();
        break;
    default:
        break;
    }
    return false;
}

// MenuBarClosedHelper ----------------------------------------
MenuBarClosedHelper::MenuBarClosedHelper(MenuBarWidget* widget)
: QObject(widget)
, m_widget(widget)
{
    widget->menuBar()->installEventFilter(this);
}

bool MenuBarClosedHelper::eventFilter(QObject* object, QEvent* event)
{
    if (object == m_widget->menuBar()) {
        switch (event->type()) {
            case QEvent::ActionAdded:
            case QEvent::ActionRemoved:
            case QEvent::ActionChanged:
                menuBarActionEvent(static_cast<QActionEvent*>(event));
                break;
            default:
                break;
        }
    } else {
        // Top-level menus
        if (event->type() == QEvent::Hide) {
            // menu hides themselves when the menubar is closed but also when
            // one goes from one menu to another. The way to know this is to
            // check the value of QMenuBar::activeAction(), but at this point
            // it has not been updated yet, so we check in a delayed method.
            QMetaObject::invokeMethod(this, "emitMenuBarClosed", Qt::QueuedConnection);
        }
    }
    return false;
}

void MenuBarClosedHelper::emitMenuBarClosed()
{
    if (!m_widget->menuBar()->activeAction()) {
        QMetaObject::invokeMethod(m_widget, "menuBarClosed");
    }
}

void MenuBarClosedHelper::menuBarActionEvent(QActionEvent* event)
{
    // Install/remove event filters on top level menus so that we can know when
    // they hide themselves (to emit menuBarClosed())
    QMenu* menu = event->action()->menu();
    if (!menu) {
        return;
    }
    switch (event->type()) {
    case QEvent::ActionAdded:
    case QEvent::ActionChanged:
        menu->installEventFilter(this);
        break;
    case QEvent::ActionRemoved:
        menu->removeEventFilter(this);
        break;
    default:
        break;
    }
}

#include "menubarwidget.moc"