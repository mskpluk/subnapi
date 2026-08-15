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
const int kTailLineCount = 20;

// Credit-line labels, most authoritative first. Order matters: a file can
// carry several credits at once, and which one counts as "the author" is not
// the last one in the file but the highest-ranking label. A person who
// actually translated the dialogue outranks the studio or broadcaster that
// commissioned it - on a file crediting both "Wersja polska: Canal +" and
// "Tekst: Anna Maria Nowak" the author is Anna Maria Nowak. (The official
// NapiProjekt client picks "canal +" here; that is the client being wrong,
// not a convention worth copying.)
//
// "Korekta:", "Synchronizacja:" and similar are deliberately absent: they
// credit proofreading and timing, not authorship.
const char *const kAuthorLabels[] = {
    "tłumaczenie", "tekst polski", "tekst", "napisy", "wersja polska",
};

// A credit line carrying one of these words is not an authorship line, so
// whatever follows its colon is never the author. All are matched as stems so
// every inflection is covered.
//
// "pobran" - a download stamp left behind by a subtitle site: "Napisy pobrane
// z napiprojekt.pl: ...". Covers pobrane, pobrany, pobrana, pobranych.
//
// "piosen" - a song credit: "Tekst piosenki: ...", "Teksty piosenek: ...".
// That names the lyricist, not the person who authored the subtitles. Covers
// piosenka, piosenki, piosenek, piosenkę.
//
// "przekład" - covers przekład, przekładu, przekłady. A bare "Przekład: ..."
// is already ignored simply by not being an author label; this entry is what
// additionally rejects the compound forms that do start with a label, such as
// "Tłumaczenie i przekład: ..." or "Napisy i przekład piosenek: ...".
const char *const kRejectedWords[] = {"pobran", "piosen", "przekład"};

bool isRejectedCreditLine(const QString &line) {
  for (const char *const word : kRejectedWords) {
    if (line.contains(QString::fromUtf8(word), Qt::CaseInsensitive)) return true;
  }
  return false;
}
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

  // Each label is matched only at the start of a line, so a stray "tekst:" in
  // the middle of a line of dialogue is not mistaken for a credit. The
  // trailing "[^:\n]*" lets qualified variants through - "Tłumaczenie na
  // zlecenie ...:", "Napisy i synchronizacja:" - capturing whatever follows
  // the colon, except for the download stamps rejected above.
  for (const char *const label : kAuthorLabels) {
    QRegularExpression pattern(
        QString("^\\s*%1[^:\\n]*:\\s*(.+)$").arg(QString::fromUtf8(label)),
        QRegularExpression::CaseInsensitiveOption |
            QRegularExpression::MultilineOption);

    QString author;
    QRegularExpressionMatchIterator it = pattern.globalMatch(tail);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      if (isRejectedCreditLine(match.captured(0))) continue;
      // Within one label the credit is conventionally the last occurrence.
      author = match.captured(1).trimmed();
    }

    if (!author.isEmpty()) return author;
  }

  return QString();
}
