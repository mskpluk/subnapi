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

#include "napiprojektdownloadengine.h"
#include "subtitlelanguage.h"
#include "utils/legacycodecs.h"
#include "utils/synchttp.h"

#include <cmath>

#include <QCryptographicHash>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStringConverter>
#include <QStringDecoder>
#include <QXmlStreamReader>
#include <QUrl>

namespace NapiProjektDownloadEngineConsts {
const unsigned long NAPI_10MB = 10485760;

const QString napiZipPassword = "iBlm8NTigvru0Jr0";
const QString napiDownloadUrlTpl =
    "http://www.napiprojekt.pl/unit_napisy/dl.php"
    "?l=%1&f=%2&t=%3&v=other&kolejka=false&nick=%4&pass=%5&napios=%6";
const QString napiCheckUserUrlTpl =
    "http://www.napiprojekt.pl/users_check.php?nick=%1&pswd=%2";
}  // namespace NapiProjektDownloadEngineConsts

using namespace NapiProjektDownloadEngineConsts;

SubtitleDownloadEngineMetadata NapiProjektDownloadEngine::metadata =
    SubtitleDownloadEngineMetadata(
        "NapiProjekt",
        QObject::tr("<b>www.napiprojekt.pl</b> subtitles download engine"),
        just(QUrl("http://www.napiprojekt.pl/rejestracja")), nothing());

NapiProjektDownloadEngine::NapiProjektDownloadEngine(
    const QString &tmpPath, const EngineConfig &config,
    const QSharedPointer<const P7ZipDecoder> &p7zipDecoder)
    : SubtitleDownloadEngine(tmpPath),
      engineConfig(config),
      p7zipDecoder(p7zipDecoder) {}

NapiProjektDownloadEngine::~NapiProjektDownloadEngine() { cleanup(); }

SubtitleDownloadEngineMetadata NapiProjektDownloadEngine::meta() const {
  return NapiProjektDownloadEngine::metadata;
}

const char *const *NapiProjektDownloadEngine::enginePixmapData() const {
  return NapiProjektDownloadEngine::pixmapData;
}

QString NapiProjektDownloadEngine::checksum(QString filename) {
  if (filename.isEmpty()) filename = movie;
  return (checkSum = checksum(filename, true));
}

bool NapiProjektDownloadEngine::lookForSubtitles(QString lang) {
  Maybe<QString> tmpPackedFileOpt = downloadByLangAndChecksum(lang, checkSum);

  if (!tmpPackedFileOpt) {
    return false;
  }

  QString tmpPackedFile = tmpPackedFileOpt.value();

  subtitlesList << SubtitleInfo(lang, meta().name(), tmpPackedFile,
                                QFileInfo(movie).completeBaseName(), "", "txt",
                                SUBTITLE_UNKNOWN);
  return true;
}

QList<SubtitleInfo> NapiProjektDownloadEngine::listSubtitles() {
  return subtitlesList;
}

bool NapiProjektDownloadEngine::download(QUuid id) {
  Maybe<SubtitleInfo> ms = resolveById(id);

  return ms && QFile::exists(ms.value().sourceLocation);
}

Maybe<QString> NapiProjektDownloadEngine::downloadByLangAndChecksum(
    QString lang, QString checksum) const {
  if (checksum.isEmpty()) return nothing();

  QString urlTxt = napiDownloadUrlTpl.arg(npLangWrapper(lang))
                       .arg(checksum)
                       .arg(npFDigest(checksum))
                       .arg(engineConfig.nick())
                       .arg(engineConfig.password())
                       .arg(napiOS());

  QUrl url(urlTxt);

  SyncHTTP http;
  QNetworkReply *reply = http.syncGet(QNetworkRequest(url));

  if (reply->error() != QNetworkReply::NoError) return nothing();

  QByteArray buffer = reply->readAll();

  if (buffer.indexOf("NPc") == 0) return nothing();

  QString tmpPackedFile = generateTmpPath();

  QFile file(tmpPackedFile);
  if (file.exists()) file.remove();
  if (!file.open(QIODevice::WriteOnly)) return nothing();

  long r = file.write(buffer);
  file.close();

  if (r < 0) return nothing();

  return just(tmpPackedFile);
}

bool NapiProjektDownloadEngine::unpack(QUuid id) {
  Maybe<SubtitleInfo> ms = resolveById(id);
  if (!ms) return false;

  if (!QFile::exists(movie)) return false;
  subtitlesTmp = tmpPath + "/" + checkSum + ".txt";

  if (QFile::exists(subtitlesTmp)) QFile::remove(subtitlesTmp);

  QString archivePath = ms.value().sourceLocation;

  bool unpacked = p7zipDecoder->unpackSecureArchiveFiles(
      archivePath, napiZipPassword, tmpPath);

  return unpacked && QFile::exists(subtitlesTmp);
}

void NapiProjektDownloadEngine::cleanup() {
  clearSubtitlesList();
  if (QFile::exists(subtitlesTmp)) QFile::remove(subtitlesTmp);
}

bool NapiProjektDownloadEngine::checkUser(const QString &nick,
                                          const QString &pass) {
  SyncHTTP http;
  QString urlTxt = napiCheckUserUrlTpl.arg(nick).arg(pass);

  QUrl url(urlTxt);
  QNetworkReply *reply = http.syncGet(QNetworkRequest(url));
  if (reply->error() != QNetworkReply::NoError) return false;

  QString buffer = reply->readAll();
  if (buffer.indexOf("ok") == 0) return true;
  return false;
}

QString NapiProjektDownloadEngine::checksum(QString filename, bool limit10M) {
  QFile file(filename);
  QByteArray fileArray;

  if (!file.open(QIODevice::ReadOnly)) return QString("");

  fileArray = limit10M ? file.read(NAPI_10MB) : file.readAll();
  file.close();

  QByteArray b = QCryptographicHash::hash(fileArray, QCryptographicHash::Md5);
  fileArray.clear();

  QString out;
  char next[3];

  for (int i = 0; i < 16; i++) {
    snprintf(next, 3, "%.2x", static_cast<unsigned char>(b[i]));
    out += next;
  }

  checkSum = out;
  return checkSum;
}

QString NapiProjektDownloadEngine::npFDigest(const QString &input) const {
  if (input.size() != 32) return "";

  int idx[] = {0xe, 0x3, 0x6, 0x8, 0x2}, mul[] = {2, 2, 5, 4, 3},
      add[] = {0x0, 0xd, 0x10, 0xb, 0x5}, a, m, i, t, v;

  QString b;
  char tmp[2] = {0, 0};

  for (int j = 0; j <= 4; j++) {
    a = add[j];
    m = mul[j];
    i = idx[j];

    tmp[0] = input[i].toLatin1();
    t = a + static_cast<int>(strtol(tmp, NULL, 16));
    v = static_cast<int>(strtol(input.mid(t, 2).toLocal8Bit(), NULL, 16));

    snprintf(tmp, 2, "%x", (v * m) % 0x10);
    b += tmp;
  }

  return b;
}

QString NapiProjektDownloadEngine::npLangWrapper(QString lang) const {
  lang = SubtitleLanguage(lang).toTwoLetter().toUpper();

  if (lang == "EN") lang = "ENG";

  return lang;
}

QString NapiProjektDownloadEngine::napiOS() const { return "Linux/UNIX"; }

QString NapiProjektDownloadEngine::normalizeHash(const QString& hash) const {
  QString normalizedHash = hash;
  if (normalizedHash.startsWith("napiprojekt:", Qt::CaseInsensitive)) {
    normalizedHash = normalizedHash.mid(12).toLower();
  }
  return normalizedHash;
}

QString NapiProjektDownloadEngine::downloadXmlForHash(const QString& hash, const QString& lang) const {
  QString normalizedHash = normalizeHash(hash);

  QString url = "http://napiprojekt.pl/api/api-napiprojekt3.php";
  
  QByteArray postData;
  postData.append("mode=17&");
  postData.append("client=NapiProjektPython&");
  postData.append("client_ver=0.1&");
  postData.append("user_nick=&");
  postData.append("user_password=&");
  postData.append("downloaded_subtitles_id=");
  postData.append(normalizedHash.toUtf8());
  postData.append("&downloaded_subtitles_lang=");
  postData.append(lang.toUtf8());
  postData.append("&the=end");

  SyncHTTP http;
  QUrl requestUrl(url);
  QNetworkRequest request{requestUrl};
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  
  QNetworkReply *reply = http.syncPost(request, postData);
  
  if (reply->error() != QNetworkReply::NoError) {
    return QString();
  }
  
  return QString::fromUtf8(reply->readAll());
}

QString NapiProjektDownloadEngine::extractSubtitlesFromXml(
    const QString& xmlContent, const QString& hash) const {
  if (!xmlContent.contains("<status>success</status>")) {
    return QString();
  }
  
  QXmlStreamReader xml(xmlContent);
  QString base64Content;
  bool inSubtitlesTag = false;
  bool inContentTag = false;
  
  while (!xml.atEnd()) {
    xml.readNext();
    
    if (xml.isStartElement()) {
      if (xml.name() == "subtitles") {
        inSubtitlesTag = true;
      } else if (inSubtitlesTag && xml.name() == "content") {
        inContentTag = true;
      }
    } else if (xml.isEndElement()) {
      if (xml.name() == "subtitles") {
        inSubtitlesTag = false;
      } else if (xml.name() == "content") {
        inContentTag = false;
      }
    } else if (xml.isCDATA() && inContentTag) {
      base64Content = xml.text().toString();
    }
  }
  
  if (base64Content.isEmpty()) {
    return QString();
  }
  
  // Decode base64
  QByteArray compressedData = QByteArray::fromBase64(base64Content.toUtf8());
  
  if (compressedData.isEmpty()) {
    return QString();
  }
  
  // Save to a temporary archive
  QString tmpArchive = tmpPath + "/qnapi_hash.7z";
  QFile archiveFile(tmpArchive);
  if (!archiveFile.open(QIODevice::WriteOnly)) {
    return QString();
  }
  archiveFile.write(compressedData);
  archiveFile.close();
  
  // Unpack
  bool unpacked = p7zipDecoder->unpackSecureArchiveFiles(tmpArchive, napiZipPassword, tmpPath);
  
  if (!unpacked) {
    QFile::remove(tmpArchive);
    return QString();
  }
  
  // Subtitles from napiprojekt are always stored in the archive as
  // <hash>.txt - we read exactly that file instead of guessing by
  // modification time, so we don't pick up a random .txt/.srt file left
  // in tmpPath by another process sharing the same temp directory.
  QString subtitleFile = tmpPath + "/" + normalizeHash(hash) + ".txt";

  QString result;
  if (QFile::exists(subtitleFile)) {
    QFile file(subtitleFile);
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray rawData = file.readAll();
      file.close();

      // --- Subtitle encoding detection ---
      // Try to detect a UTF encoding via BOM (UTF-8, UTF-16, UTF-32)
      auto utfEncoding = QStringConverter::encodingForData(rawData);
      if (utfEncoding) {
        result = QStringDecoder(*utfEncoding).decode(rawData);
      } else {
        // If not UTF, it's most likely Windows-1250 (CP1250) - the standard encoding for Polish subtitles
        result = LegacyCodecs::toUnicode(LegacyCodecs::Encoding::Windows1250,
                                         rawData);
      }
      // --- End of encoding detection ---
    }
    QFile::remove(subtitleFile);
  }
  
  QFile::remove(tmpArchive);
  return result;
}

bool NapiProjektDownloadEngine::downloadFromHashXml(const QString& hash, const QString& lang, const QString& outputPath) {
  QString xmlContent = downloadXmlForHash(hash, lang);
  
  if (xmlContent.isEmpty()) {
    return false;
  }
  
  QString subtitlesContent = extractSubtitlesFromXml(xmlContent, hash);
  
  if (subtitlesContent.isEmpty()) {
    return false;
  }
  
  QFile outFile(outputPath);
  if (!outFile.open(QIODevice::WriteOnly)) {
    return false;
  }
  
  outFile.write(subtitlesContent.toUtf8());
  outFile.close();
  
  return true;
}

const char *const NapiProjektDownloadEngine::pixmapData[] = {
    "16 16 5 1",        "   c #FFFFFF",     ".  c #000080",
    "+  c #0052CC",     "@  c #005CE6",     "#  c #BFD9FF",
    "           ...  ", "         ... .  ", "       ... ...  ",
    "     ... ....   ", "   ... ....     ", " ... ....       ",
    ".. ....         ", "....+           ", " ...............",
    " . . . . . . . .", " ...............", " .@@####@####@@.",
    " .@@#@@#@#@@#@@.", " .@@#@@#@####@@.", " .@@#@@#@#@@@@@.",
    " ..............."};
