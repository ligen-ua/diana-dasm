#pragma once

#include "oui_modal.h"
#include "oui_listbox.h"
#include "oui_filesystem.h"

namespace oui
{

    class COpenFileDialog:public oui::SimpleBrush<CModalWindow>, IListBoxOwner
    {
        using Parent_type = oui::SimpleBrush<CModalWindow>;
        std::shared_ptr<DialogColorProfile> m_colorProfile;

        std::shared_ptr<CListBox> m_filesBox;
        FileRecipientHandler_type m_resultCallback;
        std::shared_ptr<IFileSystem> m_fileSystem;

        const String m_rootFile;
    protected:
        void OnResize() override;
        void AsyncQuery(std::function<void(const ListBoxItem*, int)> handler, int offset, int size);
        void CancelAllQueries();
        void ConstructChilds() override;
        void OnDefaultRoot(const String& name, int error);

    public:
        COpenFileDialog(const String& rootFile,
            FileRecipientHandler_type resultCallback,
            std::shared_ptr<IFileSystem> fileSystem);
    };

}