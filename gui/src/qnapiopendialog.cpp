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

#include "qnapiopendialog.h"
#include "libqnapi.h"

QNapiOpenDialog::QNapiOpenDialog(QWidget* parent, const QString& caption,
                                 const QString& init_path,
                                 FilterMode filterMode)
    : QFileDialog(parent), staticConfig(LibQNapi::staticConfig()) {
  setAttribute(Qt::WA_QuitOnClose, false);
  setWindowTitle(caption);

  if (filterMode == Movies) {
    setNameFilter(tr("Video files (%1);;All files (*.*)")
                      .arg(staticConfig->movieExtensionsFilter()));
  } else if (filterMode == Subtitles) {
    setNameFilter(tr("Subtitles files (%1);;All files (*.*)")
                      .arg(staticConfig->subtitleExtensionsFilter()));
  }

  if (QFileInfo(init_path).isDir())
    setDirectory(init_path);
  else
    setDirectory(QDir::currentPath());

  QStringList sideUrls;

  sideUrls << QString(QDir::homePath() + "/Movies")
           << QString(QDir::homePath() + "/movies")
           << QString(QDir::homePath() + "/Video")
           << QString(QDir::homePath() + "/video")
           << QString(QDir::homePath() + "/Videos")
           << QString(QDir::homePath() + "/videos")
           << QString(QDir::homePath() + "/Filmy")
           << QString(QDir::homePath() + "/wideo");

  QList<QUrl> urls = sidebarUrls();

  foreach (QString sideUrl, sideUrls) {
    if (!QDir().exists(sideUrl)) continue;
    QUrl url = QUrl::fromLocalFile(sideUrl);
    if (!urls.contains(url)) urls << url;
  }

  setSidebarUrls(urls);
}

bool QNapiOpenDialog::selectFile() {
  if (!placeWindow()) return false;
  setFileMode(QFileDialog::ExistingFile);
  return exec();
}

bool QNapiOpenDialog::selectFiles() {
  if (!placeWindow()) return false;
  setFileMode(QFileDialog::ExistingFiles);
  return exec();
}

bool QNapiOpenDialog::selectDirectory() {
  if (!placeWindow()) return false;
  setFileMode(QFileDialog::Directory);
  setOption(QFileDialog::ShowDirsOnly);
  return exec();
}

bool QNapiOpenDialog::placeWindow() {
  if (isVisible()) {
    raise();
    return false;
  }

  QRect position = frameGeometry();
  position.moveCenter(screen()->availableGeometry().center());
  move(position.topLeft());

  return true;
}
