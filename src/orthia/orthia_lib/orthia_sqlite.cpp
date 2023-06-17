#include "orthia_sqlite.h"
#include "orthia_sqlite_utils.h"
#include "orthia_utils.h"
#include "algorithm"

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)

#ifdef DIANA_HAS_CPP11
#include <chrono>
#include <mutex>
#endif
namespace orthia
{
    static int g_secondsToWaitLock = 15;

#ifdef DIANA_HAS_CPP11
    int SQLiteStep_Wrapper(sqlite3_stmt* statement)
    {
        int secondsToWaitLock = IsDebuggerPresent() ? 60 : g_secondsToWaitLock;
        auto operationStart = std::chrono::steady_clock::now();
        for (;;)
        {
            int stepResult = sqlite3_step(statement);
            if (stepResult != SQLITE_BUSY)
            {
                return stepResult;
            }
            auto now = std::chrono::steady_clock::now();

            auto opDuration = now - operationStart;
            if (opDuration >= std::chrono::seconds(secondsToWaitLock))
            {
                return SQLITE_BUSY;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    int SQLiteExec_Wrapper(sqlite3* statement, const char* sql)
    {
        int secondsToWaitLock = IsDebuggerPresent() ? 60 : g_secondsToWaitLock;
        auto operationStart = std::chrono::steady_clock::now();
        for (;;)
        {
            int stepResult = sqlite3_exec(statement, sql, 0, 0, 0);
            if (stepResult != SQLITE_BUSY)
            {
                return stepResult;
            }
            auto now = std::chrono::steady_clock::now();

            auto opDuration = now - operationStart;
            if (opDuration >= std::chrono::seconds(secondsToWaitLock))
            {
                return SQLITE_BUSY;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

#else
    int SQLiteStep_Wrapper(sqlite3_stmt* statement)
    {
        for (int i = 0; i < g_secondsToWaitLock; ++i)
        {
            int stepResult = sqlite3_step(statement);
            if (stepResult != SQLITE_BUSY)
            {
                return stepResult;
            }
            Sleep(1000);
        }
        return SQLITE_BUSY;
    }

    int SQLiteExec_Wrapper(sqlite3* statement, const char* sql)
    {
        for (int i = 0; i < g_secondsToWaitLock; ++i)
        {
            int stepResult = sqlite3_exec(statement, sql, 0, 0, 0);
            if (stepResult != SQLITE_BUSY)
            {
                return stepResult;
            }
            Sleep(1000);
        }
        return SQLITE_BUSY;
    }
#endif


static bool IsNotDigit(wchar_t ch)
{
    return !(ch>=L'0' && ch<=L'9');
}

bool ConvertSQLTimeToSystemTime(const std::string & time_in,
                                SYSTEMTIME * pSt)
{
    memset(pSt, 0, sizeof(*pSt));
    if (time_in.empty())
    {
        return false;
    }

    std::string time(time_in);
    const int partsCount = 7;
    int intParts[partsCount] = {0,};
    
    std::vector<orthia::StringInfo_Ansi> parts;
    std::replace_if(time.begin(), time.end(), IsNotDigit, ' ');
    orthia::SplitString(time, " ", &parts);

    int  * pCurrentPart = intParts;
    for(std::vector<orthia::StringInfo_Ansi>::iterator it = parts.begin(), it_end = parts.end();
        it != it_end;
        ++it)
    {
        if (it->size() == 0)
            continue;
        std::string data(it->ToString());
        orthia::Trim(data);
        orthia::StringToObject(data, pCurrentPart); 
        ++pCurrentPart;
        if (pCurrentPart - intParts >= partsCount)
            break;
    }

    if (pCurrentPart - intParts < 6)
        return false;

    pSt->wYear = intParts[0];
    pSt->wMonth = intParts[1];
    pSt->wDayOfWeek = 0;
    pSt->wDay = intParts[2];
    pSt->wHour = intParts[3];
    pSt->wMinute = intParts[4];
    pSt->wSecond = intParts[5];
    pSt->wMilliseconds = intParts[6];
    return true;
}

long long SQLReadDatetime(sqlite3_stmt * statement, int pos)
{
    long long failResult = 0;
    std::string time = SQLReadUtf8String(statement, pos);
    SYSTEMTIME st;
    if (!ConvertSQLTimeToSystemTime(time, &st))
    {
        return failResult;
    }

    FILETIME ftResult = {0,0};
    if (!SystemTimeToFileTime(&st, &ftResult))
    {
        return failResult;
    }

    LARGE_INTEGER result;
    result.HighPart = ftResult.dwHighDateTime;
    result.LowPart = ftResult.dwLowDateTime;
    return result.QuadPart;
}

CSQLStatement::CSQLStatement(sqlite3_stmt * statement)
    : m_statement(statement)
{
}
CSQLStatement::~CSQLStatement()
{
    Finalize();
}
void CSQLStatement::Swap(CSQLStatement & otherToSwap)
{
    std::swap(m_statement, otherToSwap.m_statement);
}
void CSQLStatement::SQLReset()
{
    sqlite3_reset(m_statement);
    sqlite3_clear_bindings(m_statement);
}
void CSQLStatement::Finalize()
{
    if (m_statement)
    {
        sqlite3_finalize(m_statement);
        m_statement = 0;
    }
}
void CSQLStatement::Reset(sqlite3_stmt * statement)
{
    if (m_statement)
        sqlite3_finalize(m_statement);
    m_statement = statement;
}
sqlite3_stmt ** CSQLStatement::Get2()
{
    Finalize();
    return &m_statement;
}

CSQLAutoReset::CSQLAutoReset(sqlite3_stmt * statement)
    :
        m_statement(statement)
{
}
CSQLAutoReset::~CSQLAutoReset()
{
    sqlite3_reset(m_statement);
    sqlite3_clear_bindings(m_statement);
}

CSQLDatabase::CSQLDatabase()
    :
        m_database(0)
{
}
CSQLDatabase::~CSQLDatabase()
{
    Reset(0);
}
void CSQLDatabase::Swap(CSQLDatabase & otherToSwap)
{
    std::swap(m_database, otherToSwap.m_database);
}
void CSQLDatabase::Reset(sqlite3 * database)
{
    if (m_database)
    {
        sqlite3_close(m_database);
    }
    m_database = database;
}   
sqlite3 ** CSQLDatabase::Get2()
{
    Reset(0);
    return &m_database;
}

// CSQLTransaction
CSQLTransaction::CSQLTransaction(sqlite3 * database)
    :
        m_database(database)
{
    if (database)
    {
        ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(database, "BEGIN TRANSACTION"), "Can't start transaction");
    }
}
void CSQLTransaction::Init(sqlite3 * database)
{
    if (m_database)
    {
        throw std::runtime_error("Transaction already started");
    }
    m_database = database;
    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(database, "BEGIN TRANSACTION"), "Can't start transaction");
}
CSQLTransaction::~CSQLTransaction()
{
    if (m_database)
    {
        SQLiteExec_Wrapper(m_database, "ROLLBACK TRANSACTION");
    }
}
void CSQLTransaction::Commit()
{
    ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_database, "END TRANSACTION"), "Can't commit transaction");
    m_database = 0;
}   
void CSQLTransaction::Rollback()
{
    try
    {
        ORTHIA_CHECK_SQLITE(SQLiteExec_Wrapper(m_database, "ROLLBACK TRANSACTION"), "Can't rollback transaction");
    }
    catch(std::exception & e)
    {
        &e;
    }
    m_database = 0;
}   

long long SQLite_ReadInt(sqlite3_stmt * statement, bool bSilent, int defaultValue)
{
    bool bResult = false;
    long long result = 0;
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
            if (bResult)
            {
                throw std::runtime_error("SQLiteStep_Wrapper failed: only one row expected");
            }
            result = sqlite3_column_int64(statement, 0);
            bResult = true;
            continue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
    if (!bResult)
    {
        if (bSilent)
        {
            return defaultValue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed: value not found");
    }
    return result;
}

unsigned long long SQLite_ReadInt64(sqlite3_stmt* statement, bool bSilent, unsigned long long defaultValue)
{
    bool bResult = false;
    unsigned long long  result = 0;
    for (;;)
    {
        int stepResult = SQLiteStep_Wrapper(statement);
        if (stepResult == SQLITE_DONE)
        {
            break;
        }
        else
            if (stepResult == SQLITE_ROW)
            {
                if (bResult)
                {
                    throw std::runtime_error("SQLiteStep_Wrapper failed: only one row expected");
                }
                result = (unsigned long long)sqlite3_column_int64(statement, 0);
                bResult = true;
                continue;
            }
        throw std::runtime_error("SQLiteStep_Wrapper failed");
    }
    if (!bResult)
    {
        if (bSilent)
        {
            return defaultValue;
        }
        throw std::runtime_error("SQLiteStep_Wrapper failed: value not found");
    }
    return result;
}

void SQLite_ReadWideString(sqlite3_stmt * statement, 
                           bool bSilent, 
                           const std::wstring & defaultValue, 
                           std::wstring * pResult)
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
            if (bResult)
            {
                throw std::runtime_error("SQLiteStep_Wrapper failed: only one row expected");
            }
            *pResult = SQLReadWideString(statement, 0);
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
        throw std::runtime_error("SQLiteStep_Wrapper failed: value not found");
    }
}

std::string ConvertTimeToSQLite(long long time)
{
    LARGE_INTEGER value;
    value.QuadPart = time;
    
    FILETIME fileTime;
    fileTime.dwHighDateTime = value.HighPart;
    fileTime.dwLowDateTime = value.LowPart;

    SYSTEMTIME st = {0};
    if (!FileTimeToSystemTime(&fileTime, &st))
    {
        throw std::runtime_error("Can't convert time");
    }

    return ConvertSystemTimeToSQLite(st);
}

std::string ConvertSystemTimeToSQLite(const SYSTEMTIME & st)
{
    //YYYY-MM-DD HH:MM:SS.SSS 
    char buffer[64];
    _snprintf(buffer, sizeof(buffer)/sizeof(buffer[0]), 
        "%4i-%02i-%02i %02i:%02i:%02i.%03i",
        (int)st.wYear,
        (int)st.wMonth,
        (int)st.wDay,
        (int)st.wHour,
        (int)st.wMinute,
        (int)st.wSecond,
        (int)st.wMilliseconds);
    return buffer;
}

std::string SQLiteTimeFromISO8601(const std::string & time_in)
{
    std::string time = time_in;
    const int partsCount = 7;
    int intParts[partsCount] = {0,};
    
    std::vector<orthia::StringInfo_Ansi> parts;
    std::replace_if(time.begin(), time.end(), IsNotDigit, ' ');
    orthia::SplitString(time, " ", &parts);

    int  * pCurrentPart = intParts;
    for(std::vector<orthia::StringInfo_Ansi>::iterator it = parts.begin(), it_end = parts.end();
        it != it_end;
        ++it)
    {
        if (it->size() == 0)
            continue;
        std::string data(it->ToString());
        orthia::Trim(data);
        orthia::StringToObject(data, pCurrentPart); 
        ++pCurrentPart;
        if (pCurrentPart - intParts >= partsCount)
            break;
    }

    SYSTEMTIME st;
    st.wYear = intParts[0];
    st.wMonth = intParts[1];
    st.wDayOfWeek = 0;
    st.wDay = intParts[2];
    st.wHour = intParts[3];
    st.wMinute = intParts[4];
    st.wSecond = intParts[5];
    st.wMilliseconds = intParts[6];

    //YYYY-MM-DD HH:MM:SS.SSS 
    char buffer[64];
    _snprintf(buffer, sizeof(buffer)/sizeof(buffer[0]), 
        "%4i-%02i-%02i %02i:%02i:%02i.%03i",
        (int)st.wYear,
        (int)st.wMonth,
        (int)st.wDay,
        (int)st.wHour,
        (int)st.wMinute,
        (int)st.wSecond,
        (int)st.wMilliseconds);
    return buffer;

}


}