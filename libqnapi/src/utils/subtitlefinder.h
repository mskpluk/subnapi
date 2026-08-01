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

#ifndef SUBTITLEFINDER_H
#define SUBTITLEFINDER_H

#include <Maybe.h>
#include <QFileInfo>
#include <QStringList>

// Looks for a local subtitle file matching movieFile's base name (same
// directory, one of subtitleExtensions), the same "<basename>.<ext>" pairing
// rule used across the codebase for locally found subtitles.
Maybe<QString> findLocalSubtitleFile(const QFileInfo &movieFile,
                                     const QStringList &subtitleExtensions);

#endif  // SUBTITLEFINDER_H
