#pragma once

#include "orthia_utils.h"

namespace orthia
{
    // "~" and "~/..." against the given home folder (Linux; unchanged on Windows or without a home)
    PlatformString_type ExpandHomeFolder(const PlatformString_type& path, const PlatformString_type& home);

    // the long tasks started after a target opens: the code analysis of the main module, and
    // the symbols (then the analysis again with the private ones, when both are on)
    struct AutoTasks
    {
        bool analyze = true;
        bool symbols = true;
    };
    enum class AutoTasksPolicy
    {
        Fixed,      // start `tasks`
        AskIfNew    // a new database: start nothing, the UI asks; else what was chosen when it was made
    };
    struct OpenOptions
    {
        AutoTasksPolicy policy = AutoTasksPolicy::Fixed;
        AutoTasks tasks;
    };

    class CConfigOptionsStorage
    {
        PlatformString_type m_appDir;
        PlatformString_type m_dbDir;
        PlatformString_type m_binDir;
        PlatformString_type m_procDBDir;

        std::vector<PlatformString_type> m_symbolFolders;
        // --no-analysis, --no-symbols; set once before anything is opened
        AutoTasks m_cliAutoTasks;
        // one of them given: the UI opens the command line targets with them, without asking
        bool m_cliAutoTasksExplicit = false;
        // true: every target opened in this run gets a new, empty database (--overwrite-db);
        // false: the database of an earlier open is reused when it fits
        bool m_overwriteDatabases = false;
        bool m_captureModules = false;

    public:
        void Init();
        PlatformString_type GetBinFileName() const;
        PlatformString_type GetDBFileName() const;
        PlatformString_type GetDBFolder() const;
        PlatformString_type GetReadmeFileName() const;
        PlatformString_type GetParamsFileName() const;
        PlatformString_type GetMetaFileName() const;
        PlatformString_type GetDataFolder() const;
        PlatformString_type GetBinFolder() const;
        PlatformString_type GetProcDBFolder() const;
        std::vector<PlatformString_type> GetSymbolsFolders() const;
        void SetSymbolsFolders(const PlatformString_type& names);
        AutoTasks GetCliAutoTasks() const { return m_cliAutoTasks; }
        bool IsCliAutoTasksExplicit() const { return m_cliAutoTasksExplicit; }
        void SetCliAutoTasks(AutoTasks value, bool isExplicit) { m_cliAutoTasks = value; m_cliAutoTasksExplicit = isExplicit; }
        bool GetOverwriteDatabases() const { return m_overwriteDatabases; }
        void SetOverwriteDatabases(bool value) { m_overwriteDatabases = value; }
        // --capture: each analyzed module's memory into the process folder
        bool GetCaptureModules() const { return m_captureModules; }
        void SetCaptureModules(bool value) { m_captureModules = value; }
    };
}