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

#ifndef ENCODINGUTILS_H
#define ENCODINGUTILS_H

#include <Maybe.h>
#include <QByteArray>
#include <QString>
#include <QStringList>

class EncodingUtils {
 public:
  EncodingUtils();

  QString replaceDiacriticsWithASCII(const QString& str) const;
  QString detectBufferEncoding(const QByteArray& buffer) const;
  QString detectFileEncoding(const QString& filename) const;

  // Decode/encode using one of the supported encoding names: UTF-8, UTF-16,
  // windows-1250, windows-1257, ISO-8859-2, ISO-8859-13, ISO-8859-16.
  // Returns nothing() if encodingName isn't one of the supported names.
  Maybe<QString> decode(const QString& encodingName,
                        const QByteArray& bytes) const;
  Maybe<QByteArray> encode(const QString& encodingName,
                           const QString& text) const;

  // Whether decode()/encode() know this encoding name. Qt5 accepted every name
  // its codec registry carried, so a configuration written by an older version
  // can name one that is gone - and a conversion asked for in a name nothing
  // recognises does not happen at all.
  static bool isSupportedEncoding(const QString& encodingName);

 private:
  QString diacritics;
  QStringList replacements;
  QStringList codecs;
};

#endif  // ENCODINGUTILS_H
