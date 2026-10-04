#pragma once
#include "orthia_diana_print.h"
#include "oui_multiline_view.h"
#include "oui_disasm_colors.h"
#include "orthia_model_interfaces.h"

namespace oui
{
    struct DisasmWriter :orthia::ITextPrinter
    {
        std::vector<oui::MultiLineViewItem> items;
        int lastCmdSize = 0;
        void PrintLine(const orthia::PlatformString_type& line) override;
        virtual void PrintLine(const orthia::PlatformString_type& line, const oui::TextMarkup& markup, std::shared_ptr<IMultilineViewTag> tag);
    };

    struct DisasmLineContextTag:IMultilineViewTag
    {
        oui::LineIndex index;
        OPERAND_SIZE newOffset = 0;
        int absoluteAddress = 0;
        int linksToData = 0;
        std::vector<orthia::CommonReferenceInfo> xrefs;
    };
    struct MemoryPrinterOperandInfo
    {
        OPERAND_SIZE operand = 0;
        size_t offset = 0;
        int size = 0;
    };
    // what the printer's limit counts: screen lines (annotations included) or instructions only
    enum class LimitKind
    {
        Lines,
        Instructions
    };
    class MemoryPrinter:public orthia::CSubrangeMemoryPrinter<diana::CMasmString>
    {
    protected:
        oui::CTextMarkupBuilder m_textMarkupBuilder;
        using Parent_type = orthia::CSubrangeMemoryPrinter<diana::CMasmString>;

        DisasmWriter& m_writer;
        bool m_firstPrint = true;
        oui::LineIndex m_firstVirtualOffset;
        const char* m_pDataFlags = 0;
        orthia::Address_type m_dataSize = 0;
        orthia::Address_type m_routeStart = 0;
        oui::DisasmColorsProfile m_colors;
        DisasmWriter* m_pTextPrinter;
        std::vector<MemoryPrinterOperandInfo> m_operands;
        std::shared_ptr<orthia::IWorkPlaceItem> m_workspaceItem;
        oui::LineIndex m_startAddress;
        oui::LineIndex m_endAddress;
        bool m_haveEndAddress = false;
        oui::LineIndex m_stopAddress;
        orthia::IMarkupCache* m_referencesCache = nullptr;
        const LimitKind m_limitKind;

        void PackCommand(const orthia::PlatformString_type& command, std::shared_ptr<DisasmLineContextTag> tag);
        Diana_LinkedAdditionalGroupInfo* GetLinkedInfo();
    public:
        MemoryPrinter(DisasmWriter* pTextPrinter,
            int dianaMode,
            const oui::LineIndex & startAddress,
            orthia::Address_type limit,
            LimitKind limitKind,
            std::shared_ptr<orthia::IWorkPlaceItem> workspaceItem);

        void SetEndAddress(const oui::LineIndex& endAddress);
        void SetReferencesCache(orthia::IMarkupCache* cache) { m_referencesCache = cache; }
        void AddOperandPointer(OPERAND_SIZE operand, size_t offset, int size);
        void OnRange(const orthia::VmMemoryRangeInfo& vmRange, const char* pDataStart);
        void OnStream(DianaPrintContext* pDianaPrintContext, oui::LineIndex virtualOffset, bool reportNoData);

        void PrintMetaInfo(const oui::LineIndex& address,
            const orthia::MarkupLine& line);
        void PrintCommand(unsigned long long address,
            const orthia::PlatformString_type& bytes,
            const orthia::PlatformString_type& command) override;
        void PrintCommand(const oui::LineIndex& address,
            const orthia::PlatformString_type& bytes,
            const orthia::PlatformString_type& command);
        void PrintCommandEx(unsigned long long address,
            const orthia::PlatformString_type& bytes,
            const orthia::PlatformString_type& command,
            std::shared_ptr<DisasmLineContextTag> tag);
        void SetFlags(const char* pDataFlags, orthia::Address_type dataSize, orthia::Address_type routeStart);
        bool IsBadByte(orthia::Address_type virtualOffset) override;
        void Preprocess(int iRes,
            ::DianaContext& context,
            ::DianaParserResult& result,
            orthia::Address_type virtualOffset,
            bool* pPrint,
            bool* pExit) override;
        oui::LineIndex GetRealFirstAddress() const;
        // where the last OnStream stopped, and how many commands have been printed so far
        oui::LineIndex GetStopAddress() const { return m_stopAddress; }
        orthia::Address_type GetPrintedCommands() const { return m_currentCommand; }
    };

}