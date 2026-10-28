#include "orthia_stack_walk.h"

#include <cerrno>
#include <cstring>

namespace orthia
{
    namespace
    {
        const Address_type c_userAddressMask = 0x0000FFFFFFFFFFFFull;
        // how far above sp the first frame record is searched when x29 is unknown
        const Address_type c_frameSearchSize = 0x4000;
        // lr this close below pc is a return into the function pc is in: it called something already
        const Address_type c_sameFunctionDistance = 0x400;

        bool ReadUInt64(oui::IProcess& process, Address_type address, Address_type& value)
        {
            unsigned char bytes[8] = {};
            if (process.ReadExactEx2(address, bytes, sizeof(bytes)))
            {
                return false;
            }
            value = 0;
            for (int i = 7; i >= 0; --i)
            {
                value = (value << 8) | bytes[i];
            }
            return true;
        }

        // the stack region that holds sp: the frame records are in it
        struct StackRange
        {
            Address_type begin = 0;
            Address_type end = 0;
            bool Holds(Address_type address, Address_type size) const
            {
                return address >= begin && address <= end && end - address >= size;
            }
        };

        // a frame record at fp: {the caller's x29, the return address}. The next one is higher on the stack
        bool ReadFrameRecord(oui::IProcess& process, const StackRange& range, Address_type fp,
                             Address_type& nextFp, Address_type& returnAddress)
        {
            if ((fp & 7) || !range.Holds(fp, 16))
            {
                return false;
            }
            if (!ReadUInt64(process, fp, nextFp) || !ReadUInt64(process, fp + 8, returnAddress))
            {
                return false;
            }
            returnAddress = StripArm64PointerAuth(returnAddress);
            return IsArm64ReturnAddress(process, returnAddress);
        }

        // a record that the next one confirms (or the end of the chain): a return address alone may be stale
        bool IsFrameRecord(oui::IProcess& process, const StackRange& range, Address_type fp)
        {
            Address_type nextFp = 0, returnAddress = 0;
            if (!ReadFrameRecord(process, range, fp, nextFp, returnAddress))
            {
                return false;
            }
            if (!nextFp)
            {
                return true;
            }
            Address_type nextNextFp = 0, nextReturnAddress = 0;
            return nextFp > fp && ReadFrameRecord(process, range, nextFp, nextNextFp, nextReturnAddress);
        }
    }

    Address_type StripArm64PointerAuth(Address_type address)
    {
        return address & c_userAddressMask;
    }

    bool IsArm64ReturnAddress(oui::IProcess& process, Address_type address)
    {
        if (address < 4 || (address & 3))
        {
            return false;
        }
        const Address_type call = address - 4;
        oui::MemoryRegionInfo info;
        if (process.QueryMemoryRegion(call, info) || !(info.protect & oui::MemoryRegionInfo::protect_exec))
        {
            return false;
        }
        unsigned char bytes[4] = {};
        if (process.ReadExactEx2(call, bytes, sizeof(bytes)))
        {
            return false;
        }
        const unsigned int instruction = (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8) |
                                         ((unsigned int)bytes[2] << 16) | ((unsigned int)bytes[3] << 24);
        // BL imm26
        if ((instruction & 0xFC000000u) == 0x94000000u)
        {
            return true;
        }
        // BLR Xn
        if ((instruction & 0xFFFFFC1Fu) == 0xD63F0000u)
        {
            return true;
        }
        // BLRAA, BLRAB, BLRAAZ, BLRABZ
        if ((instruction & 0xFEFFF800u) == 0xD63F0800u)
        {
            return true;
        }
        return false;
    }

    int WalkArm64Stack(oui::IProcess& process, const Arm64ThreadRegisters& registers, int framesCount,
                       std::vector<Address_type>& stack)
    {
        stack.clear();
        oui::MemoryRegionInfo info;
        if (process.QueryMemoryRegion(registers.sp, info))
        {
            return EFAULT;
        }
        StackRange range;
        range.begin = registers.sp;
        range.end = info.regionBase + info.regionSize;

        Address_type fp = 0;
        if (registers.hasFrameRegisters)
        {
            // a leaf function keeps its return address in lr and has no record of its own
            const Address_type lr = StripArm64PointerAuth(registers.lr);
            const bool sameFunction = lr <= registers.pc && registers.pc - lr < c_sameFunctionDistance;
            if (!sameFunction && IsArm64ReturnAddress(process, lr))
            {
                Address_type nextFp = 0, recordReturn = 0;
                if (!ReadFrameRecord(process, range, registers.fp, nextFp, recordReturn) || recordReturn != lr)
                {
                    stack.push_back(lr);
                }
            }
            fp = registers.fp;
        }
        else
        {
            const Address_type searchEnd = range.end - registers.sp > c_frameSearchSize ? registers.sp + c_frameSearchSize : range.end;
            for (Address_type slot = (registers.sp + 7) & ~(Address_type)7; slot + 16 <= searchEnd; slot += 8)
            {
                if (IsFrameRecord(process, range, slot))
                {
                    fp = slot;
                    break;
                }
            }
        }

        while (fp && (int)stack.size() < framesCount)
        {
            Address_type nextFp = 0, returnAddress = 0;
            if (!ReadFrameRecord(process, range, fp, nextFp, returnAddress))
            {
                break;
            }
            stack.push_back(returnAddress);
            // the chain goes up the stack; 0 ends it
            if (nextFp <= fp)
            {
                break;
            }
            fp = nextFp;
        }
        if ((int)stack.size() > framesCount)
        {
            stack.resize(framesCount);
        }
        return 0;
    }
}
