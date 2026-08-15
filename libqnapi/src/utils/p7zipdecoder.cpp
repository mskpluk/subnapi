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

#include "p7zipdecoder.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

P7ZipDecoder::P7ZipDecoder(const QString& p7zipPath, int operationTimeoutMsecs)
    : p7zipPath(p7zipPath), operationTimeoutMsecs(operationTimeoutMsecs) {}

QStringList P7ZipDecoder::listArchiveFiles(const QString& archivePath) const {
  QString stdoutBuff = readP7ZipOutput({"l", "-slt", archivePath});

  QRegularExpression r("\nPath = ([^\n]*)\n");

  QStringList files;
  QRegularExpressionMatchIterator it = r.globalMatch(stdoutBuff);
  while (it.hasNext()) {
    files << it.next().captured(1);
  }

  return files;
}

bool P7ZipDecoder::unpackArchiveFile(const QString& archivePath,
                                     const QString& filename,
                                     const QString& targetPath) const {
  return runP7Zip({"e", "-y", "-o" + targetPath, archivePath, filename});
}

bool P7ZipDecoder::unpackArchiveFiles(const QString& archivePath,
                                      const QString& targetPath) const {
  return runP7Zip({"e", "-y", "-o" + targetPath, archivePath});
}

bool P7ZipDecoder::unpackSecureArchiveFiles(const QString& archivePath,
                                            const QString& password,
                                            const QString& targetPath) const {
  return runP7Zip({"e", "-y", "-p" + password, "-o" + targetPath, archivePath});
}

bool P7ZipDecoder::createSecureArchive(const QString& archivePath,
                                       const QString& password,
                                       const QString& sourceFilePath) const {
  if (QFile::exists(archivePath)) QFile::remove(archivePath);

  QFileInfo sourceInfo(sourceFilePath);
  return runP7Zip({"a", "-p" + password, archivePath, sourceInfo.fileName()},
                  sourceInfo.absolutePath());
}

bool P7ZipDecoder::runP7Zip(const QStringList& args,
                           const QString& workingDirectory) const {
  QProcess p7zip;
  if (!workingDirectory.isEmpty()) {
    p7zip.setWorkingDirectory(workingDirectory);
  }
  p7zip.start(p7zipPath, args);

  // waitForFinished() only reports that the process ended - it says nothing
  // about whether the operation worked. Without looking at the exit code, a
  // wrong archive password or a corrupt archive was reported as success, and
  // callers only noticed because they went on to check whether the file they
  // wanted had appeared.
  //
  // 7-Zip signals a fatal error with 2 or above; 1 means it finished with
  // non-fatal warnings (an unreadable file among several, say), which is not a
  // reason to discard an archive that was produced.
  bool finished = p7zip.waitForFinished(operationTimeoutMsecs);
  bool succeeded = finished && p7zip.exitStatus() == QProcess::NormalExit &&
                   p7zip.exitCode() <= 1;

  p7zip.close();

  return succeeded;
}

QString P7ZipDecoder::readP7ZipOutput(const QStringList& args) const {
  QProcess p7zip;
  p7zip.start(p7zipPath, args);

  p7zip.waitForFinished(operationTimeoutMsecs);

  QString stdoutBuff = QByteArray(p7zip.readAll());

  p7zip.close();

  return stdoutBuff;
}
