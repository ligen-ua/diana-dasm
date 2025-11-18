#include "orthia_plugins_win32.h"
#include "orthia_log.h"

namespace orthia
{
    CWin32OpenProcessPlugin::CWin32OpenProcessPlugin()
    {
    }
    CWin32OpenProcessPlugin::~CWin32OpenProcessPlugin()
    {
        if (m_pluginUninit)
        {
            m_pluginUninit();
            m_pluginUninit = 0;
        }
    }
    OpenProcess_type CWin32OpenProcessPlugin::GetOpenProcess()
    {
        return m_openProcess;
    }
    bool CWin32OpenProcessPlugin::Load()
    {
        if (m_openProcess)
        {
            return true;
        }
        auto pluginFileName = orthia::GetCurrentModuleDir();
        pluginFileName += L"orthia_proc_win32.dll";
        int error = m_plugin.Reset_Silent(pluginFileName.c_str());
        if (error)
        {
            return false;
        }
        std::string orthiaOpenProcessName("OrthiaOpenProcess");
        std::string orthiaInitName("OrthiaInit");
        std::string orthiaUninitName("OrthiaUninit");

        OpenProcess_type orthiaOpenProcess = 0;
        m_plugin.QueryFunction(orthiaOpenProcessName.c_str(), &orthiaOpenProcess, true);
        m_plugin.QueryFunction(orthiaInitName.c_str(), &m_pluginInit, true);
        m_plugin.QueryFunction(orthiaUninitName.c_str(), &m_pluginUninit, true);

        int errorCode = 0;
        if (m_pluginInit)
        {
            errorCode = m_pluginInit();
            ORTHIA_LOG(orthia::LogSeverity::Info, "PluginLoader: Plugin loaded: ", pluginFileName);
        }
        if (errorCode || !orthiaOpenProcess || !m_pluginInit || !m_pluginUninit)
        {
            if (errorCode)
            {
                ORTHIA_LOG(orthia::LogSeverity::Error, "PluginLoader: Can't init plugin, code = ", orthia::ObjectToString_Ansi(errorCode));
            }
            if (!orthiaOpenProcess)
            {
                ORTHIA_LOG(orthia::LogSeverity::Error, "PluginLoader: Function not found: ", orthiaOpenProcessName);
            }
            if (!m_pluginInit)
            {
                ORTHIA_LOG(orthia::LogSeverity::Error, "PluginLoader: Function not found: ", orthiaInitName);
            }
            if (!m_pluginUninit)
            {
                ORTHIA_LOG(orthia::LogSeverity::Error, "PluginLoader: Function not found: " + orthiaUninitName);
            }
            m_pluginInit = 0;
            m_pluginUninit = 0;
            m_plugin.Reset(0);
            return false;
        }

        m_openProcess = orthiaOpenProcess;
        return true;
    }
}

