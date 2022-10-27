#ifndef ORTHIA_H
#define ORTHIA_H

#include "orthia_dbgext.h"
#include "orthia_pointers.h"

class ApiGuard
{
    ApiGuard(const ApiGuard&);
    ApiGuard&operator =(const ApiGuard&);
public:
    ApiGuard(){}
    ~ApiGuard(){ EXIT_API(); }
};
#define ORTHIA_TRY try {
#define ORTHIA_CATCH } catch(const std::exception & e) { dprintf("Error: %s\n", e.what()); } 

#define ORTHIA_CMD_START INIT_API(); ApiGuard orthia_____api_guard; ORTHIA_TRY
#define ORTHIA_CMD_END  ORTHIA_CATCH return S_OK;

#define ORTHIA_DECLARE_API(Name) HRESULT CALLBACK Name(PDEBUG_CLIENT4 Client, PCSTR args)

namespace orthia
{

void InitLib();

class CDatabaseManager;
struct IMemoryReader;
struct IAPIHandlerDebugInterface;
orthia::intrusive_ptr<CDatabaseManager> QueryDatabaseManager();
orthia::IMemoryReader * QueryReader();
int QueryInitialDianaMode();
}


#endif 