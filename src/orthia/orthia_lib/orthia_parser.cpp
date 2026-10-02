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
static bool IsCommandSpace(ORTHIA_TCHAR ch)
{
    return ch == ORTHIA_TCHAR(' ') || ch == ORTHIA_TCHAR('\t') ||
           ch == ORTHIA_TCHAR('\r') || ch == ORTHIA_TCHAR('\n');
}

void CCommandParser::Parse(const orthia::PlatformString_type& text)
{
    auto copy = text;
    copy.erase(std::remove(copy.begin(), copy.end(), ORTHIA_TCHAR('`')), copy.end());

    // whitespace only is the same as empty
    const auto first = std::find_if_not(copy.begin(), copy.end(), IsCommandSpace);
    if (first == copy.end())
    {
        if (m_emptyHandler)
        {
            m_emptyHandler();
            return;
        }
        throw NoCmdException();
    }
    // the whole first word, for the error text: the tokenizer may stop earlier ("-" of "--file")
    const orthia::PlatformString_type firstWord(first, std::find_if(first, copy.end(), IsCommandSpace));

    auto utf8String = orthia::PlatformStringToUtf8(copy);
    orthia::CStreamTokenFileSource source;
    source.GetStream() << utf8String;
    m_tokenizer.ResetSource(&source);
    orthia::InitTokenizer(m_tokenizer);

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
        throw CmdNameNotFoundException(firstWord);
    }
    it->second(*this);
}

}