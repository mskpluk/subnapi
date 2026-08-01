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

#ifndef FILEHASH_H
#define FILEHASH_H

#include <QString>

// MD5 hex digest of a file's contents. With limitBytes >= 0, hashes only the
// first limitBytes bytes (NapiProjekt's video-hash algorithm reads the first
// 10MB); with the default -1, hashes the whole file (used for subtitle
// hashes). Returns an empty string if the file can't be opened.
QString md5HexOfFile(const QString& path, qint64 limitBytes = -1);

#endif  // FILEHASH_H
