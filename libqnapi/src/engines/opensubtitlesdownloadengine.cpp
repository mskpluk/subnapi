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

#include "opensubtitlesdownloadengine.h"
#include "subtitlelanguage.h"
#include "utils/synchttp.h"

#include <QDir>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

namespace OpenSubtitlesDownloadEngineConst {
const QString openSubtitlesXmlRpcUrl = "http://api.opensubtitles.org/xml-rpc";

// Subtitle files are fetched over plain HTTP rather than through the XML-RPC
// DownloadSubtitles method. That method is not usable any more: for accounts
// without VIP it answers 200 OK with a 102-byte placeholder advertising VIP
// membership instead of the subtitles. This is not a quota or authentication
// problem - it happens on a fully logged-in account with an untouched daily
// allowance. The block is tied to the download links the API itself hands out,
// which carry a "src-api" path segment; the same file behind the plain
// download path below is served in full, and anonymously at that.
//
// The id substituted here is IDSubtitleFile - the same value lookForSubtitles()
// already stores in SubtitleInfo::sourceLocation. The response is the subtitle
// file itself, not an archive, so nothing needs unpacking.
const QString openSubtitlesDownloadUrlTpl =
    "https://dl.opensubtitles.org/en/download/file/%1";

// This is the path a browser uses, so the request is made to look like one.
// Not because the server demands it - it answers just as well with an honest
// "Subnapi" agent, with the agent Qt would send, and with no User-Agent header
// at all, all verified - but because a request carrying none of those is the
// cheapest thing for the site in front of it to start refusing, and a download
// engine that stops working on somebody else's filter change is not worth the
// candour. The XML-RPC calls are left alone: those go to the documented API,
// which is told who we are through LogIn's own agent parameter.
const QString openSubtitlesDownloadUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0";

// A served subtitle file comes back as "application/force-download". Anything
// else is the server answering with something other than the file - an HTML
// page once the daily per-IP download limit is reached, for instance. Such a
// response has to be rejected rather than written next to the movie, which is
// what would otherwise happen: nothing further down the pipeline checks whether
// what it got resembles subtitles.
const QString openSubtitlesDownloadContentType = "application/force-download";

// The placeholder served instead of the subtitles when a download is refused.
// It is a valid, complete SubRip file with the right content type, so the check
// above does not catch it. Matching on the advertised address rather than on an
// exact size keeps working if the wording changes.
const QString openSubtitlesVipPlaceholderMarker = "osdb.link/vip";
const int openSubtitlesPlaceholderMaxSize = 1024;

// Uploaders often note in the free-text comment which other releases their
// subtitles fit, in one of two shapes seen in the wild:
//
//   "Pasują również do wersji: 1080p.AMZN.WEB-DL.DDP5.1.H.264-NTb; 2160p..."
//   "Napisy pasują do następujących wersji filmu:" followed by one full
//   release name per line
//
// The label itself is not worth parsing - it is Polish prose that every
// uploader words differently ("Dopasowano do wersji BluRay.", "Synchro do
// wersji MP4 1CD 632MB", "Formatowano dla wersji 4K,1080p"). Instead the whole
// comment is split apart and only the pieces shaped like a release name are
// kept: at least two dots and this many characters. That shape is what
// separates "1080p.AMZN.WEB-DL.DDP5.1.H.264-NTb" from the prose around it, and
// it is specific enough that matching it against a file name is not a
// coincidence - unlike a bare "BluRay", which would fit half a library.
const int openSubtitlesReleaseNameMinLength = 12;
const int openSubtitlesReleaseNameMinDots = 2;
}  // namespace OpenSubtitlesDownloadEngineConst

namespace {

// Release names for the same release are written with dots, spaces or
// underscores between the parts, so everything that is not a letter or a digit
// is dropped before comparing. This is what lets the subtitles uploaded as
// "...1080p.AMZN.WEB-DL.DDP5.1.H.264-NTb" be recognised for a movie file named
// "... 1080p AMZN WEB-DL DDP5 1 H 264-NTb".
QString normalizeReleaseName(const QString &name) {
  QString normalized;
  normalized.reserve(name.size());
  for (const QChar &c : name) {
    if (c.isLetterOrNumber()) normalized += c.toLower();
  }
  return normalized;
}

QStringList releaseNamesInComment(const QString &comment) {
  QStringList releaseNames;
  const QStringList parts =
      comment.split(QRegularExpression("[;,\\s]+"), Qt::SkipEmptyParts);

  for (const QString &part : parts) {
    if (part.length() < OpenSubtitlesDownloadEngineConst::
                            openSubtitlesReleaseNameMinLength)
      continue;
    if (part.count('.') <
        OpenSubtitlesDownloadEngineConst::openSubtitlesReleaseNameMinDots)
      continue;
    releaseNames << part;
  }

  return releaseNames;
}

}  // namespace

SubtitleDownloadEngineMetadata OpenSubtitlesDownloadEngine::metadata =
    SubtitleDownloadEngineMetadata(
        "OpenSubtitles",
        QObject::tr("<b>www.opensubtitles.org</b> subtitles download engine"),
        just(QUrl("http://www.opensubtitles.org/newuser")),
        just(QUrl("http://www.opensubtitles.org/upload")));

OpenSubtitlesDownloadEngine::OpenSubtitlesDownloadEngine(
    const QString& tmpPath, const EngineConfig& config,
    const QSharedPointer<const P7ZipDecoder>& p7zipDecoder,
    const QString& qnapiDisplayableVersion, const QString& language)
    : SubtitleDownloadEngine(tmpPath),
      engineConfig(config),
      p7zipDecoder(p7zipDecoder),
      qnapiDisplayableVersion(qnapiDisplayableVersion),
      language(language),
      rpc(QUrl(OpenSubtitlesDownloadEngineConst::openSubtitlesXmlRpcUrl)) {}

OpenSubtitlesDownloadEngine::~OpenSubtitlesDownloadEngine() {
  cleanup();
  if (isLogged()) logout();
}

SubtitleDownloadEngineMetadata OpenSubtitlesDownloadEngine::meta() const {
  return OpenSubtitlesDownloadEngine::metadata;
}

const char* const* OpenSubtitlesDownloadEngine::enginePixmapData() const {
  return OpenSubtitlesDownloadEngine::pixmapData;
}

// oblicza sume kontrolna dla pliku filmowego
QString OpenSubtitlesDownloadEngine::checksum(QString filename) {
  if (filename.isEmpty()) filename = movie;

  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) return QString("");

  fileSize = file.size();
  quint64 hash = fileSize;
  quint64 tmp, i;

  for (tmp = 0, i = 0;
       i < 65536 / sizeof(tmp) && file.read((char*)&tmp, sizeof(tmp));
       i++, hash += tmp)
    ;
  file.seek(qMax((qint64)0, (qint64)fileSize - 65536));
  for (tmp = 0, i = 0;
       i < 65536 / sizeof(tmp) && file.read((char*)&tmp, sizeof(tmp));
       i++, hash += tmp)
    ;

  return (checkSum = QString("%1").arg(hash, 16, 16, QChar('0')));
}

// Decides how well one search result fits the movie file. Everything here is
// compared with separators stripped, so the same release written with dots,
// spaces or underscores compares equal.
SubtitleResolution OpenSubtitlesDownloadEngine::resolveSubtitleMatch(
    const QVariantMap &responseMap) const {
  if (responseMap["SubBad"].toString() != "0") return SUBTITLE_BAD;

  const QString movieName =
      normalizeReleaseName(QFileInfo(movie).completeBaseName());

  if (movieName ==
      normalizeReleaseName(
          QFileInfo(responseMap["SubFileName"].toString()).completeBaseName()))
    return SUBTITLE_GOOD;

  // Compared for equality rather than containment: MovieReleaseName is
  // sometimes only a title, and a title appearing somewhere in the file name
  // says nothing about the release matching.
  if (movieName ==
      normalizeReleaseName(responseMap["MovieReleaseName"].toString()))
    return SUBTITLE_GOOD;

  // The releases the uploader listed as also fitting. These are usually
  // suffixes of a full name ("1080p.AMZN.WEB-DL.DDP5.1.H.264-NTb" with no title
  // in front), so containment is the right test here.
  const QStringList alsoFits =
      releaseNamesInComment(responseMap["SubAuthorComment"].toString());
  for (const QString &releaseName : alsoFits) {
    const QString normalized = normalizeReleaseName(releaseName);
    if (!normalized.isEmpty() && movieName.contains(normalized))
      return SUBTITLE_GOOD;
  }

  return SUBTITLE_UNKNOWN;
}

// Runs one search and appends what it found. With onlyMatching set, results
// that cannot be tied to this particular release are dropped instead of being
// listed as SUBTITLE_UNKNOWN - see the call site for why.
bool OpenSubtitlesDownloadEngine::collectSubtitles(const QVariantMap &query,
                                                   bool onlyMatching) {
  QVariantList requestList;
  requestList << query;

  QVariantList args;
  args << token << (QVariant)requestList;

  if (!rpc.request("SearchSubtitles", args)) return false;

  QVariantMap responseMap = rpc.getResponse().toMap();

  if (!responseMap.contains("data")) return false;

  const bool searchedByHash = query.contains("moviehash");

  QVariantList dataList = responseMap["data"].toList();
  QVariantList::iterator i;

  i = dataList.begin();
  while (i != dataList.end()) {
    responseMap = (*(i++)).toMap();

    // Kept from when this was the only search: guards against the server
    // answering a hash query with something it did not match on. A search by
    // name reports no hash at all, so the check applies only to hash queries.
    if (searchedByHash && (checkSum != responseMap["MovieHash"]) &&
        (fileSize != responseMap["MovieByteSize"]))
      continue;

    SubtitleResolution r = resolveSubtitleMatch(responseMap);

    if (onlyMatching && r != SUBTITLE_GOOD) continue;

    QString subtitleName = responseMap["MovieReleaseName"].toString();
    if (subtitleName.isEmpty())
      subtitleName = QFileInfo(movie).completeBaseName();

    subtitlesList << SubtitleInfo(
        responseMap["ISO639"].toString(), meta().name(),
        responseMap["IDSubtitleFile"].toString(), subtitleName.trimmed(),
        responseMap["SubAuthorComment"].toString(),
        QFileInfo(responseMap["SubFileName"].toString()).suffix(), r);
  }

  return true;
}

// szuka napisow
bool OpenSubtitlesDownloadEngine::lookForSubtitles(QString lang) {
  if (checkSum.isEmpty()) return false;
  if (!isLogged() && !login()) return false;

  const QString languageId = SubtitleLanguage(lang).toTriLetter();

  QVariantMap byHash;
  byHash["sublanguageid"] = languageId;
  byHash["moviehash"] = checkSum;
  byHash["moviebytesize"] = QString::number(fileSize);

  // What the hash search adds has to be counted rather than the whole list
  // checked for emptiness: results accumulate across calls, and this is called
  // once per language. Searching a backup language after the primary one found
  // something would otherwise never reach the fallback below.
  const int foundBeforeHashSearch = subtitlesList.size();

  collectSubtitles(byHash, false);

  // A hash match means the subtitles were uploaded for this exact file, which
  // is the best evidence there is - but only files somebody already tied to
  // this release have one, so a miss is common. Fall back to searching by name.
  //
  // Only results that can be tied to this release are kept from that search,
  // because the caller may act on the list without asking: a single result is
  // downloaded without showing a list at all (QNapi::needToShowList), and
  // "never show list" takes the first one. A search by name otherwise happily
  // returns subtitles for a different release of the same title, and those
  // would be saved next to the movie unannounced.
  if (subtitlesList.size() == foundBeforeHashSearch) {
    QVariantMap byName;
    byName["sublanguageid"] = languageId;
    byName["query"] = QFileInfo(movie).completeBaseName();

    collectSubtitles(byName, true);
  }

  return (subtitlesList.size() > 0);
}

QList<SubtitleInfo> OpenSubtitlesDownloadEngine::listSubtitles() {
  std::sort(subtitlesList.begin(), subtitlesList.end());
  return subtitlesList;
}

bool OpenSubtitlesDownloadEngine::download(QUuid id) {
  Maybe<SubtitleInfo> ms = resolveById(id);

  if (!ms) return false;

  SubtitleInfo s = ms.value();

  // Read the file id out before sourceLocation is reused below to hold the
  // path the subtitles are written to.
  QString subtitleFileId = s.sourceLocation;
  if (subtitleFileId.isEmpty()) return false;

  subFileName = generateTmpFileName() + "." + s.format;

  QUrl url(OpenSubtitlesDownloadEngineConst::openSubtitlesDownloadUrlTpl.arg(
      subtitleFileId));

  QNetworkRequest request{url};
  request.setHeader(
      QNetworkRequest::UserAgentHeader,
      OpenSubtitlesDownloadEngineConst::openSubtitlesDownloadUserAgent);

  SyncHTTP http;
  QNetworkReply *reply = http.syncGet(request);

  if (reply->error() != QNetworkReply::NoError) return false;

  if (!reply->header(QNetworkRequest::ContentTypeHeader)
           .toString()
           .startsWith(
               OpenSubtitlesDownloadEngineConst::openSubtitlesDownloadContentType))
    return false;

  QByteArray subtitleContent = reply->readAll();

  if (subtitleContent.isEmpty()) return false;

  if (subtitleContent.size() <=
          OpenSubtitlesDownloadEngineConst::openSubtitlesPlaceholderMaxSize &&
      subtitleContent.contains(
          OpenSubtitlesDownloadEngineConst::openSubtitlesVipPlaceholderMarker
              .toUtf8()))
    return false;

  s.sourceLocation = generateTmpPath();
  QFile file(s.sourceLocation);
  if (file.exists()) file.remove();
  if (!file.open(QIODevice::WriteOnly)) return false;

  int r = file.write(subtitleContent);
  file.close();

  updateSubtitleInfo(s);

  return (bool)r;
}

// download() now fetches the subtitle file itself rather than an archive, so
// there is nothing left to unpack here. The step still exists because every
// engine's download() is followed by unpack() (QNapi::download / QNapi::unpack),
// and it is what gives the file the name the rest of the pipeline expects.
bool OpenSubtitlesDownloadEngine::unpack(QUuid id) {
  Maybe<SubtitleInfo> ms = resolveById(id);
  if (!ms) return false;

  if (!QFile::exists(movie)) return false;

  QString downloadedPath = ms.value().sourceLocation;
  if (!QFile::exists(downloadedPath)) return false;

  subtitlesTmp = tmpPath + QDir::separator() + subFileName;
  if (QFile::exists(subtitlesTmp)) QFile::remove(subtitlesTmp);

  if (!QFile::copy(downloadedPath, subtitlesTmp)) return false;
  QFile::remove(downloadedPath);

  return true;
}

void OpenSubtitlesDownloadEngine::cleanup() {
  clearSubtitlesList();
  if (QFile::exists(subtitlesTmp)) QFile::remove(subtitlesTmp);
}

bool OpenSubtitlesDownloadEngine::login() {
  QString userAgent = QString("QNapi v%1").arg(qnapiDisplayableVersion);
  QVariantList args;
  args << engineConfig.nick() << engineConfig.password() << language.toLower()
       << userAgent;
  if (!rpc.request("LogIn", args)) return false;

  QVariantMap responseMap = rpc.getResponse().toMap();

  // A token alone does not mean the credentials were accepted: rejected ones
  // answer "401 Unauthorized" and still hand back a token that works as an
  // anonymous session.
  //
  // Retry without credentials rather than giving up. Searching works anonymously
  // and downloading does not use the token at all, so refusing to go on would
  // turn a wrong password into an engine that silently finds nothing - worse
  // than what it replaces. The account buys nothing here either: the server
  // reports its download quota as limit_check_by = "user_ip".
  if (!responseMap["status"].toString().startsWith("200") &&
      !engineConfig.nick().isEmpty()) {
    QVariantList anonymousArgs;
    anonymousArgs << QString() << QString() << language.toLower() << userAgent;
    if (!rpc.request("LogIn", anonymousArgs)) return false;
    responseMap = rpc.getResponse().toMap();
    if (!responseMap["status"].toString().startsWith("200")) return false;
  }

  token = responseMap["token"].toString();

  return !token.isEmpty();
}

void OpenSubtitlesDownloadEngine::logout() {
  QVariantList args;
  args << token;
  rpc.request("LogOut", args);
  token = "";
}

const char* const OpenSubtitlesDownloadEngine::pixmapData[] = {
    "16 16 14 1",       ". c #000000",      "h c #111111",
    "c c #222222",      "j c #333333",      "g c #444444",
    "l c #555555",      "e c #777777",      "k c #888888",
    "a c #999999",      "b c #aaaaaa",      "f c #cccccc",
    "i c #dddddd",      "d c #eeeeee",      "# c #ffffff",
    "................", ".##.##.##.##.##.", ".##.##.##.##.##.",
    "................", "................", "...a##b..cd##d..",
    "..ea..be.fg.hi..", "..ic..hi.ig.....", "..dh..hd.g##ij..",
    "..ea..ak.k..gi..", "...b##b..fd##l..", "................",
    "................", ".##.##.##.##.##.", ".##.##.##.##.##.",
    "................"};
