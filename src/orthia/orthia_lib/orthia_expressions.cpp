#include "orthia_expressions.h"
#include "orthia_common_print.h"

namespace orthia {

// MapNameResolver
Address_type MapNameResolver::QueryAddress(const PlatformString_type& name)
{
    auto it = names.find(name);
    if (it == names.end())
    {
        throw NameNotFound(name);
    }
    return it->second;
}
Address_type MapNameResolver::Dereference(Address_type address)
{
    auto it = addresses.find(address);
    if (it == addresses.end())
    {
        throw std::runtime_error("Can't dereference: " + orthia::PlatformStringToUtf8(orthia::AddressToString(address, sizeof(address))));
    }
    return it->second;
}

class PoiFunctionNode :public FunctionNode
{
public:
    orthia::Address_type Calc(CalcContext& calcContext)
    {
        if (m_childs.size() != 1)
        {
            throw std::runtime_error("poi function expects 1 argument");
        }
        auto address = m_childs[0].calcNode->Calc(calcContext);
        return calcContext.resolver->Dereference(address);
    }
};

// BasicNode
BasicNode::BasicNode()
{
}
void BasicNode::Init(std::shared_ptr<ICalcNode> parent, std::shared_ptr<AppendContext> appendContext)
{
    m_parent = parent;
    m_appendContext = appendContext;
}
std::shared_ptr<ICalcNode> BasicNode::GetParent_Silent()
{
    return m_parent.lock();
}
std::shared_ptr<AppendContext> BasicNode::GetAppendContext()
{
    if (!m_appendContext)
    {
        throw std::runtime_error("Invalid state");
    }
    return m_appendContext;
}
std::shared_ptr<ICalcNode> BasicNode::GetParent()
{
    auto parent = m_parent.lock();
    if (!parent)
    {
        throw std::runtime_error("Invalid state");
    }
    return parent;
}
AppendResult BasicNode::Append(const orthia::Token& token)
{
    if (token.type == orthia::Token::ttEOF)
    {
        Finalize();
        if (auto parent = m_parent.lock())
        {
            return AppendResult(parent);
        }
        return AppendResult(shared_from_this());
    }

    if (token.type == orthia::Token::ttSpecialSign)
    {
        return AppendSpecialSign(token);
    }
    else
    {
        return AppendToken(token);
    }
}

// NameNode
NameNode::NameNode()
{
}
AppendResult NameNode::Append(const orthia::Token& token)
{
    if (token.type != orthia::Token::ttEOF)
    {
        if (!GetAppendContext()->backtrackMode)
        {
            if (m_childNode)
            {
                return m_childNode->Append(token);
            }
        }
    }
    return BasicNode::Append(token);
}
AppendResult NameNode::AppendToken(const orthia::Token& token)
{
    switch (token.type)
    {
    case orthia::Token::ttName:
    {
        if (m_name.has_value())
        {
            throw TokenError(token);
        }

        m_name = ReadString(token);
        return AppendResult(shared_from_this());
    }
    };
    throw TokenError(token);
}
AppendResult NameNode::AppendSpecialSign(const orthia::Token& token)
{
    switch (token.operatorValue)
    {
    case ':':
    {
        if (m_acceptedReg || m_withBrackets)
            throw TokenError(token);

        if (!m_name.has_value())
            throw TokenError(token);

        if (!m_appendContext)
            throw TokenError(token);

        auto it = m_appendContext->segRegisters.find(orthia::Downcase(*m_name));
        if (it == m_appendContext->segRegisters.end())
        {
            throw std::runtime_error("Unknown segment register: " + orthia::ToAnsiString_Silent(*m_name));
        }

        m_acceptedReg = true;
        return AppendResult(shared_from_this());
    }
    case '[':
    {
        if (m_withBrackets)
            throw TokenError(token);

        m_withBrackets = true;
        m_childNode = std::make_shared<SummNode>(true);
        m_childNode->Init(shared_from_this(), m_appendContext);
        return AppendResult(shared_from_this());
    }
    case ']':
        if (!m_withBrackets || m_wasLastBracket)
        {
            return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
        }
        m_wasLastBracket = true;
        return AppendResult(GetParent());
    };
    throw TokenError(token);
}
orthia::Address_type NameNode::Calc(CalcContext& calcContext)
{
    if (m_childNode)
    {
        return m_childNode->Calc(calcContext);
    }
    if (m_name.has_value())
    {
        return calcContext.resolver->QueryAddress(*m_name);
    }
    throw NoTokenError();
}
void NameNode::Finalize() 
{
    if (m_withBrackets && !m_wasLastBracket)
    {
        throw NoTokenError();
    }
}

// AddressNode
AddressNode::AddressNode()
{
}
AppendResult AddressNode::AppendToken(const orthia::Token& token)
{
    switch (token.type)
    {
    case orthia::Token::ttLiteral:
    {
        if (token.literalType != orthia::Token::ttLiteralInt)
        {
            throw TokenError(token);
        }
        if (m_address.has_value())
        {
            throw TokenError(token);
        }
        const void* pData = token.pBinaryTokenStorage->QueryData(token.tokenOffset, token.tokenSize);
        m_address = Diana_ReadValue(pData, (int)token.tokenSize);
        return AppendResult(shared_from_this());
    }
    };
    return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
}
AppendResult AddressNode::AppendSpecialSign(const orthia::Token& token)
{
    return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
}
orthia::Address_type AddressNode::Calc(CalcContext& calcContext)
{
    if (!m_address.has_value())
    {
        throw NoTokenError();
    }
    return *m_address;
}

// UnaryNode
UnaryNode::UnaryNode(bool withBrackets)
    :
    SummNode(withBrackets)
{
}

AppendResult UnaryNode::Append(const orthia::Token& token)
{
    if (!m_withBrackets && m_childs.size() >= 1 && m_childs.back().calcNode)
    {
        return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
    }
    return SummNode::Append(token);
}

// MultiplyNode
MultiplyNode::MultiplyNode()
{

}
void MultiplyNode::Init2(std::shared_ptr<ICalcNode> calcNode, bool multiply)
{
    ChildInfo info;
    info.calcNode = calcNode;
    info.multiply = multiply;
    calcNode->Init(shared_from_this(), m_appendContext);
    m_childs.push_back(info);
}
MultiplyNode::ChildInfo& MultiplyNode::AllocChild(bool force)
{
    if (force && !m_childs.empty() && !m_childs.back().calcNode)
    {
        throw std::runtime_error("Internal error");
    }
    if (m_childs.empty() || force)
    {
        ChildInfo info;
        m_childs.push_back(info);
    }
    return m_childs.back();
}
void MultiplyNode::Finalize()
{
    if (m_childs.empty())
    {
        throw NoTokenError();
    }
    if (m_childs.back().multiply.has_value())
    {
        throw NoTokenError();
    }
}
AppendResult MultiplyNode::AppendToken(const orthia::Token& token)
{
    switch (token.type)
    {
    case orthia::Token::ttName:
    {
        if (!m_childs.empty() && (!m_childs.back().multiply.has_value()))
        {
            if (GetAppendContext()->backtrackMode)
            {
                return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
            }
            return m_childs.back().calcNode->Append(token);
        }
        AllocChild(true);
        auto nameNode = std::make_shared<NameNode>();
        nameNode->Init(shared_from_this(), m_appendContext);
        nameNode->Append(token);
        m_childs.back().calcNode = nameNode;
        return AppendResult(shared_from_this());
    }
    case orthia::Token::ttLiteral:
    {
        if (token.literalType != orthia::Token::ttLiteralInt)
        {
            throw TokenError(token);
        }
        if (!m_childs.empty() && (!m_childs.back().multiply.has_value()))
        {
            if (GetAppendContext()->backtrackMode)
            {
                return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
            }
            return m_childs.back().calcNode->Append(token);
        }
        AllocChild(true);
        auto addressNode = std::make_shared<AddressNode>();
        addressNode->Init(shared_from_this(), m_appendContext);
        addressNode->Append(token);
        m_childs.back().calcNode = addressNode;
        return AppendResult(shared_from_this());
    }
    };
    return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
}

AppendResult MultiplyNode::AppendSpecialSign(const orthia::Token& token)
{
    switch (token.operatorValue)
    {
    case '*':
    case '/':
    {
        if (m_childs.empty())
        {
            throw TokenError(token);
        }
        if (m_childs.back().multiply.has_value())
        {
            throw TokenError(token);
        }
        m_childs.back().multiply = token.signCharCode == '*';
        return AppendResult(shared_from_this());
    }
    case '+':
    case '-':
    case '(':
    {
        if (m_childs.empty())
        {
            throw TokenError(token);
        }
        if (m_childs.back().multiply.has_value())
        {
            bool hasBracket = token.signCharCode == '(';
            auto summNode = std::make_shared<UnaryNode>(hasBracket);
            summNode->Init(shared_from_this(), m_appendContext);
            auto& info = AllocChild(true);
            info.calcNode = summNode;
            if (!hasBracket)
            {
                return summNode->Append(token);
            }
            return AppendResult(info.calcNode);
        }
    }
    case '!':
    {
        if (m_childs.empty() || m_childs.back().multiply.has_value() || !m_childs.back().calcNode)
        {
            throw TokenError(token);
        }
        if (GetAppendContext()->backtrackMode)
        {
            return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
        }
        m_childs.back().calcNode->Append(token);
        return AppendResult(shared_from_this());
    }
    };
    return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
}

orthia::Address_type MultiplyNode::Calc(CalcContext& calcContext)
{
    if (m_childs.size() < 2)
    {
        throw NoTokenError();
    }

    auto it = m_childs.begin(), it_end = m_childs.end();
    orthia::Address_type result = it->calcNode->Calc(calcContext);
    auto prevIt = it++;
    for (; it != it_end; prevIt = it, ++it)
    {
        auto tmp = it->calcNode->Calc(calcContext);
        if (prevIt->multiply.value_or(true))
        {
            result *= tmp;
        }
        else
        {
            if (tmp == 0)
            {
                throw std::runtime_error("Can't divide by zero");
            }
            result /= tmp;
        }
    }
    return result;
}

// FunctionNode
FunctionNode::FunctionNode()
{

}
FunctionNode::ChildInfo& FunctionNode::AllocChild(bool force)
{
    if (force && !m_childs.empty() && !m_childs.back().calcNode)
    {
        throw std::runtime_error("Internal error");
    }
    if (m_childs.empty() || force)
    {
        ChildInfo info;
        m_childs.push_back(info);
    }
    return m_childs.back();
}
void FunctionNode::Finalize()
{
    if (!m_started || !m_finalized) 
    {
        throw NoTokenError();
    }
    if (m_childs.empty())
    {
        if (m_childs.back().hasComma)
        {
            throw NoTokenError();
        }
    }
}
AppendResult FunctionNode::AppendToken(const orthia::Token& token)
{
    return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
}

AppendResult FunctionNode::Append(const orthia::Token& token)
{
    if (token.type != orthia::Token::ttEOF)
    {
        if (m_started)
        {
            if (m_childs.empty() || m_childs.back().hasComma)
            {
                if (GetAppendContext()->backtrackMode)
                {
                    throw TokenError(token);
                }
                return CreateArgument(token);
            }
            if (!GetAppendContext()->backtrackMode)
            {
                return m_childs.back().calcNode->Append(token);
            }
        }
    }
    return BasicNode::Append(token);
}

AppendResult FunctionNode::CreateArgument(const orthia::Token& token)
{
    AllocChild(true);
    auto summNode = std::make_shared<SummNode>(false);
    summNode->Init(shared_from_this(), m_appendContext);
    auto res = summNode->Append(token);
    if (res.flags & res.flag_SentToParent)
    {
        res.newNode = shared_from_this();
        return res;
    }
    m_childs.back().calcNode = summNode;
    return AppendResult(shared_from_this());
}

AppendResult FunctionNode::AppendSpecialSign(const orthia::Token& token)
{
    switch (token.operatorValue)
    {
    case '(':
    {
        if (m_started)
        {
            throw TokenError(token);
        }
        m_started = true;
        return AppendResult(shared_from_this());
    }
    case ')':
    {
        if (m_finalized || !m_started)
        {
            throw TokenError(token);
        }
        m_finalized = true;
        if (m_childs.empty() == false && m_childs.back().hasComma)
        {
            throw TokenError(token);
        }
        m_childs.back().hasComma = true;
        return AppendResult(GetParent_Silent());
    }
    case ',':
    {
        if (m_childs.empty())
        {
            throw TokenError(token);
        }
        if (m_childs.back().hasComma)
        {
            throw TokenError(token);
        }
        if (m_finalized || !m_started)
        {
            throw TokenError(token);
        }
        m_childs.back().hasComma = true;
        return CreateArgument(token);
    }
    };
    return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
}

// SummNode
SummNode::SummNode(bool withBrackets)
    :
    m_withBrackets(withBrackets)
{
}
SummNode::ChildInfo& SummNode::AllocChild(bool force)
{
    if (force && !m_childs.empty() && !m_childs.back().calcNode)
    {
        throw std::runtime_error("Internal error");
    }
    
    if (m_childs.empty() || force)
    {

        ChildInfo info;
        m_childs.push_back(info);
    }
    return m_childs.back();
}
void SummNode::Finalize()
{
    if (m_childs.empty())
    {
        throw NoTokenError();
    }
    if (m_withBrackets && !m_wasLastBracket)
    {
        throw NoTokenError();
    }
    if (!m_childs.back().calcNode)
    {
        throw NoTokenError();
    }
}
AppendResult SummNode::AppendToken(const orthia::Token& token)
{
    switch (token.type)
    {
    case orthia::Token::ttReservedWord:
    {
        if (token.reservedWordId == GetAppendContext()->pTokernizerEnv->Get_poi().Get())
        {
            if (!m_childs.empty() && m_childs.back().calcNode)
            {
                if (GetAppendContext()->backtrackMode)
                {
                    return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
                }
                return m_childs.back().calcNode->Append(token);
            }
            AllocChild();
            auto nameNode = std::make_shared<PoiFunctionNode>();
            nameNode->Init(shared_from_this(), m_appendContext);
            m_childs.back().calcNode = nameNode;
            return AppendResult(nameNode);
        }
    }
    case orthia::Token::ttName:
    {
        if (!m_childs.empty() && m_childs.back().calcNode)
        {
            if (GetAppendContext()->backtrackMode)
            {
                return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
            }
            return m_childs.back().calcNode->Append(token);
        }
        AllocChild();
        auto nameNode = std::make_shared<NameNode>();
        nameNode->Init(shared_from_this(), m_appendContext);
        nameNode->Append(token);
        m_childs.back().calcNode = nameNode;
        return AppendResult(shared_from_this());
    }
    case orthia::Token::ttLiteral:
        if (token.literalType != orthia::Token::ttLiteralInt)
        {
            throw TokenError(token);
        }
        if (!m_childs.empty() && m_childs.back().calcNode)
        {
            if (GetAppendContext()->backtrackMode)
            {
                return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
            }
            return m_childs.back().calcNode->Append(token);
        }
        AllocChild();
        auto addressNode = std::make_shared<AddressNode>();
        addressNode->Init(shared_from_this(), m_appendContext);
        addressNode->Append(token);
        m_childs.back().calcNode = addressNode;
        return AppendResult(shared_from_this());
    }
    throw TokenError(token);
}

AppendResult SummNode::AppendSpecialSign(const orthia::Token& token)
{
    switch (token.operatorValue)
    {
    case '+':
    case '-':
    {
        {
            auto& info = AllocChild();
            if (!info.calcNode)
            {
                if (token.signCharCode == '-')
                {
                    info.positive = (bool)(1 - (info.positive ? 1 : 0));
                }
                return AppendResult(shared_from_this());
            }
        }
        auto& info = AllocChild(true);
        info.positive = token.signCharCode == '+';
        return AppendResult(shared_from_this());
    }
    case '(':
    {
        if (!m_childs.empty() && m_childs.back().calcNode)
        {
            throw TokenError(token);
        }

        auto child = std::make_shared<SummNode>(true);
        child->Init(shared_from_this(), m_appendContext);
        {
            auto& info = AllocChild();
            if (!info.calcNode)
            {
                info.calcNode = child;
                return AppendResult(child);
            }
        }
        auto& info = AllocChild(true);
        info.calcNode = child;
        return AppendResult(child);
    }
    case ')':
        if (m_childs.empty())
        {
            throw TokenError(token);
        }
        if (!m_withBrackets || m_wasLastBracket)
        {
            return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
        }
        m_wasLastBracket = true;
        return AppendResult(GetParent());
    case '/':
    case '*':
    {
        if (m_childs.empty() || !m_childs.back().calcNode)
        {
            throw TokenError(token);
        }
        auto mulNode = std::make_shared<MultiplyNode>();
        mulNode->Init(shared_from_this(), m_appendContext);
        mulNode->Init2(m_childs.back().calcNode, token.signCharCode == '*');
        m_childs.back().calcNode = mulNode;
        return AppendResult(mulNode);
    }
    case '!':
        if (m_childs.empty() || !m_childs.back().calcNode)
        {
            throw TokenError(token);
        }
        if (GetAppendContext()->backtrackMode)
        {
            return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
        }
        auto res = m_childs.back().calcNode->Append(token);
        if (res.flags & res.flag_SentToParent)
        {
            res.newNode = shared_from_this();
            return res;
        }
        return AppendResult(shared_from_this());
    };
    if (!m_childs.empty() && m_childs.back().calcNode)
    {
        if (GetAppendContext()->backtrackMode)
        {
            return AppendResult(shared_from_this(), AppendResult::flag_SentToParent);
        }
        return m_childs.back().calcNode->Append(token);
    }
    throw TokenError(token);
}

orthia::Address_type SummNode::Calc(CalcContext& calcContext)
{
    orthia::Address_type result = 0;
    for (auto& info : m_childs)
    {
        orthia::Address_type tmp = info.calcNode->Calc(calcContext);
        if (info.positive)
        {
            result += tmp;
        }
        else
        {
            result -= tmp;
        }
    }
    return result;
}


orthia::Address_type CaptureAddressExp(const orthia::PlatformString_type& expression, std::shared_ptr<orthia::INameResolver> resolver)
{
    //S = E $;
    //E = (E);
    //E = + E;
    //E = - E;
    //E = E + E;
    //E = E - E;
    //E = E * E;
    //E = E / E;
    orthia::CExpressionTokenizerEnv env;
    std::shared_ptr<ICalcNode> rootNode = CreateRootNode(&env);
    auto currentNode = rootNode;

    auto copy = expression;
    copy.erase(std::remove(copy.begin(), copy.end(), ORTHIA_TCHAR('`')), copy.end());

    auto utf8String = orthia::PlatformStringToUtf8(copy);
    orthia::CStreamTokenFileSource source;
    source.GetStream() << utf8String;
    env.ResetSource(&source);
    InitTokenizer(env);

    orthia::Token token;
    for (; env.GetNextToken(&token);)
    {
        currentNode = AppendToken(currentNode, token);
    }
    return CaptureAddressExp(rootNode, currentNode, token, resolver);
}

orthia::Address_type CaptureAddressExp(std::shared_ptr<ICalcNode> rootNode, 
    std::shared_ptr<ICalcNode> currentNode,
    const orthia::Token & token,
    std::shared_ptr<orthia::INameResolver> resolver)
{
    if (!rootNode)
    {
        throw std::runtime_error("Empty expression");
    }
    // send finalize to all
    const int maxDepth = 10000;
    for (int i = 0; rootNode != currentNode && i < maxDepth; ++i)
    {
        auto result = currentNode->Append(token);
        if (result.newNode)
        {
            currentNode = result.newNode;
        }
    }
    if (rootNode != currentNode)
    {
        throw std::runtime_error("Internal error");
    }
    CalcContext calcContext;
    calcContext.resolver = resolver;
    return rootNode->Calc(calcContext);
}

std::shared_ptr<ICalcNode> CreateRootNode(CExpressionTokenizerEnv* pTokernizerEnv)
{
    auto appendContext = std::make_shared<AppendContext>();
    appendContext->segRegisters.insert(ORTHIA_TCSTR("ds"));
    appendContext->segRegisters.insert(ORTHIA_TCSTR("cs"));
    appendContext->pTokernizerEnv = pTokernizerEnv;

    std::shared_ptr<ICalcNode> rootNode = std::make_shared<SummNode>(false);
    rootNode->Init(nullptr, appendContext);
    return rootNode;
}
std::shared_ptr<ICalcNode> AppendToken(std::shared_ptr<ICalcNode> currentNode_in, orthia::Token& token)
{
    std::shared_ptr<ICalcNode> currentNode = currentNode_in;
    for (;;)
    {
        auto result = currentNode->Append(token);
        if (result.newNode)
        {
            if (result.flags & result.flag_SentToParent)
            {
                currentNode = result.newNode->GetParent_Silent();
                if (!currentNode)
                {
                    throw TokenError(token);
                }
                currentNode->GetAppendContext()->backtrackMode = true;
                continue;
            }
            currentNode = result.newNode;
        }
        currentNode->GetAppendContext()->backtrackMode = false;
        break;
    }
    return currentNode;
}

void InitTokenizer(CExpressionTokenizerEnv& env)
{
    env.Get_poi().Set(env.GetReservedWordsStorage().AddReservedWord("poi"));
    env.GetTokenizer().SetWindbgStyle(true);

}


}