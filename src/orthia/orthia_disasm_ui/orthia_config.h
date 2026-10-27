#pragma once

#include "orthia_utils.h"

namespace orthia
{
    // "~" and "~/..." against the given home folder (Linux; unchanged on Windows or without a home)
    PlatformString_type ExpandHomeFolder(const PlatformString_type& path, const PlatformString_type& home);

    class CConfigOptionsStorage
    {
        PlatformString_type m_appDir;
        PlatformString_type m_dbDir;
        PlatformString_type m_binDir;
        PlatformString_type m_procDBDir;

        std::vector<PlatformString_type> m_symbolFolders;
        // false: open targets without code analysis and symbol loading (--cmd without --analyze);
        // set once before anything is opened
        bool m_deepAnalysis = true;
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
        bool GetDeepAnalysis() const { return m_deepAnalysis; }
        void SetDeepAnalysis(bool value) { m_deepAnalysis = value; }
        bool GetOverwriteDatabases() const { return m_overwriteDatabases; }
        void SetOverwriteDatabases(bool value) { m_overwriteDatabases = value; }
        // --capture: each analyzed module's memory into the process folder
        bool GetCaptureModules() const { return m_captureModules; }
        void SetCaptureModules(bool value) { m_captureModules = value; }
    };
}