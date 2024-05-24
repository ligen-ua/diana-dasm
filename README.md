# Diana-Dasm 
Diana-Dasm is a small and fast disassembler, useful for Windows kernel developers.
It is a C library, not a product or tool.

Path: src/diana_core 

Advantages:
- highly portable, has minimal runtime requirements (C runtime);
- core libraries do not require any external components;
- includes instructions emulator (diana_processor);
- has stream oriented design;
Supported platforms: i386, amd64
Supported instructions: x586/amd64/FPU/MMX/SSE/SSE2

Original SVN Repo: https://sourceforge.net/projects/diana-dasm/

# Orthia Windbg Plugin
Orthia Windbg Plugin is a plugin to Microsoft Windows Debugger (Windbg).
This section describes how to use it.

## Setup the plugin
### Step 1. Load the dll
Load the dll using the load command:
> .load full-path\orthia.dll

Note: 32-bit windbg on my win8-64 computer didn't want to load orthia.dll until I put it to the 
> C:\Program Files (x86)\Debugging Tools for Windows\winext
so the best way is to put the dll there

Note2: debugger could be also located in windows kit folder:
> C:\Program Files (x86)\Windows Kits\8.0\Debuggers\

P.S use \src\start.bat or \src\start_with_kit.cmd to build the solution

### Step2. Setup the profile 
Orthia plugin stores all the information about analyzed references at SQLite database named "profile". It is important to setup a profile database file before the work:
> !orthia.profile full-path\<profilename>.db

or
> !orthia.profile /f full-path\<profilename>.db

/f option forces the command to rewrite the existing file.

Note: profile command supports environment variables like %temp%, for example you can setup the profile like that:
> !orthia.profile /f %temp%\test.db

The initial setup is done.

## Code Analysis
The interface for code analysis:
1) reload the module by address:
> !orthia.reload <module_start_address>

Orthia analyses all the references inside the module and puts the information into the database. 
Note: module reloading could take some time, for example when working with live debug session. Orthia caches the module sections but initial data acquiring could be slow.

2) reload all modules
> !for_each_module !orthia.reload /v @#Base

3) show the references to the instruction
> !orthia.x address_expression

4) show the references to the instruction range
> !orthia.xr address_expression1 address_expression2
or
> !orthia.u  address_expression1 address_expression2

(prints also the code)

5) show all loaded modules
> !orthia.lm

6) analyze custom region
> !orthia.a address 


## Code Emulator

Orthia Code Emulator is located at VM subsystem. VM subsystem makes possible to manage the virtual environments (VM's) and run the code
(which runs inside the VM, but accesses the windbg source memory as well)

The interface of emulator:
1) Exec Command

Just executes the code starting from current point:

>    !orthia.exec <number_of_instructions>

2) VM Subsystem.

The entities:
-- Virtual Machines (the containers for memory and code)

The commands for VM management:
>     vm_vm_new            Creates a new VM
>     vm_vm_del            Deletes the VM
>     vm_vm_list           Shows the list of VMs
>     vm_vm_info           Shows the information about the VM
>     vm_vm_def            Creates and overwrites the "Default" VM with ID == 0
>     vm_vm_d(b,w,d,q,p)   Display the contents of memory in the given range
>     vm_vm_call           Calls the function inside VM\n");
>     vm_vm_u              Displays an assembly translation of the specified program code

-- VM Modules (the set of data regions)

The commands for modules management:
>     vm_mod_new           Creates a new module inside VM  
>     vm_mod_del           Deletes the module
>     vm_mod_list          Shows the list of modules
>     vm_mod_info          Shows the information about the module
    
>     vm_mod_e(b,w,d,q,p)  Writes the data to the module
>     vm_mod_write_bin     Writes the contents of file to the module
>     vm_mod_enable        Enables/disables the module. vm_vm_call command affects only enabled modules


How to access samples:
1) Install winbg as post mortem debugger
> windbg -I

2) Produce the crash
Build orthia_test.exe
Start "orthia_test.exe /crash"

3) See the instructions how to access VM functionality

\src\orthia\orthia_test\main.cpp

    // 2.  How to execute some function
    // .load orthia.dll;!orthia.profile /f %temp%\test.db; 
    // !orthia.vm_vm_def
    // !orthia.vm_vm_call 0 orthia_test!test_vm_fake --print rcx=42

    
See !vm_help for more information

# Orthia Disassembler
Orthia Disassembler is a small command line tool which can dump exe file information.

Usage:
>     dump <module> <functions> [--fmt <json>] [--base <imagebase>] [--pdb <pdbfile>]  

Currently it just dumps export functions addresses.

# PTE Case Study

Sometimes Windbg doesn't show PTE well:

>     7: kd> !pte fffff60f`523e76e8
>     Levels not implemented for this platform


There is a possible approach to emulate MiGetPteAddress and get a PTE for some address

>     7: kd> !orthia.vm_vm_call 0 nt!MiGetPteAddress --print rcx=fffff60f`523e76e8
>     Diana Error Code: DI_END
>     rax=ffffabfb07a91f38 rbx=fffff60f4ce17808 rcx=0000007b07a91f38
>     rdx=fffff60f4ce17808 rsi=0000000000000000 rdi=fffff60f523e7470
>     rip=0000000000000000 rsp=fffff60f523e72a8 rbp=ffff9881af0a0080
>      r8=0000000000000000  r9=fffff60f523e7550 r10=0000000000000000
>     r11=fffff60f523e7550 r12=0000000000000000 r13=ffff800000000000
>     r14=fffff60f4ce17808 r15=0000000000001000
>     cs=0010  ss=0018  ds=002b  es=002b  fs=0053  gs=002b  efl=00040282
>     Commands count: 6
>     Modified pages:
>     Done

Where RAX contains PTE address for specified RCX.

