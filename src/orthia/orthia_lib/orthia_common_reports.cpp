#include "orthia_common_reports.h"

namespace orthia
{


CReportRow & CReportRow::operator << (const std::wstring & value)
{
    m_pRow->append(m_pReport->BuildCell(value, m_index));
    m_pRow->append(m_pReport->GetRowColumnDelimiter());
    ++m_index;
    return *this;
}

static void AppendSpaces(std::wstring & result, int count)
{
    if (count <= 0)
    {
        return;
    }
    result.append(count, ' ');
}
                      
static void BuildLeft(int widthInSpaces, 
                      const std::wstring & name, 
                      std::wstring * pResult)
{
    *pResult = name;
    AppendSpaces(*pResult, widthInSpaces - (int)name.size());
}
static void BuildRight(int widthInSpaces, 
                       const std::wstring & name, 
                       std::wstring * pResult)
{
    AppendSpaces(*pResult, widthInSpaces - (int)name.size());
    pResult->append(name);
}
static void BuildCenter(int widthInSpaces, 
                        const std::wstring & name, 
                        std::wstring * pResult)
{
    AppendSpaces(*pResult, (widthInSpaces - (int)name.size())/2);
    pResult->append(name);
    AppendSpaces(*pResult, widthInSpaces - (int)pResult->size());
}
std::wstring CReport::BuildCell(const std::wstring & name, int index) const
{
    if ((size_t)index >= m_columns.size())
    {
        return name;
    }
    std::wstring result;
    // apply 
    const ColumnInfo & info = m_columns[index];
    switch(info.alignment)
    {
    case report::ctLeft:
        BuildLeft(info.widthInSpaces, 
                  name, 
                  &result);
        break;
    case report::ctRight:
        BuildRight(info.widthInSpaces, 
                  name, 
                  &result);
        break;
    case report::ctCenter:
        BuildCenter(info.widthInSpaces, 
                    name, 
                    &result);
        break;
    default:
        return name;
    }
    return result;
}
void CReport::AddColumn(const std::wstring & name, 
                        int widthInSpaces,
                        report::ColumnAlignment_type  alignment)
{
    m_columns.push_back(ColumnInfo(name, widthInSpaces, alignment));
}

CReportRow CReport::PrintHeader()
{
    m_row.clear();

    std::wstring tmp;
    for(std::vector<ColumnInfo>::iterator it = m_columns.begin(), it_end = m_columns.end();
        it != it_end;
        ++it)
    {
        if ((int)it->name.size() > (int)it->widthInSpaces)
        {
            it->widthInSpaces = (int)it->name.size();
        }
        tmp.clear();
        BuildCenter(it->widthInSpaces, it->name, &tmp);
        m_row.append(tmp);
        m_row.append(m_headerColumnDelimiter);
    }
    CReportRow row(&m_row, this);
    return row;
}
CReportRow CReport::StartRow()
{
    m_row.clear();
    return CReportRow(&m_row, this);
}

CReport::CReport()
{
    m_rowColumnDelimiter = L" ";
    m_headerColumnDelimiter = L"|";
}

void CReport::SetHeaderColumnDelimiter(const std::wstring & delim)
{
    m_headerColumnDelimiter = delim;
}
std::wstring CReport::GetHeaderColumnDelimiter() const
{
    return m_headerColumnDelimiter;
}
void CReport::SetRowColumnDelimiter(const std::wstring & delim)
{
    m_rowColumnDelimiter = delim;
}
std::wstring CReport::GetRowColumnDelimiter() const
{
    return m_rowColumnDelimiter;
}

}