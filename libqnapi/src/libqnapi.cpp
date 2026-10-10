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

#include "libqnapi.h"
#include "movieinfo/libmediainfomovieinfoprovider.h"
#include "version.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>

namespace {
// One-time migration for the qnapi -> subnapi rename. QSettings with
// IniFormat + UserScope keeps <org>.ini in the XDG config directory, which is
// $XDG_CONFIG_HOME when that is set - always so under Flatpak, where it points
// into the sandbox - so the new file is looked for there, not in a hard-coded
// ~/.config. The old QNapi file may be in either place: a native QNapi wrote
// it to ~/.config, which a Flatpak still reaches through --filesystem=host.
// Copies, never moves, so the old file is left for anyone still running QNapi.
void migrateConfigFromQNapiIfNeeded() {
  const QString configDir =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  const QString newConfigPath = configDir + "/subnapi.ini";
  if (QFile::exists(newConfigPath)) return;

  for (const QString& oldConfigPath :
       {configDir + "/qnapi.ini", QDir::homePath() + "/.config/qnapi.ini"}) {
    if (QFile::exists(oldConfigPath)) {
      QDir().mkpath(configDir);
      QFile::copy(oldConfigPath, newConfigPath);
      return;
    }
  }
}
}  // namespace

QString LibQNapi::appExecutableFilePath = "";
QString LibQNapi::appExecutableDir = "";

void LibQNapi::init(const QString& appExecutablePath) {
  LibQNapi::appExecutableFilePath =
      QFileInfo(appExecutablePath).absoluteFilePath();
  LibQNapi::appExecutableDir =
      QFileInfo(appExecutableFilePath).absoluteDir().path();
}

QString LibQNapi::version() { return QString(QNAPI_VERSION); }

QString LibQNapi::displayableVersion() {
  return QString(QNAPI_DISPLAYABLE_VERSION);
}

QString LibQNapi::webpageUrl() { return QString(QNAPI_URL); }

QSharedPointer<const StaticConfig> LibQNapi::staticConfig() {
  return QSharedPointer<const StaticConfig>(new StaticConfig);
}

QSharedPointer<const ConfigReader> LibQNapi::configReader() {
  return QSharedPointer<const ConfigReader>(new ConfigReader(
      appExecutableDir, staticConfig(), subtitleDownloadEngineRegistry()));
}

QSharedPointer<const ConfigWriter> LibQNapi::configWriter() {
  QString ver = version();
  return QSharedPointer<const ConfigWriter>(new ConfigWriter(ver));
}

const QNapiConfig LibQNapi::loadConfig() {
  if (LibQNapi::isPortableMode())
    return LibQNapi::configReader()->readPortableConfig(
        LibQNapi::portableConfigPath());

  migrateConfigFromQNapiIfNeeded();
  return LibQNapi::configReader()->readUserConfig();
}

void LibQNapi::writeConfig(const QNapiConfig& config) {
  if (LibQNapi::isPortableMode())
    LibQNapi::configWriter()->writePortableConfig(
        LibQNapi::portableConfigPath(), config);
  else
    LibQNapi::configWriter()->writeUserConfig(config);
}

bool LibQNapi::isPortableMode() {
  return QFileInfo(portableConfigPath()).exists();
}

QString LibQNapi::portableConfigPath() {
  return appExecutableDir + QDir::separator() + "subnapi.ini";
}

QString LibQNapi::uiLanguage(const GeneralConfig& generalConfig) {
  if (generalConfig.uiLanguage().isEmpty()) {
    return QLocale::system().name();
  } else {
    return generalConfig.uiLanguage();
  }
}

QSharedPointer<const SubtitleDownloadEnginesRegistry>
LibQNapi::subtitleDownloadEngineRegistry() {
  return QSharedPointer<const SubtitleDownloadEnginesRegistry>(
      new SubtitleDownloadEnginesRegistry(
          LibQNapi::displayableVersion(),
          LibQNapi::staticConfig()->subtitleExtensions(),
          &LibQNapi::p7zipDecoder));
}

QSharedPointer<const MovieInfoProvider> LibQNapi::movieInfoProvider() {
  return QSharedPointer<const MovieInfoProvider>(
      new LibmediainfoMovieInfoProvider);
}

QSharedPointer<const SubtitleConverter> LibQNapi::subtitleConverter(
    const PostProcessingConfig& ppConfig) {
  return QSharedPointer<const SubtitleConverter>(new SubtitleConverter(
      LibQNapi::subtitleFormatsRegistry(), LibQNapi::movieInfoProvider(),
      ppConfig.skipConvertAds()));
}

QSharedPointer<const SubtitleFormatsRegistry>
LibQNapi::subtitleFormatsRegistry() {
  return QSharedPointer<SubtitleFormatsRegistry>(new SubtitleFormatsRegistry);
}

QSharedPointer<const P7ZipDecoder> LibQNapi::p7zipDecoder(
    const QString& p7zipPath) {
  return QSharedPointer<const P7ZipDecoder>(new P7ZipDecoder(p7zipPath));
}

QSharedPointer<const SubtitleMatcher> LibQNapi::subtitleMatcher(
    const QNapiConfig& config) {
  return QSharedPointer<const SubtitleMatcher>(
      new SubtitleMatcher(config.generalConfig().noBackup(),
                          config.postProcessingConfig().enabled(),
                          config.postProcessingConfig().subFormat(),
                          config.postProcessingConfig().subExtension(),
                          config.generalConfig().changePermissionsEnabled(),
                          config.generalConfig().changePermissionsTo(),
                          LibQNapi::subtitleFormatsRegistry()));
}

QSharedPointer<const SubtitlePostProcessor> LibQNapi::subtitlePostProcessor(
    const PostProcessingConfig& config) {
  return QSharedPointer<const SubtitlePostProcessor>(
      new SubtitlePostProcessor(config, LibQNapi::subtitleConverter(config)));
}
