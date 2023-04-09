#pragma once
#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{
    class CPanelCommonContext;
    class CPanelWindow:public CWindow
    {
        std::function<String()> m_getCaption;
        Size m_preferredSize;
    public:
        CPanelWindow(std::function<String()> getCaption);
        String GetCaption() const;
        void Activate() override;
        void Deactivate() override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;

        void SetPreferredSize(const Size& preferredSize);
        Size GetPreferredSize() const;
    };
    enum class PanelOrientation
    {
        None,
        Left,
        Right,
        Top,
        Bottom
    };

    PanelOrientation Reverse(const PanelOrientation& panelOrientation);

    struct PanelInfo
    {
        int preferredWidth = 0;
        int preferredHeight = 0;
        PanelInfo()
        {
        }
    };
    
    class CPanelGroupWindow:public CWindow
    {
        struct ResizeState
        {
            Size panelSize;
            std::shared_ptr<CPanelGroupWindow> resizeTarget;
            std::shared_ptr<CPanelGroupWindow> applyTarget;
            
            bool SaveState();
        };
        void ApplyState(const ResizeState& state);
        ResizeState GetHeaderResizeState();

        friend class CPanelContainerWindow;
        PanelOrientation m_childOrintation = PanelOrientation::None;
        std::shared_ptr<CPanelGroupWindow> m_child;
        std::vector<std::shared_ptr<CPanelWindow>> m_panels;
        
        Size m_precalcSize;
        bool m_drawLeftBorder = false; 
        String m_chunk;
        std::shared_ptr<PanelColorProfile> m_panelColorProfile;
        int m_activePanelIndex = 0;
        Point m_lastMouseMovePoint;

        // draw result
        int m_lastTabY = 0;
        std::vector<Range> m_lastTabRanges;
        std::shared_ptr<CPanelCommonContext> m_panelCommonContext;

        void AdjustHorizontally(const Rect& clientRect, CWindow* left, CWindow* right, int leftWidth, int rightWidth);
        void AdjustVertically(const Rect& clientRect, CWindow* top, CWindow* bottom, int topHeight, int bottomHeight);

        void PaintTitle(const Rect& rect, DrawParameters& parameters);
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;

        void ReserveTitleSpace(CWindow* wnd, Point& position, Size& size);
        void CalcSize();
        bool HasPreferredSize();

        // drag handlers
        bool Drag_ResizeHandler_TopBottom(DragEvent event,
            const Point& initialPoint,
            const Point& currentPoint,
            std::shared_ptr<CWindow> wnd,
            const ResizeState& originalState);
    public:
        CPanelGroupWindow(std::shared_ptr<PanelColorProfile> panelColorProfile,
            std::shared_ptr<CPanelCommonContext> panelCommonContext);
        bool HasPanels() const;
        Rect GetClientRect() const;
        void ConstuctChilds() override;
        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;
        void AddPanel(std::shared_ptr<CPanelWindow> panel,
            const PanelInfo& info);
        void OnResize() override;
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        void Activate() override;
        std::shared_ptr<CPanelWindow> GetActivePanel();
        std::shared_ptr<CPanelCommonContext> GetPanelCommonContext();
        bool SwitchPanel(int index);
    };
    class CPanelCommonContext
    {
        std::set<std::shared_ptr<CPanelGroupWindow>> m_allGroups;

        std::weak_ptr<CPanelWindow> m_currentActivePanel;
        std::weak_ptr<CPanelGroupWindow> m_currentActiveGroup;
    public:
        CPanelCommonContext();

        // context switching 
        void Register(std::shared_ptr<CPanelGroupWindow> group);
        void ActivateNextGroup(std::shared_ptr<CPanelGroupWindow> caller);

        // deactivation logic
        void OnActivate(std::shared_ptr<CPanelGroupWindow> group);
        void OnActivate(std::shared_ptr<CPanelWindow> panel);

    };

    class CPanelContainerWindow:public oui::WithBorder<oui::SimpleBrush<CWindow>>
    {
        using Parent_type = oui::WithBorder<oui::SimpleBrush<CWindow>>;

        std::shared_ptr<CPanelGroupWindow> m_rootGroup;
        std::shared_ptr<PanelColorProfile> m_panelColorProfile;
        std::shared_ptr<CPanelCommonContext> m_panelCommonContext;
        
        bool HasPanels() const;
    public:
        CPanelContainerWindow();
        void ConstuctChilds() override;
        std::shared_ptr<CPanelGroupWindow> AddGroup(std::shared_ptr<CPanelGroupWindow> rootGroup,
            const PanelOrientation& location);
        void AddPanel(std::shared_ptr<CPanelGroupWindow> group,
            const PanelOrientation& location,
            std::shared_ptr<CPanelWindow> panel,
            const PanelInfo& info);
        void OnResize() override;
        Rect GetClientRect() const override;
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
    };
}