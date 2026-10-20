#pragma once

#include "orthia_utils.h"
#include <functional>
#include <memory>
#include <vector>

namespace orthia
{

// Read-only random access to a file of the debug target (a module's own image).
// The target may be remote, so such files are never read directly: the UI implements this
// over oui::IFileSystem. Host-side files, such as PDBs the user collected in symbol folders,
// may be read with std::filesystem.
struct IFileRangeReader
{
    virtual ~IFileRangeReader() {}
    virtual unsigned long long GetSize() const = 0;
    // reads exactly data.size() bytes at offset; false on an error or a short read
    virtual bool Read(unsigned long long offset, std::vector<char>& data) = 0;
};

// opens a target file for reading; nullptr when it can't be opened
using FileOpener_type = std::function<std::unique_ptr<IFileRangeReader>(const PlatformString_type& name)>;

}
