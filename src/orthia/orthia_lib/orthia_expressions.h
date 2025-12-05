#pragma once

#include "orthia_tokenizer.h"
#include <optional>
#include <unordered_set>

namespace orthia {

struct ICalcNode;
class CExpressionTokenizerEnv;

struct AppendResult
{
    static const int flag_SentToParent = 1;
    
    std::shared_ptr<ICalcNode> newNode;
    int flags = 0;

    AppendResult(std::shared_ptr<ICalcNode> newNode_in, int flags_in = 0)
        :   
        newNode(newNode_in), flags(flags_in)
    {
    }
};


struct INameResolver
{
    virtual ~INameResolver() {}
    virtual Address_type QueryAddress(const PlatformString_type& name) = 0;
    virtual Address_type Dereference(Address_type address) = 0;
};

struct AppendContext
{
    bool backtrackMode = false;
    std::unordered_set<PlatformString_type> segRegisters;
    CExpressionTokenizerEnv* pTokernizerEnv = 0;
};
struct CalcContext
{
    std::shared_ptr<orthia::INameResolver> resolver;
};
struct ICalcNode :public std::enable_shared_from_this<ICalcNode>
{
    virtual ~ICalcNode() {}
    virtual void Init(std::shared_ptr<ICalcNode> parent, std::shared_ptr<AppendContext> appendContext) = 0;
    virtual AppendResult Append(const orthia::Token& token) = 0;
    virtual orthia::Address_type Calc(CalcContext & calcContext) = 0;
    virtual std::shared_ptr<ICalcNode> GetParent_Silent() = 0;
    virtual std::shared_ptr<AppendContext> GetAppendContext() = 0;
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
    std::shared_ptr<AppendContext> m_appendContext;
public:
    BasicNode();
    AppendResult Append(const orthia::Token& token) override;
    void Init(std::shared_ptr<ICalcNode> parent, std::shared_ptr<AppendContext> appendContext) override;
    std::shared_ptr<ICalcNode> GetParent();
    std::shared_ptr<ICalcNode> GetParent_Silent() override;
    std::shared_ptr<AppendContext> GetAppendContext() override;

    virtual void Finalize() {}
    virtual AppendResult AppendToken(const orthia::Token& token) = 0;
    virtual AppendResult AppendSpecialSign(const orthia::Token& token) = 0;
};


class NameNode : public BasicNode
{
    std::optional<orthia::PlatformString_type> m_name;
    bool m_acceptedReg = false;
    bool m_withBrackets = false;
    bool m_wasLastBracket = false;
    std::shared_ptr<ICalcNode> m_childNode;

public:
    NameNode();
    AppendResult Append(const orthia::Token& token) override;
    AppendResult AppendToken(const orthia::Token& token) override;
    AppendResult AppendSpecialSign(const orthia::Token& token) override;
    orthia::Address_type Calc(CalcContext& calcContext) override;
    void Finalize() override;
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


class FunctionNode : public BasicNode
{
protected:
    struct ChildInfo
    {
        bool hasComma = false;
        std::shared_ptr<ICalcNode> calcNode;
    };
    std::vector<ChildInfo> m_childs;
    bool m_started = false;
    bool m_finalized = false;
public:
    FunctionNode();
    AppendResult AppendToken(const orthia::Token& token) override;
    ChildInfo& AllocChild(bool force = false);
    void Finalize() override;
    AppendResult Append(const orthia::Token& token) override;
    AppendResult AppendSpecialSign(const orthia::Token& token) override;
    AppendResult CreateArgument(const orthia::Token& token);
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
    std::map<Address_type, Address_type> addresses;

    Address_type QueryAddress(const PlatformString_type& name);
    Address_type Dereference(Address_type address);
};
orthia::Address_type CaptureAddressExp(const orthia::PlatformString_type& expression, 
    std::shared_ptr<orthia::INameResolver> resolver);

orthia::Address_type CaptureAddressExp(std::shared_ptr<ICalcNode> rootNode, 
    std::shared_ptr<ICalcNode> currentNode,
    const orthia::Token& token,
    std::shared_ptr<orthia::INameResolver> resolver);

std::shared_ptr<ICalcNode> CreateRootNode(CExpressionTokenizerEnv* pTokernizerEnv);
std::shared_ptr<ICalcNode> AppendToken(std::shared_ptr<ICalcNode> currentNode_in, orthia::Token& token);

class ReservedWord
{
    int m_id = 0;
public:
    void Set(int id)
    {
        m_id = id;
    }
    int Get() const
    {
        return m_id;
    }
};

class CExpressionTokenizerEnv:public CTokenizerEnv
{
    ReservedWord m_rwPoi;
public:
    ReservedWord& Get_poi() { return m_rwPoi; }
};
void InitTokenizer(CExpressionTokenizerEnv& envs);

}