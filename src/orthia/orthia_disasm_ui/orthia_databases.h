#pragma once

#include "orthia_utils.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>

namespace oui
{
    struct IFileSystem;
    struct IProcessSystem;
}
namespace orthia
{
    class CConfigOptionsStorage;

    // One folder of the data folder: db/<sha1> for a file, proc/[pid] name for a process.
    struct DatabaseInfo
    {
        enum class Kind { File, Process };
        Kind kind = Kind::File;
        PlatformString_type folder;         // full path, no trailing slash
        PlatformString_type folderName;     // sha1 or "[pid] name"
        PlatformString_type originalName;   // from DIRINFO; empty when missing
        unsigned long long sizeBytes = 0;
        // the newest write time of the files inside, or of the folder itself when it is empty
        std::filesystem::file_time_type lastWrite = std::filesystem::file_time_type::min();
        unsigned long long pid = 0;         // processes only; 0 when the name does not parse
        bool hasDataDb = false;
        bool pendingDelete = false;         // left by a delete that failed half way
    };

    // What data.db says; read with a read-only connection, so unknown fields stay unset.
    struct DatabaseDetails
    {
        bool mainModuleKnown = false;
        int mainModuleFlags = 0;            // ModuleInfo::flags_*
        bool commentsKnown = false;
        long long comments = 0;
    };

    enum class ProcessState
    {
        Alive,      // the pid runs the same program the folder was made for
        Gone,       // no such process
        Reused,     // the pid runs another program
        Unknown     // no access, or the provider gave an unexpected error
    };

    struct CleanupEntry
    {
        DatabaseInfo info;
        PlatformString_type reason;
    };

    // a proc/ folder not written for this long is removed at start and by cleanup
    const std::chrono::hours g_procFolderExpiration(48);
    // a db/ folder without data.db is only removed when it is this old: it may be being created
    const std::chrono::hours g_brokenFolderGracePeriod(1);

    // "[1234] notepad.exe" -> 1234; 0 when the name has no pid prefix
    unsigned long long ParseProcFolderPid(const PlatformString_type& folderName);

    std::vector<DatabaseInfo> EnumerateDatabases(const CConfigOptionsStorage& config);
    std::vector<DatabaseInfo> EnumerateProcDatabases(const PlatformString_type& procFolderWithSlash);
    DatabaseDetails QueryDatabaseDetails(const CConfigOptionsStorage& config, const DatabaseInfo& info);

    // Selectors: a sha1 prefix (6+ hex digits), a pid (decimal) or a file name (anything else,
    // hashed by hashFile). Throws when a selector matches nothing or more than one database.
    using HashFileHandler_type = std::function<PlatformString_type(const PlatformString_type& fileName)>;
    std::vector<DatabaseInfo> ResolveDatabaseSelectors(const std::vector<DatabaseInfo>& all,
        const std::vector<PlatformString_type>& selectors,
        HashFileHandler_type hashFile);

    // Renames the folder first, so a folder in use (Windows) is left untouched. Throws on failure.
    void DeleteDatabaseFolder(const DatabaseInfo& info);

    using QueryProcessStateHandler_type = std::function<ProcessState(const DatabaseInfo& info)>;
    // openFolders: folders of the items open in this instance, never selected
    std::vector<CleanupEntry> SelectDatabasesForCleanup(const std::vector<DatabaseInfo>& all,
        const std::map<PlatformString_type, PlatformString_type>& openFolders,
        std::filesystem::file_time_type now,
        QueryProcessStateHandler_type queryProcessState);

    // Asks the process provider whether the process of a proc/ folder still runs.
    ProcessState QueryProcessState(oui::IProcessSystem& processSystem,
        oui::IFileSystem& fileSystem,
        const DatabaseInfo& info);

    // At start: removes the proc/ folders not written for g_procFolderExpiration.
    void CleanupExpiredProcFolders(const PlatformString_type& procFolderWithSlash);
}
