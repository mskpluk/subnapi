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

#ifndef __FRMNAPIPROJEKTUPLOAD_H__
#define __FRMNAPIPROJEKTUPLOAD_H__

#include "config/qnapiconfig.h"
#include "engines/napiprojektuploadengine.h"
#include "qnapithread.h"
#include "ui_frmnapiprojektupload.h"

#include <QDialog>

class UploadThread : public QNapiThread {
  Q_OBJECT

 public:
  UploadThread();

  void setConfig(const QNapiConfig &configuration) { config = configuration; }
  void setRequest(const NapiProjektUploadRequest &req) { request = req; }

  void run();

  NapiProjektUploadResult result;
  QString serverMessage;

 signals:
  void uploadFinished();

 private:
  QNapiConfig config;
  NapiProjektUploadRequest request;
};

class frmNapiProjektUpload : public QDialog {
  Q_OBJECT

 public:
  frmNapiProjektUpload(QWidget *parent = 0,
                       Qt::WindowFlags f = Qt::WindowFlags());

  void setCorrectionMode(bool correction);

 public slots:
  void reject() override;

 private slots:
  void selectVideoFile();
  void selectSubtitleFile();
  void correctionToggled(bool correction);
  void okClicked();
  void uploadFinished();

 private:
  Ui::frmNapiProjektUpload ui;
  UploadThread uploadThread;
};

#endif  // __FRMNAPIPROJEKTUPLOAD_H__
