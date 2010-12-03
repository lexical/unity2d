#include <QX11Info>
#include <QDebug>

#include "windowimageprovider.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

WindowImageProvider::WindowImageProvider() :
    QDeclarativeImageProvider(QDeclarativeImageProvider::Pixmap) {
}

WindowImageProvider::~WindowImageProvider() {
}

QPixmap WindowImageProvider::requestPixmap(const QString &id,
                                              QSize *size,
                                              const QSize &requestedSize) {
    Window win = (Window) id.toULong();
    XWindowAttributes attr;
    XGetWindowAttributes(QX11Info::display(), win, &attr);
    if (attr.map_state != IsViewable) {
        // If the window is unmapped, return an empty pixmap that is filled with
        // the "transparent" color. This will basically allow the icon below it to
        // show through. It's a bit of a trick but should be good enugh for now.
        QPixmap pixmap = (requestedSize.isValid()) ? QPixmap(requestedSize) : QPixmap();
        pixmap.fill(Qt::transparent);
        return pixmap;
    }

    QPixmap shot = QPixmap::fromX11Pixmap(win);
    if (!shot.isNull()) {
        if (requestedSize.isValid()) {
            shot = shot.scaled(requestedSize);
        }
        size->setWidth(shot.width());
        size->setHeight(shot.height());
        return shot;
    } else {
        // In case the capture fails, let's do the same trick as in the unmapped case
        QPixmap pixmap = (requestedSize.isValid()) ? QPixmap(requestedSize) : QPixmap();
        pixmap.fill(Qt::transparent);
        return pixmap;
    }

}

/*! Tries to ask the X Composite extension (if supported) to redirect all
    windows on all screens to backing storage. This does not have
    any effect if another application already requested the same
    thing (typically the window manager does this).
*/
void WindowImageProvider::activateComposite() {
    int event_base;
    int error_base;

    Display *display = QX11Info::display();
    bool compositeSupport = false;

    if (XCompositeQueryExtension(display, &event_base, &error_base)) {
        int major = 0;
        int minor = 2;
        XCompositeQueryVersion(display, &major, &minor);

        if (major > 0 || minor >= 2) {
            compositeSupport = true;
            qDebug().nospace() << "Server supports the Composite extension (ver "
                    << major << "." << minor << ")";
        }
        else {
            qDebug().nospace() << "Server supports the Composite extension, but "
                                  "version is < 0.2 (ver " << major << "." << minor << ")";
        }
    } else {
        qDebug() << "Server doesn't support the Composite extension.";
    }

    if (compositeSupport) {
        int screens = ScreenCount(display);
        for (int i = 0; i < screens; ++i) {
            XCompositeRedirectSubwindows(display, RootWindow(display, i),
                                         CompositeRedirectAutomatic);
        }
    }
}

