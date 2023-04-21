#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{
    struct ListBoxItem
    {
        String text;
        std::function<void()> openHandler;
    };

    struct IListBoxOwner
    {
        virtual ~IListBoxOwner() {}
        virtual void AsyncQuery(std::function<void(const ListBoxItem*, int)> handler, int offset, int size) = 0;
        virtual void CancelAllQueries() = 0;
    };

    class CListBox:public WithBorder<CWindow>
    {
        using Parent_type = WithBorder<CWindow>;

        std::shared_ptr<DialogColorProfile> m_colorProfile;
        Rect m_lastRect;
        IListBoxOwner* m_owner;

        // page state
        int m_offset = 0;
        int m_size = 0;
        std::vector<ListBoxItem> m_pageState;
    protected:
        void OnResize() override;
    public:
        CListBox(std::shared_ptr<DialogColorProfile> colorProfile, IListBoxOwner* owner);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
        void Destroy() override;
    };

}