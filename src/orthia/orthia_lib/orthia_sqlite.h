#ifndef ORTHIA_SQLITE_H
#define ORTHIA_SQLITE_H

#include "orthia_utils.h"

struct sqlite3;
struct sqlite3_stmt;
namespace orthia
{

class CSQLStatement
{
    CSQLStatement(const CSQLStatement&);
    CSQLStatement&operator = (const CSQLStatement&);
    sqlite3_stmt * m_statement;
public:
    CSQLStatement(sqlite3_stmt * statement=0);
    ~CSQLStatement();
    void Swap(CSQLStatement & otherToSwap);
    void Finalize();
    void Reset(sqlite3_stmt * statement);
    void SQLReset();
    sqlite3_stmt * Get() { return m_statement; }
    sqlite3_stmt ** Get2();
};
class CSQLDatabase
{
    CSQLDatabase(const CSQLDatabase &);
    CSQLDatabase&operator =(const CSQLDatabase &);
    sqlite3 * m_database;
public:
    CSQLDatabase();
    ~CSQLDatabase();
    void Reset(sqlite3 * database);
    void Swap(CSQLDatabase & otherToSwap);
    sqlite3 * Get() { return m_database; }
    sqlite3 ** Get2();
};

class CSQLDatabase2:public CSQLDatabase, public orthia::RefCountedBase
{
public:
    CSQLDatabase2(CSQLDatabase & otherToSwap)
    {
        Swap(otherToSwap);
    }
};

class CSQLTransaction
{
    CSQLTransaction(const CSQLTransaction &);
    CSQLTransaction&operator =(const CSQLTransaction &);
    sqlite3 * m_database;
public:
    CSQLTransaction(sqlite3 * database = 0);
    ~CSQLTransaction();
    void Init(sqlite3 * database);
    void Commit();
    void Rollback();
};


class CSQLAutoReset
{
    sqlite3_stmt * m_statement;

    CSQLAutoReset(const CSQLAutoReset&);
    CSQLAutoReset&operator =(const CSQLAutoReset&);
public:
    CSQLAutoReset(sqlite3_stmt * statement);
    ~CSQLAutoReset();
};

long long SQLite_ReadInt(sqlite3_stmt * statement, bool bSilent, int defaultValue);
void SQLite_ReadWideString(sqlite3_stmt * statement, bool bSilent, const std::wstring & defaultValue, std::wstring * pResult);

#define ORTHIA_CHECK_SQLITE(Expression, Text) { int orthia____code = (Expression); if (orthia____code != SQLITE_OK) { std::stringstream orthia____stream; orthia____stream<<"[SQLITE] "<<Text<<", code: "<<orthia____code; throw std::runtime_error(orthia____stream.str()); }} 
#define ORTHIA_CHECK_SQLITE2(Expression) ORTHIA_CHECK_SQLITE(Expression, "Error")

std::string ConvertTimeToSQLite(long long time);
bool ConvertSQLTimeToSystemTime(const std::string & time_in,
                                SYSTEMTIME * pSt);
std::string SQLiteTimeFromISO8601(const std::string & time);
std::string ConvertSystemTimeToSQLite(const SYSTEMTIME & st);

int SQLiteStep_Wrapper(sqlite3_stmt* statement);
int SQLiteExec_Wrapper(sqlite3* statement, const char* sql);

}
#endif
