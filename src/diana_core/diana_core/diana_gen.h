#ifndef DIANA_GEN_H
#define DIANA_GEN_H

typedef enum {diana_orNone, diana_orRegMem, diana_orRegistry,
              diana_orMemory, diana_orSreg, diana_orImmediate,
              diana_orOffset, diana_orEAX, diana_orRel, diana_orRel16x32, diana_orMemory16x32,
              diana_orPtr, diana_orDX, diana_orCL, diana_orAnyCR, diana_orDR6or7, diana_orTR6or7,
              diana_orECX, diana_orEsDi, diana_orESI_ptr,

              diana_orRegMMX, diana_orRegXMM, diana_orMemoryXMM, diana_orMemoryMMX, diana_orRegistry32,
              diana_orReg32mem16, diana_orFPU_ST, diana_orFPU_ST0, diana_orFPU_STi,
              diana_orReg16_32_64_mem16, diana_orRegMem_exact, diana_orMemory_exact
} DianaOperands_type;

typedef enum {diana_reg_none, diana_reg_ES, diana_reg_CS, diana_reg_SS, diana_reg_DS, diana_reg_FS, diana_reg_GS} DianaSreg_type;

struct _dianaBaseGenObject;
struct _dianaBaseGenObject * Diana_GetRootLine();

struct _dianaGroupInfo;
struct _dianaGroupInfo * Diana_GetGroups();
long Diana_GetGroupsCount();

#endif