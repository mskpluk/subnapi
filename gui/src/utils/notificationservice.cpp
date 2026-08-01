/****************************************************************************
** QNapi
** Copyright (C) 2008-2017 Piotr Krzemiński <pio.krzeminski@gmail.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
*****************************************************************************/

#include "notificationservice.h"

#include <QMessageBox>

#if defined(Q_OS_LINUX) && defined(QT_DBUS_LIB)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QVariant>
#endif

void NotificationService::showInfo(const QString& title, const QString& message,
                                   bool preferDesktopNotifications) {
  if (preferDesktopNotifications && showDesktopNotification(title, message)) {
    return;
  }

  QMessageBox::information(nullptr, title, message);
}

bool NotificationService::showDesktopNotification(const QString& title,
                                                  const QString& message) {
#if defined(Q_OS_LINUX) && defined(QT_DBUS_LIB)
  QDBusInterface notifications(
      "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
      "org.freedesktop.Notifications", QDBusConnection::sessionBus());

  if (!notifications.isValid()) {
    return false;
  }

  // Icon is resolved by name on the host: "subnapi" for a regular install
  // (hicolor), the app id for Flatpak (only the exported, id-named icons
  // are visible outside the sandbox).
  QString iconName = qEnvironmentVariable("FLATPAK_ID", "subnapi");
  QDBusReply<uint> reply = notifications.call(
      "Notify", "subnapi", static_cast<uint>(0), iconName, title, message,
      QStringList(), QVariantMap(), 5000);

  return reply.isValid();
#else
  Q_UNUSED(title);
  Q_UNUSED(message);
  return false;
#endif
}
