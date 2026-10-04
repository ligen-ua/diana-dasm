#include "orthia_image_identity.h"
#include "orthia_common_format.h"
#include "orthia_pe.h"
#include "orthia_elf.h"
#include <cstring>

extern "C"
{
#include "diana_pe_defs.h"
}

namespace orthia
{
    namespace
    {
        const char* const g_attr_format = "id_format";
        const char* const g_attr_timestamp = "id_timestamp";
        const char* const g_attr_sizeofimage = "id_sizeofimage";
        const char* const g_attr_guid = "id_guid";
        const char* const g_attr_age = "id_age";
        const char* const g_attr_buildid = "id_buildid";
        const char* const g_attr_sha1 = "sha1";

        const DI_UINT16 g_peMagic32 = 0x10b;
        const DI_UINT16 g_peMagic64 = 0x20b;

        bool ReadUInt32(const CCommonFormatParser& parser, const char* name, DI_UINT32& value)
        {
            unsigned long long tmp = 0;
            if (!parser.QueryMetadata(name, &tmp))
            {
                return false;
            }
            value = (DI_UINT32)tmp;
            return true;
        }
    }

    std::string BytesToHex(const void* pData, size_t size)
    {
        static const char digits[] = "0123456789abcdef";
        const unsigned char* p = (const unsigned char*)pData;
        std::string result;
        result.reserve(size * 2);
        for (size_t i = 0; i < size; ++i)
        {
            result.push_back(digits[p[i] >> 4]);
            result.push_back(digits[p[i] & 0xF]);
        }
        return result;
    }

    bool HexToBytes(const std::string& hex, std::vector<unsigned char>& bytes)
    {
        bytes.clear();
        if (hex.size() % 2)
        {
            return false;
        }
        bytes.reserve(hex.size() / 2);
        for (size_t i = 0; i < hex.size(); i += 2)
        {
            unsigned value = 0;
            for (int k = 0; k < 2; ++k)
            {
                char c = hex[i + k];
                unsigned digit = 0;
                if (c >= '0' && c <= '9') digit = c - '0';
                else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
                else return false;
                value = (value << 4) | digit;
            }
            bytes.push_back((unsigned char)value);
        }
        return true;
    }

    bool QueryImageIdentity(ISimpleFile& file, ImageIdentity& identity)
    {
        identity = ImageIdentity();
        if (auto* pe = dynamic_cast<CSimplePeFile*>(&file))
        {
            auto* impl = pe->GetImpl();
            if (!impl || !impl->mappedPE.pImpl)
            {
                return false;
            }
            identity.format = ImageIdentity::format_Pe;
            identity.timeDateStamp = impl->mappedPE.pImpl->ntHeaders.FileHeader.TimeDateStamp;
            // the header's SizeOfImage (section aligned), what debuggers match on; not diana's
            // sizeOfModule, which is the unaligned end of the last section
            if (impl->mappedPE.pImpl->dianaMode == DIANA_MODE64)
            {
                identity.sizeOfImage = ((Diana_PeFile64_impl*)impl->mappedPE.pImpl)->pOptionalHeader->SizeOfImage;
            }
            else
            {
                identity.sizeOfImage = ((Diana_PeFile32_impl*)impl->mappedPE.pImpl)->pOptionalHeader->SizeOfImage;
            }

            DIANA_UUID guid = { 0, };
            DI_UINT32 age = 0;
            if (pe->QueryGUID(&guid, &age) == DI_SUCCESS)
            {
                identity.hasGuid = true;
                identity.guid = guid;
                identity.age = age;
            }
            return true;
        }
        if (auto* elf = dynamic_cast<CSimpleElfFile*>(&file))
        {
            if (!elf->GetImpl())
            {
                return false;
            }
            identity.format = ImageIdentity::format_Elf;
            std::vector<DI_UINT8> buildId;
            if (elf->QueryBuildId(buildId) == DI_SUCCESS && !buildId.empty())
            {
                identity.buildIdHex = BytesToHex(buildId.data(), buildId.size());
            }
            return true;
        }
        return false;
    }

    bool QueryPeHeaderIdentity(const char* pData, size_t size, ImageIdentity& identity)
    {
        identity = ImageIdentity();
        if (!pData || size < sizeof(DIANA_IMAGE_DOS_HEADER))
        {
            return false;
        }
        DIANA_IMAGE_DOS_HEADER dos;
        memcpy(&dos, pData, sizeof(dos));
        if (dos.e_magic[0] != 'M' || dos.e_magic[1] != 'Z' || dos.e_lfanew < 0)
        {
            return false;
        }
        const size_t ntOffset = (size_t)dos.e_lfanew;
        // signature + file header + the optional header up to and including SizeOfImage
        const size_t needed32 = sizeof(DIANA_IMAGE_NT_HEADERS) + offsetof(DIANA_IMAGE_OPTIONAL_HEADER32, SizeOfImage) + sizeof(DI_UINT32);
        const size_t needed64 = sizeof(DIANA_IMAGE_NT_HEADERS) + offsetof(DIANA_IMAGE_OPTIONAL_HEADER64, SizeOfImage) + sizeof(DI_UINT32);
        if (ntOffset > size || size - ntOffset < sizeof(DIANA_IMAGE_NT_HEADERS) + sizeof(DI_UINT16))
        {
            return false;
        }
        DIANA_IMAGE_NT_HEADERS nt;
        memcpy(&nt, pData + ntOffset, sizeof(nt));
        if (memcmp(nt.Signature, "PE\0\0", 4) != 0)
        {
            return false;
        }
        const char* pOptional = pData + ntOffset + sizeof(DIANA_IMAGE_NT_HEADERS);
        DI_UINT16 magic = 0;
        memcpy(&magic, pOptional, sizeof(magic));

        DI_UINT32 sizeOfImage = 0;
        if (magic == g_peMagic32)
        {
            if (size - ntOffset < needed32) return false;
            memcpy(&sizeOfImage, pOptional + offsetof(DIANA_IMAGE_OPTIONAL_HEADER32, SizeOfImage), sizeof(sizeOfImage));
        }
        else if (magic == g_peMagic64)
        {
            if (size - ntOffset < needed64) return false;
            memcpy(&sizeOfImage, pOptional + offsetof(DIANA_IMAGE_OPTIONAL_HEADER64, SizeOfImage), sizeof(sizeOfImage));
        }
        else
        {
            return false;
        }
        identity.format = ImageIdentity::format_Pe;
        identity.timeDateStamp = nt.FileHeader.TimeDateStamp;
        identity.sizeOfImage = sizeOfImage;
        return true;
    }

    void WriteImageIdentity(CCommonFormatBuilder& builder, const ImageIdentity& identity)
    {
        if (!identity.IsValid())
        {
            return;
        }
        builder.AddMetadata(std::string(g_attr_format), (int)identity.format);
        if (identity.format == ImageIdentity::format_Pe)
        {
            builder.AddMetadata(std::string(g_attr_timestamp), (unsigned long long)identity.timeDateStamp);
            builder.AddMetadata(std::string(g_attr_sizeofimage), (unsigned long long)identity.sizeOfImage);
            if (identity.hasGuid)
            {
                builder.AddMetadata(std::string(g_attr_guid), BytesToHex(&identity.guid, sizeof(identity.guid)));
                builder.AddMetadata(std::string(g_attr_age), (unsigned long long)identity.age);
            }
        }
        else if (identity.format == ImageIdentity::format_Elf)
        {
            if (!identity.buildIdHex.empty())
            {
                builder.AddMetadata(std::string(g_attr_buildid), identity.buildIdHex);
            }
        }
        if (!identity.sha1Hex.empty())
        {
            builder.AddMetadata(std::string(g_attr_sha1), identity.sha1Hex);
        }
    }

    bool ReadImageIdentity(const CCommonFormatParser& parser, ImageIdentity& identity)
    {
        identity = ImageIdentity();
        int format = 0;
        if (!parser.QueryMetadata(std::string(g_attr_format), &format))
        {
            return false;
        }
        identity.format = format;
        if (format == ImageIdentity::format_Pe)
        {
            ReadUInt32(parser, g_attr_timestamp, identity.timeDateStamp);
            ReadUInt32(parser, g_attr_sizeofimage, identity.sizeOfImage);
            std::string guidHex;
            std::vector<unsigned char> guidBytes;
            if (parser.QueryMetadata(std::string(g_attr_guid), &guidHex) &&
                HexToBytes(guidHex, guidBytes) &&
                guidBytes.size() == sizeof(identity.guid))
            {
                memcpy(&identity.guid, guidBytes.data(), sizeof(identity.guid));
                identity.hasGuid = true;
                ReadUInt32(parser, g_attr_age, identity.age);
            }
        }
        else if (format == ImageIdentity::format_Elf)
        {
            parser.QueryMetadata(std::string(g_attr_buildid), &identity.buildIdHex);
        }
        else
        {
            identity.format = ImageIdentity::format_Unknown;
            return false;
        }
        parser.QueryMetadata(std::string(g_attr_sha1), &identity.sha1Hex);
        return true;
    }

    std::string DescribeIdentityMismatch(const ImageIdentity& recorded, const ImageIdentity& actual)
    {
        if (recorded.format != actual.format)
        {
            return "format mismatch";
        }
        if (recorded.format == ImageIdentity::format_Pe)
        {
            if (recorded.timeDateStamp != actual.timeDateStamp)
            {
                return "timestamp mismatch";
            }
            if (recorded.sizeOfImage != actual.sizeOfImage)
            {
                return "size mismatch";
            }
            if (recorded.hasGuid && actual.hasGuid &&
                DIANA_UUID_Compare(&recorded.guid, &actual.guid) != 0)
            {
                return "debug GUID mismatch";
            }
            return std::string();
        }
        if (recorded.format == ImageIdentity::format_Elf)
        {
            if (recorded.buildIdHex != actual.buildIdHex)
            {
                return "build-id mismatch";
            }
            return std::string();
        }
        return "unknown format";
    }

    bool SameIdentity(const ImageIdentity& recorded, const ImageIdentity& actual)
    {
        return recorded.IsValid() && DescribeIdentityMismatch(recorded, actual).empty();
    }
}
