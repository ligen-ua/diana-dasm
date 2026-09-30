#include "test_common.h"
#include "orthia_image_identity.h"
#include "orthia_common_format.h"
#include "orthia_files.h"
#include "orthia_pe.h"
#include "orthia_elf.h"
#include "ot_common.h"

extern "C"
{
#include "diana_pe_defs.h"
}

std::vector<char> LoadElfTestFile(const orthia::PlatformString_type& name);

static orthia::ImageIdentity MakePeIdentity()
{
    orthia::ImageIdentity id;
    id.format = orthia::ImageIdentity::format_Pe;
    id.timeDateStamp = 0x5A3B1C0D;
    id.sizeOfImage = 0x123000;
    id.hasGuid = true;
    id.guid.Data1 = 0x01020304;
    id.guid.Data2 = 0x0506;
    id.guid.Data3 = 0x0708;
    for (int i = 0; i < 8; ++i)
    {
        id.guid.Data4[i] = (unsigned char)(0x10 + i);
    }
    id.age = 3;
    id.sha1Hex = "0123456789abcdef0123456789abcdef01234567";
    return id;
}

static void test_hex_round_trip()
{
    const unsigned char data[] = { 0x00, 0x7f, 0x80, 0xff, 0x0a };
    std::string hex = orthia::BytesToHex(data, sizeof(data));
    DIANA_TEST_ASSERT(hex == "007f80ff0a");

    std::vector<unsigned char> back;
    DIANA_TEST_ASSERT(orthia::HexToBytes(hex, back));
    DIANA_TEST_ASSERT(back.size() == sizeof(data) && memcmp(back.data(), data, sizeof(data)) == 0);

    DIANA_TEST_ASSERT(orthia::HexToBytes("ABcd", back) && back.size() == 2 && back[0] == 0xAB && back[1] == 0xCD);
    DIANA_TEST_ASSERT(!orthia::HexToBytes("abc", back));
    DIANA_TEST_ASSERT(!orthia::HexToBytes("zz", back));
}

static void test_pe_xml_round_trip()
{
    orthia::ImageIdentity id = MakePeIdentity();

    orthia::CCommonFormatBuilder builder;
    builder.AddMetadata(std::string("fullname"), std::string("c:\\windows\\system32\\hal.dll"));
    orthia::WriteImageIdentity(builder, id);
    std::string xml;
    builder.Produce(&xml);

    orthia::CCommonFormatParser parser;
    DIANA_TEST_ASSERT(parser.Parse(xml));

    orthia::ImageIdentity back;
    DIANA_TEST_ASSERT(orthia::ReadImageIdentity(parser, back));
    DIANA_TEST_ASSERT(back.format == orthia::ImageIdentity::format_Pe);
    DIANA_TEST_ASSERT(back.timeDateStamp == id.timeDateStamp);
    DIANA_TEST_ASSERT(back.sizeOfImage == id.sizeOfImage);
    DIANA_TEST_ASSERT(back.hasGuid);
    DIANA_TEST_ASSERT(DIANA_UUID_Compare(&back.guid, &id.guid) == 0);
    DIANA_TEST_ASSERT(back.age == id.age);
    DIANA_TEST_ASSERT(back.sha1Hex == id.sha1Hex);
    DIANA_TEST_ASSERT(orthia::SameIdentity(id, back));

    // the other fields of the record survive
    std::string fullName;
    DIANA_TEST_ASSERT(parser.QueryMetadata(std::string("fullname"), &fullName));
    DIANA_TEST_ASSERT(fullName == "c:\\windows\\system32\\hal.dll");
}

static void test_elf_xml_round_trip()
{
    orthia::ImageIdentity id;
    id.format = orthia::ImageIdentity::format_Elf;
    id.buildIdHex = "21342db32e32e961622c0d19322ad591d56a65a8";

    orthia::CCommonFormatBuilder builder;
    orthia::WriteImageIdentity(builder, id);
    std::string xml;
    builder.Produce(&xml);

    orthia::CCommonFormatParser parser;
    DIANA_TEST_ASSERT(parser.Parse(xml));
    orthia::ImageIdentity back;
    DIANA_TEST_ASSERT(orthia::ReadImageIdentity(parser, back));
    DIANA_TEST_ASSERT(back.format == orthia::ImageIdentity::format_Elf);
    DIANA_TEST_ASSERT(back.buildIdHex == id.buildIdHex);
    DIANA_TEST_ASSERT(back.sha1Hex.empty());
    DIANA_TEST_ASSERT(orthia::SameIdentity(id, back));
}

static void test_old_record_without_identity()
{
    orthia::CCommonFormatBuilder builder;
    builder.AddMetadata(std::string("fullname"), std::string("hal.dll"));
    builder.AddMetadata(std::string("flags"), 0);
    std::string xml;
    builder.Produce(&xml);

    orthia::CCommonFormatParser parser;
    DIANA_TEST_ASSERT(parser.Parse(xml));
    orthia::ImageIdentity back;
    DIANA_TEST_ASSERT(!orthia::ReadImageIdentity(parser, back));
    DIANA_TEST_ASSERT(!back.IsValid());

    // an invalid recorded identity never matches anything
    DIANA_TEST_ASSERT(!orthia::SameIdentity(back, MakePeIdentity()));
}

static void test_same_identity()
{
    const orthia::ImageIdentity recorded = MakePeIdentity();

    orthia::ImageIdentity actual = recorded;
    DIANA_TEST_ASSERT(orthia::SameIdentity(recorded, actual));
    DIANA_TEST_ASSERT(orthia::DescribeIdentityMismatch(recorded, actual).empty());

    actual = recorded;
    actual.timeDateStamp++;
    DIANA_TEST_ASSERT(!orthia::SameIdentity(recorded, actual));
    DIANA_TEST_ASSERT(orthia::DescribeIdentityMismatch(recorded, actual) == "timestamp mismatch");

    actual = recorded;
    actual.sizeOfImage += 0x1000;
    DIANA_TEST_ASSERT(!orthia::SameIdentity(recorded, actual));
    DIANA_TEST_ASSERT(orthia::DescribeIdentityMismatch(recorded, actual) == "size mismatch");

    actual = recorded;
    actual.guid.Data4[7] ^= 0xFF;
    DIANA_TEST_ASSERT(!orthia::SameIdentity(recorded, actual));
    DIANA_TEST_ASSERT(orthia::DescribeIdentityMismatch(recorded, actual) == "debug GUID mismatch");

    // the GUID is ignored when either side has none
    actual = recorded;
    actual.hasGuid = false;
    actual.guid = DIANA_UUID();
    DIANA_TEST_ASSERT(orthia::SameIdentity(recorded, actual));
    orthia::ImageIdentity recordedNoGuid = recorded;
    recordedNoGuid.hasGuid = false;
    actual = recorded;
    actual.guid.Data1 ^= 1;
    DIANA_TEST_ASSERT(orthia::SameIdentity(recordedNoGuid, actual));

    // the age is informational only (public symbols legitimately differ)
    actual = recorded;
    actual.age = 99;
    DIANA_TEST_ASSERT(orthia::SameIdentity(recorded, actual));

    // sha1 is not part of the comparison
    actual = recorded;
    actual.sha1Hex = "ffff";
    DIANA_TEST_ASSERT(orthia::SameIdentity(recorded, actual));

    actual = recorded;
    actual.format = orthia::ImageIdentity::format_Elf;
    DIANA_TEST_ASSERT(!orthia::SameIdentity(recorded, actual));
}

static void test_pe_header_identity()
{
    // a minimal DOS header + PE64 headers, nothing else
    std::vector<char> image(0x200, 0);
    DIANA_IMAGE_DOS_HEADER dos = { 0, };
    dos.e_magic[0] = 'M';
    dos.e_magic[1] = 'Z';
    dos.e_lfanew = 0x80;
    memcpy(image.data(), &dos, sizeof(dos));

    DIANA_IMAGE_NT_HEADERS nt = { 0, };
    memcpy(nt.Signature, "PE\0\0", 4);
    nt.FileHeader.TimeDateStamp = 0x11223344;
    nt.FileHeader.SizeOfOptionalHeader = sizeof(DIANA_IMAGE_OPTIONAL_HEADER64);
    memcpy(image.data() + dos.e_lfanew, &nt, sizeof(nt));

    DIANA_IMAGE_OPTIONAL_HEADER64 opt = { 0, };
    opt.Magic = 0x20b;
    opt.SizeOfImage = 0x45000;
    memcpy(image.data() + dos.e_lfanew + sizeof(nt), &opt, sizeof(opt));

    orthia::ImageIdentity id;
    DIANA_TEST_ASSERT(orthia::QueryPeHeaderIdentity(image.data(), image.size(), id));
    DIANA_TEST_ASSERT(id.format == orthia::ImageIdentity::format_Pe);
    DIANA_TEST_ASSERT(id.timeDateStamp == 0x11223344);
    DIANA_TEST_ASSERT(id.sizeOfImage == 0x45000);
    DIANA_TEST_ASSERT(!id.hasGuid);

    // 32-bit optional header
    DIANA_IMAGE_OPTIONAL_HEADER32 opt32 = { 0, };
    opt32.Magic = 0x10b;
    opt32.SizeOfImage = 0x6000;
    memcpy(image.data() + dos.e_lfanew + sizeof(nt), &opt32, sizeof(opt32));
    DIANA_TEST_ASSERT(orthia::QueryPeHeaderIdentity(image.data(), image.size(), id));
    DIANA_TEST_ASSERT(id.sizeOfImage == 0x6000);

    // truncated before SizeOfImage
    DIANA_TEST_ASSERT(!orthia::QueryPeHeaderIdentity(image.data(), dos.e_lfanew + sizeof(nt) + 0x10, id));
    // not a PE
    image[0] = 'X';
    DIANA_TEST_ASSERT(!orthia::QueryPeHeaderIdentity(image.data(), image.size(), id));
    DIANA_TEST_ASSERT(!orthia::QueryPeHeaderIdentity(nullptr, 0, id));
}

#ifdef WIN32
static void test_pe_identity_of_running_exe()
{
    std::vector<wchar_t> moduleNameBuffer(2048);
    GetModuleFileNameW(GetModuleHandle(0), &moduleNameBuffer.front(), (ULONG)moduleNameBuffer.size());

    std::vector<char> peFile;
    orthia::LoadFileToVector(&moduleNameBuffer.front(), peFile);
    DIANA_TEST_ASSERT(!peFile.empty());

    orthia::ImageIdentity fromHeader;
    DIANA_TEST_ASSERT(orthia::QueryPeHeaderIdentity(peFile.data(), peFile.size(), fromHeader));

    orthia::CSimplePeFile pe;
    orthia::MapFileParameters params;
    params.mapFlags = DIANA_PE_MAP_DO_NOT_RELOCATE;
    pe.MapFile(peFile, params);

    orthia::ImageIdentity fromImage;
    DIANA_TEST_ASSERT(orthia::QueryImageIdentity(pe, fromImage));
    DIANA_TEST_ASSERT(fromImage.format == orthia::ImageIdentity::format_Pe);
    DIANA_TEST_ASSERT(fromImage.timeDateStamp == fromHeader.timeDateStamp);
    DIANA_TEST_ASSERT(fromImage.sizeOfImage == fromHeader.sizeOfImage);
    // SizeOfImage is section aligned, the mapped vector ends at the last section's VirtualSize
    DIANA_TEST_ASSERT(fromImage.sizeOfImage >= pe.GetMappedFile().size());
    DIANA_TEST_ASSERT(orthia::SameIdentity(fromHeader, fromImage));
    // the header variant has no GUID, the mapped one should (this exe is linked with /DEBUG)
    DIANA_TEST_ASSERT(fromImage.hasGuid);

    // WriteImage: patch the first two bytes and read them back through the mapped image
    const char patch[2] = { 'X', 'Y' };
    DIANA_TEST_ASSERT(pe.WriteImage(pe.GetImageBase(), patch, sizeof(patch)));
    DIANA_TEST_ASSERT(pe.GetMappedFile()[0] == 'X' && pe.GetMappedFile()[1] == 'Y');
    // out of range
    DIANA_TEST_ASSERT(!pe.WriteImage(pe.GetImageBase() - 1, patch, sizeof(patch)));
    DIANA_TEST_ASSERT(!pe.WriteImage(pe.GetImageEnd() - 1, patch, sizeof(patch)));
    DIANA_TEST_ASSERT(pe.WriteImage(pe.GetImageEnd() - 2, patch, sizeof(patch)));
}
#endif

static void test_elf_identity()
{
    std::vector<char> data = LoadElfTestFile(ORTHIA_TCSTR("dmesg"));
    DIANA_TEST_ASSERT(!data.empty());

    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    elf.MapFile(data, params);

    orthia::ImageIdentity id;
    DIANA_TEST_ASSERT(orthia::QueryImageIdentity(elf, id));
    DIANA_TEST_ASSERT(id.format == orthia::ImageIdentity::format_Elf);
    DIANA_TEST_ASSERT(id.buildIdHex == "21342db32e32e961622c0d19322ad591d56a65a8");

    orthia::ImageIdentity other = id;
    other.buildIdHex[0] = '0';
    DIANA_TEST_ASSERT(!orthia::SameIdentity(id, other));
    DIANA_TEST_ASSERT(orthia::DescribeIdentityMismatch(id, other) == "build-id mismatch");

    // WriteImage on the ELF image
    const char patch[4] = { 1, 2, 3, 4 };
    DIANA_TEST_ASSERT(elf.WriteImage(elf.GetImageBase() + 0x10, patch, sizeof(patch)));
    DIANA_TEST_ASSERT(memcmp(elf.GetMappedFile().data() + 0x10, patch, sizeof(patch)) == 0);
    DIANA_TEST_ASSERT(!elf.WriteImage(elf.GetImageEnd(), patch, sizeof(patch)));
}

void test_image_identity()
{
    DIANA_TEST(test_hex_round_trip());
    DIANA_TEST(test_pe_xml_round_trip());
    DIANA_TEST(test_elf_xml_round_trip());
    DIANA_TEST(test_old_record_without_identity());
    DIANA_TEST(test_same_identity());
    DIANA_TEST(test_pe_header_identity());
#ifdef WIN32
    DIANA_TEST(test_pe_identity_of_running_exe());
#endif
    DIANA_TEST(test_elf_identity());
}
