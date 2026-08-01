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

#ifndef LEGACYCODECS_H
#define LEGACYCODECS_H

#include <Maybe.h>
#include <QByteArray>
#include <QString>

// Replacement for the single-byte Central/Eastern European codecs that
// QTextCodec used to provide (windows-125x, ISO-8859-*). Qt6's QStringConverter
// only knows Unicode/Latin1/System encodings, so these five are implemented
// directly using static mapping tables instead.
class LegacyCodecs {
 public:
  enum class Encoding {
    Windows1250,
    Windows1257,
    Iso8859_2,
    Iso8859_13,
    Iso8859_16
  };

  static Maybe<Encoding> encodingForName(const QString& name);
  static QString toUnicode(Encoding encoding, const QByteArray& bytes);
  static QByteArray fromUnicode(Encoding encoding, const QString& text);
};

#endif  // LEGACYCODECS_H
