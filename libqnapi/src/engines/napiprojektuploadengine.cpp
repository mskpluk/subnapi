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

#include "napiprojektuploadengine.h"

#include "subtitlelanguage.h"
#include "utils/encodingutils.h"
#include "utils/filehash.h"
#include "utils/synchttp.h"
#include "version.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QUrl>

namespace {
const unsigned long NAPI_10MB = 10485760;
// Same archive password the download path uses (napiprojektdownloadengine.cpp) -
// confirmed unchanged for uploads by decrypting real captured archives.
const QString kArchivePassword = "iBlm8NTigvru0Jr0";
// HTTPS, not plain HTTP: the upload below puts the account password in the
// request body, and base64 is encoding, not protection. The endpoint serves
// the same responses over TLS.
const QString kUploadUrl = "https://napiprojekt.pl/api/api-napiprojekt3.php";
// "client" is allowlisted server-side - anything other than these two values
// gets an explicit access-denied response (confirmed by live-testing mode=2056
// with several client strings). Must match the real client's own identity.
const QString kClient = "NapiProjekt";
}  // namespace

NapiProjektUploadEngine::NapiProjektUploadEngine(
    const QString& tmpPath, const EngineConfig& config,
    const QSharedPointer<const P7ZipDecoder>& p7zipDecoder)
    : tmpPath(tmpPath), engineConfig(config), p7zipDecoder(p7zipDecoder) {}

bool NapiProjektUploadEngine::convertSubtitleToBomCrlf(
    const QString& subtitleFilePath, const QString& destFilePath) const {
  QFile src(subtitleFilePath);
  if (!src.open(QIODevice::ReadOnly)) return false;
  QByteArray raw = src.readAll();
  src.close();

  EncodingUtils encodingUtils;
  QString detectedEncoding = encodingUtils.detectFileEncoding(subtitleFilePath);
  if (detectedEncoding.isEmpty()) return false;

  Maybe<QString> decoded = encodingUtils.decode(detectedEncoding, raw);
  if (!decoded) return false;

  QString normalized =
      decoded.value().split(QRegularExpression("\r\n|\n|\r")).join("\r\n");

  Maybe<QByteArray> encoded = encodingUtils.encode("UTF-8", normalized);
  if (!encoded) return false;

  QByteArray withBom;
  withBom.reserve(3 + encoded.value().size());
  withBom.append('\xEF').append('\xBB').append('\xBF');
  withBom.append(encoded.value());

  QFile dst(destFilePath);
  if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  qint64 written = dst.write(withBom);
  dst.close();

  return written == withBom.size();
}

QString NapiProjektUploadEngine::npLangWrapper(QString lang) const {
  return SubtitleLanguage(lang).toTwoLetter().toUpper();
}

QString NapiProjektUploadEngine::lastServerMessage() const {
  return serverMessage;
}

NapiProjektUploadResult NapiProjektUploadEngine::upload(
    const NapiProjektUploadRequest& request) {
  serverMessage.clear();

  if (!QFile::exists(request.videoFilePath) ||
      !QFile::exists(request.subtitleFilePath)) {
    return NapiProjektUploadResult::ConversionFailed;
  }

  QString uploadTmpDir = tmpPath + "/upload";
  QDir().mkpath(uploadTmpDir);

  QString videoHash = md5HexOfFile(request.videoFilePath, NAPI_10MB);
  if (videoHash.isEmpty()) return NapiProjektUploadResult::ConversionFailed;

  // The inner 7z entry must be a bare "<video_hash>.txt" with no path prefix
  // (confirmed from real captured archives) - the converted temp file is
  // named accordingly so createSecureArchive picks it up verbatim.
  QString convertedSubtitlePath = uploadTmpDir + "/" + videoHash + ".txt";
  QString archivePath = uploadTmpDir + "/" + videoHash + ".7z";

  auto cleanup = [&]() {
    if (QFile::exists(convertedSubtitlePath)) QFile::remove(convertedSubtitlePath);
    if (QFile::exists(archivePath)) QFile::remove(archivePath);
  };

  if (!convertSubtitleToBomCrlf(request.subtitleFilePath, convertedSubtitlePath)) {
    cleanup();
    return NapiProjektUploadResult::ConversionFailed;
  }

  QString subtitlesHash = md5HexOfFile(convertedSubtitlePath);
  if (subtitlesHash.isEmpty()) {
    cleanup();
    return NapiProjektUploadResult::ConversionFailed;
  }

  if (!p7zipDecoder->createSecureArchive(archivePath, kArchivePassword,
                                         convertedSubtitlePath)) {
    cleanup();
    return NapiProjektUploadResult::ArchiveCreationFailed;
  }

  QFile archiveFile(archivePath);
  if (!archiveFile.open(QIODevice::ReadOnly)) {
    cleanup();
    return NapiProjektUploadResult::ArchiveCreationFailed;
  }
  QByteArray archiveBytes = archiveFile.readAll();
  archiveFile.close();

  auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

  auto addTextPart = [multiPart](const QString& name, const QString& value) {
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QVariant(QString("form-data; name=\"%1\"").arg(name)));
    part.setBody(value.toUtf8());
    multiPart->append(part);
  };

  addTextPart("mode", request.mode == NapiProjektUploadMode::Correction
                          ? "1024"
                          : "512");
  addTextPart("client", kClient);
  addTextPart("client_ver", QNAPI_VERSION);
  addTextPart("user_nick", engineConfig.nick());
  addTextPart("user_password",
             QString::fromLatin1(engineConfig.password().toUtf8().toBase64()));
  addTextPart("SubtitlesHash", subtitlesHash);
  addTextPart("SubtitlesAutor", request.author);
  addTextPart("SubtitlesLang", npLangWrapper(request.language));
  if (request.mode == NapiProjektUploadMode::Correction) {
    addTextPart("SubtitlesComment", request.comment);
  }

  QHttpPart filePart;
  filePart.setHeader(
      QNetworkRequest::ContentDispositionHeader,
      QVariant(QString("form-data; name=\"subtitles\"; filename=\"%1.zip\"")
                   .arg(videoHash)));
  filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                     QVariant("subtitles/zip"));
  filePart.setBody(archiveBytes);
  multiPart->append(filePart);

  SyncHTTP http;
  QNetworkRequest req{QUrl(kUploadUrl)};
  QNetworkReply* reply = http.syncPost(req, multiPart);

  cleanup();

  if (reply->error() != QNetworkReply::NoError) {
    return NapiProjektUploadResult::NetworkError;
  }

  QString response = QString::fromUtf8(reply->readAll());
  if (response.contains("<status>uploaded</status>")) {
    return NapiProjektUploadResult::Uploaded;
  }

  // The reason the server gave, so it can be repeated to the user instead of
  // a bare "rejected".
  //
  // Read with an XML parser rather than picked out with a pattern, because the
  // accented letters arrive as character references - "znajduj&#x105;" for
  // "znajdują" - and a message shown to a Polish-speaking user with every
  // second word broken is barely better than no message. The parser resolves
  // those; matching the text out by hand does not.
  QXmlStreamReader xml(response);
  while (!xml.atEnd()) {
    if (xml.readNext() == QXmlStreamReader::StartElement &&
        xml.name() == QStringLiteral("error")) {
      serverMessage = xml.readElementText().trimmed();
      break;
    }
  }

  // Nothing named the reason, so the whole reply stands in for it: tags become
  // spaces rather than vanishing, or text from neighbouring elements ends up
  // run together into one word.
  if (serverMessage.isEmpty()) {
    serverMessage = response.trimmed()
                        .replace(QRegularExpression("<[^>]*>"), " ")
                        .simplified();
  }

  return NapiProjektUploadResult::ServerRejected;
}

NapiProjektCheckResult NapiProjektUploadEngine::checkSubtitlesExist(
    const QString& videoHash, const QString& lang) const {
  QByteArray postData;
  postData.append("mode=2056&");
  postData.append("client=" + kClient.toUtf8() + "&");
  postData.append("client_ver=" + QByteArray(QNAPI_VERSION) + "&");
  // Deliberately blank, as in the real client's own requests. This check does
  // not authenticate: the server returns the same answer for no credentials, a
  // correct password and a wrong one alike (verified against all four cases).
  // Sending the account password anyway bought nothing and put it on the wire
  // once per file scanned.
  postData.append("user_nick=&");
  postData.append("user_password=&");
  postData.append("check_subs_id=" + videoHash.toUtf8() + "&");
  postData.append("check_subs_lang=" +
                  QUrl::toPercentEncoding(npLangWrapper(lang)));

  SyncHTTP http;
  QNetworkRequest req{QUrl(kUploadUrl)};
  req.setHeader(QNetworkRequest::ContentTypeHeader,
               "application/x-www-form-urlencoded");
  QNetworkReply* reply = http.syncPost(req, postData);

  if (reply->error() != QNetworkReply::NoError) {
    return NapiProjektCheckResult::NetworkError;
  }

  QString response = QString::fromUtf8(reply->readAll());
  if (response.contains("<available>yes</available>")) {
    return NapiProjektCheckResult::Available;
  }
  return NapiProjektCheckResult::NotAvailable;
}
