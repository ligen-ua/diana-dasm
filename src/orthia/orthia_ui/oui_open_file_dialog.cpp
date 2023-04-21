#include "oui_open_file_dialog.h"

namespace oui
{
    void COpenFileDialog::OnResize()
    {
        const auto clientRect = GetClientRect();

        if (clientRect.size.width < 5 || clientRect.size.height < 5)
        {
            Size zeroSize;
            m_filesBox->Resize(zeroSize);
            return;
        }

        Rect boxRect = clientRect;
        boxRect.position.x += 2;
        boxRect.position.y += 4;
        boxRect.size.width -= 4;
        boxRect.size.height -= 4;

        m_filesBox->MoveTo(boxRect.position);
        m_filesBox->Resize(boxRect.size);

        Parent_type::OnResize();
    }
    void COpenFileDialog::AsyncQuery(std::function<void(const ListBoxItem*, int)> handler, int offset, int size)
    {
    }
    void COpenFileDialog::CancelAllQueries()
    {
    }
    void COpenFileDialog::OnDefaultRoot(const String& name, int error)
    {

    }

    COpenFileDialog::COpenFileDialog(const String& rootFile, 
        FileRecipientHandler_type resultCallback,
        std::shared_ptr<IFileSystem> fileSystem)
        :
            m_resultCallback(resultCallback),
            m_fileSystem(fileSystem),
            m_rootFile(rootFile)
    {
        m_colorProfile = std::make_shared<DialogColorProfile>();
        QueryDefaultColorProfile(*m_colorProfile);

        IListBoxOwner* owner = this;
        m_filesBox = std::make_shared<CListBox>(m_colorProfile, owner);
    }
    void COpenFileDialog::ConstructChilds()
    {
        AddChild(m_filesBox);

        if (m_rootFile.native.empty())
        {
            m_fileSystem->AsyncQueryDefaultRoot(GetThread(), 
                [pthis=GetPtr_t<COpenFileDialog>(this)](const String& name, int error) 
                    { pthis->OnDefaultRoot(name, error); }
            );
        }
        else
        {
            OnDefaultRoot(m_rootFile, 0);
        }
    }
}