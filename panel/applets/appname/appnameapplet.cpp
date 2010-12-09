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
#include "appnameapplet.h"

// Local
#include "menubarwidget.h"
#include "windowhelper.h"

// Unity-qt
#include <debug_p.h>

// Bamf
#include <bamf-application.h>
#include <bamf-matcher.h>

// Qt
#include <QAbstractButton>
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

static const char* METACITY_DIR = "/usr/share/themes/Ambiance/metacity-1";

static const int WINDOW_BUTTONS_RIGHT_MARGIN = 4;

namespace UnityQt
{

class WindowButton : public QAbstractButton
{
public:
    WindowButton(const QString& prefix, QWidget* parent = 0)
    : QAbstractButton(parent)
    , m_prefix(prefix)
    , m_normalPix(loadPix("normal"))
    , m_hoverPix(loadPix("prelight"))
    , m_downPix(loadPix("pressed"))
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
        setAttribute(Qt::WA_Hover);
    }

    QSize minimumSizeHint() const
    {
        return m_normalPix.size();
    }

protected:
    void paintEvent(QPaintEvent*)
    {
        QPainter painter(this);
        QPixmap pix;
        if (isDown()) {
            pix = m_downPix;
        } else if (underMouse()) {
            pix = m_hoverPix;
        } else {
            pix = m_normalPix;
        }
        painter.drawPixmap((width() - pix.width()) / 2, (height() - pix.height()) / 2, pix);
    }

private:
    QString m_prefix;
    QPixmap m_normalPix;
    QPixmap m_hoverPix;
    QPixmap m_downPix;

    QPixmap loadPix(const QString& name)
    {
        QString path = QString("%1/%2_focused_%3.png")
            .arg(METACITY_DIR)
            .arg(m_prefix)
            .arg(name);
        return QPixmap(path);
    }
};

struct AppNameAppletPrivate
{
    AppNameApplet* q;
    QWidget* m_windowButtonWidget;
    WindowButton* m_closeButton;
    WindowButton* m_minimizeButton;
    WindowButton* m_maximizeButton;
    QLabel* m_label;
    WindowHelper* m_windowHelper;
    MenuBarWidget* m_menuBarWidget;

    void setupLabel()
    {
        m_label = new QLabel;
        QFont font = m_label->font();
        font.setBold(true);
        m_label->setFont(font);
    }

    void setupWindowButtonWidget()
    {
        m_windowButtonWidget = new QWidget;
        QHBoxLayout* layout = new QHBoxLayout(m_windowButtonWidget);
        layout->setContentsMargins(0, 0, WINDOW_BUTTONS_RIGHT_MARGIN, 0);
        layout->setSpacing(0);
        m_closeButton = new WindowButton("close");
        m_minimizeButton = new WindowButton("minimize");
        m_maximizeButton = new WindowButton("unmaximize");
        layout->addWidget(m_closeButton);
        layout->addWidget(m_minimizeButton);
        layout->addWidget(m_maximizeButton);
        QObject::connect(m_closeButton, SIGNAL(clicked()), m_windowHelper, SLOT(close()));
        QObject::connect(m_minimizeButton, SIGNAL(clicked()), m_windowHelper, SLOT(minimize()));
        QObject::connect(m_maximizeButton, SIGNAL(clicked()), m_windowHelper, SLOT(unmaximize()));
    }

    void setupWindowHelper()
    {
        m_windowHelper = new WindowHelper(q);
        QObject::connect(m_windowHelper, SIGNAL(stateChanged()),
            q, SLOT(updateWidgets()));
    }
};

AppNameApplet::AppNameApplet()
: d(new AppNameAppletPrivate)
{
    d->q = this;

    d->setupWindowHelper();
    d->setupLabel();
    d->setupWindowButtonWidget();
    d->m_menuBarWidget = new MenuBarWidget;

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(d->m_windowButtonWidget);
    layout->addWidget(d->m_label);
    layout->addWidget(d->m_menuBarWidget);

    updateWidgets();
}

AppNameApplet::~AppNameApplet()
{
    delete d;
}

void AppNameApplet::updateLabel()
{
    BamfWindow* bamfWindow = BamfMatcher::get_default().active_window();
    d->m_label->setText(bamfWindow ? bamfWindow->name() : QString());
}

void AppNameApplet::updateWidgets()
{
    updateLabel();

    bool isMaximized = d->m_windowHelper->isMaximized();
    bool under = window()->underMouse();

    d->m_windowButtonWidget->setVisible(isMaximized);

    d->m_label->setVisible(!(isMaximized && under));
    bool labelIsCropped = !isMaximized && under;
    d->m_label->setMaximumWidth(labelIsCropped
        ? d->m_windowButtonWidget->sizeHint().width()
        : QWIDGETSIZE_MAX);

    d->m_menuBarWidget->setVisible(under);
}

bool AppNameApplet::event(QEvent* event)
{
    if (event->type() == QEvent::ParentChange) {
        // Install an event filter on the panel to detect mouse over
        window()->installEventFilter(this);
    }
    return Applet::event(event);
}

bool AppNameApplet::eventFilter(QObject*, QEvent* event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
        updateWidgets();
        break;
    default:
        break;
    }
    return false;
}

} // namespace

#include "appnameapplet.moc"
