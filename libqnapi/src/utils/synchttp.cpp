/*****************************************************************************
**  Copyright (C) 2005 by Iulian M
**  eti@erata.net
**
**  modifications by Piotr Krzemiński
**  Copyright (C) 2008-2017
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

#include "synchttp.h"

namespace {
// Qt sets no transfer timeout of its own - the default is 0, meaning wait for
// as long as it takes. A server that accepts a connection and then says
// nothing therefore blocks these calls forever, and they are made from worker
// threads that the dialogs join with wait() when they close: one silent
// server and closing the window locks up the whole application.
//
// Generous enough for a slow subtitle server on a slow connection, finite
// enough that nothing waits on it indefinitely. The timeout counts inactivity,
// not total duration, so a large download that keeps arriving is unaffected.
const int kTransferTimeoutMsecs = 30000;

QNetworkRequest withTimeout(const QNetworkRequest& req) {
  QNetworkRequest timed(req);
  timed.setTransferTimeout(kTransferTimeoutMsecs);
  return timed;
}
}  // namespace

SyncHTTP::SyncHTTP() : manager(this) {}

QNetworkReply* SyncHTTP::syncGet(const QNetworkRequest& req) {
  QNetworkReply* reply = manager.get(withTimeout(req));
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  connect(reply, &QNetworkReply::errorOccurred, &loop, &QEventLoop::quit);
  loop.exec();
  return reply;
}

QNetworkReply* SyncHTTP::syncPost(const QNetworkRequest& req,
                                  const QByteArray& data) {
  QNetworkReply* reply = manager.post(withTimeout(req), data);
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  connect(reply, &QNetworkReply::errorOccurred, &loop, &QEventLoop::quit);
  loop.exec();
  return reply;
}

QNetworkReply* SyncHTTP::syncPost(const QNetworkRequest& req,
                                  QHttpMultiPart* multiPart) {
  QNetworkReply* reply = manager.post(withTimeout(req), multiPart);
  multiPart->setParent(reply);
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  connect(reply, &QNetworkReply::errorOccurred, &loop, &QEventLoop::quit);
  loop.exec();
  return reply;
}
