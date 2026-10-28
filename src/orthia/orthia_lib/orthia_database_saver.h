#ifndef ORTHIA_DATABASE_SAVER_H
#define ORTHIA_DATABASE_SAVER_H

#include "orthia_diana_module.h"
#include "orthia_database_module.h"

namespace orthia
{

class CDatabaseSaver
{
    ExternalTargetFilter_type m_externalTargetFilter;
    // a reference out of the module that the filter drops
    bool IsDropped(const CDianaModule & dianaModule, const CommonReferenceInfo & ref) const;
public:
    explicit CDatabaseSaver(ExternalTargetFilter_type externalTargetFilter = ExternalTargetFilter_type());
    void Save(CDianaModule & dianaModule,
              CDatabaseManager & databaseManager,
              const orthia::PlatformString_type & moduleName,
              bool replaceModule);
    void SaveWithDedup(CDianaModule & dianaModule,
                       CDatabaseManager & databaseManager,
                       const orthia::PlatformString_type & moduleName);
};

}


#endif