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

#include "frmnapiprojektscan.h"

#include "forms/frmscan.h"  // ScanFilesThread - reused for the directory walk
#include "libqnapi.h"
#include "qnapiopendialog.h"
#include "utils/filehash.h"
#include "utils/subtitleauthordetector.h"
#include "utils/subtitlefinder.h"

#include <QCloseEvent>
#include <QFileInfo>
#include <QIcon>
#include <QInputDialog>
#include <QMessageBox>
#include <QTableWidgetItem>

namespace {
const unsigned long NAPI_10MB = 10485760;
}  // namespace

void NapiProjektScanThread::run() {
  abort = false;

  QSharedPointer<const StaticConfig> staticConfig = LibQNapi::staticConfig();
  QSharedPointer<const P7ZipDecoder> p7zipDecoder = LibQNapi::p7zipDecoder(
      config.generalConfig().p7zipPath());
  NapiProjektUploadEngine uploadEngine(config.generalConfig().tmpPath(),
                                       config.enginesConfig()["NapiProjekt"],
                                       p7zipDecoder);
  QSharedPointer<const SubtitleConverter> subtitleConverter =
      LibQNapi::subtitleConverter(config.postProcessingConfig());

  ScanFilesThread scanner;
  scanner.setSearchPath(searchPath);
  scanner.setFilters(staticConfig->movieExtensionsFilter());
  scanner.setSkipFilters("");
  scanner.setSkipIfSubtitlesExists(false);
  scanner.setFollowSymLinks(false);

  // scanner is a local object living entirely inside this call, but its
  // *thread affinity* is the background thread (it was constructed here,
  // while this thread's run() is executing) while `this` (NapiProjektScanThread)
  // was constructed as a dialog member on the GUI thread - so AutoConnection
  // would resolve to Queued and defer these lambdas until after scanner (and
  // everything it captures by reference) has already been destroyed by the
  // time run() returns. Force Direct so they run synchronously, in place,
  // as intended.
  connect(&scanner, &ScanFilesThread::folderChange, this,
          &NapiProjektScanThread::folderChange, Qt::DirectConnection);
  connect(&scanner, &ScanFilesThread::addFile, this,
          [&](const QString &videoPath) {
            if (abort) {
              // scanner has its own, separate abort flag - propagate so the
              // inner directory walk actually stops instead of running to
              // completion while we silently drop its results.
              scanner.requestAbort();
              return;
            }

            QFileInfo videoInfo(videoPath);
            Maybe<QString> subtitlePath = findLocalSubtitleFile(
                videoInfo, staticConfig->subtitleExtensions());
            if (!subtitlePath) return;

            NapiProjektScanRow row;
            row.videoPath = videoPath;
            row.subtitlePath = subtitlePath.value();
            row.videoHash = md5HexOfFile(videoPath, NAPI_10MB);
            row.author = detectSubtitleAuthor(row.subtitlePath);
            row.format = subtitleConverter->detectFormat(row.subtitlePath);

            if (abort) return;

            NapiProjektCheckResult checkResult =
                uploadEngine.checkSubtitlesExist(row.videoHash, "PL");
            row.alreadyInDatabase =
                (checkResult == NapiProjektCheckResult::Available);

            emit rowFound(row);
          },
          Qt::DirectConnection);

  // ScanFilesThread::run() is called directly (not via start()), so it
  // executes synchronously on this thread rather than spawning a nested
  // QThread.
  scanner.run();

  emit scanFinished();
}

void NapiProjektBatchUploadThread::run() {
  abort = false;

  QSharedPointer<const P7ZipDecoder> p7zipDecoder = LibQNapi::p7zipDecoder(
      config.generalConfig().p7zipPath());
  NapiProjektUploadEngine engine(config.generalConfig().tmpPath(),
                                 config.enginesConfig()["NapiProjekt"],
                                 p7zipDecoder);

  for (int i = 0; i < requests.size(); i++) {
    if (abort) return;
    NapiProjektUploadResult result = engine.upload(requests[i]);
    emit itemFinished(i, result);
  }

  emit batchFinished();
}

frmNapiProjektScan::frmNapiProjektScan(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f), batchSuccessCount(0), batchFailCount(0) {
  qRegisterMetaType<NapiProjektScanRow>("NapiProjektScanRow");
  qRegisterMetaType<NapiProjektUploadResult>("NapiProjektUploadResult");

  ui.setupUi(this);
  ui.twResults->horizontalHeader()->setStretchLastSection(false);
  ui.twResults->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Stretch);

  connect(ui.pbDirectorySelect, &QPushButton::clicked, this,
          &frmNapiProjektScan::selectDirectory);
  connect(ui.pbScan, &QPushButton::clicked, this,
          &frmNapiProjektScan::scanClicked);
  connect(ui.twResults, &QTableWidget::cellClicked, this,
          &frmNapiProjektScan::cellClicked);
  connect(ui.pbSendSelected, &QPushButton::clicked, this,
          &frmNapiProjektScan::sendSelectedClicked);

  connect(&scanThread, &NapiProjektScanThread::folderChange, this,
          &frmNapiProjektScan::folderChange);
  connect(&scanThread, &NapiProjektScanThread::rowFound, this,
          &frmNapiProjektScan::rowFound);
  connect(&scanThread, &NapiProjektScanThread::scanFinished, this,
          &frmNapiProjektScan::scanFinished);

  connect(&batchUploadThread, &NapiProjektBatchUploadThread::itemFinished,
          this, &frmNapiProjektScan::itemFinished);
  connect(&batchUploadThread, &NapiProjektBatchUploadThread::batchFinished,
          this, &frmNapiProjektScan::batchFinished);
}

void frmNapiProjektScan::closeEvent(QCloseEvent *event) {
  // Same reasoning as UploadThread in frmnapiprojektupload.cpp: run()'s
  // last statement is a queued emit, so by the time a finished-slot fires
  // run() has already returned - but abort+wait() here makes closing mid-
  // scan/mid-batch (not just the normal-completion path) safe too, instead
  // of relying on timing.
  if (scanThread.isRunning()) {
    scanThread.requestAbort();
    scanThread.wait();
  }
  if (batchUploadThread.isRunning()) {
    batchUploadThread.requestAbort();
    batchUploadThread.wait();
  }
  event->accept();
}

void frmNapiProjektScan::selectDirectory() {
  QNapiOpenDialog openDialog(this, tr("Select the folder to scan"),
                             ui.leDirectory->text(), QNapiOpenDialog::None);
  if (openDialog.selectDirectory()) {
    ui.leDirectory->setText(openDialog.selectedFiles().first());
  }
}

void frmNapiProjektScan::scanClicked() {
  if (!QFileInfo(ui.leDirectory->text()).isDir()) {
    QMessageBox::warning(this, tr("Subnapi"), tr("Select a valid directory."));
    return;
  }

  rows.clear();
  included.clear();
  ui.twResults->setRowCount(0);
  ui.pbSendSelected->setEnabled(false);
  ui.pbScan->setEnabled(false);
  ui.leDirectory->setEnabled(false);
  ui.pbDirectorySelect->setEnabled(false);
  ui.lbAction->setText(tr("Scanning..."));

  scanThread.setSearchPath(ui.leDirectory->text());
  scanThread.setConfig(LibQNapi::loadConfig());
  scanThread.start();
}

void frmNapiProjektScan::folderChange(const QString &folder) {
  ui.lbAction->setText(tr("Scanning directory <b>%1</b>...").arg(folder));
}

void frmNapiProjektScan::rowFound(NapiProjektScanRow row) {
  int r = rows.size();
  rows << row;
  included << !row.alreadyInDatabase;

  auto readOnlyItem = [](const QString &text) {
    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
  };

  ui.twResults->insertRow(r);
  ui.twResults->setItem(
      r, 0, readOnlyItem(QFileInfo(row.subtitlePath).fileName()));
  ui.twResults->setItem(r, 1, new QTableWidgetItem(row.author));
  ui.twResults->setItem(r, 2, readOnlyItem(row.format));
  ui.twResults->setItem(r, 3, readOnlyItem(tr("Polish")));
  ui.twResults->setItem(r, 4, readOnlyItem(""));

  setRowIncluded(r, included[r]);
}

void frmNapiProjektScan::setRowIncluded(int row, bool isIncluded) {
  included[row] = isIncluded;

  QTableWidgetItem *statusItem = ui.twResults->item(row, 4);
  if (isIncluded) {
    statusItem->setIcon(QIcon(":/ui/accept.png"));
    statusItem->setText(tr("Ready to send"));
  } else if (rows[row].alreadyInDatabase) {
    statusItem->setIcon(QIcon(":/ui/exclamation.png"));
    statusItem->setText(tr("Subtitles are already in the database"));
  } else {
    statusItem->setIcon(QIcon(":/ui/exclamation.png"));
    statusItem->setText(tr("Ignored"));
  }

  updateSendButtonEnabled();
}

void frmNapiProjektScan::cellClicked(int row, int column) {
  // Only the Status column toggles inclusion - the Author column (1) is
  // double-click-editable, and a plain cellClicked fires before the edit
  // starts, so without this guard starting an edit would also flip the
  // row's status underneath the user.
  if (column != 4) return;
  if (row < 0 || row >= included.size()) return;
  setRowIncluded(row, !included[row]);
}

void frmNapiProjektScan::updateSendButtonEnabled() {
  ui.pbSendSelected->setEnabled(included.contains(true) &&
                                !scanThread.isRunning() &&
                                !batchUploadThread.isRunning());
}

void frmNapiProjektScan::scanFinished() {
  scanThread.wait();

  ui.pbScan->setEnabled(true);
  ui.leDirectory->setEnabled(true);
  ui.pbDirectorySelect->setEnabled(true);
  ui.lbAction->setText(rows.isEmpty()
                           ? tr("No local subtitle files found.")
                           : tr("Found %1 subtitle file(s).").arg(rows.size()));
  updateSendButtonEnabled();
}

void frmNapiProjektScan::sendSelectedClicked() {
  bool anyCorrections = false;
  for (int i = 0; i < rows.size(); i++) {
    if (included[i] && rows[i].alreadyInDatabase) {
      anyCorrections = true;
      break;
    }
  }

  QString correctionComment;
  if (anyCorrections) {
    bool ok = false;
    correctionComment = QInputDialog::getText(
        this, tr("Subnapi"),
        tr("These subtitles are already in NapiProjekt's database - this "
           "will be sent as a correction. Briefly describe what you "
           "fixed:"),
        QLineEdit::Normal, "", &ok);
    if (!ok) return;
  }

  QVector<NapiProjektUploadRequest> requests;
  QVector<int> requestRowIndex;

  for (int i = 0; i < rows.size(); i++) {
    if (!included[i]) continue;

    NapiProjektUploadRequest req;
    req.videoFilePath = rows[i].videoPath;
    req.subtitleFilePath = rows[i].subtitlePath;
    req.language = "PL";
    req.author = ui.twResults->item(i, 1)->text();
    req.mode = rows[i].alreadyInDatabase ? NapiProjektUploadMode::Correction
                                         : NapiProjektUploadMode::NewSubtitles;
    req.comment = rows[i].alreadyInDatabase ? correctionComment : "";

    requests << req;
    requestRowIndex << i;
    ui.twResults->item(i, 4)->setText(tr("Sending..."));
  }

  if (requests.isEmpty()) return;

  batchUploadRowIndex = requestRowIndex;
  batchSuccessCount = 0;
  batchFailCount = 0;

  ui.pbSendSelected->setEnabled(false);
  ui.pbScan->setEnabled(false);
  ui.lbAction->setText(tr("Sending selected subtitles..."));

  batchUploadThread.setConfig(LibQNapi::loadConfig());
  batchUploadThread.setRequests(requests);
  batchUploadThread.start();
}

void frmNapiProjektScan::itemFinished(int index, NapiProjektUploadResult result) {
  if (index < 0 || index >= batchUploadRowIndex.size()) return;
  int row = batchUploadRowIndex[index];

  QTableWidgetItem *statusItem = ui.twResults->item(row, 4);
  switch (result) {
    case NapiProjektUploadResult::Uploaded:
      statusItem->setIcon(QIcon(":/ui/accept.png"));
      statusItem->setText(tr("Sent"));
      batchSuccessCount++;
      break;
    default:
      statusItem->setIcon(QIcon(":/ui/exclamation.png"));
      statusItem->setText(tr("Failed to send"));
      batchFailCount++;
      break;
  }
}

void frmNapiProjektScan::batchFinished() {
  batchUploadThread.wait();

  ui.pbScan->setEnabled(true);
  ui.lbAction->setText(tr("Done."));
  updateSendButtonEnabled();

  if (batchFailCount == 0) {
    QMessageBox::information(
        this, tr("Subnapi"),
        tr("Sent %1 subtitle file(s) successfully.").arg(batchSuccessCount));
  } else {
    QMessageBox::warning(
        this, tr("Subnapi"),
        tr("Sent %1 of %2 subtitle file(s) successfully; %3 failed.")
            .arg(batchSuccessCount)
            .arg(batchSuccessCount + batchFailCount)
            .arg(batchFailCount));
  }
}
