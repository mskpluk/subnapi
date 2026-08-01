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

  fileContent.clear();
  fileContent.append(
      encodingUtils.replaceDiacriticsWithASCII(contentStr.value()).toUtf8());

  if (!f.open(QIODevice::WriteOnly)) return false;

  f.write(fileContent);
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

  QStringList lines = content.split(QRegularExpression("\r\n|\n|\r"));
  if (!lines.isEmpty() && lines.last().isEmpty() &&
      (content.endsWith('\n') || content.endsWith('\r'))) {
    lines.removeLast();
  }

  // Filter lines
  QStringList filtered;
  foreach (QString line, lines) {
    bool found = false;
    foreach (QString word, wordList) {
      if (line.contains(word, Qt::CaseInsensitive)) {
        found = true;
        break;
      }
    }
    if (!found) {
      filtered << line;
    }
  }

  // Write back with same encoding
  QString outContent;
  foreach (QString line, filtered) {
    outContent += line + "\n";
  }
  Maybe<QByteArray> encoded = encodingUtils.encode(fromCodec, outContent);
  if (!encoded) return false;

  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

  f.write(encoded.value());
  f.close();

  return true;
}
