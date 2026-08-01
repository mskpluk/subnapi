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

#include "subtitleauthordetector.h"
#include "encodingutils.h"

#include <QFile>
#include <QRegularExpression>

namespace {
const int kTailLineCount = 10;
}  // namespace

QString detectSubtitleAuthor(const QString &subtitleFilePath) {
  QFile file(subtitleFilePath);
  if (!file.open(QIODevice::ReadOnly)) return QString();

  QByteArray raw = file.readAll();
  file.close();

  EncodingUtils encodingUtils;
  QString detectedEncoding = encodingUtils.detectFileEncoding(subtitleFilePath);
  if (detectedEncoding.isEmpty()) return QString();

  Maybe<QString> decoded = encodingUtils.decode(detectedEncoding, raw);
  if (!decoded) return QString();

  QStringList lines = decoded.value().split(QRegularExpression("\r\n|\n|\r"));
  int startIdx = qMax(0, lines.size() - kTailLineCount);
  QString tail = lines.mid(startIdx).join("\n");

  // Matches "Tłumaczenie:", "tłumaczenie:", "tekst polski:", "tekst:",
  // "Tłumaczenie na zlecenie ...:" - anything starting with tłumaczenie/tekst
  // up to the next colon, capturing what follows it.
  static const QRegularExpression authorPattern(
      "(?:tłumaczenie|tekst)[^:\\n]*:\\s*(.+)",
      QRegularExpression::CaseInsensitiveOption);

  QString author;
  QRegularExpressionMatchIterator it = authorPattern.globalMatch(tail);
  while (it.hasNext()) {
    // Several matches can occur in the tail (e.g. a stray "tekst:" in
    // dialogue); the credit line is conventionally the last one.
    author = it.next().captured(1).trimmed();
  }
  return author;
}
