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

#include "subtitlematcher.h"
#include <QDir>
#include <QFile>

SubtitleMatcher::SubtitleMatcher(
    bool _noBackup, bool _isPostProcessingEnabled, QString _ppSubFormat,
    QString _ppSubExtension, bool _changePermissions,
    QString _changePermissionsTo,
    const QSharedPointer<const SubtitleFormatsRegistry>&
        subtitleFormatsRegistry)
    : noBackup(_noBackup),
      isPostProcessingEnabled(_isPostProcessingEnabled),
      ppSubFormat(_ppSubFormat),
      ppSubExtension(_ppSubExtension),
      changePermissions(_changePermissions),
      changePermissionsTo(_changePermissionsTo),
      subtitleFormatsRegistry(subtitleFormatsRegistry) {}

bool SubtitleMatcher::matchSubtitles(QString subtitlesTmpFilePath,
                                     QString targetMovieFilePath) const {
  QFileInfo subtitlesTmpFileInfo(subtitlesTmpFilePath);

  if (!subtitlesTmpFileInfo.exists()) return false;

  QString targetExtension = selectTargetExtension(subtitlesTmpFileInfo);

  QString targetSubtitlesFilePath =
      constructSubtitlePath(targetMovieFilePath, targetExtension);

  if (!isWritablePath(targetSubtitlesFilePath)) return false;

  removeOrCopy(targetMovieFilePath, targetSubtitlesFilePath);

  bool result = QFile::copy(subtitlesTmpFilePath, targetSubtitlesFilePath);

  if (changePermissions) {
    fixFilePermissions(targetSubtitlesFilePath, changePermissionsTo);
  }

  return result;
}

QString SubtitleMatcher::selectTargetExtension(
    QFileInfo subtitlesTmpFileInfo) const {
  QString targetExtension = subtitlesTmpFileInfo.suffix();

  if (isPostProcessingEnabled) {
    if (!ppSubFormat.isEmpty() && ppSubExtension.isEmpty()) {
      // Named after what the file really holds. When the conversion failed -
      // frame-based subtitles and no frame rate to be had from the movie - the
      // content is still in its old format, and calling it .srt hands the
      // player a file it cannot read.
      const QString actualFormat = detectFormat(subtitlesTmpFileInfo.filePath());
      targetExtension =
          subtitleFormatsRegistry
              ->select(actualFormat.isEmpty() ? ppSubFormat : actualFormat)
              ->defaultExtension();
    } else if (!ppSubExtension.isEmpty()) {
      targetExtension = ppSubExtension;
    }
  }

  return targetExtension;
}

QString SubtitleMatcher::detectFormat(const QString &subtitlesFilePath) const {
  QFile file(subtitlesFilePath);
  if (!file.open(QIODevice::ReadOnly)) return QString();
  // The detectors look at timestamps, which are ASCII in every encoding, and
  // the first lines are enough - as for SubtitleConverter::detectFormat.
  QStringList lines;
  while (lines.size() < 15 && !file.atEnd()) {
    QString line = QString::fromUtf8(file.readLine());
    while (line.endsWith('\n') || line.endsWith('\r')) line.chop(1);
    lines << line;
  }
  foreach (QString format, subtitleFormatsRegistry->listFormatNames()) {
    if (subtitleFormatsRegistry->select(format)->detect(lines)) return format;
  }
  return QString();
}

QString SubtitleMatcher::constructSubtitlePath(QString targetMovieFilePath,
                                               QString targetExtension,
                                               QString baseSuffix) const {
  QFileInfo targetMovieFileInfo(targetMovieFilePath);
  return targetMovieFileInfo.path() + QDir::separator() +
         targetMovieFileInfo.completeBaseName() + baseSuffix + "." +
         targetExtension;
}

bool SubtitleMatcher::isWritablePath(QString path) const {
  QFileInfo pathFileInfo(path);
  return QFileInfo(pathFileInfo.absolutePath()).isWritable();
}

void SubtitleMatcher::removeOrCopy(QString targetMoviefilePath,
                                   QString targetSubtitlesFilePath) const {
  if (QFile::exists(targetSubtitlesFilePath)) {
    if (!noBackup) {
      QFileInfo targetSubtitlesFileInfo(targetSubtitlesFilePath);
      QString newName = constructSubtitlePath(
          targetMoviefilePath, targetSubtitlesFileInfo.suffix(), tr("_copy"));

      if (QFile::exists(newName)) QFile::remove(newName);

      QFile::rename(targetSubtitlesFilePath, newName);
    } else
      QFile::remove(targetSubtitlesFilePath);
  }
}

void SubtitleMatcher::fixFilePermissions(QString targetSubtitlesFilePath,
                                         QString changePermissionsTo) const {
  bool validPermissions;
  int permInt = changePermissionsTo.toInt(&validPermissions, 8);

  if (validPermissions) {
    int perm = 0;
    perm |= (permInt & 0700) << 2;
    perm |= (permInt & 0070) << 1;
    perm |= (permInt & 0007);
    QFile::setPermissions(targetSubtitlesFilePath, QFile::Permissions(perm));
  }
}
