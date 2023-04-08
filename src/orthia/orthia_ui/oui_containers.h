#pragma once
#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{
    class CPanelCommonContext;
    class CPanelWindow:public CWindow
    {
        std::function<String()> m_getCaption;
    public:
        CPanelWindow(std::function<String()> getCaption);
        String GetCaption() const;
        void Activate() override;
        void Deactivate() override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
    };
    enum class PanelOrientation
    {
        None,
        Left,
        Right,
        Top,
        Bottom
    };
    struct PanelInfo
    {
        int fixedWidth = 0;
        int fixedHeight = 0;
        PanelInfo()
        {
        }
    };
    
    class CPanelGroupWindow:public CWindow
    {
        friend class CPanelContainerWindow;
        PanelOrientation m_childOrintation = PanelOrientation::None;
        std::shared_ptr<CPanelGroupWindow> m_child;
        std::vector<std::shared_ptr<CPanelWindow>> m_panels;
        int m_fixedWidth = 0;
        int m_fixedHeight = 0;
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
        bool AddPanel(const std::vector<PanelOrientation>& location, 
            std::shared_ptr<CPanelWindow> panel,
            const PanelInfo& info);
        void OnResize() override;
        Rect GetClientRect() const override;
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
    };
}