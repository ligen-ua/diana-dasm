How to use orthia plugin in windbg session.
------------------
Configuration

Step 1. Load the dll
Load the dll using the load command:
    .load <full-path>\orthia.dll

Note: 32-bit windbg on my win8-64 computer didn't want to load orthia.dll until I put it to the 
    C:\Program Files (x86)\Debugging Tools for Windows\winext
so the best way is to put the dll there

Note2: debugger could be also located in windows kit folder:
    C:\Program Files (x86)\Windows Kits\8.0\Debuggers\


Step2. Setup the profile specifying the database file
    !orthia.profile <full-path>\<profilename>.db
or
    !orthia.profile /f <full-path>\<profilename>.db
/f option allows the command to rewrite the existing file.
Note: profile command supports environment variables like %temp%, for example you can setup the profile like that:
    !orthia.profile /f %temp%\test.db
The initial setup is done.

------------------
Code Analysis

The code analysis interface:
1) reload the module by address:
    !orthia.reload <module_start_address>
Orthia analyses all the references inside the module and puts the into the database. 
Note: module reloading could take some time, for example when working with live debug session. Orthia caches the module sections but initial data acquiring could be slow.
2) reload all modules
    !for_each_module !orthia.reload /v @#Base
3) show the references to the instruction
    !orthia.x <address_expression>
4) show the references to the instruction range
    !orthia.xr <address_expression1> <address_expression2>
5) show all loaded modules
    !orthia.lm
6) analyze custom region
    !orthia.a <address> 
 
If you have any questions or proposals feel free to ask me with ligen.work@gmail.com.
Enjoy!

P.S use \src\start.bat and \src\start_with_kit.cmd to build the solution

------------------
Code Emulator

Orthia Code Emulator is located at VM subsystem. VM subsystem makes possible to manage the virtual environments (VM's) and run the code
(which runs inside the VM, but accesses the windbg source memory as well)

The interface of emulator:
1) Exec Command
Just execute the code starting from the current point (legacy command)

    orthia.exec <number_of_instructions>

2) VM subsystem. 

The entities:
1. Virtual Machines (the containers for memory and code)

The commands for VM management:
    vm_vm_new            Creates a new VM
    vm_vm_del            Deletes the VM
    vm_vm_list           Shows the list of VMs
    vm_vm_info           Shows the information about the VM
    vm_vm_def            Creates and overwrites the "Default" VM with ID == 0
    vm_vm_d(b,w,d,q,p)   Display the contents of memory in the given range
    vm_vm_call           Calls the function inside VM
    vm_vm_u              Displays an assembly translation of the specified program code

2. VM Modules (the set of data regions)

The commands for modules management:
    vm_mod_new           Creates a new module inside VM  
    vm_mod_del           Deletes the module
    vm_mod_list          Shows the list of modules
    vm_mod_info          Shows the information about the module
    
    vm_mod_e(b,w,d,q,p)  Writes the data to the module
    vm_mod_write_bin     Writes the contents of file to the module
    vm_mod_enable        Enables/disables the module. vm_vm_call command affects only enabled modules


How to access samples:
1. install winbg as post mortem debugger
windbg -I

2. produce the crash
build orthia_test.exe
start "orthia_test.ext /crash"

3. see the instructions how to access VM functionality

\src\orthia\orthia_test\main.cpp

    // 2.  How to execute some function
    // .load orthia.dll;!orthia.profile /f %temp%\test.db; 
    // !orthia.vm_vm_def
    // !orthia.vm_vm_call 0 orthia_test!test_vm_fake --print

    
See !vm_help for more information


https://sourceforge.net/p/diana-dasm/wiki/Orthia%20Windbg%20plugin/
ligen-ua