#pragma once

#include "orthia_tokenizer.h"
#include <optional>

namespace orthia {

struct ICalcNode;
struct AppendResult
{
    std::shared_ptr<ICalcNode> newNode;

    AppendResult(std::shared_ptr<ICalcNode> newNode_in)
        :   
        newNode(newNode_in)
    {
    }
};


struct INameResolver
{
    virtual ~INameResolver() {}
    virtual Address_type QueryAddress(const PlatformString_type& name) = 0;
};

struct CalcContext
{
    std::shared_ptr<orthia::INameResolver> resolver;
};
struct ICalcNode :public std::enable_shared_from_this<ICalcNode>
{
    virtual ~ICalcNode() {}
    virtual void Init(std::shared_ptr<ICalcNode> parent) = 0;
    virtual AppendResult Append(const orthia::Token& token) = 0;
    virtual orthia::Address_type Calc(CalcContext & calcContext) = 0;
};
struct TokenError :public std::runtime_error
{
    orthia::Token token;
    TokenError(const orthia::Token& token_in)
        :
        std::runtime_error("Invalid token"),
        token(token_in)
    {
    }
};
struct NoTokenError :public std::runtime_error
{
    NoTokenError()
        :
        std::runtime_error("A value expected")
    {
    }
};
struct NameNotFound:public std::runtime_error
{
    NameNotFound(const orthia::PlatformString_type& name)
        :
        std::runtime_error("Name not found: " + orthia::PlatformStringToUtf8(name))
    {
    }
};
class BasicNode : public ICalcNode
{
protected:
    std::weak_ptr<ICalcNode> m_parent;
public:
    BasicNode();
    AppendResult Append(const orthia::Token& token) override;
    void Init(std::shared_ptr<ICalcNode> parent) override;
    std::shared_ptr<ICalcNode> GetParent();

    virtual void Finalize() {}
    virtual AppendResult AppendToken(const orthia::Token& token) = 0;
    virtual AppendResult AppendSpecialSign(const orthia::Token& token) = 0;
};


class NameNode : public BasicNode
{
    std::optional<orthia::PlatformString_type> m_name;
public:
    NameNode();
    AppendResult AppendToken(const orthia::Token& token) override;
    AppendResult AppendSpecialSign(const orthia::Token& token) override;
    orthia::Address_type Calc(CalcContext& calcContext) override;
};

class AddressNode : public BasicNode
{
    std::optional<orthia::Address_type> m_address;
public:
    AddressNode();
    AppendResult AppendToken(const orthia::Token& token) override;
    AppendResult AppendSpecialSign(const orthia::Token& token) override;
    orthia::Address_type Calc(CalcContext& calcContext) override;
};

class MultiplyNode : public BasicNode
{
    struct ChildInfo
    {
        std::optional<bool> multiply;
        std::shared_ptr<ICalcNode> calcNode;
    };
    std::vector<ChildInfo> m_childs;
public:
    MultiplyNode();
    void Init2(std::shared_ptr<ICalcNode> calcNode, bool multiply);
    ChildInfo& AllocChild(bool force = false);
    void Finalize() override;
    AppendResult AppendToken(const orthia::Token& token) override;
    AppendResult AppendSpecialSign(const orthia::Token& token) override;
    orthia::Address_type Calc(CalcContext& calcContext) override;
};

class SummNode : public BasicNode
{
protected:
    bool m_withBrackets = false;
    bool m_wasLastBracket = false;

    struct ChildInfo
    {
        bool positive = true;
        std::shared_ptr<ICalcNode> calcNode;
    };
    std::vector<ChildInfo> m_childs;
public:
    SummNode(bool withBrackets);
    ChildInfo& AllocChild(bool force = false);
    void Finalize() override;
    AppendResult AppendToken(const orthia::Token& token) override;
    AppendResult AppendSpecialSign(const orthia::Token& token) override;
    orthia::Address_type Calc(CalcContext& calcContext) override;
};

class UnaryNode : public SummNode
{
public:
    UnaryNode(bool withBrackets);
    AppendResult Append(const orthia::Token& token) override;
};

struct MapNameResolver:orthia::INameResolver
{
    std::map<PlatformString_type, Address_type> names;
    virtual Address_type QueryAddress(const PlatformString_type& name)
    {
        auto it = names.find(name);
        if (it == names.end())
        {
            throw NameNotFound(name);
        }
        return it->second;
    }
};
orthia::Address_type CaptureAddressExp(const orthia::PlatformString_type& expression, std::shared_ptr<orthia::INameResolver> resolver);

}