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

#include "encodingutils.h"
#include "utils/legacycodecs.h"

#include <QFile>
#include <QStringDecoder>
#include <QStringEncoder>

EncodingUtils::EncodingUtils() {
  diacritics = QString::fromUtf8(
      "ąćęłńśżźĄĆĘŁŃŚŻŹŠŒŽšœžŸ¥µÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝßàáâãäåæçèéêëìíîïð"
      "ñòóôõöøùúûüýÿ");
  replacements << "a"
               << "c"
               << "e"
               << "l"
               << "n"
               << "s"
               << "z"
               << "z"
               << "A"
               << "C"
               << "E"
               << "L"
               << "N"
               << "S"
               << "Z"
               << "Z"
               << "S"
               << "OE"
               << "Z"
               << "s"
               << "oe"
               << "z"
               << "Y"
               << "Y"
               << "u"
               << "A"
               << "A"
               << "A"
               << "A"
               << "A"
               << "A"
               << "AE"
               << "C"
               << "E"
               << "E"
               << "E"
               << "E"
               << "I"
               << "I"
               << "I"
               << "I"
               << "D"
               << "N"
               << "O"
               << "O"
               << "O"
               << "O"
               << "O"
               << "O"
               << "U"
               << "U"
               << "U"
               << "U"
               << "Y"
               << "s"
               << "a"
               << "a"
               << "a"
               << "a"
               << "a"
               << "a"
               << "ae"
               << "c"
               << "e"
               << "e"
               << "e"
               << "e"
               << "i"
               << "i"
               << "i"
               << "i"
               << "o"
               << "n"
               << "o"
               << "o"
               << "o"
               << "o"
               << "o"
               << "o"
               << "u"
               << "u"
               << "u"
               << "u"
               << "y"
               << "y";

  codecs << "windows-1257"
         << "ISO-8859-13"
         << "ISO-8859-16"
         << "ISO-8859-2"
         << "windows-1250"
         << "UTF-8";
}

QString EncodingUtils::replaceDiacriticsWithASCII(const QString &str) const {
  QString output;
  for (int i = 0; i < str.length(); i++) {
    QChar c = str[i];
    int dIndex = diacritics.indexOf(c);
    if (dIndex < 0) {
      output.append(c);
    } else {
      QString replacement = replacements[dIndex];
      output.append(replacement);
    }
  }
  return output;
}

QString EncodingUtils::detectBufferEncoding(const QByteArray &buffer) const {
  // A byte-order mark unambiguously identifies UTF-16 (and UTF-8), which the
  // legacy-codec matching loop below can never detect on its own.
  auto bomEncoding = QStringConverter::encodingForData(buffer);
  if (bomEncoding) {
    if (*bomEncoding == QStringConverter::Utf16 ||
        *bomEncoding == QStringConverter::Utf16LE ||
        *bomEncoding == QStringConverter::Utf16BE) {
      return "UTF-16";
    }
    if (*bomEncoding == QStringConverter::Utf8) return "UTF-8";
  }

  // The result of decode() has to be assigned: it hands back a proxy that only
  // performs the conversion once something turns it into a QString. Dropping it
  // means no conversion ever runs, so hasError() stays false and every file
  // without a BOM is reported as UTF-8 - which also makes the codec matching
  // below unreachable.
  QStringDecoder utf8Decoder(QStringConverter::Utf8);
  const QString utf8Decoded = utf8Decoder.decode(buffer);
  Q_UNUSED(utf8Decoded);
  if (!utf8Decoder.hasError()) return "UTF-8";

  // UTF-8 has just been ruled out, so it is no candidate here, and a file with
  // no Polish letters at all - English or Czech subtitles with "…" or "é" -
  // falls back to windows-1250 rather than to the one encoding known to be
  // wrong. Guessing UTF-8 there turned every such byte into U+FFFD for good,
  // once the subtitles were written back.
  int bestMatch = 0;
  QString from = "windows-1250";

  foreach (QString codec, codecs) {
    if (codec == "UTF-8") continue;
    const QString text = decode(codec, buffer).value();

    QStringList chars = QString::fromUtf8("ą/ś/ż/ć/ń/ł/ó/ę").split("/");

    int found = 0;

    foreach (QString c, chars) {
      if (text.contains(c, Qt::CaseInsensitive)) {
        ++found;
      }
    }

    if (found >= bestMatch) {
      bestMatch = found;
      from = codec;
    }
  }

  return from;
}

bool EncodingUtils::isSupportedEncoding(const QString &encodingName) {
  if (encodingName.startsWith("UTF-16", Qt::CaseInsensitive)) return true;
  if (encodingName.compare("UTF-8", Qt::CaseInsensitive) == 0) return true;
  return (bool)LegacyCodecs::encodingForName(encodingName);
}

Maybe<QString> EncodingUtils::decode(const QString &encodingName,
                                     const QByteArray &bytes) const {
  if (encodingName.startsWith("UTF-16", Qt::CaseInsensitive))
    return just(QStringDecoder(QStringConverter::Utf16).decode(bytes));

  if (encodingName.compare("UTF-8", Qt::CaseInsensitive) == 0)
    return just(QString::fromUtf8(bytes));

  Maybe<LegacyCodecs::Encoding> legacy =
      LegacyCodecs::encodingForName(encodingName);
  if (legacy) return just(LegacyCodecs::toUnicode(legacy.value(), bytes));

  return nothing();
}

Maybe<QByteArray> EncodingUtils::encode(const QString &encodingName,
                                        const QString &text) const {
  if (encodingName.startsWith("UTF-16", Qt::CaseInsensitive))
    return just(QStringEncoder(QStringConverter::Utf16,
                               QStringConverter::Flag::WriteBom)
                    .encode(text));

  if (encodingName.compare("UTF-8", Qt::CaseInsensitive) == 0)
    return just(text.toUtf8());

  Maybe<LegacyCodecs::Encoding> legacy =
      LegacyCodecs::encodingForName(encodingName);
  if (legacy) return just(LegacyCodecs::fromUnicode(legacy.value(), text));

  return nothing();
}

QString EncodingUtils::detectFileEncoding(const QString &filename) const {
  QFile f(filename);
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QString enc = detectBufferEncoding(f.readAll());
    f.close();
    return enc;
  }
  return "";
}
