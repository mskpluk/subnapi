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

#include "filehash.h"

#include <QCryptographicHash>
#include <QFile>

QString md5HexOfFile(const QString& path, qint64 limitBytes) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return QString();

  QByteArray data = limitBytes >= 0 ? file.read(limitBytes) : file.readAll();
  file.close();

  return QString::fromLatin1(
      QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex());
}
