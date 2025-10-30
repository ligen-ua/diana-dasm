#include "orthia_model_modules.h"
#include "orthia_pe.h"
#include "diana_pe_cpp.h"
#include "orthia_streams.h"
#include "orthia_memory_cache.h"

namespace orthia
{
    oui::String CImportsLoader::NormalizeName(const std::string& dllName)
    {
        oui::String str = orthia::Utf8ToPlatformString(dllName);
        return NormalizeName(str);
    }
    oui::String CImportsLoader::NormalizeName(const oui::String& str)
    {
        int platformError = 0;
        oui::String normalName;
        std::tie(platformError, normalName) = m_pFs->SyncNormalizeName(str, true);
        if (platformError)
        {
            throw orthia::CWin32Exception("Can't normalize name: " + orthia::PlatformStringToUtf8(str.native), platformError);
        }
        return normalName;
    }
    oui::String CImportsLoader::LocateFile(const oui::String& dllName)
    {
        int platformError = 0;
        oui::String normalName;
        std::tie(platformError, normalName) = m_pFs->SyncLocateFile(dllName, m_dianaMode);
        if (platformError)
        {
            throw orthia::CWin32Exception("Can't locate name: " + orthia::PlatformStringToUtf8(dllName.native), platformError);
        }
        return normalName;
    }
    bool CImportsLoader::CheckConflicts(std::shared_ptr<orthia::CSimplePeFile> peFile)
    {
        auto modAddress = peFile->GetImageBase();
        auto modEnd = peFile->GetImageEnd();

        for (auto& mod : m_mappedModules)
        {
            auto curAddress = mod.second.peFile->GetImageBase();
            auto curEnd = mod.second.peFile->GetImageEnd();

            if (curAddress > modAddress &&
                curAddress < modEnd)
            {
                return true;
            }
            if (curEnd > modAddress && 
                curEnd < modEnd)
            {
                return true;
            }
            if (curAddress <= modAddress &&
                curEnd >= modEnd)
            {
                return true;
            }
            if (modAddress <= curAddress &&
                modEnd >= curEnd)
            {
                return true;
            }
        }
        return false;
    }
    void CImportsLoader::RelocateModule(std::shared_ptr<orthia::CSimplePeFile> peFile)
    {
        // check 
        OPERAND_SIZE lastPossibleAddress = DI_MAX_OPERAND_SIZE;
        switch (m_dianaMode)
        {
        case 4:
            lastPossibleAddress = std::numeric_limits<uint32_t>::max();
            break;
        case 2:
            lastPossibleAddress = std::numeric_limits<uint16_t>::max();
            break;
        }

        if (m_freeSpaceStart > lastPossibleAddress)
        {
            throw std::runtime_error("Can't load module");
        }

        auto freeSpaceSize = lastPossibleAddress - m_freeSpaceStart;
        if (freeSpaceSize < peFile->GetImageEnd())
        {
            throw std::runtime_error("Can't load module");
        }
    }
    CImportsLoader::ModuleIterator CImportsLoader::LoadModule(const std::string& dllName)
    {
        auto name = NormalizeName(dllName);
        {
            auto it = m_mappedModules.find(name.native);
            if (it != m_mappedModules.end())
            {
                return it;
            }
        }
        auto fullName = LocateFile(name);

        int platformError = 0;
        std::shared_ptr<oui::IFile2> file;
        std::tie(platformError, file) = m_pFs->SyncOpenFile(oui::FileUnifiedId(fullName));
        if (platformError)
        {
            throw orthia::CWin32Exception("Can't open file: " + orthia::PlatformStringToUtf8(fullName.native), platformError);
        }

        std::vector<char> binPeFile;
        oui::String error = ReadFileToVector(file, binPeFile);
        if (!error.native.empty())
        {
            throw orthia::CWin32Exception("Can't read file: " + orthia::PlatformStringToUtf8(fullName.native) + "\n" + orthia::PlatformStringToUtf8(error.native), platformError);
        }

        auto mappedPE = std::make_shared<orthia::CSimplePeFile>();
        orthia::MapFileParameters params;
        mappedPE->MapFile(binPeFile, params);

        if (CheckConflicts(mappedPE))
        {
            RelocateModule(mappedPE);
        }
        ModuleInfo info;
        info.peFile = mappedPE;
        info.fullName = fullName;
        if (m_freeSpaceStart < mappedPE->GetImageEnd())
        {
            m_freeSpaceStart = mappedPE->GetImageEnd();
        }
        return m_mappedModules.insert({ name.native, info }).first;
    }

    void CImportsLoader::QueryFunctionByOrdinal(const char* pDllName,
        DI_UINT32 ordinal,
        OPERAND_SIZE* pAddress)
    {
        QueryFunctionImpl(pDllName, "", ordinal, pAddress);
    }
    void CImportsLoader::QueryFunctionByName(const char* pDllName,
        const char* pFunctionName,
        DI_UINT32 hint,
        OPERAND_SIZE* pAddress)
    {
        QueryFunctionImpl(pDllName, pFunctionName, DI_MAX_OPERAND_SIZE, pAddress);
    }
    void CImportsLoader::QueryFunctionImpl(const char* pDllName,
        const char* pFunctionName,
        OPERAND_SIZE ordinalIn,
        OPERAND_SIZE* pAddress)
    {
        try
        {
            if (!pDllName || !pFunctionName || !pAddress)
            {
                throw std::runtime_error("Invalid argument");
            }

            std::string dllName(pDllName);
            std::string functionName(pFunctionName);
            OPERAND_SIZE ordinal = ordinalIn;

            const int maxTryCount = 3;
            for (int i = 0; i < maxTryCount; ++i)
            {
                ModuleIterator it = LoadModule(dllName);

                auto ordinalToPass = ordinal;
                if (ordinalToPass == DI_MAX_OPERAND_SIZE)
                {
                    ordinalToPass = DIANA_PE_INVALID_ORDINAL_VALUE;
                }

                OPERAND_SIZE forwardOffset = 0;
                *pAddress = it->second.peFile->DiGetProcAddress(functionName.c_str(), &forwardOffset, (DI_UINT16)ordinalToPass);

                if (!forwardOffset)
                {
                    return;
                }

                auto fwString = it->second.peFile->DiReadForwardingString(forwardOffset);
                DI_CHECK_CPP(diana::ParseForwarderString(fwString, dllName, functionName, ordinal));
            }
            throw std::runtime_error("Can't process forwarding");
        }
        catch (const std::exception& e)
        {
            oui::LogOutput(oui::LogFlags::Error, e.what());
        }
    }

    // CImportsLoader
    void CImportsLoader::LoadModules(const oui::String & fileName, 
        std::shared_ptr<orthia::CSimplePeFile> peFile,
        std::shared_ptr<oui::IFileSystem> pFs)
    {
        m_pFs = pFs;
        if (!m_pFs)
        {
            throw std::runtime_error("Unknown filesystem");
        }
        m_dianaMode = peFile->GetImpl()->mappedPE.pImpl->dianaMode;

        oui::String shortFileName;
        orthia::UnparseFileNameFromFullFileName(fileName.native, &shortFileName.native);
        ModuleInfo info;
        info.peFile = peFile;
        info.originalFile = true;
        info.fullName = fileName;
        m_mappedModules[NormalizeName(shortFileName).native] = info;
    
        auto imageBase = peFile->GetImageBase();

        orthia::CReaderOverVector reader(imageBase, peFile->GetMappedPeFile());
        orthia::CMemoryStorageOfModifiedData mappedFile(&reader);
        orthia::DianaAnalyzerReadWriteStream writeStream(&mappedFile);

        std::vector<char> page(4096);
        DI_CHECK_CPP(DianaPeFile_LinkImports(&peFile->GetImpl()->mappedPE,
            imageBase,
            &writeStream,
            &page.front(),
            (ULONG)page.size(),
            GetParent()));
    }
}
