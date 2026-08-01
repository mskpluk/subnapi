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

#ifndef SUBTITLEAUTHORDETECTOR_H
#define SUBTITLEAUTHORDETECTOR_H

#include <QString>

// Looks for a translator credit line among the last few lines of a subtitle
// file - e.g. "Tłumaczenie: ktoś", "tekst polski: ktoś",
// "Tłumaczenie na zlecenie X: ktoś" - and returns the text after the colon.
// Returns an empty string if nothing matches (the field is meant to be
// user-editable afterward, not a hard failure).
QString detectSubtitleAuthor(const QString &subtitleFilePath);

#endif  // SUBTITLEAUTHORDETECTOR_H
