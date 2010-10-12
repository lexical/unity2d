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
#ifndef DATETIMEINDICATOR_H
#define DATETIMEINDICATOR_H

// Local
#include "abstractindicator.h"

// Qt
#include <QTimer>

class DateTimeIndicator : public AbstractIndicator
{
    Q_OBJECT
public:
    DateTimeIndicator(QObject* parent=0);

private Q_SLOTS:
    void updateText();
private:
    Q_DISABLE_COPY(DateTimeIndicator)
    QTimer* m_timer;
    QString m_format;

    void setupTimer();
    void setupMenu();
    void readConfig();
    void updateTimer();
};

#endif /* DATETIMEINDICATOR_H */
