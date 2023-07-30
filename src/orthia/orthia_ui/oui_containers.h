#pragma once
#include "oui_window.h"
#include "oui_win_styles.h"
#include "oui_layouts.h"

namespace oui
{
    struct PanelLayout;
    class CPanelCommonContext;
    class CPanelContainerWindow;
    class CPanelWindow;


    class CPanelWindow:public CWindow
    {
        std::function<String()> m_getCaption;
        std::weak_ptr<CPanelCommonContext> m_panelCommonContext;

        void ActivateImpl();
    public:
        CPanelWindow(std::function<String()> getCaption);
        void InitContext(std::shared_ptr<CPanelCommonContext> panelCommonContext);
        String GetCaption() const;
        void Activate() override;
        void Deactivate() override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
        void OnChildFocused() override;
        void SetVisible(bool value) override;
    };

    class CPanelGroupWindow:public CWindow
    {
        struct ResizeState
        {
            Size panelSize;
            std::shared_ptr<PanelLayout> resizeTarget;
            std::shared_ptr<CWindow> applyTarget;
            bool resizeTargetIsMe = false;

            bool SaveState();
        };

        static const int statef_Vertical = 1;
        static const int statef_Horizontal = 2;
        void ApplyState(const ResizeState& state, int flags);

        ResizeState GetHeaderResizeState();
        ResizeState GetHorizontalResizeState();

        friend class CPanelContainerWindow;
        std::vector<std::shared_ptr<CPanelWindow>> m_panels;
        
        Size m_precalcSize;
        bool m_drawLeftBorder = false; 
        
        // temporary data for title painting
        static String m_chunk;
        
        std::shared_ptr<PanelColorProfile> m_panelColorProfile;
        int m_activePanelIndex = 0;
        Point m_lastMouseMovePoint;
        GroupInfo m_groupInfo;

        String m_tag;
        // draw result
        int m_lastTabY = 0;
        std::vector<Range> m_lastTabRanges;
        std::weak_ptr<CPanelCommonContext> m_panelCommonContext;
        std::weak_ptr<PanelLayout> m_layout;

        void PaintTitle(const Rect& rect, DrawParameters& parameters);
        void PaintLeftBorder(const Rect& rect, DrawParameters& parameters);
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;

        // drag handlers
        bool Drag_ResizeHandler_TopBottom(DragEvent event,
            const Point& initialPoint,
            const Point& currentPoint,
            std::shared_ptr<CWindow> wnd,
            const ResizeState& originalState);
        bool Drag_ResizeHandler_LeftRight(DragEvent event,
            const Point& initialPoint,
            const Point& currentPoint,
            std::shared_ptr<CWindow> wnd,
            const ResizeState& originalState);
    public:
        CPanelGroupWindow(std::shared_ptr<PanelColorProfile> panelColorProfile,
            std::shared_ptr<CPanelCommonContext> panelCommonContext);

        CPanelGroupWindow& SetTag(const String& tag) { m_tag = tag; return *this;  }
        String GetTag() const { return m_tag; }

        void SetLayout(std::shared_ptr<PanelLayout> layout);

        CPanelGroupWindow& SetInfo(const GroupInfo& panelInfo);
        const GroupInfo& Info() const;
        GroupInfo& Info();

        void SetPreferredSize(const Size& size);

        bool HasPanels() const;
        int GetPanelsCount() const;
        Rect GetClientRect() const;
        void ConstructChilds() override;
        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;
        void AddPanel(std::shared_ptr<CPanelWindow> panel);
        void OnResize() override;
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        void Activate() override;
        std::shared_ptr<CPanelWindow> GetActivePanel();
        std::shared_ptr<CPanelCommonContext> GetPanelCommonContext();
        bool SwitchPanel(int index);

        void SetLeftBorderState(bool state);
    };

    class CPanelCommonContext
    {
        std::set<std::shared_ptr<CPanelGroupWindow>> m_allGroups;

        std::weak_ptr<CPanelWindow> m_currentActivePanel;
        std::weak_ptr<CPanelGroupWindow> m_currentActiveGroup;

        std::shared_ptr<PanelLayout> m_rootLayout;
        std::weak_ptr<CPanelContainerWindow> m_panelContainerWindow;

    public:
        CPanelCommonContext();

        void Register(std::shared_ptr<CPanelContainerWindow> containerWindow);
        std::shared_ptr<CPanelContainerWindow> GetContainerWindow();

        // context switching 
        void Register(std::shared_ptr<CPanelGroupWindow> group);
        void ActivateNextGroup(std::shared_ptr<CPanelGroupWindow> caller);

        // deactivation logic
        void OnActivate(std::shared_ptr<CPanelGroupWindow> group);
        void OnActivate(std::shared_ptr<CPanelWindow> panel);

        std::shared_ptr<PanelLayout> GetRootLayout();
        void SetRootLayout(std::shared_ptr<PanelLayout> layout);

        decltype(m_allGroups)::iterator GroupBegin() { return m_allGroups.begin(); }
        decltype(m_allGroups)::iterator GroupEnd() { return m_allGroups.end(); }
    };

    
    enum class GroupLocation
    {
        Left,
        Right,
        Top,
        Bottom
    };
    enum class GroupAttachMode
    {
        Sibling,
        Child
    };
    class CPanelContainerWindow:public oui::WithBorder<oui::SimpleBrush<CWindow>>
    {
        struct GroupLocationContext
        {
            std::shared_ptr<PanelLayout> layout;
            std::shared_ptr<PanelLayout> parentLayout;
            std::function<void(std::shared_ptr<PanelLayout>)> replaceLayout;
            std::function<void(std::shared_ptr<PanelLayout>)> replaceParentLayout;
        };
        using Parent_type = oui::WithBorder<oui::SimpleBrush<CWindow>>;

        std::shared_ptr<PanelColorProfile> m_panelColorProfile;
        std::shared_ptr<CPanelCommonContext> m_panelCommonContext;
        std::shared_ptr<CPanelGroupWindow> m_defaultGroup;

        int m_groupsCount = 0;
        bool m_repositionCacheValid = false;

        bool HasPanels() const;
        std::shared_ptr<CPanelGroupWindow> AttachPanelImpl(std::shared_ptr<PanelLayout> layout, 
            GroupLocation location,
            std::function<void(std::shared_ptr<PanelLayout>)> replaceLayout);
        
        bool QueryContextImpl(std::shared_ptr<CPanelGroupWindow> group,
            GroupLocationContext &ctx);
        bool QueryContextImpl(std::shared_ptr<CPanelGroupWindow> group,
            decltype(PanelLayout::data)::iterator parentIt,
            GroupLocationContext& ctx,
            int level);

    public:
        CPanelContainerWindow();
        void ConstructChilds() override;
        int GetGroupsCount() const { return m_groupsCount; }

        std::shared_ptr<CPanelGroupWindow> CreateDefaultGroup();

        std::shared_ptr<CPanelCommonContext> GetCommonContext() { return m_panelCommonContext;  }

        std::shared_ptr<CPanelGroupWindow> AttachNewGroup(std::shared_ptr<CPanelGroupWindow> where,
            GroupLocation location,
            GroupAttachMode mode);

        void OnResize() override;
        Rect GetClientRect() const override;
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        void OnVisibleChange(CPanelWindow* panel);

    };
}