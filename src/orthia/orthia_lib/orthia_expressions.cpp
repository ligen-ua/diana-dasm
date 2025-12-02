#include "orthia_expressions.h"

namespace orthia {

// BasicNode
BasicNode::BasicNode()
{
}
void BasicNode::Init(std::shared_ptr<ICalcNode> parent)
{
    m_parent = parent;
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
    throw TokenError(token);
}
orthia::Address_type NameNode::Calc(CalcContext& calcContext)
{
    if (m_name.has_value())
    {
        return calcContext.resolver->QueryAddress(*m_name);
    }
    throw NoTokenError();
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
    return GetParent()->Append(token);
}
AppendResult AddressNode::AppendSpecialSign(const orthia::Token& token)
{
    throw TokenError(token);
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
        return GetParent()->Append(token);
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
    calcNode->Init(shared_from_this());
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
            return m_childs.back().calcNode->Append(token);
        }
        AllocChild(true);
        auto nameNode = std::make_shared<NameNode>();
        nameNode->Init(shared_from_this());
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
            return m_childs.back().calcNode->Append(token);
        }
        AllocChild(true);
        auto addressNode = std::make_shared<AddressNode>();
        addressNode->Init(shared_from_this());
        addressNode->Append(token);
        m_childs.back().calcNode = addressNode;
        return AppendResult(shared_from_this());
    }
    };
    return GetParent()->Append(token);
}

AppendResult MultiplyNode::AppendSpecialSign(const orthia::Token& token)
{
    switch (token.signCharCode)
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
            summNode->Init(shared_from_this());
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
        m_childs.back().calcNode->Append(token);
        return AppendResult(shared_from_this());
    }
    };
    return GetParent()->Append(token);
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
    case orthia::Token::ttName:
    {
        if (!m_childs.empty() && m_childs.back().calcNode)
        {
            return m_childs.back().calcNode->Append(token);
        }
        AllocChild();
        auto nameNode = std::make_shared<NameNode>();
        nameNode->Init(shared_from_this());
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
            return m_childs.back().calcNode->Append(token);
        }
        AllocChild();
        auto addressNode = std::make_shared<AddressNode>();
        addressNode->Init(shared_from_this());
        addressNode->Append(token);
        m_childs.back().calcNode = addressNode;
        return AppendResult(shared_from_this());
    }
    throw TokenError(token);
}

AppendResult SummNode::AppendSpecialSign(const orthia::Token& token)
{
    switch (token.signCharCode)
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
        child->Init(shared_from_this());
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
            auto parent = m_parent.lock();
            if (!parent)
            {
                throw TokenError(token);
            }
            return parent->Append(token);
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
        mulNode->Init(shared_from_this());
        mulNode->Init2(m_childs.back().calcNode, token.signCharCode == '*');
        m_childs.back().calcNode = mulNode;
        return AppendResult(mulNode);
    }
    case '!':
        if (m_childs.empty() || !m_childs.back().calcNode)
        {
            throw TokenError(token);
        }
        m_childs.back().calcNode->Append(token);
        return AppendResult(shared_from_this());
    };
    if (!m_childs.empty() && m_childs.back().calcNode)
    {
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
    std::shared_ptr<ICalcNode> rootNode = std::make_shared<SummNode>(false);
    auto currentNode = rootNode;

    auto copy = expression;
    copy.erase(std::remove(copy.begin(), copy.end(), ORTHIA_TCHAR('`')), copy.end());

    auto utf8String = orthia::PlatformStringToUtf8(copy);
    orthia::CStreamTokenFileSource source;
    source.GetStream() << utf8String;
    orthia::CTokenizerEnv env;
    env.ResetSource(&source);
    env.GetTokenizer().SetWindbgStyle(true);

    orthia::Token token;
    for (; env.GetNextToken(&token);)
    {
        auto result = currentNode->Append(token);
        if (result.newNode)
        {
            currentNode = result.newNode;
        }
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

}