https://sourceforge.net/projects/diana-dasm/ 

DiDasm is a small and fast disassembler, useful for Windows kernel developers.
Advantages:
- highly portable, has minimal runtime requirements (C runtime);
- core libraries do not require any external components;
- includes instructions emulator (diana_processor);
- has stream oriented design;
Supported platforms: i386, amd64
Supported instructions: x586/amd64/FPU/MMX/SSE/SSE2

--- Third party libraries ---
1. libpdb 
Path: diana_core/libpdb
Source: https://github.com/shareef12/libpdb
Author:  Christian Sharpsten 
License: MIT License

2. softfloat
Path: libpdb
Source: diana_core/diana_processor/softfloat
Author:  John R. Hauser
License: AS IS, see diana_core\diana_processor\softfloat\softfloat.h
