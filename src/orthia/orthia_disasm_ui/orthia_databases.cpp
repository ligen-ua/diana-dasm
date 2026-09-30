#include "orthia_databases.h"
#include "orthia_config.h"
#include "orthia_model_interfaces.h"
#include "orthia_common_format.h"
#include "oui_filesystem.h"
#include "oui_processes.h"
#include "orthia_sqlite.h"
#include "sqlite3.h"
#include <algorithm>
#include <cerrno>
#include <fstream>
#include <set>

namespace fs = std::filesystem;

namespace orthia
{
    // appended to a folder being deleted, see DeleteDatabaseFolder
    static const PlatformString_type g_pendingDeleteSuffix = ORTHIA_TCSTR(".deleting");

    static bool EndsWith(const PlatformString_type& text, const PlatformString_type& suffix)
    {
        return text.size() >= suffix.size() &&
            text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
    static bool IsHexString(const PlatformString_type& text)
    {
        for (auto ch : text)
        {
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')))
            {
                return false;
            }
        }
        return !text.empty();
    }
    static bool IsDecimalString(const PlatformString_type& text)
    {
        for (auto ch : text)
        {
            if (ch < '0' || ch > '9')
            {
                return false;
            }
        }
        return !text.empty();
    }
    static PlatformString_type ToLowerAscii(PlatformString_type text)
    {
        for (auto& ch : text)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                ch = (PlatformString_type::value_type)(ch - 'A' + 'a');
            }
        }
        return text;
    }

    unsigned long long ParseProcFolderPid(const PlatformString_type& folderName)
    {
        // the name is SyncSanitizeName(IProcess::GetFullFileNameForUI()): "[<pid>] <name>"
        if (folderName.size() < 3 || folderName[0] != '[')
        {
            return 0;
        }
        auto end = folderName.find(']');
        if (end == PlatformString_type::npos)
        {
            return 0;
        }
        auto digits = folderName.substr(1, end - 1);
        if (!IsDecimalString(digits) || digits.size() > 19)
        {
            return 0;
        }
        unsigned long long pid = 0;
        for (auto ch : digits)
        {
            pid = pid * 10 + (unsigned long long)(ch - '0');
        }
        return pid;
    }

    // DIRINFO: a header line, then 'Original Name: "<path>"'
    static PlatformString_type ReadOriginalName(const fs::path& readmeFile)
    {
        std::ifstream stream(readmeFile, std::ios::binary);
        if (!stream)
        {
            return PlatformString_type();
        }
        std::string line;
        std::getline(stream, line);
        if (!std::getline(stream, line))
        {
            return PlatformString_type();
        }
        auto first = line.find('"');
        auto last = line.rfind('"');
        if (first != std::string::npos && last != first)
        {
            line = line.substr(first + 1, last - first - 1);
        }
        return Utf8ToPlatformString(line);
    }

    static DatabaseInfo ReadFolderInfo(const CConfigOptionsStorage* config,
        const fs::directory_entry& entry,
        const PlatformString_type& parentWithSlash,
        DatabaseInfo::Kind kind)
    {
        DatabaseInfo info;
        info.kind = kind;
        info.folderName = entry.path().filename().native();
        info.folder = parentWithSlash + info.folderName;
        info.pendingDelete = EndsWith(info.folderName, g_pendingDeleteSuffix);
        if (kind == DatabaseInfo::Kind::Process)
        {
            info.pid = ParseProcFolderPid(info.folderName);
        }

        const PlatformString_type dbFileName = config ? config->GetDBFileName() : ORTHIA_TCSTR("data.db");
        const PlatformString_type readmeFileName = config ? config->GetReadmeFileName() : ORTHIA_TCSTR("DIRINFO");

        std::error_code ec;
        for (const auto& fileEntry : fs::recursive_directory_iterator(entry.path(), ec))
        {
            std::error_code ec2;
            if (fileEntry.is_regular_file(ec2))
            {
                auto size = fileEntry.file_size(ec2);
                if (!ec2)
                {
                    info.sizeBytes += size;
                }
            }
            auto wt = fs::last_write_time(fileEntry, ec2);
            if (!ec2 && wt > info.lastWrite)
            {
                info.lastWrite = wt;
            }
        }
        if (info.lastWrite == fs::file_time_type::min())
        {
            std::error_code ec2;
            auto wt = fs::last_write_time(entry, ec2);
            if (!ec2)
            {
                info.lastWrite = wt;
            }
        }
        std::error_code ec3;
        info.hasDataDb = fs::is_regular_file(entry.path() / dbFileName, ec3);
        info.originalName = ReadOriginalName(entry.path() / readmeFileName);
        return info;
    }

    static void EnumerateFolder(const CConfigOptionsStorage* config,
        const PlatformString_type& parentWithSlash,
        DatabaseInfo::Kind kind,
        std::vector<DatabaseInfo>& result)
    {
        std::vector<DatabaseInfo> found;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(fs::path(parentWithSlash), ec))
        {
            std::error_code ec2;
            if (!entry.is_directory(ec2))
            {
                continue;
            }
            found.push_back(ReadFolderInfo(config, entry, parentWithSlash, kind));
        }
        std::sort(found.begin(), found.end(), [](const DatabaseInfo& a, const DatabaseInfo& b) {
            return a.folderName < b.folderName;
        });
        result.insert(result.end(), found.begin(), found.end());
    }

    std::vector<DatabaseInfo> EnumerateDatabases(const CConfigOptionsStorage& config)
    {
        std::vector<DatabaseInfo> result;
        EnumerateFolder(&config, config.GetDBFolder(), DatabaseInfo::Kind::File, result);
        EnumerateFolder(&config, config.GetProcDBFolder(), DatabaseInfo::Kind::Process, result);
        return result;
    }
    std::vector<DatabaseInfo> EnumerateProcDatabases(const PlatformString_type& procFolderWithSlash)
    {
        std::vector<DatabaseInfo> result;
        EnumerateFolder(nullptr, procFolderWithSlash, DatabaseInfo::Kind::Process, result);
        return result;
    }

    DatabaseDetails QueryDatabaseDetails(const CConfigOptionsStorage& config, const DatabaseInfo& info)
    {
        DatabaseDetails details;
        if (!info.hasDataDb)
        {
            return details;
        }
        auto dbFileName = AddSlash2(info.folder) + config.GetDBFileName();

        CSQLDatabase database;
        // read-only: never migrates or creates anything, and works next to another instance
        int rc = sqlite3_open_v2(PlatformStringToUtf8(dbFileName).c_str(), database.Get2(), SQLITE_OPEN_READONLY, nullptr);
        if (rc != SQLITE_OK)
        {
            return details;
        }
        sqlite3_busy_timeout(database.Get(), 500);

        {
            CSQLStatement statement;
            if (sqlite3_prepare_v2(database.Get(), "SELECT count(*) FROM tbl_comments", -1, statement.Get2(), nullptr) == SQLITE_OK &&
                sqlite3_step(statement.Get()) == SQLITE_ROW)
            {
                details.comments = sqlite3_column_int64(statement.Get(), 0);
                details.commentsKnown = true;
            }
        }
        {
            // the main module is the lowest address: dependencies are placed after it
            CSQLStatement statement;
            if (sqlite3_prepare_v2(database.Get(), "SELECT meta_mod_id, meta_info FROM tbl_metainfo WHERE meta_type = 1", -1, statement.Get2(), nullptr) == SQLITE_OK)
            {
                bool found = false;
                unsigned long long mainAddress = 0;
                std::string mainInfo;
                while (sqlite3_step(statement.Get()) == SQLITE_ROW)
                {
                    auto address = (unsigned long long)sqlite3_column_int64(statement.Get(), 0);
                    if (found && address >= mainAddress)
                    {
                        continue;
                    }
                    auto text = (const char*)sqlite3_column_text(statement.Get(), 1);
                    found = true;
                    mainAddress = address;
                    mainInfo = text ? text : "";
                }
                CCommonFormatParser parser;
                if (found && parser.Parse(mainInfo))
                {
                    int flags = 0;
                    parser.QueryMetadata("flags", &flags);
                    details.mainModuleFlags = flags;
                    details.mainModuleKnown = true;
                }
            }
        }
        return details;
    }

    static PlatformString_type DescribeMatches(const std::vector<const DatabaseInfo*>& matches)
    {
        PlatformString_type text;
        for (auto* info : matches)
        {
            if (!text.empty())
            {
                text += ORTHIA_TCSTR(", ");
            }
            text += info->folderName;
        }
        return text;
    }

    std::vector<DatabaseInfo> ResolveDatabaseSelectors(const std::vector<DatabaseInfo>& all,
        const std::vector<PlatformString_type>& selectors,
        HashFileHandler_type hashFile)
    {
        const size_t minPrefixSize = 6;
        std::vector<DatabaseInfo> result;
        std::set<PlatformString_type> added;
        for (const auto& selector : selectors)
        {
            std::vector<const DatabaseInfo*> matches;
            if (IsHexString(selector))
            {
                // a number may be a pid, a sha1 prefix, or both
                const auto lowered = ToLowerAscii(selector);
                const bool isPid = IsDecimalString(selector);
                const auto pid = isPid ? ParseProcFolderPid(ORTHIA_TCSTR("[") + selector + ORTHIA_TCSTR("]")) : 0;
                for (const auto& info : all)
                {
                    if (info.pendingDelete)
                    {
                        continue;
                    }
                    if (info.kind == DatabaseInfo::Kind::File)
                    {
                        if (selector.size() >= minPrefixSize &&
                            ToLowerAscii(info.folderName).compare(0, lowered.size(), lowered) == 0)
                        {
                            matches.push_back(&info);
                        }
                    }
                    else if (pid && info.pid == pid)
                    {
                        matches.push_back(&info);
                    }
                }
                if (matches.empty() && !isPid && selector.size() < minPrefixSize)
                {
                    throw std::runtime_error("A database prefix needs at least 6 hex digits: " + PlatformStringToUtf8(selector));
                }
            }
            else
            {
                const auto sha1 = ToLowerAscii(hashFile(selector));
                for (const auto& info : all)
                {
                    if (!info.pendingDelete && info.kind == DatabaseInfo::Kind::File && ToLowerAscii(info.folderName) == sha1)
                    {
                        matches.push_back(&info);
                    }
                }
                if (matches.empty())
                {
                    throw std::runtime_error("No database for file: " + PlatformStringToUtf8(selector) +
                        " (sha1 " + PlatformStringToUtf8(sha1) + ")");
                }
            }
            if (matches.empty())
            {
                throw std::runtime_error("No database matches: " + PlatformStringToUtf8(selector));
            }
            if (matches.size() > 1)
            {
                throw std::runtime_error("Ambiguous: " + PlatformStringToUtf8(selector) +
                    " matches " + PlatformStringToUtf8(DescribeMatches(matches)));
            }
            if (added.insert(matches[0]->folder).second)
            {
                result.push_back(*matches[0]);
            }
        }
        return result;
    }

    void DeleteDatabaseFolder(const DatabaseInfo& info)
    {
        fs::path folder(info.folder);
        fs::path target = folder;
        if (!info.pendingDelete)
        {
            // Windows refuses to rename a folder with an open file inside, so a database in use by
            // another instance fails here, before anything is removed
            target += g_pendingDeleteSuffix;
            std::error_code ec;
            fs::rename(folder, target, ec);
            if (ec)
            {
                throw std::runtime_error("Can't delete " + PlatformStringToUtf8(info.folderName) +
                    ", it may be open in another Orthia: " + ec.message());
            }
        }
        std::error_code ec;
        fs::remove_all(target, ec);
        if (ec)
        {
            throw std::runtime_error("Can't delete " + PlatformStringToUtf8(info.folderName) + ": " + ec.message());
        }
    }

    static bool IsOlderThan(const DatabaseInfo& info, std::filesystem::file_time_type now, std::chrono::hours age)
    {
        if (info.lastWrite == fs::file_time_type::min())
        {
            return false;
        }
        return info.lastWrite < now && (now - info.lastWrite) > age;
    }

    std::vector<CleanupEntry> SelectDatabasesForCleanup(const std::vector<DatabaseInfo>& all,
        const std::map<PlatformString_type, PlatformString_type>& openFolders,
        std::filesystem::file_time_type now,
        QueryProcessStateHandler_type queryProcessState)
    {
        std::vector<CleanupEntry> result;
        for (const auto& info : all)
        {
            if (openFolders.count(info.folder))
            {
                continue;
            }
            if (info.pendingDelete)
            {
                result.push_back({ info, ORTHIA_TCSTR("unfinished delete") });
                continue;
            }
            if (info.kind == DatabaseInfo::Kind::File)
            {
                if (!info.hasDataDb && IsOlderThan(info, now, g_brokenFolderGracePeriod))
                {
                    result.push_back({ info, ORTHIA_TCSTR("no data.db") });
                }
                continue;
            }
            if (IsOlderThan(info, now, g_procFolderExpiration))
            {
                result.push_back({ info, ORTHIA_TCSTR("not written for 48 hours") });
                continue;
            }
            if (!info.pid || !queryProcessState)
            {
                continue;
            }
            switch (queryProcessState(info))
            {
            case ProcessState::Gone:
                result.push_back({ info, ORTHIA_TCSTR("process has exited") });
                break;
            case ProcessState::Reused:
                result.push_back({ info, ORTHIA_TCSTR("pid now belongs to another program") });
                break;
            default:
                break;
            }
        }
        return result;
    }

    static bool IsNoSuchProcessError(int error)
    {
#ifdef DIANA_HAS_WIN32
        // OpenProcess on a pid that does not exist
        return error == ERROR_INVALID_PARAMETER;
#else
        return error == ENOENT || error == ESRCH;
#endif
    }

    ProcessState QueryProcessState(oui::IProcessSystem& processSystem,
        oui::IFileSystem& fileSystem,
        const DatabaseInfo& info)
    {
        if (!info.pid)
        {
            return ProcessState::Unknown;
        }
        int error = 0;
        std::shared_ptr<oui::IProcess> process;
        std::tie(error, process) = processSystem.SyncOpenProcess(oui::ProcessUnifiedId(info.pid));
        if (!process)
        {
            return IsNoSuchProcessError(error) ? ProcessState::Gone : ProcessState::Unknown;
        }
        // the folder was named the same way when the process was opened
        auto name = fileSystem.SyncSanitizeName(process->GetFullFileNameForUI());
        return name.native == info.folderName ? ProcessState::Alive : ProcessState::Reused;
    }

    void CleanupExpiredProcFolders(const PlatformString_type& procFolderWithSlash)
    {
        const auto now = fs::file_time_type::clock::now();
        for (const auto& info : EnumerateProcDatabases(procFolderWithSlash))
        {
            if (info.pendingDelete || IsOlderThan(info, now, g_procFolderExpiration))
            {
                try
                {
                    DeleteDatabaseFolder(info);
                }
                catch (std::exception&)
                {
                    // in use, try again at the next start
                }
            }
        }
    }
}
