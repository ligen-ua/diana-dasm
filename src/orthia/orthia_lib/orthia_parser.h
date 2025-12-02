#pragma once

#include "orthia_tokenizer.h"
#include <functional>

namespace orthia
{
struct NoCmdException :public std::runtime_error
{
    NoCmdException()
        :
        std::runtime_error("Command expected")
    {
    }
};
struct CmdNameNotFoundException :public std::runtime_error
{
    CmdNameNotFoundException(const orthia::PlatformString_type& name)
        :
        std::runtime_error("Command not found: " + orthia::PlatformStringToUtf8(name))
    {
    }
};
class CCommandParser
{
    CTokenizerEnv m_tokenizer;

public:
    using CmdHandler_type = std::function<void(CCommandParser& parser)>;
protected:
    std::unordered_map< orthia::PlatformString_type, CmdHandler_type> m_handlers;
    std::function<void()> m_emptyHandler;
public:
    
    CCommandParser();
    CTokenizerEnv& GetTokenizer() { return m_tokenizer; }
    void SetEmptyHandler(std::function<void()> && emptyCmdHandler);
    void SetHandler(const orthia::PlatformString_type& cmdName, CmdHandler_type&& handler);
    void Parse(const orthia::PlatformString_type& text);
};

}