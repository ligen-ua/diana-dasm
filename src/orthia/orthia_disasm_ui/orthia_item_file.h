#pragma once

#include "orthia_model_interfaces.h"
#include "orthia_simple_file.h"

namespace orthia
{
    class CCommonFormatParser;
    class CCommonFormatBuilder;
    class ImageSourceTable;
    struct IImageSource;

    struct FileWorkplaceItem :std::enable_shared_from_this<FileWorkplaceItem>, BaseWorkPlaceItem
    {
        std::shared_ptr<orthia::ISimpleFile> file;
        oui::String fullName, shortName;
        std::shared_ptr<CModuleManager> moduleManager;
        Address_type moduleLastValidAddress = 0;
        std::shared_ptr<CFilePersistentItemStorage> persistentItemStorage;
        // one source per module in tbl_modules; built once after the database is ready
        std::shared_ptr<const ImageSourceTable> imageSources;

        FileWorkplaceItem(std::shared_ptr<CFilePersistentItemStorage> peristentItemStorage_in);

        // Builds imageSources from tbl_modules and the module metainfo (identity, src_kind).
        // Runs the cheap header pre-check of linked PE dependencies, so call it off the UI thread.
        void InitImageSources(int executableType);

        // public interface
        WorkAddressData ReadData(Address_type address, Address_type size) override;
        WorkAddressRangeInfo GetRangeInfo(Address_type address) const override;
        const std::shared_ptr<CModuleManager> GetModuleManager() const override;
        oui::String GetShortName() const override;
        void ReloadModules() override;
        void GetModules(std::vector<orthia::ModuleInfo>& modules) const override;
        int GetModulesCount() const override;
        Address_type GetMainModuleAddress() const override;
        std::shared_ptr<IPeristentItemStorage> GetPersistentStorage() override;
        int GetDianaMode() const override;
        void QueryNames(Address_type moduleAddress, const NameSelectionKey& name, int count, std::vector<NameInfo>& names) const override;
        int QueryNamesCount(Address_type moduleAddress, const NameSelectionKey& name) const override;

        int GetModulesEx(bool calcCount, std::vector<orthia::ModuleInfo>& modules) const;
        MarkupRangeInfo QueryMarkupRange(Address_type address, IMarkupCache* cache = nullptr) const override;
        void QueryMarkupRange(Address_type address, int index, int count, MarkupRange& range, IMarkupCache* cache = nullptr) const override;
        NameInfo QueryAddressName(Address_type address) const override;
        std::shared_ptr<::DianaMovableReadStream> CreateDisasmStream(Address_type addressStart) override;
        Address_type QueryAddressByName(const oui::String& text, Address_type defValue) const override;
        std::shared_ptr<IMemoryReader> CreateMemoryReader() override;
        void UpdateModuleFlags(Address_type moduleAddress, int flagsToSet, int flagsToRemove) override;
        void OnModuleSymbolsLoaded(Address_type moduleAddress) override;
        void QuerySections(Address_type moduleBase, std::vector<SectionInfo>& sections_out) override;

        // Writes the import targets recorded in the database back into the main module's IAT.
        // The first open links them while loading the dependencies; this restores the same image
        // on reopen without touching the dependency files. Returns the number of slots written.
        int ApplyLinkedImports();

    private:
        NameInfo QueryAddressNameImpl(Address_type address) const;

    };

    class CClassicDatabase;
    struct ImageIdentity;

    // extra module metainfo describing the image source (see orthia_image_source.h)
    struct ModuleSourceMeta
    {
        const char* srcKind = nullptr;            // g_meta_src_kind_* or nullptr to omit
        const ImageIdentity* identity = nullptr;  // written as id_* attributes when set
        bool iatSlots = false;                    // main module: import rows are keyed by IAT slot
    };
    void InsertModuleMetaInfo(orthia::intrusive_ptr<CClassicDatabase> database, Address_type moduleAddress, const oui::String & fullName, int moduleFlags, int builtInModuleFlags = 0, const ModuleSourceMeta* source = nullptr);
    void UpdateModuleMetaInfo(orthia::intrusive_ptr<CClassicDatabase> database, Address_type moduleAddress, std::function<bool(CCommonFormatParser&, CCommonFormatBuilder&)> handler);
    void InsertName(orthia::intrusive_ptr<CClassicDatabase> database, Address_type moduleAddress, const orthia::NameInfo& info, Address_type metaInfoAddres);
}