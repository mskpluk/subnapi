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

#include "frmprogress.h"
#include <QCloseEvent>
#include <QScreen>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include "libqnapi.h"
#include "qnapiapp.h"

frmProgress::frmProgress(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f) {
  qRegisterMetaType<SubtitleInfoList>("QNapiSubtitleInfoList");

  ui.setupUi(this);

  setAttribute(Qt::WA_DeleteOnClose, false);
  setAttribute(Qt::WA_QuitOnClose, false);

  setBatchMode(false);

  connect(&getThread, SIGNAL(fileNameChange(const QString &)), ui.lbFileName,
          SLOT(setText(const QString &)));
  connect(&getThread, SIGNAL(actionChange(const QString &)), ui.lbAction,
          SLOT(setText(const QString &)));
  connect(&getThread, SIGNAL(progressChange(int, int, float)), this,
          SLOT(updateProgress(int, int, float)));
  connect(&getThread, SIGNAL(selectSubtitles(QString, SubtitleInfoList)), this,
          SLOT(selectSubtitles(QString, SubtitleInfoList)));
  connect(this, SIGNAL(subtitlesSelected(int)), &getThread,
          SLOT(subtitlesSelected(int)));
  connect(&getThread, SIGNAL(finished()), this, SLOT(downloadFinished()));

  QRect position = frameGeometry();
  position.moveCenter(screen()->availableGeometry().center());
  move(position.topLeft());
}

void frmProgress::receiveRequest(const QString &request) {
  enqueueFile(request);
  if (!getThread.isRunning()) download(LibQNapi::loadConfig());
  raise();
  activateWindow();
}

void frmProgress::enqueueFile(const QString &filePath) {
  if (QFile::exists(filePath)) {
    getThread.enqueue(filePath);
    updateProgress(-1, getThread.queueSize(), -1);
  }
}

void frmProgress::enqueueFiles(const QStringList &fileList) {
  for (int i = 0; i < fileList.size(); i++) {
    enqueueFile(fileList.at(i));
  }
}

bool frmProgress::download(const QNapiConfig &config) {
  // Already running: the files just queued are picked up by this run, or by
  // the follow-up downloadFinished() starts. Its configuration stays as it is;
  // replacing it under the worker would be a data race.
  if (getThread.isRunning()) return true;

  if (summary.isVisible()) {
    summary.close();
  }

  QNapi napi(config);

  // TODO: perform these checks earlier
  if (!napi.checkP7ZipPath()) {
    QMessageBox::warning(0, tr("Can not find p7zip!"),
                         tr("The path to the program p7zip is incorrect!"));
    return false;
  }

  if (!napi.checkTmpPath()) {
    QMessageBox::warning(
        0, tr("Invalid temporary directory!"),
        tr("Unable to write to the temporary directory! Check your settings."));
    return false;
  }

  if (!getThread.hasQueuedFiles()) {
    QMessageBox::warning(
        0, tr("No files!"),
        tr("Can't download subtitles as no movie files specified!"));
    return false;
  }

  if (!isVisible()) {
    QRect position = frameGeometry();
    position.moveCenter(screen()->availableGeometry().center());
    move(position.topLeft());
    show();
  }

  showSummary = true;
  closeRequested = false;
  ui.pbCancel->setEnabled(true);

  getThread.setConfig(config);
  getThread.start();
  return true;
}

void frmProgress::updateProgress(int current, int all, float stageProgress) {
  static int lastCurrent, lastAll;
  static float lastStageProgress;

  static QMutex m;
  m.lock();

  if (current >= 0) lastCurrent = current;
  if (all >= 0) lastAll = all;
  if (stageProgress >= 0) lastStageProgress = stageProgress;

  QString windowTitle =
      (lastAll > 1) ? QString(tr("Subnapi - downloading subtitles (%1/%2)"))
                          .arg(lastCurrent + 1)
                          .arg(lastAll)
                    : QString(tr("Subnapi - downloading subtitles..."));
  setWindowTitle(windowTitle);

  ui.pbProgress->setMaximum(lastAll * 100);
  ui.pbProgress->setValue(lastCurrent * 100 + (int)(lastStageProgress * 100));

  m.unlock();
}

void frmProgress::selectSubtitles(QString fileName,
                                  SubtitleInfoList subtitles) {
  frmSelect.setFileName(fileName);
  frmSelect.setSubtitlesList(subtitles);

  int selIdx;

  if (frmSelect.exec() == QDialog::Accepted) {
    selIdx = frmSelect.getSelectedIndex();
  } else {
    selIdx = -1;
  }

  emit subtitlesSelected(selIdx);
}

void frmProgress::downloadFinished() {
  // finished() arrives a moment before the thread stops counting as running.
  // Only that thread can be about to end here, so this returns at once - and
  // from here on download() can trust isRunning(). Not later: the summary
  // below runs its own event loop, which may start a new run.
  getThread.wait();
  hide();

  const QStringList leftover = getThread.takeUnprocessed();

  mutex.lock();
  if (showSummary) {
    if (!getThread.criticalMessage.isEmpty()) {
      QMessageBox::critical(0, tr("Critical error!"),
                            getThread.criticalMessage);
    } else if (!getThread.subStatusList.isEmpty()) {
      summary.setSummaryList(getThread.subStatusList);
      summary.exec();
    }
  }

  if (closeRequested) close();

  const bool stopped = closeRequested || getThread.wasAborted() ||
                       !getThread.criticalMessage.isEmpty();
  mutex.unlock();

  // Files added after the worker had finished with the queue - from the tray
  // or a second instance, a moment too late - get a run of their own instead
  // of being dropped. Not after a cancel or an error, which stop everything.
  if (!leftover.isEmpty() && !stopped) {
    enqueueFiles(leftover);
    if (download(getThread.config)) return;
  }

  // A file sent by a second instance while the summary was open has started a
  // run of its own; quitting now would take it down unfinished. Its own
  // downloadFinished() quits once it is done.
  if (batchMode && !getThread.isRunning()) qApp->quit();
}

void frmProgress::closeEvent(QCloseEvent *event) {
  if (getThread.isRunning()) {
    if (QMessageBox::question(
            this, tr("Subnapi"),
            tr("Do you want to cancel subtitles downloading?"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      mutex.lock();
      showSummary = false;
      getThread.requestAbort();
      ui.lbAction->setText(tr("Finishing the tasks..."));
      ui.lbFileName->setText("");
      ui.pbCancel->setEnabled(false);
      qApp->processEvents();
      closeRequested = true;
      mutex.unlock();
      event->ignore();
    } else {
      event->ignore();
    }
  } else {
    event->accept();
  }
}

void frmProgress::dragEnterEvent(QDragEnterEvent *event) {
  QUrl url(event->mimeData()->urls().at(0));
  QFileInfo fi(url.toLocalFile());
  if (fi.exists() && fi.isFile()) event->acceptProposedAction();
}

void frmProgress::dropEvent(QDropEvent *event) {
  QList<QUrl> urlList;
  QFileInfo info;

  urlList = event->mimeData()->urls();

  foreach (QUrl url, urlList) {
    info.setFile(url.toLocalFile());
    if (!info.exists() || !info.isFile()) continue;
    enqueueFile(url.toLocalFile());
  }
}

GetThread::GetThread() : config(LibQNapi::loadConfig()) {
  connect(this, SIGNAL(criticalError(const QString &)), this,
          SLOT(setCriticalMessage(const QString &)));
}

void GetThread::subtitlesSelected(int idx) {
  selIdx = idx;
  waitForDlg.unlock();
}

#define ABORT_POINT \
  {                 \
    if (abort) {    \
      return;       \
    }               \
  }

void GetThread::run() {
  abort = false;
  criticalMessage.clear();
  {
    QMutexLocker lock(&queueMutex);
    processed = 0;
    if (queue.isEmpty()) return;
  }

  napiSuccess = napiFail = 0;
  subStatusList.clear();

  QNapi napi(config, specificEngine);

  emit progressChange(0, queueSize(), 0.0f);

  QString language = config.generalConfig().language();
  QString languageBackup = config.generalConfig().backupLanguage();

  // Files are taken one at a time, so one added while this runs is picked up
  // in the same run.
  QString file;
  int total = 0;
  for (int i = 0; takeFile(i, &file, &total); i++) {
    ABORT_POINT

    QFileInfo fi(file);
    emit fileNameChange(fi.fileName());

    napi.setMoviePath(file);

    emit progressChange(i, total, 0.1f);
    emit actionChange(tr("Checking permissions of the video directory..."));

    if (!napi.checkWritePermissions()) {
      emit criticalError(tr("No permission to write to the directory '%1'!")
                             .arg(QFileInfo(file).path()));
      return;
    }

    napi.clearSubtitlesList();

    emit progressChange(i, total, 0.3f);
    emit actionChange(tr("Calculating checksum of the file..."));

    napi.checksum();

    ABORT_POINT

    bool found = false;
    SearchPolicy sp = config.generalConfig().searchPolicy();

    if (sp == SP_SEARCH_ALL_WITH_BACKUP_LANG) {
      foreach (QString e, napi.listLoadedEngines()) {
        emit progressChange(i, total, 0.4f);
        emit actionChange(
            tr("Searching for subtitles [%1] (%2)...").arg(language, e));
        found = napi.lookForSubtitles(language, e) || found;

        ABORT_POINT

        if (languageBackup.isEmpty()) continue;
        emit actionChange(
            tr("Searching for subtitles in alternative language [%1] (%2)...")
                .arg(languageBackup, e));
        found = napi.lookForSubtitles(languageBackup, e) || found;

        ABORT_POINT
      }
    } else {
      foreach (QString e, napi.listLoadedEngines()) {
        emit progressChange(i, total, 0.4f);
        emit actionChange(
            tr("Searching for subtitles [%1] (%2)...").arg(language, e));
        found = napi.lookForSubtitles(language, e) || found;

        if (sp == SP_BREAK_IF_FOUND && found) {
          break;
        }

        ABORT_POINT
      }

      if (!found && !languageBackup.isEmpty()) {
        foreach (QString e, napi.listLoadedEngines()) {
          emit progressChange(i, total, 0.45f);
          emit actionChange(
              tr("Searching for subtitles in alternative language [%1] (%2)...")
                  .arg(languageBackup, e));
          found = napi.lookForSubtitles(languageBackup, e) || found;

          if (sp == SP_BREAK_IF_FOUND && found) break;

          ABORT_POINT
        }
      }
    }

    if (!found) {
      ++napiFail;
      subStatusList << SubtitleInfo::fromFailed(file, napi.error());
      continue;
    }

    // jesli mozna i potrzeba, listujemy dostepne napisy
    if (napi.needToShowList()) {
      emit selectSubtitles(QFileInfo(file).fileName(),
                           napi.listSubtitles());

      waitForDlg.lock();
      waitForDlg.lock();
      waitForDlg.unlock();
    } else {
      selIdx = napi.bestIdx();
    }

    ABORT_POINT

    if (selIdx == -1) {
      ++napiFail;
      subStatusList << SubtitleInfo::fromFailed(file);
      continue;
    }

    emit progressChange(i, total, 0.5);
    emit actionChange(tr("Downloading subtitles file..."));

    if (!napi.download(selIdx)) {
      ABORT_POINT

      ++napiFail;
      subStatusList << SubtitleInfo::fromFailed(file);
      continue;
    }

    ABORT_POINT

    emit progressChange(i, total, 0.65f);
    emit actionChange(tr("Unpacking subtitles file..."));

    if (!napi.unpack(selIdx)) {
      ++napiFail;
      subStatusList << SubtitleInfo::fromFailed(file);
      continue;
    }

    if (napi.ppEnabled()) {
      emit progressChange(i, total, 0.8f);
      emit actionChange(tr("Post-processing subtitles..."));
      napi.postProcessSubtitles();
    }

    emit progressChange(i, total, 0.9f);
    emit actionChange(tr("Adjusting subtitles..."));

    if (!napi.matchSubtitles()) {
      ABORT_POINT

      ++napiFail;
      subStatusList << SubtitleInfo::fromFailed(file);

      emit criticalError(tr("Could not adjust subtitles!"));
      return;
    }

    ++napiSuccess;
    subStatusList << napi.listSubtitles().at(selIdx);

    napi.cleanup();

    emit progressChange(i, total, 1.0f);
  }

  emit progressChange(total - 1, total, 1);
}

void GetThread::enqueue(const QString &file) {
  QMutexLocker lock(&queueMutex);
  queue << file;
}

bool GetThread::hasQueuedFiles() const {
  QMutexLocker lock(&queueMutex);
  return !queue.isEmpty();
}

int GetThread::queueSize() const {
  QMutexLocker lock(&queueMutex);
  return queue.size();
}

bool GetThread::takeFile(int i, QString *file, int *total) {
  QMutexLocker lock(&queueMutex);
  if (i >= queue.size()) return false;
  *file = queue.at(i);
  *total = queue.size();
  processed = i + 1;
  return true;
}

QStringList GetThread::takeUnprocessed() {
  QMutexLocker lock(&queueMutex);
  const QStringList rest = queue.mid(processed);
  queue.clear();
  processed = 0;
  return rest;
}
