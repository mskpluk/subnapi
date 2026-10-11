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

#include "subtitlepostprocessor.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

SubtitlePostProcessor::SubtitlePostProcessor(
    const PostProcessingConfig& ppConfig,
    const QSharedPointer<const SubtitleConverter>& subtitleConverter)
    : ppConfig(ppConfig), subtitleConverter(subtitleConverter) {}

void SubtitlePostProcessor::perform(const QString& movieFilePath,
                                    const QString& subtitleFilePath) const {
  if (ppConfig.removeLines()) {
    ppRemoveLinesContainingWords(subtitleFilePath, ppConfig.removeLinesWords());
  }

  switch (ppConfig.encodingChangeMethod()) {
    case ECM_REPLACE_DIACRITICS:
      ppReplaceDiacriticsWithASCII(subtitleFilePath);
      break;
    case ECM_CHANGE:
      if (!ppConfig.encodingAutoDetectFrom() ||
          !ppChangeSubtitlesEncoding(subtitleFilePath, ppConfig.encodingTo())) {
        ppChangeSubtitlesEncoding(subtitleFilePath, ppConfig.encodingFrom(),
                                  ppConfig.encodingTo());
      }
      break;
    case ECM_ORIGINAL:
      // Nie ruszaj pobranych napisów!
      break;
  }

  if (!ppConfig.subFormat().isEmpty()) {
    QString targetFormat = ppConfig.subFormat();
    subtitleConverter->convertSubtitles(subtitleFilePath, targetFormat,
                                        subtitleFilePath, movieFilePath);
  }
}

bool SubtitlePostProcessor::ppReplaceDiacriticsWithASCII(
    const QString& subtitleFilePath) const {
  if (!QFileInfo(subtitleFilePath).exists()) return false;

  QString from = encodingUtils.detectFileEncoding(subtitleFilePath);

  if (from.isEmpty()) return false;

  QFile f(subtitleFilePath);
  if (!f.open(QIODevice::ReadOnly)) return false;

  QByteArray fileContent = f.readAll();
  f.close();

  Maybe<QString> contentStr = encodingUtils.decode(from, fileContent);
  if (!contentStr) return false;

  const QString replaced =
      encodingUtils.replaceDiacriticsWithASCII(contentStr.value());

  // Written back in the encoding it was read in, not as UTF-8. The replacement
  // table covers letters only, so anything else non-ASCII survives it - the
  // typographic quotes and dashes that fill Polish subtitles among them - and
  // writing those as UTF-8 left the rest of the file in its original encoding
  // with a few UTF-8 sequences buried in it. A player reading the file whole
  // shows those as mojibake.
  Maybe<QByteArray> encoded = encodingUtils.encode(from, replaced);
  if (!encoded) return false;

  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

  f.write(encoded.value());
  f.close();

  return true;
}

bool SubtitlePostProcessor::ppChangeSubtitlesEncoding(
    const QString& subtitleFilePath, const QString& from,
    const QString& to) const {
  QFile f(subtitleFilePath);
  if (!f.open(QIODevice::ReadOnly)) return false;

  QByteArray fileContent = f.readAll();
  f.close();

  Maybe<QString> contentStr = encodingUtils.decode(from, fileContent);
  if (!contentStr) return false;

  Maybe<QByteArray> encoded = encodingUtils.encode(to, contentStr.value());
  if (!encoded) return false;

  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

  f.write(encoded.value());
  f.close();

  return true;
}

bool SubtitlePostProcessor::ppChangeSubtitlesEncoding(
    const QString& subtitleFilePath, const QString& to) const {
  if (!QFileInfo(subtitleFilePath).exists()) return false;

  QString from = encodingUtils.detectFileEncoding(subtitleFilePath);

  if (from.isEmpty()) return false;

  return ppChangeSubtitlesEncoding(subtitleFilePath, from, to);
}

bool SubtitlePostProcessor::ppRemoveLinesContainingWords(
    const QString& subtitleFilePath, QStringList wordList) const {
  if (!QFileInfo(subtitleFilePath).exists()) return false;

  wordList = wordList.filter(QRegularExpression("^(.+)$"));

  QString fromCodec = encodingUtils.detectFileEncoding(subtitleFilePath);

  if (fromCodec.isEmpty()) fromCodec = ppConfig.encodingFrom();

  QFile f(subtitleFilePath);
  if (!f.open(QIODevice::ReadOnly)) return false;

  Maybe<QString> contentOpt = encodingUtils.decode(fromCodec, f.readAll());
  f.close();
  if (!contentOpt) return false;
  QString content = contentOpt.value();

  // Whatever this file uses to end its lines is what it gets written back
  // with, and a file that did not end in a line break still will not. Removing
  // a few advertising lines should not also rewrite every line ending in the
  // file - SRT is conventionally CRLF and some players are particular about it.
  const QString lineEnding = content.contains("\r\n") ? "\r\n"
                             : content.contains('\r') ? "\r"
                                                      : "\n";
  const bool endedWithLineBreak =
      content.endsWith('\n') || content.endsWith('\r');

  QStringList lines = content.split(QRegularExpression("\r\n|\n|\r"));
  if (!lines.isEmpty() && lines.last().isEmpty() && endedWithLineBreak) {
    lines.removeLast();
  }
  const QStringList filtered = removeLinesContainingWords(lines, wordList);

  // Write back with same encoding
  QString outContent = filtered.join(lineEnding);
  if (endedWithLineBreak && !outContent.isEmpty()) outContent += lineEnding;

  Maybe<QByteArray> encoded = encodingUtils.encode(fromCodec, outContent);
  if (!encoded) return false;

  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

  f.write(encoded.value());
  f.close();

  return true;
}

namespace {
bool mentionsAny(const QString& text, const QStringList& words) {
  for (const QString& word : words) {
    if (text.contains(word, Qt::CaseInsensitive)) return true;
  }
  return false;
}
}  // namespace

QStringList SubtitlePostProcessor::removeLinesContainingWords(
    const QStringList& lines, const QStringList& words) {
  static const QRegularExpression srtTiming(
      QStringLiteral("^\\s*\\d{1,2}:\\d{2}:\\d{2}[,.]\\d{1,3}\\s*-->"));

  bool isSrt = false;
  for (const QString& line : lines) {
    if (srtTiming.match(line).hasMatch()) {
      isSrt = true;
      break;
    }
  }

  QStringList result;

  // MicroDVD, MPL2, TMPlayer: one subtitle per line, so the line goes.
  if (!isSrt) {
    for (const QString& line : lines) {
      if (!mentionsAny(line, words)) result << line;
    }
    return result;
  }

  // SRT: blocks separated by empty lines - a number, a timing line, the text.
  // A match anywhere in the text takes the whole block, so an advertisement
  // spread over two lines needs only one of them on the list and leaves no
  // empty subtitle behind. The numbers that remain are counted again.
  QList<QStringList> blocks;
  QStringList current;
  for (const QString& line : lines) {
    if (line.trimmed().isEmpty()) {
      if (!current.isEmpty()) blocks << current;
      current.clear();
    } else {
      current << line;
    }
  }
  if (!current.isEmpty()) blocks << current;

  int number = 0;
  for (const QStringList& block : blocks) {
    int timing = -1;
    for (int i = 0; i < block.size() && i < 2; ++i) {
      if (srtTiming.match(block.at(i)).hasMatch()) timing = i;
    }
    if (timing < 0) {
      // Not a subtitle at all; filtered line by line as before.
      QStringList kept;
      for (const QString& line : block) {
        if (!mentionsAny(line, words)) kept << line;
      }
      if (!kept.isEmpty()) {
        if (!result.isEmpty()) result << QString();
        result << kept;
      }
      continue;
    }
    bool advertising = false;
    for (int i = timing + 1; i < block.size() && !advertising; ++i) {
      advertising = mentionsAny(block.at(i), words);
    }
    if (advertising) continue;

    if (!result.isEmpty()) result << QString();
    if (timing == 1) result << QString::number(++number);
    result << block.mid(timing);
  }
  return result;
}
