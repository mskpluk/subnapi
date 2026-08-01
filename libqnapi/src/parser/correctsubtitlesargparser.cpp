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

#include "correctsubtitlesargparser.h"
#include "qnapicommand.h"

#include <QFileInfo>

CorrectSubtitlesArgParser::CorrectSubtitlesArgParser() {}

QVariant CorrectSubtitlesArgParser::parse(const QStringList& args,
                                          const QNapiConfig& config) const {
  int idx = args.indexOf("-r");
  if (idx == -1) {
    idx = args.indexOf("--correct");
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

  QString author;
  int authorIdx = args.indexOf("-a");
  if (authorIdx == -1) {
    authorIdx = args.indexOf("--author");
  }
  if (authorIdx != -1 && authorIdx + 1 < args.size()) {
    author = args[authorIdx + 1];
  }

  QString comment;
  int commentIdx = args.indexOf("-m");
  if (commentIdx == -1) {
    commentIdx = args.indexOf("--comment");
  }
  if (commentIdx != -1 && commentIdx + 1 < args.size()) {
    comment = args[commentIdx + 1];
  }

  return QVariant::fromValue(ParsedCommand{
      config, QVariant::fromValue(QNapiCommand::UploadSubtitles{
                  videoFilePath, subtitleFilePath, author, comment, true})});
}

Maybe<CliArgParser::HelpInfo> CorrectSubtitlesArgParser::helpInfo() const {
  return just(HelpInfo{
      "-r", "--correct", tr("video subs"),
      tr("Send a subtitles correction to NapiProjekt (requires an account "
         "configured in Settings; optionally pass -a [name] for author "
         "and -m [text] for a comment)")});
}
