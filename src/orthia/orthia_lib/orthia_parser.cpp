#include "orthia_parser.h"

namespace orthia
{

CCommandParser::CCommandParser()
{

}
void CCommandParser::SetHandler(const orthia::PlatformString_type& cmdName, CmdHandler_type&& handler)
{
    m_handlers.insert({ cmdName, std::move(handler)});
}
void CCommandParser::SetEmptyHandler(std::function<void()> && emptyCmdHandler)
{
    m_emptyHandler = std::move(emptyCmdHandler);
}
void CCommandParser::Parse(const orthia::PlatformString_type& text)
{
    auto copy = text;
    copy.erase(std::remove(copy.begin(), copy.end(), ORTHIA_TCHAR('`')), copy.end());

    auto utf8String = orthia::PlatformStringToUtf8(copy);
    orthia::CStreamTokenFileSource source;
    source.GetStream() << utf8String;
    m_tokenizer.ResetSource(&source);
    m_tokenizer.GetTokenizer().SetWindbgStyle(true);

    orthia::Token token;
    m_tokenizer.GetNextToken(&token, CTokenizer::flags_ForceGetName);
    if (token.type == Token::ttEOF)
    {
        if (m_emptyHandler)
        {
            m_emptyHandler();
            return;
        }
        throw NoCmdException();
    }

    auto cmdName = orthia::ReadString(token);
    auto it = m_handlers.find(cmdName);
    if (it == m_handlers.end())
    {
        throw CmdNameNotFoundException(cmdName);
    }
    it->second(*this);
}

}