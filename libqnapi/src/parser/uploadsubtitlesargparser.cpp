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

#include "uploadsubtitlesargparser.h"
#include "qnapicommand.h"

#include <QFileInfo>

namespace {
// Reads the value that follows a switch, refusing one that is itself a switch.
// Without that check, forgetting the value silently promoted the next option
// to be it - "-a -m fixed timing" uploaded "-m" as the subtitle author, and
// nothing objected, because from the parser's point of view a value was there.
Maybe<QString> optionValue(const QStringList& args, const QString& shortSwitch,
                           const QString& longSwitch, bool* missingValue) {
  *missingValue = false;

  int idx = args.indexOf(shortSwitch);
  if (idx == -1) idx = args.indexOf(longSwitch);
  if (idx == -1) return nothing();

  if (idx + 1 >= args.size() || args[idx + 1].startsWith("-")) {
    *missingValue = true;
    return nothing();
  }

  return just(args[idx + 1]);
}
}  // namespace

UploadSubtitlesArgParser::UploadSubtitlesArgParser() {}

QVariant UploadSubtitlesArgParser::parse(const QStringList& args,
                                         const QNapiConfig& config) const {
  int idx = args.indexOf("-u");
  if (idx == -1) {
    idx = args.indexOf("--upload");
  }
  if (idx == -1) {
    return QVariant::fromValue(NothingParsed());
  }

  if (idx + 2 >= args.size()) {
    return QVariant::fromValue(
        ParseError{tr("You need to pass a video file path and a subtitles "
                      "file path after %1 argument")
                       .arg(args[idx])});
  }

  QString videoFilePath = args[idx + 1];
  QString subtitleFilePath = args[idx + 2];

  if (!QFileInfo(videoFilePath).isFile()) {
    return QVariant::fromValue(
        ParseError{tr("Invalid video file passed: %1").arg(videoFilePath)});
  }
  if (!QFileInfo(subtitleFilePath).isFile()) {
    return QVariant::fromValue(ParseError{
        tr("Invalid subtitles file passed: %1").arg(subtitleFilePath)});
  }

  bool missingValue = false;

  Maybe<QString> authorOpt =
      optionValue(args, "-a", "--author", &missingValue);
  if (missingValue) {
    return QVariant::fromValue(
        ParseError{tr("You need to pass an author name after -a argument")});
  }
  QString author = authorOpt ? authorOpt.value() : QString();

  return QVariant::fromValue(ParsedCommand{
      config, QVariant::fromValue(QNapiCommand::UploadSubtitles{
                  videoFilePath, subtitleFilePath, author, "", false})});
}

Maybe<CliArgParser::HelpInfo> UploadSubtitlesArgParser::helpInfo() const {
  return just(HelpInfo{
      "-u", "--upload", tr("video subs"),
      tr("Upload new subtitles to NapiProjekt (requires an account "
         "configured in Settings; optionally pass -a [name] for author)")});
}
