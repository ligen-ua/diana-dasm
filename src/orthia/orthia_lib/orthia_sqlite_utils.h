#ifndef ORTHIA_SQLITE_UTILS_H
#define ORTHIA_SQLITE_UTILS_H

#include "orthia_sqlite.h"
#include "sqlite3.h"
namespace orthia
{
class CBaseOperation
{
    CBaseOperation(const CBaseOperation & op);
    CBaseOperation & operator =(const CBaseOperation & op);
public:
    CBaseOperation()
    {
    }
    virtual ~CBaseOperation()
    {
    }
};


inline 
void SQLExecute(sqlite3_stmt * statement)
{
    int stepResult = SQLiteStep_Wrapper(statement);
    if (stepResult != SQLITE_DONE)
    {
        throw std::runtime_error("Can't execute");
    }
}
inline 
bool SQLStep(sqlite3_stmt * statement)
{
    int stepResult = SQLiteStep_Wrapper(statement);
    if (stepResult == SQLITE_DONE)
    {
        return false;
    }
    if (stepResult == SQLITE_ROW)
    {
        return true;
    }
    throw std::runtime_error("Can't execute");
}

inline 
void SQLBindWideString(sqlite3_stmt * statement, const std::wstring & value, int pos)
{
    std::string utf8 = orthia::Utf16ToUtf8(value);
    ORTHIA_CHECK_SQLITE(sqlite3_bind_text(statement, pos, utf8.c_str(), (int)utf8.size(), SQLITE_TRANSIENT),
                      "Can't bind parameter at position: "<<pos);
}
inline 
void SQLBindUtf8String(sqlite3_stmt * statement, const std::string & utf8, int pos)
{
    ORTHIA_CHECK_SQLITE(sqlite3_bind_text(statement, pos, utf8.c_str(), (int)utf8.size(), SQLITE_TRANSIENT),
                      "Can't bind parameter at position: "<<pos);
}

inline 
void SQLBindBlob(sqlite3_stmt * statement, const std::vector<char> & value, int pos)
{
    if (value.empty())
    {
        ORTHIA_CHECK_SQLITE(sqlite3_bind_null(statement, pos),
                          "Can't bind parameter at position: "<<pos);   
        return;
    }
    ORTHIA_CHECK_SQLITE(sqlite3_bind_blob(statement, pos, &value.front(), (int)value.size(), SQLITE_TRANSIENT),
                      "Can't bind parameter at position: "<<pos);
}
inline 
void SQLBindInt(sqlite3_stmt * statement, int value, int pos)
{
    ORTHIA_CHECK_SQLITE(sqlite3_bind_int(statement, pos, value),
                      "Can't bind parameter at position: "<<pos);
}
inline 
void SQLBindInt64(sqlite3_stmt * statement, long long value, int pos)
{
    ORTHIA_CHECK_SQLITE(sqlite3_bind_int64(statement, pos, value),
                      "Can't bind parameter at position: "<<pos);
}
inline 
void SQLBindTime(sqlite3_stmt * statement, long long value, int pos)
{
    std::string str = ConvertTimeToSQLite(value);
    ORTHIA_CHECK_SQLITE(sqlite3_bind_text(statement, pos, str.c_str(), (int)str.size(), SQLITE_TRANSIENT),
                      "Can't bind parameter at position: "<<pos);
}
inline 
void SQLBindDouble(sqlite3_stmt * statement, double value, int pos)
{
    ORTHIA_CHECK_SQLITE(sqlite3_bind_double(statement, pos, value),
                      "Can't bind parameter at position: "<<pos);
}
inline 
void SQLBindSQLTime(sqlite3_stmt * statement, const std::string & value, int pos)
{
    ORTHIA_CHECK_SQLITE(sqlite3_bind_text(statement, pos, value.c_str(), (int)value.size(), SQLITE_TRANSIENT),
                      "Can't bind parameter at position: "<<pos);
}
inline 
std::wstring SQLReadWideString(sqlite3_stmt * statement, int pos, bool allowNull = true)
{
    char * pData = (char*)sqlite3_column_text(statement, pos);
    if (!pData)
    {
        if (allowNull)
        {
            return std::wstring();
        }
        throw std::runtime_error("String can't be empty");
    }
    return orthia::Utf8ToUtf16(pData);
}
inline 
std::string SQLReadUtf8String(sqlite3_stmt * statement, int pos, bool allowNull = true)
{
    char * pData = (char*)sqlite3_column_text(statement, pos);
    if (!pData)
    {
        if (allowNull)
        {
            return std::string();
        }
        throw std::runtime_error("String can't be empty");
    }
    return pData;
}

long long SQLReadDatetime(sqlite3_stmt * statement, int pos);

inline
int SQLReadInt(sqlite3_stmt * statement, int pos)
{
    return sqlite3_column_int(statement, pos);
}
inline 
long long SQLReadInt64(sqlite3_stmt * statement, int pos)
{
    return sqlite3_column_int64(statement, pos);
}


inline 
void SQLReadBlob(sqlite3_stmt * statement, int pos, std::vector<char> * pResult)
{
    pResult->clear();
    int bytesCount = sqlite3_column_bytes(statement, pos);
    const char * pBlob = (const char*)sqlite3_column_blob(statement, pos);
    if (!pBlob)
        return;
    pResult->assign(pBlob, pBlob + bytesCount);
}

// structs
struct SQLResult_1_Int
{
    typedef int Type;
    int m_value;
    SQLResult_1_Int(int value = 0)
        :
            m_value(value)
    {
    }
    void Deserialize(sqlite3_stmt * statement)
    {
        m_value = sqlite3_column_int(statement, 0);
    }
    void Export(int * pValue)
    {
        *pValue = m_value;
    }
};
struct SQLResult_1_Int64
{
    typedef long long Type;
    long long m_value;
    SQLResult_1_Int64(long long value = 0)
        :
            m_value(value)
    {
    }
    void Deserialize(sqlite3_stmt * statement)
    {
        m_value = SQLReadInt64(statement, 0);
    }
    void Export(long long * pValue)
    {
        *pValue = m_value;
    }
};
struct SQLResult_1_Datetime
{
    typedef long long Type;
    long long m_value;
    SQLResult_1_Datetime(long long value = 0)
        :
            m_value(value)
    {
    }
    void Deserialize(sqlite3_stmt * statement)
    {
        m_value = SQLReadDatetime(statement, 0);
    }
    void Export(long long * pValue)
    {
        *pValue = m_value;
    }
};


struct SQLResult_1_DatetimeSQL
{
    typedef std::string Type;
    std::string m_value;
    SQLResult_1_DatetimeSQL()
    {
    }
    void Deserialize(sqlite3_stmt * statement)
    {
        m_value = SQLReadUtf8String(statement, 0);
    }
    void Export(std::string * pValue)
    {
        *pValue = m_value;
    }
};

struct SQLResult_1_WideString
{
    typedef std::wstring Type;
    std::wstring m_value;
    SQLResult_1_WideString(const std::wstring & value = std::wstring())
        :
            m_value(value)
    {
    }
    void Deserialize(sqlite3_stmt * statement)
    {
        m_value = SQLReadWideString(statement, 0);
    }
    void Export(std::wstring * pValue)
    {
        *pValue = m_value;
    }
};

struct SQLResult_1_Blob
{
    typedef std::vector<char> Type;
    std::vector<char> m_value;
    SQLResult_1_Blob()
    {
    }
    void Deserialize(sqlite3_stmt * statement)
    {
        int bytesCount = sqlite3_column_bytes(statement, 0);
        const char * pBlob = (const char*)sqlite3_column_blob(statement, 0);
        if (!pBlob)
            return;
        m_value.assign(pBlob, pBlob + bytesCount);
    }
    void Export(std::vector<char> * pValue)
    {
        *pValue = m_value;
    }
};
struct SQLResult_1_Double
{
    typedef double Type;
    double m_value;
    SQLResult_1_Double(double value = 0.0)
        :
            m_value(value)
    {
    }
    void Deserialize(sqlite3_stmt * statement)
    {
        m_value = sqlite3_column_double(statement, 0);
    }
    void Export(double * pValue)
    {
        *pValue = m_value;
    }
};
inline bool SQLite_IsExists(sqlite3_stmt * statement)
{
    int stepResult = SQLiteStep_Wrapper(statement);
    return (stepResult == SQLITE_ROW);
}
template<class RowRecipientType>
bool SQLite_ReadCustom(sqlite3_stmt * statement, 
                       bool bSilent, 
                       const RowRecipientType & defaultValue, 
                       RowRecipientType * pResult,
                       bool bAllowMultiply,
                       bool bAllowNoResults)
{
    bool bResult = false;
    for(;;)
    {
        int stepResult = SQLiteStep_Wrapper(statement);
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        else
        if (stepResult == SQLITE_ROW)
        {
            if (bResult && !bAllowMultiply)
            {
                throw std::runtime_error("SQLiteStep_Wrapper failed: only one row expected");
            }
            pResult->Deserialize(statement);
            bResult = true;
            continue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
    if (!bResult)
    {
        if (bSilent)
        {
            *pResult = defaultValue;
        }
        if (!bAllowNoResults)
        {
            throw std::runtime_error("SQLiteStep_Wrapper failed: value not found");
        }
    }
    return bResult;
}

}
#endif
