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

#include "frmnapiprojektupload.h"

#include "libqnapi.h"
#include "qnapiopendialog.h"
#include "subtitlelanguage.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

UploadThread::UploadThread() : config(LibQNapi::loadConfig()) {}

void UploadThread::run() {
  QString tmpPath = config.generalConfig().tmpPath();
  QString p7zipPath = config.generalConfig().p7zipPath();
  EngineConfig engineConfig = config.enginesConfig()["NapiProjekt"];
  QSharedPointer<const P7ZipDecoder> p7zipDecoder =
      LibQNapi::p7zipDecoder(p7zipPath);

  NapiProjektUploadEngine engine(tmpPath, engineConfig, p7zipDecoder);
  result = engine.upload(request);
  serverMessage = engine.lastServerMessage();

  emit uploadFinished();
}

frmNapiProjektUpload::frmNapiProjektUpload(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f) {
  ui.setupUi(this);

  foreach (QString lang, SubtitleLanguage("").listLanguages()) {
    ui.cbLang->addItem(lang, SubtitleLanguage(lang).toTwoLetter());
  }
  int plIdx = ui.cbLang->findData("pl");
  if (plIdx >= 0) ui.cbLang->setCurrentIndex(plIdx);

  setCorrectionMode(false);

  connect(ui.pbVideoFileSelect, &QPushButton::clicked, this,
          &frmNapiProjektUpload::selectVideoFile);
  connect(ui.pbSubtitleFileSelect, &QPushButton::clicked, this,
          &frmNapiProjektUpload::selectSubtitleFile);
  connect(ui.cbCorrection, &QCheckBox::toggled, this,
          &frmNapiProjektUpload::correctionToggled);
  connect(ui.buttonBox, &QDialogButtonBox::accepted, this,
          &frmNapiProjektUpload::okClicked);
  connect(ui.buttonBox, &QDialogButtonBox::rejected, this,
          &frmNapiProjektUpload::reject);
  connect(&uploadThread, &UploadThread::uploadFinished, this,
          &frmNapiProjektUpload::uploadFinished);
}

void frmNapiProjektUpload::setCorrectionMode(bool correction) {
  ui.cbCorrection->setChecked(correction);
  correctionToggled(correction);
}

void frmNapiProjektUpload::selectVideoFile() {
  QNapiOpenDialog openDialog(this, tr("Choose a video file"),
                             QFileInfo(ui.leVideoFile->text()).path(),
                             QNapiOpenDialog::Movies);
  if (openDialog.selectFile()) {
    ui.leVideoFile->setText(openDialog.selectedFiles().first());
  }
}

void frmNapiProjektUpload::selectSubtitleFile() {
  QNapiOpenDialog openDialog(this, tr("Choose a subtitles file"),
                             QFileInfo(ui.leSubtitleFile->text()).path(),
                             QNapiOpenDialog::Subtitles);
  if (openDialog.selectFile()) {
    ui.leSubtitleFile->setText(openDialog.selectedFiles().first());
  }
}

void frmNapiProjektUpload::correctionToggled(bool correction) {
  ui.lbComment->setVisible(correction);
  ui.leComment->setVisible(correction);
  setWindowTitle(correction
                     ? tr("Subnapi - Send a subtitles correction to NapiProjekt")
                     : tr("Subnapi - Upload subtitles to NapiProjekt"));
}

// An upload cannot be taken back once it has started - it is a write to
// somebody else's database - so there is no way out of this dialog while one
// is running. Rather than quietly swallowing the attempt, the buttons that
// would leave are visibly disabled for the duration, so that a dead Escape key
// reads as deliberate instead of as a frozen window.
void frmNapiProjektUpload::setUploadInProgress(bool inProgress) {
  if (QPushButton *ok = ui.buttonBox->button(QDialogButtonBox::Ok))
    ok->setEnabled(!inProgress);
  if (QPushButton *cancel = ui.buttonBox->button(QDialogButtonBox::Cancel))
    cancel->setEnabled(!inProgress);

  ui.pbVideoFileSelect->setEnabled(!inProgress);
  ui.pbSubtitleFileSelect->setEnabled(!inProgress);
  ui.cbCorrection->setEnabled(!inProgress);
}

void frmNapiProjektUpload::reject() {
  if (uploadThread.isRunning()) return;
  QDialog::reject();
}

void frmNapiProjektUpload::closeEvent(QCloseEvent *event) {
  // The window manager's own close button goes around the disabled Cancel, so
  // it is refused here too - otherwise this one route out would still be the
  // silent kind.
  if (uploadThread.isRunning()) {
    event->ignore();
    return;
  }
  QDialog::closeEvent(event);
}

void frmNapiProjektUpload::okClicked() {
  if (!QFile::exists(ui.leVideoFile->text())) {
    QMessageBox::warning(this, tr("Subnapi"), tr("Select a valid video file."));
    return;
  }
  if (!QFile::exists(ui.leSubtitleFile->text())) {
    QMessageBox::warning(this, tr("Subnapi"),
                         tr("Select a valid subtitles file."));
    return;
  }

  QNapiConfig config = LibQNapi::loadConfig();
  EngineConfig napiEngineConfig = config.enginesConfig()["NapiProjekt"];
  if (napiEngineConfig.nick().isEmpty() ||
      napiEngineConfig.password().isEmpty()) {
    QMessageBox::warning(
        this, tr("Subnapi"),
        tr("Configure your NapiProjekt account (nick and password) in "
           "Settings before uploading."));
    return;
  }

  NapiProjektUploadRequest request;
  request.videoFilePath = ui.leVideoFile->text();
  request.subtitleFilePath = ui.leSubtitleFile->text();
  request.language = ui.cbLang->currentData().toString();
  request.author = ui.leAuthor->text();
  request.mode = ui.cbCorrection->isChecked()
                     ? NapiProjektUploadMode::Correction
                     : NapiProjektUploadMode::NewSubtitles;
  request.comment = ui.cbCorrection->isChecked() ? ui.leComment->text() : "";

  setUploadInProgress(true);
  ui.lbStatus->setText(tr("Uploading..."));

  uploadThread.setConfig(config);
  uploadThread.setRequest(request);
  uploadThread.start();
}

void frmNapiProjektUpload::uploadFinished() {
  // uploadFinished() is emitted as the last statement of UploadThread::run(),
  // over a queued (cross-thread) connection - by the time this slot runs,
  // run() has already returned, but wait() makes that guarantee explicit
  // instead of relying on timing. Without it, accept() below could destroy
  // the dialog (and its uploadThread member) while QThread considers itself
  // still running.
  uploadThread.wait();

  setUploadInProgress(false);
  ui.lbStatus->setText("");

  switch (uploadThread.result) {
    case NapiProjektUploadResult::Uploaded:
      QMessageBox::information(this, tr("Subnapi"),
                               ui.cbCorrection->isChecked()
                                   ? tr("Correction sent, pending moderation.")
                                   : tr("Subtitles uploaded successfully!"));
      accept();
      return;
    case NapiProjektUploadResult::ConversionFailed:
      QMessageBox::critical(this, tr("Subnapi"),
                            tr("Could not convert the subtitles file."));
      return;
    case NapiProjektUploadResult::ArchiveCreationFailed:
      QMessageBox::critical(this, tr("Subnapi"),
                            tr("Could not create the upload archive."));
      return;
    case NapiProjektUploadResult::NetworkError:
      QMessageBox::critical(this, tr("Subnapi"),
                            tr("A network error occurred while uploading."));
      return;
    case NapiProjektUploadResult::ServerRejected:
      QMessageBox::critical(
          this, tr("Subnapi"),
          uploadThread.serverMessage.isEmpty()
              ? tr("NapiProjekt rejected the upload.")
              : tr("NapiProjekt rejected the upload: %1")
                    .arg(uploadThread.serverMessage));
      return;
  }
}
