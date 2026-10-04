#pragma once

#include <string>
#include <vector>
extern "C"
{
#include "diana_core.h"
#include "diana_uids.h"
}

namespace orthia
{
    class ISimpleFile;
    class CCommonFormatBuilder;
    class CCommonFormatParser;

    // What identifies a build of an executable image, so that a dependency found on disk
    // later can be checked against the one whose exports were recorded at first open.
    // PE: TimeDateStamp + SizeOfImage (what debuggers use), plus the RSDS GUID when present.
    // ELF: the GNU build-id note.
    struct ImageIdentity
    {
        enum Format
        {
            format_Unknown = 0,
            format_Pe = 1,
            format_Elf = 2
        };

        int format = format_Unknown;

        // PE
        DI_UINT32 timeDateStamp = 0;
        DI_UINT32 sizeOfImage = 0;
        bool hasGuid = false;
        DIANA_UUID guid = { 0, };
        DI_UINT32 age = 0;

        // ELF: lowercase hex
        std::string buildIdHex;

        // raw file, lowercase hex; optional, filled by the caller who has the file bytes
        std::string sha1Hex;

        bool IsValid() const { return format != format_Unknown; }
    };

    // Reads the identity of an already mapped image (dynamic_cast to the PE/ELF class inside).
    // Returns false for an unsupported format or an unmapped object.
    bool QueryImageIdentity(ISimpleFile& file, ImageIdentity& identity);

    // The cheap variant: parses only the DOS/NT headers of a PE file from its first bytes,
    // without mapping. Enough for TimeDateStamp and SizeOfImage; the GUID is not filled.
    bool QueryPeHeaderIdentity(const char* pData, size_t size, ImageIdentity& identity);

    // Persistence in the module metainfo XML (attributes id_*).
    void WriteImageIdentity(CCommonFormatBuilder& builder, const ImageIdentity& identity);
    // Returns false when the XML has no identity fields (an old database).
    bool ReadImageIdentity(const CCommonFormatParser& parser, ImageIdentity& identity);

    // True when `actual` is the same build as `recorded`. The GUID takes part only when
    // both sides have one; the sha1 field is not compared.
    bool SameIdentity(const ImageIdentity& recorded, const ImageIdentity& actual);

    // Describes the first difference, for logs and `modinfo`; empty when identical.
    std::string DescribeIdentityMismatch(const ImageIdentity& recorded, const ImageIdentity& actual);

    // lowercase hex <-> bytes, used for the GUID and build-id attributes
    std::string BytesToHex(const void* pData, size_t size);
    bool HexToBytes(const std::string& hex, std::vector<unsigned char>& bytes);
}
