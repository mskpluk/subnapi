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

#include "clisubtitlesuploader.h"
#include "engines/napiprojektuploadengine.h"
#include "libqnapi.h"

#include <QFileInfo>

namespace CliSubtitlesUploader {

enum ExitCode {
  EC_OK = 0,
  EC_P7ZIP_UNAVAILABLE = 2,
  EC_CANNOT_WRITE_TMP_DIR = 3,
  EC_NAPIPROJEKT_NOT_CONFIGURED = 10,
  EC_CONVERSION_FAILED = 11,
  EC_ARCHIVE_CREATION_FAILED = 12,
  EC_NETWORK_ERROR = 13,
  EC_SERVER_REJECTED = 14
};

int configChecks(const Console& c, const QNapiConfig& config) {
  QString p7zipPath = config.generalConfig().p7zipPath();
  if (!QFileInfo(p7zipPath).isExecutable()) {
    c.printLine(tr("Invalid path to p7zip executable: %1").arg(p7zipPath));
    return EC_P7ZIP_UNAVAILABLE;
  }

  QString tmpPath = config.generalConfig().tmpPath();
  QFileInfo tmp(tmpPath);
  if (!tmp.isDir() || !tmp.isWritable()) {
    c.printLine(tr("Can't write to temporary directory: %1").arg(tmpPath));
    return EC_CANNOT_WRITE_TMP_DIR;
  }

  return EC_OK;
}

int uploadSubtitlesFor(const Console& c,
                       const QNapiCommand::UploadSubtitles& request,
                       const QNapiConfig& config) {
  int ccErr = configChecks(c, config);
  if (ccErr != 0) {
    return ccErr;
  }

  EngineConfig napiEngineConfig = config.enginesConfig()["NapiProjekt"];
  if (napiEngineConfig.nick().isEmpty() ||
      napiEngineConfig.password().isEmpty()) {
    c.printLine(
        tr("Configure your NapiProjekt account (nick and password) in "
           "Settings before uploading."));
    return EC_NAPIPROJEKT_NOT_CONFIGURED;
  }

  c.printLineOrdinary(request.correction ? tr("Sending a subtitles "
                                              "correction to NapiProjekt...")
                                         : tr("Uploading subtitles to "
                                              "NapiProjekt..."));

  NapiProjektUploadRequest uploadRequest;
  uploadRequest.videoFilePath = request.videoFilePath;
  uploadRequest.subtitleFilePath = request.subtitleFilePath;
  uploadRequest.language = config.generalConfig().language();
  uploadRequest.author = request.author;
  uploadRequest.comment = request.comment;
  uploadRequest.mode = request.correction
                          ? NapiProjektUploadMode::Correction
                          : NapiProjektUploadMode::NewSubtitles;

  QString tmpPath = config.generalConfig().tmpPath();
  QString p7zipPath = config.generalConfig().p7zipPath();
  QSharedPointer<const P7ZipDecoder> p7zipDecoder =
      LibQNapi::p7zipDecoder(p7zipPath);

  NapiProjektUploadEngine engine(tmpPath, napiEngineConfig, p7zipDecoder);
  NapiProjektUploadResult result = engine.upload(uploadRequest);

  switch (result) {
    case NapiProjektUploadResult::Uploaded:
      c.printLineOrdinary(request.correction
                              ? tr("Correction sent, pending moderation.")
                              : tr("Subtitles uploaded successfully!"));
      return EC_OK;
    case NapiProjektUploadResult::ConversionFailed:
      c.printLineError(tr("Could not convert the subtitles file."));
      return EC_CONVERSION_FAILED;
    case NapiProjektUploadResult::ArchiveCreationFailed:
      c.printLineError(tr("Could not create the upload archive."));
      return EC_ARCHIVE_CREATION_FAILED;
    case NapiProjektUploadResult::NetworkError:
      c.printLineError(tr("A network error occurred while uploading."));
      return EC_NETWORK_ERROR;
    case NapiProjektUploadResult::ServerRejected:
      c.printLineError(tr("NapiProjekt rejected the upload."));
      return EC_SERVER_REJECTED;
  }

  return EC_OK;
}

};  // namespace CliSubtitlesUploader
