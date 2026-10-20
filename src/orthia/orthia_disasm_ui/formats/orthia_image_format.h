#pragma once

#include "../orthia_model_interfaces.h"
#include "orthia_simple_file.h"
#include "orthia_pe.h"          // MapFileParameters

namespace orthia
{
    class CModuleManager;

    // How to read an image (PE, ELF, ...): one stateless object per format, found through the
    // registry below. Not to be confused with IImageSource (orthia_image_source.h), which says
    // where a module's bytes come from (the opened file, a linked dependency, ...).
    struct IImageFormat
    {
        virtual ~IImageFormat() = default;
        // stable, lowercase: "pe", "elf". For logs, --format and saved open parameters
        virtual const char* GetId() const = 0;
        // shown to the user: "PE", "ELF"
        virtual const char* GetDisplayName() const = 0;
        // ModuleInfo::builtInFlags_moduleType*, stored in the database
        virtual int GetBuiltInFlag() const = 0;

        // whether the first bytes of a file are this format; false for formats only the user picks
        virtual bool Detect(const char* header, size_t size) const = 0;
        // throws std::exception with a message for the user when the image is malformed
        virtual std::shared_ptr<ISimpleFile> MapFile(const std::vector<char>& data,
            const MapFileParameters& params) const = 0;
        // adds the opened file's own module to a new database; reader reads the mapped file
        virtual void RegisterMainModule(CModuleManager& manager,
            ISimpleFile& file,
            IMemoryReader& reader,
            const PlatformString_type& shortName,
            bool deepAnalysis) const = 0;
    };

    // every format, in detection order
    const std::vector<const IImageFormat*>& GetImageFormats();
    // nullptr when no format recognizes the bytes
    const IImageFormat* FindFormatByData(const char* header, size_t size);
    const IImageFormat* FindFormatById(const std::string& id);
    // transitional, for the code that still passes DIANA_EXECUTABLE_TYPE_* around
    const IImageFormat* FindFormatByExecutableType(int executableType);
}
