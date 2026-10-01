#pragma once

#include "orthia_interfaces.h"

namespace orthia
{

struct ImageSection
{
    PlatformString_type name;
    Address_type address = 0;   // 0: not loaded (an ELF section without SHF_ALLOC)
    Address_type size = 0;
    PlatformString_type flagsShort;
    std::vector<std::pair<PlatformString_type, PlatformString_type>> attributes;
};

struct ImageSections
{
    std::vector<ImageSection> sections;
    // ELF only: the rows are program headers read from memory, because the section headers
    // could not be read; reason says why
    bool segments = false;
    PlatformString_type reason;
    // ELF only: the file the section headers were read from
    PlatformString_type source;
};

// PE section headers are mapped with the image, so they are read from memory.
// ELF section headers are not part of any PT_LOAD segment: in a process (or a dump of one)
// the memory at e_shoff is something else. They are read from imageFile, after checking that
// its ELF and program headers are the ones mapped at moduleBase; otherwise the program headers
// from memory are listed instead. imageFile may be empty.
void QueryImageSections(IMemoryReader* reader,
    Address_type moduleBase,
    const PlatformString_type& imageFile,
    ImageSections& result);

}
