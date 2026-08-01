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

#include "guimain.h"
#include <signal.h>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QFileDialog>
#include <iostream>
#include "config/qnapiconfig.h"
#include "libqnapi.h"
#include "qnapicommand.h"
#include "utils/helphelper.h"
#include "engines/napiprojektdownloadengine.h"
#include "utils/p7zipdecoder.h"
#include "utils/notificationservice.h"

namespace GuiMain {

int processCommand(QNapiApp &app, QVariant cliCommand,
                   const QNapiConfig &config,
                   const QList<QSharedPointer<CliArgParser>> &cliArgParsers) {
  using namespace QNapiCommand;

  if (cliCommand.canConvert<RunCLIApp>()) {
    QStringList cliArgs = cliCommand.value<RunCLIApp>().arguments;
    return GuiMain::runCLI(cliArgs);

  } else if (cliCommand.canConvert<ShowHelp>()) {
    auto helpLines = HelpHelper::formatHelpLinesText(cliArgParsers, 30, 64);

    showHelpText(helpLines.join("\n"));

    return 0;
  } else if (cliCommand.canConvert<ShowHelpLanguages>()) {
    showHelpLanguages(config);

    return 0;
  } else if (cliCommand.canConvert<ShowOptions>()) {
    app.showSettings();
    return 0;

  } else if (cliCommand.canConvert<ScanDirectory>()) {
    app.setQuitOnLastWindowClosed(true);
    app.progress()->setBatchMode(true);
    app.showScanDialog(config, cliCommand.value<ScanDirectory>().directory);
    return app.exec();

  } else if (cliCommand.canConvert<DownloadSubtitles>()) {
    QStringList movieFilePaths =
        cliCommand.value<DownloadSubtitles>().movieFilePaths;

    // Check if this is a napiprojekt: link
    if (movieFilePaths.size() == 1 && 
        movieFilePaths.first().startsWith("napiprojekt:", Qt::CaseInsensitive)) {
      
      QString hash = movieFilePaths.first();
      QString language = config.generalConfig().language();
      if (language.isEmpty()) language = "PL";
      
      QString saveFilePath = QFileDialog::getSaveFileName(
        nullptr,
        QObject::tr("Save subtitles as"),
        QString("subtitles.srt"),
        QObject::tr("Subtitle files (*.srt *.txt *.sub);;All files (*.*)")
      );
      
      if (saveFilePath.isEmpty()) {
        return 0;
      }
      
      QString tmpPath = config.generalConfig().tmpPath();
      QString p7zipPath = config.generalConfig().p7zipPath();
      EngineConfig engineConfig = config.enginesConfig()["NapiProjekt"];
      QSharedPointer<const P7ZipDecoder> p7zipDecoder = 
          LibQNapi::p7zipDecoder(p7zipPath);
      
      NapiProjektDownloadEngine engine(tmpPath, engineConfig, p7zipDecoder);
      
      if (engine.downloadFromHashXml(hash, language, saveFilePath)) {
        NotificationService::showInfo(
            QObject::tr("Subnapi"),
            QObject::tr("Subtitles downloaded successfully!"),
            config.generalConfig().desktopNotificationsEnabled());
        return 0;
      } else {
        QMessageBox::critical(nullptr, 
          QObject::tr("Subnapi - Error"), 
          QObject::tr("Failed to download subtitles"));
        return 1;
      }
    }

    if (!app.isInstanceAllowed()) {
      foreach (QString movieFilePath, movieFilePaths) {
        QFileInfo fi(movieFilePath);
        app.sendRequest(fi.absoluteFilePath());
      }
      return 0;
    }

    app.setQuitOnLastWindowClosed(true);
    app.progress()->setBatchMode(true);
    app.progress()->enqueueFiles(movieFilePaths);
    app.progress()->download(config);
    return app.exec();
  }

  return 1;
}

void showHelpText(const QString &helpText) {
  QMessageBox helpDialog(QMessageBox::Information, "Subnapi", helpText,
                         QMessageBox::Close);

  QSpacerItem *horizontalSpacer =
      new QSpacerItem(720, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
  QGridLayout *layout = (QGridLayout *)helpDialog.layout();
  layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1,
                  layout->columnCount());

  helpDialog.setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  helpDialog.exec();
}

void showHelpLanguages(const QNapiConfig &config) {
  QStringList helpLanguagesLines;
  helpLanguagesLines << tr(
      "List of languages recognized by Subnapi, including corresponding");
  helpLanguagesLines << tr("two-letter language codes:");
  helpLanguagesLines << "";

  SubtitleLanguage L, LB;
  QStringList langs = L.listLanguages();

  foreach (QString lang, langs) {
    L.setLanguage(lang);
    helpLanguagesLines << QString(" %1 - %2").arg(L.toTwoLetter()).arg(lang);
  }

  L.setLanguage(config.generalConfig().language());
  LB.setLanguage(config.generalConfig().backupLanguage());

  helpLanguagesLines << "";
  helpLanguagesLines << tr("Current default subtitles language: %1 (%2)")
                            .arg(L.toFullName())
                            .arg(L.toTwoLetter());

  if (LB.toFullName().isEmpty()) {
    helpLanguagesLines << tr("No alternative subtitles language has been set");
  } else {
    helpLanguagesLines << tr("Current alternative subtitles language: %1 (%2)")
                              .arg(LB.toFullName())
                              .arg(LB.toTwoLetter());
  }

  showHelpText(helpLanguagesLines.join("\n"));
}

void showArgParserError(const QString &errorMessage) {
  QMessageBox::critical(0, tr("Subnapi - command line argument parsing error"),
                        errorMessage, QMessageBox::Close);
}

static QString cliExecutableName = "subnapic";

Maybe<QString> cliExecutablePath() {
  QStringList searchPaths = {LibQNapi::appExecutableDir};

  searchPaths << QProcess::systemEnvironment()
                     .filter(QRegularExpression("^PATH=(.*)$"))
                     .value(0)
                     .mid(5)
                     .split(":")
              << "/bin"
              << "/usr/bin"
              << "/usr/local/bin";
  searchPaths.removeAll("");

  foreach (QString path, searchPaths) {
    QString candidate = path + QDir::separator() + cliExecutableName;
    if (QFileInfo(candidate).isExecutable()) {
      return just(candidate);
    }
  }

  return nothing();
}

bool isCLIAvailable() { return cliExecutablePath(); }

int runCLI(const QStringList &args) {
  auto maybeCliExecutablePath = cliExecutablePath();
  if (!maybeCliExecutablePath) {
    QMessageBox::critical(0, tr("Subnapi - CLI executable not found"),
                          tr("You have requested to run Subnapi in command-line "
                             "mode, but unfortunately command-line binary '%1' "
                             "could not be found.")
                              .arg(cliExecutableName),
                          QMessageBox::Close);
    return 1;
  } else {
    return QProcess::execute(maybeCliExecutablePath.value(), args);
  }
}

void installTranslation(QCoreApplication &app, QTranslator *qtTranslator,
                        QTranslator *translator, const QNapiConfig &config) {
  QString uiLanguage = LibQNapi::uiLanguage(config.generalConfig());

  QString resourceDir = QLibraryInfo::location(QLibraryInfo::TranslationsPath);

  qtTranslator->load("qt_" + uiLanguage, resourceDir);
  app.installTranslator(qtTranslator);

  translator->load("subnapi_" + uiLanguage, ":/translations");
  app.installTranslator(translator);
}

void regSignal() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = sigHandler;
  sigaction(SIGTERM, &sa, 0);
  sigaction(SIGINT, &sa, 0);
}

void sigHandler(int sig) {
  Q_UNUSED(sig);

  std::cout << std::endl
            << QObject::tr("Subnapi: deleting temporary files...").toStdString()
            << std::endl;

  const QNapiConfig config = LibQNapi::loadConfig();
  QString tmpPath = config.generalConfig().tmpPath();
  QDir tmpDir(tmpPath);

  QStringList filters;
  filters << "Subnapi-*-rc";
  filters << "Subnapi.*.tmp";

  QFileInfoList files = tmpDir.entryInfoList(filters);

  foreach (QFileInfo file, files) { QFile::remove(file.filePath()); }

  std::cout << QObject::tr("Subnapi: finished.").toStdString() << std::endl;

  exit(666);
}

}  // namespace GuiMain
