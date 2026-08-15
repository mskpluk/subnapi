/*****************************************************************************
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

#include "syncxmlrpc.h"

#include <QTimer>

namespace {
// Neither slot below is called if the server accepts the connection and then
// says nothing, which would leave the wait here running for good - on a worker
// thread that the dialogs join with wait() when they close. Guarded rather
// than trusted; matches the timeout SyncHTTP applies to its own requests.
const int kRequestTimeoutMsecs = 30000;
}  // namespace

SyncXmlRpc::SyncXmlRpc(const QUrl &endPoint) : rpc(endPoint) {}

bool SyncXmlRpc::request(const QString &method, const QVariantList &args) {
  result = false;

  rpc.call(method, args, this, SLOT(whenResponse(QVariant &)), this,
           SLOT(whenFault(int, const QString &)));

  QTimer timeout;
  timeout.setSingleShot(true);
  connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  timeout.start(kRequestTimeoutMsecs);

  loop.exec();

  // Left as a failure when the timer is what ended the wait: neither slot ran,
  // so nothing was received and result still holds the false set above.
  return result;
}

QVariant SyncXmlRpc::getResponse() const { return resp; }

QVariant SyncXmlRpc::getError() const {
  return QString::number(error) + " " + message;
}

void SyncXmlRpc::whenResponse(QVariant &response) {
  result = true;
  resp = response;
  loop.exit();
}

void SyncXmlRpc::whenFault(int err, const QString &msg) {
  result = false;
  error = err;
  message = msg;
  loop.exit();
}
