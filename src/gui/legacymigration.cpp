#include "legacymigration.h"

#ifdef Q_OS_MAC

#include "config.h"
#include "common/utility.h"
#include "common/version.h"
#include "libsync/configfile.h"
#include "libsync/theme.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSettings>
#include <QStringList>
#include <QTextStream>
#include <QVariant>
#include <QVersionNumber>

#endif // Q_OS_MAC

namespace OCC {

#ifdef Q_OS_MAC

namespace {

using MigrationLogs = QStringList;

// Same directory the normal log uses: --logdir, the folder of --logfile, otherwise the
// temporary log dir "<appName>-logdir" (used when logToTemporaryLogDir is enabled).
QString migrationLogDirectory(const QString &optionLogDir, const QString &optionLogFile)
{
    if (!optionLogDir.isEmpty()) {
        return QDir(optionLogDir).absolutePath();
    }
    if (!optionLogFile.isEmpty() && optionLogFile != QLatin1String("-")) {
        return QFileInfo(optionLogFile).absolutePath();
    }
    return QDir::tempPath() + QLatin1Char('/') + QCoreApplication::applicationName() + QStringLiteral("-logdir");
}

// Migration runs before normal logging is configured, so the messages are written to their
// own file afterwards. Every run that does real work gets a new file, the name carries the time.
void writeMigrationLogs(const MigrationLogs &logs, const QString &optionLogDir, const QString &optionLogFile)
{
    if (logs.isEmpty()) {
        return; // nothing was attempted, no file
    }

    const QString dir = migrationLogDirectory(optionLogDir, optionLogFile);
    if (!QDir().mkpath(dir)) {
        qWarning().noquote() << "Migration: cannot create log directory" << dir;
        return;
    }

    // <appName>-migration-yyyyMMdd_HHmmss.log, never overwritten (counter if the name exists)
    const QString base = QStringLiteral("%1/%2-migration-%3")
                             .arg(dir, QCoreApplication::applicationName(),
                                 QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    QString path = base + QStringLiteral(".log");
    for (int i = 1; QFile::exists(path); ++i) {
        path = QStringLiteral("%1_%2.log").arg(base).arg(i);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning().noquote() << "Migration: cannot write" << path << file.errorString();
        return;
    }

    QTextStream stream(&file);
    for (const QString &line : logs) {
        stream << line << '\n';
    }
    stream.flush();
    qInfo().noquote() << "Migration: log written to" << path;
}

// Keys whose value must never end up in a log file.
bool isSecretKey(const QString &key)
{
    static const QStringList markers = {
        QStringLiteral("password"), QStringLiteral("passwd"), QStringLiteral("secret"),
        QStringLiteral("token"), QStringLiteral("oauth"), QStringLiteral("credential")};
    const QString lower = key.toLower();
    for (const QString &marker : markers) {
        if (lower.contains(marker)) {
            return true;
        }
    }
    return false;
}

// Full value as text, no length limit. Secrets are masked (but "empty" stays visible, that
// is useful when debugging), binary blobs (window geometry etc.) are only described by size.
QString migrationValue(const QString &key, const QVariant &value)
{
    if (value.typeId() == QMetaType::QByteArray) {
        return QStringLiteral("<%1 bytes>").arg(value.toByteArray().size());
    }

    QString text;
    if (value.typeId() == QMetaType::QStringList) {
        text = value.toStringList().join(QStringLiteral(", "));
    } else if (value.typeId() == QMetaType::QVariantList) {
        QStringList items;
        for (const QVariant &item : value.toList()) {
            items << item.toString();
        }
        text = items.join(QStringLiteral(", "));
    } else {
        text = value.toString();
    }

    if (isSecretKey(key)) {
        return text.isEmpty() ? QStringLiteral("<empty>") : QStringLiteral("***");
    }
    return text.isEmpty() ? QStringLiteral("<empty>") : text;
}

void addSection(MigrationLogs &logs, const QString &title, int count)
{
    logs << QString();
    logs << QStringLiteral("=== %1 (%2) ===").arg(title).arg(count);
}

// One "name = value" line per setting; a replaced target value is shown as "(was ...)".
QString settingLine(const QString &field, const QString &key, const QVariant &value, const QHash<QString, QVariant> &previous)
{
    QString text = QStringLiteral("  %1 = %2").arg(field, migrationValue(key, value));
    if (previous.contains(key)) {
        text += QStringLiteral("   (was: %1)").arg(migrationValue(key, previous.value(key)));
    }
    return text;
}

// Marks the migration as done so it is not retried (and no new log file is created) on every start.
void markMigrated(QSettings &settings)
{
    settings.setValue(QStringLiteral("LegacyMigrated"), true);
    settings.sync();
}

void migrateLegacySettings(MigrationLogs &logs)
{
    const QString legacyAppName = QStringLiteral(LEGACY_APPLICATION_SHORTNAME); // autostart registration
    const QString legacyPrefDir = QStringLiteral(LEGACY_APPLICATION_NAME); // ~/Library/Preferences/<dir>/
    const QString legacyCfgName = legacyAppName + QStringLiteral(".cfg");

    const QString targetCfgPath = ConfigFile::configFile();
    const QString oldCfgPath = QDir::homePath() + QStringLiteral("/Library/Preferences/") + legacyPrefDir
        + QLatin1Char('/') + legacyCfgName;

    QSettings newSettings(targetCfgPath, QSettings::IniFormat);
    if (newSettings.value(QStringLiteral("LegacyMigrated"), false).toBool()) {
        return; // already done, stay silent so normal starts do not create files
    }

    const QFileInfo oldInfo(oldCfgPath);
    if (!oldInfo.isFile() || !oldInfo.isReadable()) {
        return; // fresh install, nothing to migrate
    }

    logs << QStringLiteral("migration start");
    logs << QStringLiteral("  client version : %1").arg(OCC::Version::displayString());
    logs << QStringLiteral("  legacy config  : %1 (%2 bytes)").arg(oldCfgPath).arg(oldInfo.size());
    logs << QStringLiteral("  target config  : %1").arg(targetCfgPath);
    logs << QStringLiteral("  legacy app     : name \"%1\", short name \"%2\"").arg(legacyPrefDir, legacyAppName);

    // Permanent failures below mark the migration as done, transient ones are retried on the next start.
    if (oldInfo.canonicalFilePath() == QFileInfo(targetCfgPath).canonicalFilePath()) {
        logs << QStringLiteral("RESULT failed: legacy and target config are the same file, nothing copied");
        markMigrated(newSettings);
        return;
    }

    QSettings oldSettings(oldCfgPath, QSettings::IniFormat);
    if (oldSettings.status() != QSettings::NoError) {
        const bool permanent = oldSettings.status() == QSettings::FormatError;
        logs << QStringLiteral("RESULT failed: cannot parse legacy config (status %1), %2")
                    .arg(static_cast<int>(oldSettings.status()))
                    .arg(permanent ? QStringLiteral("format error, will not retry") : QStringLiteral("will retry on next start"));
        if (permanent) {
            markMigrated(newSettings);
        }
        return;
    }

    // Only configs written by the legacy client (v2.7.5) are migrated.
    static const QVersionNumber legacyVersion(2, 7, 5);
    const QString oldVersionString = oldSettings.value(QStringLiteral("clientVersion")).toString();
    const QVersionNumber oldVersion = QVersionNumber::fromString(oldVersionString);
    if (QVersionNumber::commonPrefix(oldVersion, legacyVersion) != legacyVersion) {
        logs << QStringLiteral("RESULT skipped: legacy config version is \"%1\", only %2 is migrated")
                    .arg(oldVersionString, legacyVersion.toString());
        markMigrated(newSettings);
        return;
    }
    logs << QStringLiteral("  legacy version : %1").arg(oldVersionString);

    QStringList keys = oldSettings.allKeys();
    if (keys.isEmpty()) {
        logs << QStringLiteral("RESULT failed: legacy config has no keys");
        markMigrated(newSettings);
        return;
    }
    keys.sort();

    // Copy everything. Remember target values that get replaced.
    QHash<QString, QVariant> previous;
    for (const QString &key : keys) {
        const QVariant value = oldSettings.value(key);
        if (newSettings.contains(key) && newSettings.value(key) != value) {
            previous.insert(key, newSettings.value(key));
        }
        newSettings.setValue(key, value);
    }
    newSettings.sync();
    if (newSettings.status() != QSettings::NoError) {
        logs << QStringLiteral("RESULT failed: cannot write target config (status %1), will retry on next start")
                    .arg(static_cast<int>(newSettings.status()));
        return;
    }

    // Group by account / folder / other. Keys are sorted, so every group is sorted too.
    QMap<QString, QStringList> accounts; // account id -> setting lines
    QMap<QString, QStringList> folders; // "account id/folder id" -> setting lines
    QStringList others;
    for (const QString &key : keys) {
        const QVariant value = oldSettings.value(key);
        const QStringList parts = key.split(QLatin1Char('/'));
        if (parts.value(0) == QLatin1String("Accounts") && parts.size() >= 3) {
            const QString accountId = parts.value(1);
            if (parts.value(2) == QLatin1String("Folders") && parts.size() >= 5) {
                folders[accountId + QLatin1Char('/') + parts.value(3)]
                    << settingLine(parts.mid(4).join(QLatin1Char('/')), key, value, previous);
            } else {
                accounts[accountId] << settingLine(parts.mid(2).join(QLatin1Char('/')), key, value, previous);
            }
        } else {
            others << settingLine(key, key, value, previous);
        }
    }

    logs << QString();
    logs << QStringLiteral("copied %1 keys: %2 accounts, %3 folders, %4 other settings, %5 replaced existing values")
                .arg(keys.size()).arg(accounts.size()).arg(folders.size()).arg(others.size()).arg(previous.size());

    addSection(logs, QStringLiteral("accounts"), accounts.size());
    for (auto it = accounts.cbegin(); it != accounts.cend(); ++it) {
        logs << QStringLiteral("account %1").arg(it.key());
        logs << it.value();
    }

    addSection(logs, QStringLiteral("folders"), folders.size());
    for (auto it = folders.cbegin(); it != folders.cend(); ++it) {
        logs << QStringLiteral("folder %1").arg(it.key());
        logs << it.value();
    }

    addSection(logs, QStringLiteral("other settings"), others.size());
    logs << others;

    // Check what really landed in the target file.
    QSettings verify(targetCfgPath, QSettings::IniFormat);
    int missing = 0;
    QStringList missingKeys;
    for (const QString &key : keys) {
        if (!verify.contains(key)) {
            ++missing;
            missingKeys << key;
        }
    }
    logs << QString();
    logs << QStringLiteral("verify: %1 of %2 keys present in target config").arg(keys.size() - missing).arg(keys.size());
    for (const QString &key : missingKeys) {
        logs << QStringLiteral("  MISSING %1").arg(key);
    }

    // Compare case-insensitive: the macOS file system is, so for equal names the "legacy"
    // autostart entry is the current one and must not be removed.
    if (legacyAppName.compare(Theme::instance()->appName(), Qt::CaseInsensitive) != 0) {
        OCC::Utility::setLaunchOnStartup(legacyAppName, legacyAppName, false);
        logs << QStringLiteral("removed legacy autostart entry \"%1\"").arg(legacyAppName);
    } else {
        logs << QStringLiteral("legacy autostart entry \"%1\" kept, same name as current app").arg(legacyAppName);
    }

    markMigrated(newSettings);
    logs << QStringLiteral("RESULT done: LegacyMigrated set in target config");
}

} // namespace

#endif // Q_OS_MAC

void runLegacySettingsMigration(const QString &optionLogDir, const QString &optionLogFile)
{
#ifdef Q_OS_MAC
    // Best effort: every failure inside only ends up in the migration log, it never stops the start.
    // (No try/catch, the client is built with -fno-exceptions.)
    MigrationLogs logs;
    migrateLegacySettings(logs);
    writeMigrationLogs(logs, optionLogDir, optionLogFile);
#else
    Q_UNUSED(optionLogDir)
    Q_UNUSED(optionLogFile)
#endif
}

} // namespace OCC