#ifndef ORTHIA_DATABASE_SAVER_H
#define ORTHIA_DATABASE_SAVER_H

#include "orthia_diana_module.h"
#include "orthia_database_module.h"

namespace orthia
{
    
class CDatabaseSaver
{
public:
    CDatabaseSaver();
    void Save(CDianaModule & dianaModule,
              CDatabaseManager & databaseManager,
              const std::wstring & moduleName);
};

}


#endif