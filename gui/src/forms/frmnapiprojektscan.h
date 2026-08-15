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

#ifndef __FRMNAPIPROJEKTSCAN_H__
#define __FRMNAPIPROJEKTSCAN_H__

#include "config/qnapiconfig.h"
#include "engines/napiprojektuploadengine.h"
#include "qnapithread.h"
#include "ui_frmnapiprojektscan.h"

#include <QDialog>
#include <QMetaType>
#include <QVector>

struct NapiProjektScanRow {
  QString videoPath;
  QString subtitlePath;
  QString videoHash;
  QString author;
  QString format;
  // Whether NapiProjekt already holds subtitles for this video. Deliberately
  // not a bool: the check can also fail to reach the server, and that is not
  // the same answer as "not there". Collapsing the two would offer a file for
  // upload that may well be a duplicate.
  NapiProjektCheckResult checkResult;
};

Q_DECLARE_METATYPE(NapiProjektScanRow)

// Walks a directory for video files with a matching local subtitle file,
// and for each one: computes the video hash, detects the subtitle format
// and translator credit, and asks NapiProjekt (mode=2056, read-only)
// whether it already has subtitles for that hash. Emits one rowFound() per
// pair as it goes, so the table fills in incrementally.
class NapiProjektScanThread : public QNapiThread {
  Q_OBJECT

 public:
  void setSearchPath(const QString &path) { searchPath = path; }
  void setConfig(const QNapiConfig &configuration) { config = configuration; }

  void run();

 signals:
  void folderChange(const QString &folder);
  void rowFound(NapiProjektScanRow row);
  void scanFinished();

 private:
  QString searchPath;
  QNapiConfig config;
};

// Sequentially uploads a batch of rows via NapiProjektUploadEngine::upload().
class NapiProjektBatchUploadThread : public QNapiThread {
  Q_OBJECT

 public:
  void setConfig(const QNapiConfig &configuration) { config = configuration; }
  void setRequests(const QVector<NapiProjektUploadRequest> &reqs) {
    requests = reqs;
  }

  void run();

 signals:
  void itemFinished(int index, NapiProjektUploadResult result,
                    const QString& serverMessage);
  void batchFinished();

 private:
  QNapiConfig config;
  QVector<NapiProjektUploadRequest> requests;
};

class frmNapiProjektScan : public QDialog {
  Q_OBJECT

 public:
  frmNapiProjektScan(QWidget *parent = 0, Qt::WindowFlags f = Qt::WindowFlags());

 private slots:
  void selectDirectory();
  void scanClicked();
  void folderChange(const QString &folder);
  void rowFound(NapiProjektScanRow row);
  void scanFinished();
  void cellClicked(int row, int column);
  void sendSelectedClicked();
  void itemFinished(int index, NapiProjektUploadResult result,
                    const QString& serverMessage);
  void batchFinished();

 private:
  void closeEvent(QCloseEvent *event);
  void setRowIncluded(int row, bool included);
  void updateSendButtonEnabled();

  Ui::frmNapiProjektScan ui;
  NapiProjektScanThread scanThread;
  NapiProjektBatchUploadThread batchUploadThread;

  QVector<NapiProjektScanRow> rows;
  QVector<bool> included;
  QVector<int> batchUploadRowIndex;
  int batchSuccessCount;
  int batchFailCount;
};

#endif  // __FRMNAPIPROJEKTSCAN_H__
