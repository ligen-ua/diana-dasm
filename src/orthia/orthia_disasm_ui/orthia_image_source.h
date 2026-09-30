#pragma once

#include <atomic>
#include <mutex>
#include "orthia_model_interfaces.h"
#include "orthia_image_identity.h"
#include "orthia_simple_file.h"

namespace orthia
{
    // Where a module's bytes come from. The layout (which modules exist and at which base)
    // lives in tbl_modules; the source says how to get the image for a range.
    enum class ImageSourceKind : int
    {
        None = 0,         // unresolved dependency, or an old database without identity
        Main = 1,         // the opened file itself (target.bin)
        LinkedFile = 2,   // the dependency file on disk, loaded lazily if its identity still matches
        Captured = 3,     // reserved: raw file bytes from the shared image store (phase 2)
        MemoryImage = 4   // reserved: an image captured from a live process (phase 3)
    };

    enum class ImageState : int
    {
        Unavailable = 0,  // nothing to load (kind None)
        NotLoaded = 1,    // not touched yet
        Ready = 2,        // image in memory
        Stale = 3,        // the file is missing or is another build than the one recorded
        Failed = 4        // the file could not be mapped
    };

    // module metainfo XML attributes (see InsertModuleMetaInfo)
    inline const char* const g_meta_src_kind = "src_kind";
    inline const char* const g_meta_src_kind_linked = "linked";
    inline const char* const g_meta_src_kind_none = "none";
    // main module only: fnc_Import rows carry the IAT slot address in meta_address,
    // so the linked slots can be replayed into the image on reopen
    inline const char* const g_meta_iat_slots = "iat_slots";

    struct IImageSource
    {
        virtual ~IImageSource() = default;
        virtual ImageSourceKind GetKind() const = 0;
        virtual Address_type GetBase() const = 0;
        virtual Address_type GetSize() const = 0;
        // never does I/O: safe for the UI and `lm`
        virtual ImageState GetState() const = 0;
        virtual PlatformString_type GetStateReason() const = 0;
        // lazy: may read and map the file on first call; nullptr unless the image is Ready
        virtual std::shared_ptr<const ISimpleFile> GetImage() = 0;

        bool Contains(Address_type address) const
        {
            return address >= GetBase() && address - GetBase() < GetSize();
        }
        Address_type GetLastValidAddress() const
        {
            return GetSize() ? GetBase() + GetSize() - 1 : GetBase();
        }
    };

    // the opened file, always in memory
    class CMainImageSource : public IImageSource
    {
        std::shared_ptr<ISimpleFile> m_file;
    public:
        explicit CMainImageSource(std::shared_ptr<ISimpleFile> file);
        ImageSourceKind GetKind() const override { return ImageSourceKind::Main; }
        Address_type GetBase() const override;
        Address_type GetSize() const override;
        ImageState GetState() const override { return ImageState::Ready; }
        PlatformString_type GetStateReason() const override { return PlatformString_type(); }
        std::shared_ptr<const ISimpleFile> GetImage() override { return m_file; }
    };

    // a range with no image: an unresolved dependency, or one recorded before identities existed
    class CNullImageSource : public IImageSource
    {
        Address_type m_base, m_size;
        PlatformString_type m_reason;
    public:
        CNullImageSource(Address_type base, Address_type size, const PlatformString_type& reason);
        ImageSourceKind GetKind() const override { return ImageSourceKind::None; }
        Address_type GetBase() const override { return m_base; }
        Address_type GetSize() const override { return m_size; }
        ImageState GetState() const override { return ImageState::Unavailable; }
        PlatformString_type GetStateReason() const override { return m_reason; }
        std::shared_ptr<const ISimpleFile> GetImage() override { return nullptr; }
    };

    // a dependency on disk: mapped on first touch, relocated to the recorded base, and served
    // only when the file is still the build recorded at first open
    class CLinkedFileImageSource : public IImageSource
    {
        Address_type m_base, m_size;
        PlatformString_type m_path;
        ImageIdentity m_identity;
        int m_executableType;
        int m_dianaMode;

        std::atomic<int> m_state;
        mutable std::mutex m_lock;        // guards m_image and m_reason, held for the whole load
        std::shared_ptr<const ISimpleFile> m_image;
        PlatformString_type m_reason;

        void SetState(ImageState state, const std::string& reason);
        std::shared_ptr<const ISimpleFile> LoadImpl();
    public:
        CLinkedFileImageSource(Address_type base,
            Address_type size,
            const PlatformString_type& path,
            const ImageIdentity& identity,
            int executableType,
            int dianaMode);

        ImageSourceKind GetKind() const override { return ImageSourceKind::LinkedFile; }
        Address_type GetBase() const override { return m_base; }
        Address_type GetSize() const override { return m_size; }
        ImageState GetState() const override { return (ImageState)m_state.load(); }
        PlatformString_type GetStateReason() const override;
        std::shared_ptr<const ISimpleFile> GetImage() override;
        const PlatformString_type& GetPath() const { return m_path; }

        // Cheap check of the file's headers against the recorded identity, without mapping:
        // marks the source Stale when the file is missing or another build. PE only.
        void PreCheckHeader();
    };

    // all sources of an item, immutable after construction, sorted by base
    class ImageSourceTable
    {
        std::vector<std::shared_ptr<IImageSource>> m_sources;
        std::shared_ptr<IImageSource> m_main;
    public:
        void Add(std::shared_ptr<IImageSource> source);
        void SetMain(std::shared_ptr<IImageSource> main);
        void Sort();

        IImageSource* GetMain() const { return m_main.get(); }
        // the source whose range contains the address, or nullptr
        IImageSource* Find(Address_type address) const;
        // the first source starting after the address, or nullptr
        IImageSource* FindNext(Address_type address) const;
        const std::vector<std::shared_ptr<IImageSource>>& GetAll() const { return m_sources; }
    };

    // Routes reads to the source owning the address; a read never crosses a module boundary,
    // like CMemoryReaderOnLoadedData clamps at the image end.
    class CCompositeMemoryReader : public IMemoryReader
    {
        std::shared_ptr<const ImageSourceTable> m_table;
    public:
        explicit CCompositeMemoryReader(std::shared_ptr<const ImageSourceTable> table);
        void Read(Address_type offset,
            Address_type bytesToRead,
            void* pBuffer,
            Address_type* pBytesRead,
            int flags,
            Address_type selectorValue,
            DianaUnifiedRegister selectorHint) override;
    };

    // `lm` and the modules window status: unresolved | linked | stale | unlinked, then analysis, symbols
    PlatformString_type ModuleStatusText(const ModuleInfo& mod);
    // the image source line of `modinfo`, e.g. "linked C:\...\hal.dll" or "stale (timestamp mismatch)"
    PlatformString_type ModuleImageSourceText(const ModuleInfo& mod);
}
