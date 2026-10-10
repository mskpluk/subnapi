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

#ifndef __FRMPROGRESS__H__
#define __FRMPROGRESS__H__

#include <QMutex>
#include <QRecursiveMutex>

#include "ui_frmprogress.h"

#include "frmsummary.h"

#include "config/qnapiconfig.h"

#include <Maybe.h>
#include "frmlistsubtitles.h"
#include "qnapi.h"
#include "qnapiopendialog.h"
#include "qnapithread.h"
#include "subtitleinfo.h"

class GetThread : public QNapiThread {
  Q_OBJECT

 public:
  GetThread();

 signals:
  void fileNameChange(const QString &newfileName);
  void actionChange(const QString &newAction);
  void progressChange(int current, int all, float stageProgress);
  void criticalError(const QString &message);
  void selectSubtitles(QString fileName, SubtitleInfoList subtitles);

 private slots:
  void setCriticalMessage(const QString &msg) { criticalMessage = msg; }
  void subtitlesSelected(int idx);

 public:
  void setSpecificEngine(Maybe<QString> engine) { specificEngine = engine; }

  void setConfig(const QNapiConfig &configuration) { config = configuration; }

  void run();

  // The queue is shared with the GUI thread, which keeps adding files while
  // run() works through it, so it is only ever touched under queueMutex.
  void enqueue(const QString &file);
  bool hasQueuedFiles() const;
  int queueSize() const;
  // Files run() has not reached - added after it had finished with the queue,
  // or left behind by a cancel. Empties the queue.
  QStringList takeUnprocessed();
  bool wasAborted() const { return abort; }

 private:
  // The file at position i and the queue's current length; false once i is
  // past the end, which is also the moment the worker stops taking files.
  bool takeFile(int i, QString *file, int *total);

  mutable QMutex queueMutex;
  QStringList queue;
  int processed = 0;

 public:
  Maybe<QString> specificEngine;
  QList<SubtitleInfo> subStatusList;
  int napiSuccess, napiFail;
  QString criticalMessage;
  QMutex waitForDlg;
  int selIdx;
  QNapiConfig config;
};

class frmProgress : public QWidget {
  Q_OBJECT

 public:
  frmProgress(QWidget *parent = 0, Qt::WindowFlags f = Qt::WindowFlags());

  // Ignored while a download runs: the worker reads the engine once, as it
  // starts, and writing it under its feet would be a data race.
  void clearSpecificEngine() {
    if (!getThread.isRunning()) getThread.setSpecificEngine(nothing());
  }
  void setSpecificEngine(QString engine) {
    if (!getThread.isRunning()) getThread.setSpecificEngine(just(engine));
  }

  void setBatchMode(bool value) { batchMode = value; }
  bool isBatchMode() { return batchMode; }

 signals:
  void subtitlesSelected(int idx);

 public slots:
  void receiveRequest(const QString &request);
  void enqueueFile(const QString &file);
  void enqueueFiles(const QStringList &fileList);
  bool download(const QNapiConfig &config);
  void updateProgress(int current, int all, float stageProgress);
  void selectSubtitles(QString fileName, SubtitleInfoList subtitles);
  void downloadFinished();

 private:
  void closeEvent(QCloseEvent *event);
  void dragEnterEvent(QDragEnterEvent *event);
  void dropEvent(QDropEvent *event);

  Ui::frmProgress ui;
  GetThread getThread;
  frmListSubtitles frmSelect;
  frmSummary summary;

  bool batchMode, showSummary, closeRequested;
  // Recursive: downloadFinished() holds it across the summary dialog, whose
  // event loop can run closeEvent() or another downloadFinished() on this same
  // thread, and a plain mutex would deadlock there.
  QRecursiveMutex mutex;
};

#endif
