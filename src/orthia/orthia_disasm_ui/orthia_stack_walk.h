#pragma once

#include "oui_processes.h"

namespace orthia
{
    // The registers of an AArch64 thread that the walk starts from
    struct Arm64ThreadRegisters
    {
        Address_type pc = 0;
        Address_type sp = 0;
        // x29 and x30; Linux gives sp and pc only (/proc/<pid>/task/<tid>/syscall)
        Address_type fp = 0;
        Address_type lr = 0;
        bool hasFrameRegisters = false;
    };

    // a return address without its pointer authentication code: user addresses have 48 bits
    Address_type StripArm64PointerAuth(Address_type address);

    // the instruction before address is a call (BL, BLR, BLRAA/BLRAB and their Z forms) in executable memory
    bool IsArm64ReturnAddress(oui::IProcess& process, Address_type address);

    // The return addresses of an AArch64 thread, the innermost first: lr when the thread is in a leaf
    // function, then the frame records {x29, x30} that x29 chains (the Windows ABI always keeps the
    // chain; Linux code mostly does). Without fp and lr, the first record is searched up from sp, and
    // the return address of a leaf function is lost.
    // 0, or an errno-style error when sp is in no memory
    int WalkArm64Stack(oui::IProcess& process, const Arm64ThreadRegisters& registers, int framesCount,
                       std::vector<Address_type>& stack);
}
