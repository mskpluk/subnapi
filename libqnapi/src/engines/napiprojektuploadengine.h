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

#ifndef NAPIPROJEKTUPLOADENGINE_H
#define NAPIPROJEKTUPLOADENGINE_H

#include "config/engineconfig.h"
#include "utils/p7zipdecoder.h"

#include <QMetaType>
#include <QSharedPointer>
#include <QString>

enum class NapiProjektUploadMode { NewSubtitles, Correction };

struct NapiProjektUploadRequest {
  QString videoFilePath;     // only read to compute the video hash used to
                             // name the uploaded archive
  QString subtitleFilePath;  // local file, original encoding/line-endings
  QString language;          // e.g. "PL"
  QString author;            // free text, optional
  QString comment;           // only sent when mode == Correction
  NapiProjektUploadMode mode;
};

enum class NapiProjektUploadResult {
  Uploaded,
  ConversionFailed,
  ArchiveCreationFailed,
  NetworkError,
  ServerRejected
};
Q_DECLARE_METATYPE(NapiProjektUploadResult)

enum class NapiProjektCheckResult { Available, NotAvailable, NetworkError };

// Uploads subtitles (new or a correction to existing ones) to NapiProjekt via
// api-napiprojekt3.php (mode 512 / 1024). The wire protocol it implements was
// reverse engineered; it is not documented by NapiProjekt.
class NapiProjektUploadEngine {
 public:
  NapiProjektUploadEngine(
      const QString& tmpPath, const EngineConfig& config,
      const QSharedPointer<const P7ZipDecoder>& p7zipDecoder);

  NapiProjektUploadResult upload(const NapiProjektUploadRequest& request) const;

  // Read-only check_subtitles query (mode=2056) - whether NapiProjekt
  // already has any subtitles for this video hash and language. Needs no
  // video metadata (confirmed by live-testing a minimal field set against
  // production - see PROTOCOL_NOTES_2026.md).
  NapiProjektCheckResult checkSubtitlesExist(const QString& videoHash,
                                             const QString& lang) const;

 private:
  QString tmpPath;
  EngineConfig engineConfig;
  QSharedPointer<const P7ZipDecoder> p7zipDecoder;

  // Re-encodes subtitleFilePath as UTF-8 with a leading BOM and CRLF line
  // endings (confirmed required by decrypting real uploads made by the
  // official client) and writes the result to destFilePath.
  bool convertSubtitleToBomCrlf(const QString& subtitleFilePath,
                                const QString& destFilePath) const;

  QString npLangWrapper(QString lang) const;
};

#endif  // NAPIPROJEKTUPLOADENGINE_H
