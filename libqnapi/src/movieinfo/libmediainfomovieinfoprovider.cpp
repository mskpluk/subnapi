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

#include "libmediainfomovieinfoprovider.h"
#include <QRegularExpression>
#include <string>

#ifndef UNICODE
#define UNICODE
#endif

#ifdef UNICODE
#define _UNICODE
#endif

#include "MediaInfo/MediaInfo.h"
using namespace MediaInfoLib;

const Maybe<MovieInfo> LibmediainfoMovieInfoProvider::getMovieInfo(
    const QString& moviePath) const {
  MediaInfo mi;
  mi.Option(__T("Internet"), __T("No"));

  mi.Open(moviePath.toStdWString());

#define GET_VIDEO_INFO(__streamIdx, __key) \
  QString::fromStdWString(                 \
      std::wstring(mi.Get(Stream_Video, __streamIdx, __T(__key), Info_Text)))

  QString widthS = GET_VIDEO_INFO(0, "Width");
  QString heightS = GET_VIDEO_INFO(0, "Height");
  QString frameRateS = GET_VIDEO_INFO(0, "FrameRate");
  QString durationS = GET_VIDEO_INFO(0, "Duration");

  mi.Close();

  QRegularExpressionMatch mWidth = QRegularExpression("(\\d+)").match(widthS);
  if (!mWidth.hasMatch()) return nothing();

  QRegularExpressionMatch mHeight =
      QRegularExpression("(\\d+)").match(heightS);
  if (!mHeight.hasMatch()) return nothing();

  QRegularExpressionMatch mFrameRate =
      QRegularExpression("(\\d+).(\\d+)").match(frameRateS);
  if (!mFrameRate.hasMatch()) return nothing();
  long frFloor = mFrameRate.captured(1).toLong();
  long frFrac = mFrameRate.captured(2).toLong();

  QRegularExpressionMatch mDuration =
      QRegularExpression("(\\d+)").match(durationS);
  if (!mDuration.hasMatch()) return nothing();

  MovieInfo info(mWidth.captured(1).toInt(), mHeight.captured(1).toInt(),
                 (double)frFloor + (double)frFrac / 1000.0,
                 mDuration.captured(1).toDouble() / 1000.0);

  return just<MovieInfo>(info);
}
