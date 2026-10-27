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

    // meta.xml of a db/ or proc/ folder: what the folder holds and which Orthia wrote it.
    // Folders made before it existed have none: formatVersion 0.
    struct FolderMeta
    {
        int formatVersion = 0;              // g_folderFormatVersion of the writer
        int minReaderVersion = 0;           // the lowest g_folderFormatVersion that may reuse the folder
        int analysisVersion = 0;            // g_analysisVersion of the writer: recorded, nothing depends on it
        std::string orthiaVersion;          // ORTHIA_UI_VERSION of the writer
        long long createdUnix = 0;          // UTC seconds since 1970
        std::string kind;                   // g_folderKindFile or g_folderKindProcess
        int dianaMode = 0;                  // the pointer size, DIANA_MODE*
        int dianaArch = 0;                  // DIANA_ARCH_*

        // files
        std::string format;                 // IImageFormat::GetId()
        unsigned long long imageBase = 0;
        unsigned long long size = 0;        // of the file
        std::string sha1;

        // processes
        unsigned long long pid = 0;
        PlatformString_type name;           // "[pid] name"
        long long startUnix = 0;            // see oui::ProcessStartInfo
        std::string startKey;
    };

    // the layout of a folder: data.db, target.bin, DIRINFO, parameters.xml, meta.xml.
    // Raise it when a folder made by this version can't be used by an older one, and raise
    // g_folderMinReaderVersion with it
    const int g_folderFormatVersion = 1;
    const int g_folderMinReaderVersion = 1;
    // raise when the analysis finds more in the same image; recorded only
    const int g_analysisVersion = 1;
    extern const char* const g_folderKindFile;
    extern const char* const g_folderKindProcess;

    // false when the file is missing or is not a meta.xml
    bool ReadFolderMeta(const PlatformString_type& fileName, FolderMeta& meta);
    // fills the writer fields (versions, created) when they are 0; throws on failure
    void WriteFolderMeta(const PlatformString_type& fileName, FolderMeta meta);
    // false for a folder written by a newer Orthia that this one must not reuse
    bool IsFolderReadable(const FolderMeta& meta);

    // "20261021-153012" in UTC; empty for 0
    PlatformString_type FormatProcStartStamp(long long unixSeconds);
    // the proc/ folder name of a process before sanitizing: "[pid] name 20261021-153012", or
    // "[pid] name" when the start is unknown
    PlatformString_type MakeProcFolderName(const PlatformString_type& processName, long long startUnix);

    // One folder of the data folder: db/<sha1> for a file, proc/[pid] name <start> for a process.
    struct DatabaseInfo
    {
        enum class Kind { File, Process };
        Kind kind = Kind::File;
        PlatformString_type folder;         // full path, no trailing slash
        PlatformString_type folderName;     // sha1, "[pid] name <start>" or (before meta.xml) "[pid] name"
        PlatformString_type originalName;   // from DIRINFO; empty when missing
        unsigned long long sizeBytes = 0;
        // the newest write time of the files inside, or of the folder itself when it is empty
        std::filesystem::file_time_type lastWrite = std::filesystem::file_time_type::min();
        unsigned long long pid = 0;         // processes only; 0 when the name does not parse
        std::string startKey;               // processes only, from meta.xml; empty when unknown
        bool hasDataDb = false;
        bool pendingDelete = false;         // left by a delete that failed half way
        // processes only: snapshots of floating code, evidence. No cleanup removes it, .database delete does
        bool hasFloatingSnapshots = false;
        // processes only: a .capture or --capture, evidence as the snapshots are
        bool hasCapture = false;
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
        Alive,      // the pid runs the same process the folder was made for
        Gone,       // no such process
        Reused,     // the pid runs another process (another start, or before meta.xml another program)
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

    // "[1234] notepad.exe 20261021-153012" -> 1234; 0 when the name has no pid prefix
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
