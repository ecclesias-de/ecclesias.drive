#pragma once

#include "owncloudguilib.h"

#include <QString>

namespace OCC {

/**
 * One-time migration of the legacy client settings (v2.7.5) to the current config file.
 * macOS only, a no-op on every other platform.
 *
 * Has to run before checkClientVersion() (the copied legacy "clientVersion" key would
 * overwrite the version bump) and before setupLogging() (the migrated config decides the
 * log setup). Messages are collected in memory and written to their own
 * "<appName>-migration-<datetime>.log" in the log directory; nothing is written if
 * there was nothing to migrate.
 *
 * TODO: remove after the configs have been migrated from v2.7.5 to v6.0.3
 */
OWNCLOUDGUI_EXPORT void runLegacySettingsMigration(const QString &optionLogDir, const QString &optionLogFile);

}