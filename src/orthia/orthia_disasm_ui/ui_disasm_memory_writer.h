#pragma once
#include "orthia_diana_print.h"
#include "oui_multiline_view.h"
#include "oui_disasm_colors.h"

namespace oui
{
    const std::uint32_t g_region_id_address = oui::g_id_user_range + 1;
    const std::uint32_t g_region_id_operand = oui::g_id_user_range + 2;

    struct DisasmWriter :orthia::ITextPrinter
    {
        std::vector<oui::MultiLineViewItem> items;
        int lastCmdSize = 0;
        void PrintLine(const std::wstring& line) override;
        void PrintLine(const std::wstring& line, const oui::TextMarkup& markup, std::shared_ptr<IMultilineViewTag> tag);
    };

    struct DisasmLineContextTag:IMultilineViewTag
    {
        OPERAND_SIZE address = 0;
        OPERAND_SIZE newOffset = 0;
        int absoluteAddress = 0;
        int linksToData = 0;
    };
    struct MemoryPrinterOperandInfo
    {
        OPERAND_SIZE operand = 0;
        size_t offset = 0;
        int size = 0;
    };
    class MemoryPrinter:public orthia::CSubrangeMemoryPrinter<diana::CMasmString>
    {
        oui::CTextMarkupBuilder m_textMarkupBuilder;
        using Parent_type = orthia::CSubrangeMemoryPrinter<diana::CMasmString>;

        DisasmWriter& m_writer;
        bool m_firstPrint = true;
        orthia::Address_type m_firstVirtualOffset;
        const char* m_pDataFlags = 0;
        orthia::Address_type m_routeStart = 0;
        oui::DisasmColorsProfile m_colors;
        DisasmWriter* m_pTextPrinter;
        std::vector<MemoryPrinterOperandInfo> m_operands;
    public:
        MemoryPrinter(DisasmWriter* pTextPrinter,
            int dianaMode,
            orthia::Address_type startAddress,
            orthia::Address_type sizeInCommands,
            DisasmWriter& writer);
        void AddOperandPointer(OPERAND_SIZE operand, size_t offset, int size);

        void PrintCommand(unsigned long long address,
            const std::wstring& bytes,
            const std::wstring& command) override;

        void SetFlags(const char* pDataFlags, orthia::Address_type routeStart);
        bool IsBadByte(orthia::Address_type virtualOffset) override;
        void Preprocess(int iRes,
            ::DianaContext& context,
            ::DianaParserResult& result,
            orthia::Address_type virtualOffset,
            bool* pPrint,
            bool* pExit) override;
        orthia::Address_type GetRealFirstAddress() const;
    };

}