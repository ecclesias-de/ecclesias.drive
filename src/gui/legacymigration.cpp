#include "legacymigration.h"

#include <QtGlobal>

// The migration exists for macOS (~/Library/Preferences/<name>/) and Windows (%APPDATA%\<name>\).
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
#define LEGACY_MIGRATION 1
#endif

#ifdef LEGACY_MIGRATION

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
#include <QMap>
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QTextStream>
#include <QVariant>
#include <QVersionNumber>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#endif

#endif // LEGACY_MIGRATION

namespace OCC {

#ifdef LEGACY_MIGRATION

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
    const bool isList = value.typeId() == QMetaType::QStringList || value.typeId() == QMetaType::QVariantList;
    const QString text = isList ? value.toStringList().join(QStringLiteral(", ")) : value.toString();
    if (text.isEmpty()) {
        return QStringLiteral("<empty>");
    }
    return isSecretKey(key) ? QStringLiteral("***") : text;
}

// "=== title (count) ===", then one block per item ("account 0", "folder 0/1") with its setting lines.
void addGroups(MigrationLogs &logs, const QString &title, const QString &itemLabel, const QMap<QString, QStringList> &groups)
{
    logs << QString() << QStringLiteral("=== %1 (%2) ===").arg(title).arg(groups.size());
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        logs << QStringLiteral("%1 %2").arg(itemLabel, it.key());
        logs << it.value();
    }
}

// Marks the migration as done so it is not retried (and no new log file is created) on every start.
void markMigrated(QSettings &settings)
{
    settings.setValue(QStringLiteral("LegacyMigrated"), true);
    settings.sync();
}

// Where the legacy (v2.7.5) client stored "<shortname>.cfg". The legacy app has a different name than
// the current one, so ConfigFile::configFile() cannot be used.
//   macOS:   ~/Library/Preferences/<LEGACY_APPLICATION_NAME>/<shortname>.cfg
//   Windows: %APPDATA%\<LEGACY_APPLICATION_NAME>\<shortname>.cfg
QString legacyConfigPath(const QString &legacyAppName, const QString &legacyPrefDir)
{
#ifdef Q_OS_MAC
    const QString base = QDir::homePath() + QStringLiteral("/Library/Preferences");
#else
    const QString base = QDir::fromNativeSeparators(qEnvironmentVariable("APPDATA"));
    if (base.isEmpty()) {
        return {};
    }
#endif
    return base + QLatin1Char('/') + legacyPrefDir + QLatin1Char('/') + legacyAppName + QStringLiteral(".cfg");
}

#ifdef Q_OS_WIN

// ---- Explorer sidebar entries of old-style clients ---------------------------------------------------
//
// Clients up to 4.x (v2.7.5, v3) registered one shell folder per sync folder: HKCU\Software\Classes\CLSID\{guid}
// plus a link in ...\Explorer\Desktop\NameSpace\{guid}, and marked the link with an "ApplicationName" value
// (the build-time APPLICATION_NAME). Clients from 5.0 on (v6) get their sidebar entry from the Windows VFS
// instead: a sync root in HKLM\...\SyncRootManager, and entries WITHOUT that marker. Nothing removes the
// old-style entries any more, so they would show up next to the VFS entry of the same folder.
//
// Safety rules, so that nothing v6 owns or needs is ever lost:
//   - only entries with the marker of an old-style client are candidates (v6's own entries have none),
//   - a candidate is removed only if it is redundant:
//       * v6 has a Windows VFS sync root for the same folder (the entry is a duplicate), or
//       * v6 does not sync that folder at all (the shortcut is unrelated to v6),
//   - it is kept if v6 syncs the folder without VFS (no VFS entry exists, the old one is the only shortcut),
//     and always when the folder is unknown or the config cannot be read,
//   - only registry keys are deleted: no files, no sync root, no account, no config value.
// Stateless and cheap, so it runs on every start. After the first start v6 may not have registered its sync
// root yet, a duplicate is then removed on the next start.

QString normalizedPath(const QString &path)
{
    QString p = QDir::fromNativeSeparators(path).trimmed().toLower();
    while (p.endsWith(QLatin1Char('/'))) {
        p.chop(1);
    }
    return p;
}

const wchar_t *wide(const QString &s)
{
    return reinterpret_cast<const wchar_t *>(s.utf16());
}

// 64-bit registry view
HKEY openKey(HKEY root, const QString &path, REGSAM access)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, wide(path), 0, access | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return nullptr;
    }
    return key;
}

QStringList subKeyNames(HKEY root, const QString &path)
{
    QStringList names;
    HKEY key = openKey(root, path, KEY_ENUMERATE_SUB_KEYS);
    if (!key) {
        return names;
    }
    wchar_t name[256];
    for (DWORD index = 0;; ++index) {
        DWORD length = 256;
        if (RegEnumKeyExW(key, index, name, &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
            break;
        }
        names << QString::fromWCharArray(name, static_cast<int>(length));
    }
    RegCloseKey(key);
    return names;
}

QStringList valueNames(HKEY root, const QString &path)
{
    QStringList names;
    HKEY key = openKey(root, path, KEY_QUERY_VALUE);
    if (!key) {
        return names;
    }
    wchar_t name[512];
    for (DWORD index = 0;; ++index) {
        DWORD length = 512;
        if (RegEnumValueW(key, index, name, &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
            break;
        }
        names << QString::fromWCharArray(name, static_cast<int>(length));
    }
    RegCloseKey(key);
    return names;
}

// empty valueName = the default value of the key
QString stringValue(HKEY root, const QString &keyPath, const QString &valueName)
{
    HKEY key = openKey(root, keyPath, KEY_QUERY_VALUE);
    if (!key) {
        return {};
    }
    const wchar_t *name = valueName.isEmpty() ? nullptr : wide(valueName);
    DWORD type = 0;
    DWORD size = 0;
    QString result;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) == ERROR_SUCCESS
        && (type == REG_SZ || type == REG_EXPAND_SZ)) {
        QByteArray buffer(static_cast<int>(size) + 2, '\0');
        if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size) == ERROR_SUCCESS) {
            result = QString::fromWCharArray(reinterpret_cast<const wchar_t *>(buffer.constData()));
        }
    }
    RegCloseKey(key);
    return result;
}

void deleteKeyTree(const QString &parentPath, const QString &subKey)
{
    HKEY parent = openKey(HKEY_CURRENT_USER, parentPath, KEY_READ | KEY_WRITE | DELETE);
    if (!parent) {
        return;
    }
    RegDeleteTreeW(parent, wide(subKey));
    RegCloseKey(parent);
}

void deleteValue(const QString &keyPath, const QString &valueName)
{
    HKEY key = openKey(HKEY_CURRENT_USER, keyPath, KEY_SET_VALUE);
    if (!key) {
        return;
    }
    RegDeleteValueW(key, wide(valueName));
    RegCloseKey(key);
}

// Folders for which this client registered a Windows VFS sync root:
// HKLM\...\SyncRootManager\<appName>!<SID>!<id>\UserSyncRoots (one value per user, the data is the folder).
QSet<QString> vfsSyncRootFolders()
{
    QSet<QString> folders;
    const QString manager = QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SyncRootManager");
    const QString prefix = QCoreApplication::applicationName() + QLatin1Char('!');
    const QStringList ids = subKeyNames(HKEY_LOCAL_MACHINE, manager);
    for (const QString &id : ids) {
        if (!id.startsWith(prefix, Qt::CaseInsensitive)) {
            continue;
        }
        const QString roots = manager + QLatin1Char('\\') + id + QStringLiteral("\\UserSyncRoots");
        const QStringList users = valueNames(HKEY_LOCAL_MACHINE, roots);
        for (const QString &user : users) {
            folders.insert(normalizedPath(stringValue(HKEY_LOCAL_MACHINE, roots, user)));
        }
    }
    return folders;
}

// Local folders of all accounts in the current config. ok is false if the config could not be read.
QSet<QString> configuredFolders(bool &ok)
{
    QSet<QString> folders;
    const QSettings settings(ConfigFile::configFile(), QSettings::IniFormat);
    ok = settings.status() == QSettings::NoError;
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        const QStringList parts = key.split(QLatin1Char('/'));
        if (parts.size() >= 5 && parts.value(0) == QLatin1String("Accounts") && parts.value(2) == QLatin1String("Folders")
            && parts.last() == QLatin1String("localPath")) {
            folders.insert(normalizedPath(settings.value(key).toString()));
        }
    }
    return folders;
}

void removeOldStyleSidebarEntries(MigrationLogs &logs)
{
    // markers written by old-style clients: the legacy app (v2.7.5) and this app's own name (v3/v4 share it with v6)
    const QStringList markers = {QStringLiteral(LEGACY_APPLICATION_NAME), QStringLiteral(APPLICATION_NAME)};
    const QString nameSpace = QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace");
    const QString nameSpace32 = QStringLiteral("Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace");
    const QString hideIcons = QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel");
    const QString clsid = QStringLiteral("Software\\Classes\\CLSID");
    const QString clsid32 = QStringLiteral("Software\\Classes\\Wow6432Node\\CLSID");

    QSet<QString> vfsFolders;
    QSet<QString> syncedFolders;
    bool configOk = false;
    bool loaded = false; // read the folders lazily, most starts have no old-style entry at all

    int removed = 0;
    const QStringList guids = subKeyNames(HKEY_CURRENT_USER, nameSpace);
    for (const QString &guid : guids) {
        const QString link = nameSpace + QLatin1Char('\\') + guid;
        const QString appName = stringValue(HKEY_CURRENT_USER, link, QStringLiteral("ApplicationName"));
        if (appName.isEmpty() || !markers.contains(appName, Qt::CaseInsensitive)) {
            continue; // not an old-style entry: VFS entries, other apps
        }
        const QString folder = stringValue(HKEY_CURRENT_USER, clsid + QLatin1Char('\\') + guid + QStringLiteral("\\Instance\\InitPropertyBag"),
            QStringLiteral("TargetFolderPath"));
        if (folder.isEmpty()) {
            continue; // nothing to relate it to, keep
        }
        if (!loaded) {
            vfsFolders = vfsSyncRootFolders();
            syncedFolders = configuredFolders(configOk);
            loaded = true;
        }

        const QString path = normalizedPath(folder);
        QString reason;
        if (vfsFolders.contains(path)) {
            reason = QStringLiteral("duplicate of the Windows VFS entry of the same folder");
        } else if (configOk && !syncedFolders.contains(path)) {
            reason = QStringLiteral("folder is not synced by this client");
        } else {
            continue; // synced without VFS (or config unreadable): the old entry is the only shortcut, keep
        }

        const QString title = stringValue(HKEY_CURRENT_USER, link, QString());
        deleteKeyTree(clsid, guid);
        deleteKeyTree(clsid32, guid);
        deleteKeyTree(nameSpace, guid);
        deleteKeyTree(nameSpace32, guid);
        deleteValue(hideIcons, guid);
        logs << QStringLiteral("removed old-style sidebar entry %1 (title \"%2\", folder \"%3\", ApplicationName \"%4\"): %5")
                    .arg(guid, title, folder, appName, reason);
        ++removed;
    }
    if (removed > 0) {
        // ask Explorer to refresh; if an entry is still shown, refreshing (F5) or restarting Explorer removes it
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
}

#endif // Q_OS_WIN

void migrateLegacySettings(MigrationLogs &logs)
{
    const QString legacyAppName = QStringLiteral(LEGACY_APPLICATION_SHORTNAME); // cfg file name, autostart registration
    const QString legacyPrefDir = QStringLiteral(LEGACY_APPLICATION_NAME); // config folder name of the legacy app

    const QString targetCfgPath = ConfigFile::configFile();
    QSettings newSettings(targetCfgPath, QSettings::IniFormat);
    if (newSettings.value(QStringLiteral("LegacyMigrated"), false).toBool()) {
        return; // already done, stay silent so normal starts do not create files
    }

    // No legacy config: the legacy client was never used by this user, or this is v3 -> v6
    // (same app name, the config is already in place). Nothing to do, stay silent.
    const QString oldCfgPath = legacyConfigPath(legacyAppName, legacyPrefDir);
    const QFileInfo oldInfo(oldCfgPath);
    if (oldCfgPath.isEmpty() || !oldInfo.isFile()) {
        return;
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

    // A target that already has accounts is in use (e.g. v3 -> v6 while an old v2.7.5 config is
    // still lying around): never overwrite it with older settings.
    for (const QString &key : newSettings.allKeys()) {
        if (key.startsWith(QLatin1String("Accounts/"))) {
            logs << QStringLiteral("RESULT skipped: target config already has accounts, nothing copied");
            markMigrated(newSettings);
            return;
        }
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
    if (QVersionNumber::commonPrefix(QVersionNumber::fromString(oldVersionString), legacyVersion) != legacyVersion) {
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

    // Copy everything and collect the log lines in one pass, grouped by account / folder / other.
    // Keys are sorted, so every group is sorted too. A replaced target value is shown as "(was ...)".
    QMap<QString, QStringList> accounts; // account id -> setting lines
    QMap<QString, QStringList> folders; // "account id/folder id" -> setting lines
    QStringList others;
    int replaced = 0;
    for (const QString &key : keys) {
        const QVariant value = oldSettings.value(key);

        const QStringList parts = key.split(QLatin1Char('/'));
        QStringList *group = &others;
        QString field = key;
        if (parts.value(0) == QLatin1String("Accounts") && parts.size() >= 3) {
            const QString accountId = parts.value(1);
            if (parts.value(2) == QLatin1String("Folders") && parts.size() >= 5) {
                group = &folders[accountId + QLatin1Char('/') + parts.value(3)];
                field = parts.mid(4).join(QLatin1Char('/'));
            } else {
                group = &accounts[accountId];
                field = parts.mid(2).join(QLatin1Char('/'));
            }
        }

        QString line = QStringLiteral("  %1 = %2").arg(field, migrationValue(key, value));
        if (newSettings.contains(key) && newSettings.value(key) != value) {
            line += QStringLiteral(" (was: %1)").arg(migrationValue(key, newSettings.value(key)));
            ++replaced;
        }
        *group << line;

        newSettings.setValue(key, value);
    }
    newSettings.sync();
    if (newSettings.status() != QSettings::NoError) {
        logs << QStringLiteral("RESULT failed: cannot write target config (status %1), will retry on next start")
                    .arg(static_cast<int>(newSettings.status()));
        return;
    }

    logs << QString();
    logs << QStringLiteral("copied %1 keys: %2 accounts, %3 folders, %4 other settings, %5 replaced existing values")
                .arg(keys.size()).arg(accounts.size()).arg(folders.size()).arg(others.size()).arg(replaced);
    addGroups(logs, QStringLiteral("accounts"), QStringLiteral("account"), accounts);
    addGroups(logs, QStringLiteral("folders"), QStringLiteral("folder"), folders);
    logs << QString() << QStringLiteral("=== other settings (%1) ===").arg(others.size());
    logs << others;

    // Check what really landed in the target file.
    const QSettings verify(targetCfgPath, QSettings::IniFormat);
    QStringList missing;
    for (const QString &key : keys) {
        if (!verify.contains(key)) {
            missing << key;
        }
    }
    logs << QString();
    logs << QStringLiteral("verify: %1 of %2 keys present in target config").arg(keys.size() - missing.size()).arg(keys.size());
    for (const QString &key : missing) {
        logs << QStringLiteral("  MISSING %1").arg(key);
    }

    // Autostart: the legacy client registered itself under its own name. Names are compared
    // case-insensitive (macOS file system, Windows registry): an equal name is the current app's
    // own entry and stays. macOS uses the short name, the Windows Run value can carry either one.
    QStringList legacyAutostartNames = {legacyAppName};
#ifdef Q_OS_WIN
    legacyAutostartNames << legacyPrefDir;
#endif
    legacyAutostartNames.removeDuplicates();
    for (const QString &name : legacyAutostartNames) {
        if (name.compare(Theme::instance()->appName(), Qt::CaseInsensitive) != 0) {
            OCC::Utility::setLaunchOnStartup(name, name, false);
            logs << QStringLiteral("removed legacy autostart entry \"%1\"").arg(name);
        } else {
            logs << QStringLiteral("legacy autostart entry \"%1\" kept, same name as current app").arg(name);
        }
    }

    markMigrated(newSettings);
    logs << QStringLiteral("RESULT done: LegacyMigrated set in target config");
}

} // namespace

#endif // LEGACY_MIGRATION

void runLegacySettingsMigration(const QString &optionLogDir, const QString &optionLogFile)
{
#ifdef LEGACY_MIGRATION
    // Best effort: every failure inside only ends up in the migration log, it never stops the start.
    // (No try/catch, the client is built with -fno-exceptions.)
    MigrationLogs logs;
    migrateLegacySettings(logs);
#ifdef Q_OS_WIN
    removeOldStyleSidebarEntries(logs);
#endif
    writeMigrationLogs(logs, optionLogDir, optionLogFile);
#else
    Q_UNUSED(optionLogDir)
    Q_UNUSED(optionLogFile)
#endif
}

} // namespace OCC