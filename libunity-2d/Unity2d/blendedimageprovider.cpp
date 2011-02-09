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

#include "blendedimageprovider.h"
#include <QPainter>

BlendedImageProvider::BlendedImageProvider() : QDeclarativeImageProvider(QDeclarativeImageProvider::Image)
{
}

BlendedImageProvider::~BlendedImageProvider()
{
}

#include <QDebug>
QImage BlendedImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    /* id is of the form [FILENAME]color=[COLORNAME]alpha=[FLOAT] */
    QRegExp rx("^(.+)color=(.+)alpha=(\\d+(?:\\.\\d+)?)$");
    if (rx.indexIn(id)) {
        qWarning() << "BlendedImageProvider: faile to match id:" << id;
        return QImage();
    }
    QStringList list = rx.capturedTexts();

    QString fileName = list[1];
    if (fileName.isEmpty()) {
        qWarning() << "BlendedImageProvider: filename can't be empty.";
        return QImage();
    }

    QString colorName = list[2];
    if (!QColor::isValidColor(colorName)) {
        /* Passing a named color of the form #RRGGBB is impossible
           due to the fact that QML Image considers the source an URL and strips any anchor
           from the string it passes to this method (i.e. everything after the #).
           As a workaround we allow passing the color as RRGGBB and when a sting isn't an
           SVG color name we try interpreting it as an RRGGBB color by adding back the #.
        */
        colorName.prepend("#");
        if (!QColor::isValidColor(colorName)) {
            qWarning() << "BlendedImageProvider: invalid color name:" << list[2];
            return QImage();
        }
    }
    QColor color(colorName);

    bool valid = false;
    float alpha = list[3].toFloat(&valid);
    if (!valid) {
        qWarning() << "BlendedImageProvider: can't convert alpha to floating point:" << list[3];
        return QImage();
    }
    color.setAlphaF(alpha);

    QImage image(fileName);
    if (image.isNull()) {
        qWarning() << "BlendedImageProvider: failed to load image from file:" << fileName;
        return QImage();
    }

    if (requestedSize.width() == 0 && requestedSize.height() != 0) {
        image = image.scaledToHeight(requestedSize.height(), Qt::SmoothTransformation);
    } else if (requestedSize.width() != 0 && requestedSize.height() == 0) {
        image = image.scaledToWidth(requestedSize.width(), Qt::SmoothTransformation);
    } else if (requestedSize.isValid()) {
        image = image.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    if (size) {
        *size = image.size();
    }

    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter.fillRect(image.rect(), color);
    painter.end();

    image.save("/tmp/tests/" + QString(id).replace("/", "_") + ".png");

    return image;
}
