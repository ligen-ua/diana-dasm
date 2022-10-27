#pragma once


#include "orthia_common_time.h"


namespace orthia
{

namespace report
{
    typedef enum {ctLeft, ctRight, ctCenter}  ColumnAlignment_type;
}
class CReport;
class CReportRow
{
    std::wstring * m_pRow;
    CReport * m_pReport;
    int m_index;
public:
    CReportRow(std::wstring * pRow,
               CReport * pReport)
        :
            m_pRow(pRow),
            m_pReport(pReport),
            m_index(0)
    {
    }
    CReportRow & operator << (const std::wstring & value);
    std::wstring GetValue() const
    {
        return *m_pRow;
    }
    const wchar_t * GetStr() const
    {
        return m_pRow->c_str();
    }
    size_t GetSize() const
    {
        return m_pRow->size();
    }
};
class CReport
{
public:
    void AddColumn(const std::wstring & name, 
                   int widthInSpaces,
                   report::ColumnAlignment_type alignment);

    CReportRow StartRow();
    CReportRow PrintHeader();

    CReport();
    void SetRowColumnDelimiter(const std::wstring & delim);
    std::wstring GetRowColumnDelimiter() const;
    void SetHeaderColumnDelimiter(const std::wstring & delim);
    std::wstring GetHeaderColumnDelimiter() const;

    std::wstring BuildCell(const std::wstring & name, int index) const;
private:

    struct ColumnInfo
    {
        std::wstring name;
        int widthInSpaces;
        report::ColumnAlignment_type alignment;

        ColumnInfo(const std::wstring & name_in,
                   int widthInSpaces_in,
                   report::ColumnAlignment_type alignment_in)
            :
                name(name_in),
                widthInSpaces(widthInSpaces_in),
                alignment(alignment_in)
        {
        }
    };

    std::wstring m_row;
    std::wstring m_rowColumnDelimiter, m_headerColumnDelimiter;
    std::vector<ColumnInfo> m_columns;

    CReport(const CReport&);
    CReport&operator =(const CReport&);
};


}