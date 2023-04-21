#include "oui_containers.h"
#include "oui_layouts_calc.h"

namespace oui
{
    static const auto g_left = String::char_type('<');
    static const auto g_right = String::char_type('>');
    static const auto g_braceLeft = String::char_type('[');
    static const auto g_braceRight = String::char_type(']');

    int g_defaultPreferredWidth = 20;
    int g_defaultPreferredHeight = 10;

    CPanelWindow::CPanelWindow(std::function<String()> getCaption)
        :
        m_getCaption(getCaption)
    {
    }
    String CPanelWindow::GetCaption() const
    {
        return m_getCaption();
    }
    void CPanelWindow::Activate()
    {
        if (!IsActive())
        {
            CWindow::Activate();
            if (auto parent = GetParent_t<CPanelGroupWindow>(this))
            {
                parent->GetPanelCommonContext()->OnActivate(GetPtr_t<CPanelWindow>(this));
                parent->Activate();
            }
        }
        CWindow::SetFocus();
    }
    void CPanelWindow::Deactivate()
    {
        if (IsActive())
        {
            CWindow::Deactivate();
            if (auto parent = GetParent())
            {
                parent->Invalidate();
            }
        }
    }
    bool CPanelWindow::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        if (evt.mouseEvent.button == MouseButton::Left && evt.mouseEvent.state == MouseState::Pressed)
        {
            Activate();
            return true;
        }
        if (evt.mouseEvent.button == MouseButton::Move)
        {
            return true;
        }
        return CWindow::HandleMouseEvent(rect, evt);
    }
    // CPanelGroupWindow
    CPanelGroupWindow::CPanelGroupWindow(std::shared_ptr<PanelColorProfile> panelColorProfile,
        std::shared_ptr<CPanelCommonContext> panelCommonContext)
        :
        m_panelColorProfile(panelColorProfile),
        m_panelCommonContext(panelCommonContext)
    {
    }
    void CPanelGroupWindow::Activate()
    {
        if (!IsActive())
        {
            CWindow::Activate();
            auto activePanel = GetActivePanel();
            if (activePanel)
            {
                activePanel->Activate();
            }
            GetPanelCommonContext()->OnActivate(GetPtr_t<CPanelGroupWindow>(this));
        }
    }
    void CPanelGroupWindow::ConstructChilds()
    {
        // first panel should be visible
        bool visiblePanel = true;
        for (auto& panel : m_panels)
        {
            AddChild(panel);
            panel->SetVisible(visiblePanel);
            visiblePanel = false;
        }
    }
    void CPanelGroupWindow::AddPanel(std::shared_ptr<CPanelWindow> panel)
    {
        m_panels.push_back(panel);
    }
    bool CPanelGroupWindow::HasPanels() const
    {
        return !m_panels.empty();
    }
    void CPanelGroupWindow::SetPreferredSize(const Size& size)
    {
        m_groupInfo.preferredSize = size;
    }
    CPanelGroupWindow& CPanelGroupWindow::SetInfo(const GroupInfo& panelInfo)
    {
        m_groupInfo = panelInfo;
        return *this;
    }
    void CPanelGroupWindow::SetLayout(std::shared_ptr<PanelLayout> layout)
    {
        m_layout = layout;
    }
    const GroupInfo& CPanelGroupWindow::Info() const
    {
        return m_groupInfo;
    }
    GroupInfo& CPanelGroupWindow::Info()
    {
        return m_groupInfo;
    }
    Rect CPanelGroupWindow::GetClientRect() const
    {
        Rect rect = CWindow::GetClientRect();
        return rect;
    }

    std::shared_ptr<CPanelWindow> CPanelGroupWindow::GetActivePanel()
    {
        auto size = m_panels.size();
        if (m_activePanelIndex < 0 || m_activePanelIndex >= size)
        {
            return 0;
        }
        return m_panels[m_activePanelIndex];
    }

    void CPanelGroupWindow::OnResize()
    {
        // resize childs
        const auto size = GetSize();
        const Rect clientRect = GetClientRect();

        // got some panel
        auto activePanel = GetActivePanel();
        if (activePanel)
        {
            Rect clientRectWithTitle = clientRect;
            ++clientRectWithTitle.position.y;
            --clientRectWithTitle.size.height;
            activePanel->MoveTo(clientRectWithTitle.position);
            activePanel->Resize(clientRectWithTitle.size);
            return;
        }
    }
    void CPanelGroupWindow::ApplyState(const ResizeState& state)
    {
        auto newState = state;
        if (newState.panelSize.width <= 0)
        {
            newState.panelSize.width = 1;
        }
        if (newState.panelSize.height <= 0)
        {
            newState.panelSize.height = 1;
        }

        ResizeLayoutY(state.resizeTarget, newState.panelSize.height, state.resizeTargetIsMe);
        state.applyTarget->ForceResize();
    }

    bool CPanelGroupWindow::ResizeState::SaveState()
    {
        if (resizeTarget && applyTarget)
        {
            panelSize = resizeTarget->rect.size;
            return true;
        }
        return false;
    }
    CPanelGroupWindow::ResizeState CPanelGroupWindow::GetHeaderResizeState()
    {
        CPanelGroupWindow::ResizeState result;

        auto layout = m_layout.lock();
        if (!layout)
        {
            return result;
        }

        // climb up for a proper layout
        bool resizeTargetIsMe = true;
        for (;;)
        {
            auto parentLayout = layout->parentLayout.lock();
            if (!parentLayout)
            {
                return result;
            }

            auto it = parentLayout->data.begin(), it_end = parentLayout->data.end();
            for (;
                it != it_end;
                ++it)
            {
                if (*it == layout)
                {
                    break;
                }
            }
            if (it == parentLayout->data.end())
            {
                return result;
            }
            const auto myIt = it;
            if (!(*myIt)->stretchHeight)
            {
                result.resizeTarget = (*myIt);
                result.resizeTargetIsMe = resizeTargetIsMe;
                result.applyTarget = m_panelCommonContext->GetContainerWindow();
                return result;
            }
            if (parentLayout->type == PanelItemType::Vertical)
            {
                // my layout is auto stretched, its size doesn't matter
                // check on above
                it = myIt;
                if (it != parentLayout->data.begin())
                {
                    --it;
                    if (!(*it)->stretchHeight)
                    {
                        result.resizeTargetIsMe = false;
                        result.resizeTarget = (*it);
                        result.applyTarget = m_panelCommonContext->GetContainerWindow();
                        return result;
                    }
                }
                return result;
            }
            resizeTargetIsMe = false;
            layout = parentLayout;
        }
        return result;
    }

    bool CPanelGroupWindow::Drag_ResizeHandler_TopBottom(DragEvent event,
        const Point& initialPoint,
        const Point& currentPoint,
        std::shared_ptr<CWindow> wnd,
        const ResizeState& originalState)
    {
        switch (event)
        {
        default:
            return false;

        case DragEvent::Progress:
        case DragEvent::Drop:
        {
            int differenceY = currentPoint.y - initialPoint.y;
            auto newState = originalState;

            if (originalState.resizeTargetIsMe)
                differenceY = -differenceY;

            newState.panelSize.height += differenceY;
            ApplyState(newState);
        }
        break;
        case DragEvent::Cancel:
            ApplyState(originalState);
        }
        return true;
    }
    bool CPanelGroupWindow::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        if (!HasPanels())
        {
            return false;
        }
        m_lastMouseMovePoint = evt.mouseEvent.point;
        if (m_lastTabY != evt.mouseEvent.point.y)
        {
            return false;
        }
        if (evt.mouseEvent.button == MouseButton::Left && evt.mouseEvent.state == MouseState::Pressed)
        {
            int index = 0;
            bool captionClick = false;
            for (auto& range : m_lastTabRanges)
            {
                if (IsInside(range, evt.mouseEvent.point.x))
                {
                    captionClick = true;
                    break;
                }
                ++index;
            }
            if (captionClick)
            {
                // TODO handle caption drag-n-drop
            }
            else
            {
                ResizeState resiseState = GetHeaderResizeState();
                if (resiseState.SaveState())
                {
                    RegisterDragEvent(m_lastMouseMovePoint, [this, resiseState = resiseState](DragEvent evt,
                        const Point& initialPoint,
                        const Point& currentPoint,
                        std::shared_ptr<CWindow> wnd) {
                        return Drag_ResizeHandler_TopBottom(evt,
                        initialPoint,
                        currentPoint,
                        wnd,
                        resiseState);
                    });
                }
            }
            if (index != m_activePanelIndex)
            {
                SwitchPanel(index);
            }
            else
            {
                m_panels[m_activePanelIndex]->Activate();
            }
        }
        Invalidate(false);
        return true;
    }
    bool CPanelGroupWindow::SwitchPanel(int index)
    {
        if (m_activePanelIndex == index)
        {
            return true;
        }
        if (index < 0 || index >= (int)m_panels.size())
        {
            return false;
        }
        auto oldPanel = m_panels[m_activePanelIndex];
        auto newPanel = m_panels[index];

        newPanel->MoveTo(oldPanel->GetPosition());
        newPanel->Resize(oldPanel->GetSize());
        oldPanel->SetVisible(false);
        newPanel->SetVisible(true);
        m_activePanelIndex = index;
        newPanel->Activate();
        return true;
    }
    bool CPanelGroupWindow::ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext)
    {
        if (evt.keyEvent.valid)
        {
            switch (evt.keyEvent.virtualKey)
            {
                // TODO: Add Ctrl + for resizing
            //case VirtualKey::Up:
            //case VirtualKey::Down:

            case VirtualKey::Tab:
                // switch groups
                if (!evt.keyState.HasModifiers())
                {
                    m_panelCommonContext->ActivateNextGroup(GetPtr_t<CPanelGroupWindow>(this));
                    return true;
                }

                // switch panels
                if ((evt.keyState.state & (evt.keyState.AnyShift | evt.keyState.AnyCtrl)) != 0)
                {
                    if (!m_panels.empty())
                    {
                        int newIndex = m_activePanelIndex + 1;
                        if (newIndex >= (int)m_panels.size())
                        {
                            newIndex = 0;
                        }
                        this->SwitchPanel(newIndex);
                        return true;
                    }
                }
                break;
            }
        }
        return CWindow::ProcessEvent(evt, evtContext);
    }

    String CPanelGroupWindow::m_chunk;
    void CPanelGroupWindow::PaintTitle(const Rect& rect, DrawParameters& parameters)
    {
        if (rect.size.width <= 0 || rect.size.height <= 0)
        {
            return;
        }

        m_lastTabRanges.clear();

        auto activePanel = m_panels[m_activePanelIndex];
        Rect clientRect;
        clientRect.position = activePanel->GetPosition();
        clientRect.size = activePanel->GetSize();
        if (clientRect.position.y)
        {
            --clientRect.position.y;
        }
        clientRect.size.height = 1;

        auto absClientRect = clientRect;
        absClientRect.position.x += rect.position.x;
        absClientRect.position.y += rect.position.y;
        m_lastTabY = absClientRect.position.y;

        int index = 0;
        Point target = absClientRect.position;
        const Point initialTarget = target;
        int symbolsLeft = absClientRect.size.width;

        m_chunk.native.clear();

        // prepare symbols
        const PanelBorderSymbols symbols = GetPanelBorderSymbols();

        // check minimum size
        const int minSize = 10;
        if (absClientRect.size.width <= minSize)
        {
            // just draw the line
            m_chunk.native.resize(absClientRect.size.width, symbols.horizontal);

            parameters.console.PaintText(target,
                m_panelColorProfile->borderText,
                m_panelColorProfile->borderBackground,
                m_chunk.native);
            return;
        }

        // draw prefix
        m_chunk.native.clear();
        m_chunk.native.resize(3, symbols.horizontal);
        m_chunk.native[2] = oui::String::symSpace;

        parameters.console.PaintText(target,
            m_panelColorProfile->borderText,
            m_panelColorProfile->borderBackground,
            m_chunk.native);

        target.x += (int)m_chunk.native.size();
        symbolsLeft -= (int)m_chunk.native.size();
        symbolsLeft -= (int)m_chunk.native.size();

        for (auto panel : m_panels)
        {
            const bool selected = (m_activePanelIndex == index);

            m_chunk.native.clear();
            m_chunk.native.append(1, String::symSpace);
            m_chunk.native.append(1, g_braceLeft);

            if (selected)
                m_chunk.native.append(1, g_braceLeft);
            else
                m_chunk.native.append(1, String::symSpace);

            m_chunk.native.append(1, String::symSpace);
            m_chunk.native.append(panel->GetCaption().native);
            m_chunk.native.append(1, String::symSpace);
            if (selected)
                m_chunk.native.append(1, g_braceRight);
            else
                m_chunk.native.append(1, String::symSpace);
            m_chunk.native.append(1, g_braceRight);
            m_chunk.native.append(1, String::symSpace);


            PanelCaptionProfile* currentColors = &m_panelColorProfile->normal;
            if (m_activePanelIndex == index && panel->IsActiveOrFocused())
            {
                currentColors = &m_panelColorProfile->selected;
            }
            if (symbolsLeft <= 0)
            {
                break;
            }
            int tableCharsCount = CutString(m_chunk.native, symbolsLeft);

            if (IsMouseOn() &&
                m_lastMouseMovePoint.y == target.y &&
                m_lastMouseMovePoint.x >= target.x &&
                m_lastMouseMovePoint.x < (target.x + tableCharsCount))
            {
                currentColors = &m_panelColorProfile->mouseHighlight;
            }
            m_lastTabRanges.push_back({ target.x, (target.x + tableCharsCount) });
            parameters.console.PaintText(target,
                currentColors->text,
                currentColors->background,
                m_chunk.native);

            target.x += tableCharsCount;
            symbolsLeft -= tableCharsCount;
            index++;
        }

        // draw suffix
        m_chunk.native.clear();

        const int processedSize = target.x - initialTarget.x;
        m_chunk.native.resize(absClientRect.size.width - processedSize, symbols.horizontal);

        if (!m_chunk.native.empty())
        {
            m_chunk.native[0] = oui::String::symSpace;
            parameters.console.PaintText(target,
                m_panelColorProfile->borderText,
                m_panelColorProfile->borderBackground,
                m_chunk.native);
        }
    }
    std::shared_ptr<CPanelCommonContext> CPanelGroupWindow::GetPanelCommonContext()
    {
        return m_panelCommonContext;
    }
    void CPanelGroupWindow::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        if (HasPanels())
        {
            PaintTitle(rect, parameters);
        }
        CWindow::DoPaint(rect, parameters);
    }
    // CPanelCommonContext
    CPanelCommonContext::CPanelCommonContext()
    {
    }
    void CPanelCommonContext::SetRootLayout(std::shared_ptr<PanelLayout> layout)
    {
        m_rootLayout = layout;
    }
    void CPanelCommonContext::Register(std::shared_ptr<CPanelContainerWindow> containerWindow)
    {
        m_panelContainerWindow = containerWindow;
    }
    std::shared_ptr<CPanelContainerWindow> CPanelCommonContext::GetContainerWindow()
    {
        return m_panelContainerWindow.lock();
    }
    std::shared_ptr<PanelLayout> CPanelCommonContext::GetRootLayout()
    {
        return m_rootLayout;
    }
    void CPanelCommonContext::Register(std::shared_ptr<CPanelGroupWindow> group)
    {
        m_allGroups.insert(group);
    }
    void CPanelCommonContext::ActivateNextGroup(std::shared_ptr<CPanelGroupWindow> caller)
    {
        if (m_allGroups.empty())
        {
            return;
        }
        auto it = m_allGroups.find(caller);

        for (int i = 0; i < m_allGroups.size(); ++i)
        {
            if (it == m_allGroups.end())
            {
                it = m_allGroups.begin();
            }
            else
            {
                ++it;
                if (it == m_allGroups.end())
                {
                    it = m_allGroups.begin();
                }
            }
            auto group = *it;
            if (group->HasPanels())
            {
                group->Activate();
                return;
            }
        }
    }
    void CPanelCommonContext::OnActivate(std::shared_ptr<CPanelGroupWindow> group)
    {
        auto currentActiveGroup = m_currentActiveGroup.lock();
        if (currentActiveGroup != group)
        {
            if (currentActiveGroup)
            {
                currentActiveGroup->Deactivate();
            }
        }
        m_currentActiveGroup = group;
    }
    void CPanelCommonContext::OnActivate(std::shared_ptr<CPanelWindow> panel)
    {
        auto currentActivePanel = m_currentActivePanel.lock();
        if (currentActivePanel != panel)
        {
            if (currentActivePanel)
            {
                currentActivePanel->Deactivate();
            }
        }
        m_currentActivePanel = panel;
    }

    // CPanelContainerWindow
    CPanelContainerWindow::CPanelContainerWindow()
    {
        SetBorderStyle(oui::BorderStyle::Thin);
        m_panelColorProfile = std::make_shared<PanelColorProfile>();
        QueryDefaultColorProfile(*m_panelColorProfile);

        m_panelCommonContext = std::make_shared<CPanelCommonContext>();
    }
    std::shared_ptr<CPanelGroupWindow> CPanelContainerWindow::CreateDefaultGroup()
    {
        if (m_panelCommonContext->GetRootLayout())
        {
            return nullptr;
        }
        auto group = std::make_shared<CPanelGroupWindow>(m_panelColorProfile, m_panelCommonContext);
        auto rootLayout = std::make_shared<PanelLayout>(group);
        m_panelCommonContext->SetRootLayout(rootLayout);
        ++m_groupsCount;
        group->SetInfo(GroupInfo(GroupInfo::StretchAll()));
        m_defaultGroup = group;
        return group;
    }
    std::shared_ptr<CPanelGroupWindow> CPanelContainerWindow::AttachPanelImpl(std::shared_ptr<PanelLayout> layout,
        GroupLocation location,
        std::function<void(std::shared_ptr<PanelLayout>)> replaceLayout)
    {
        auto rootLayout = layout;
        if (rootLayout->type == PanelItemType::None)
        {
            auto newGroup = std::make_shared<CPanelGroupWindow>(m_panelColorProfile, m_panelCommonContext);
            switch (location)
            {
            case GroupLocation::Left:
                rootLayout->ResetType(PanelItemType::Horizontal);
                rootLayout->data.push_front(std::make_shared<PanelLayout>(newGroup));
                return newGroup;

            case GroupLocation::Right:
                rootLayout->ResetType(PanelItemType::Horizontal);
                rootLayout->data.push_back(std::make_shared<PanelLayout>(newGroup));
                return newGroup;

            case GroupLocation::Top:
                rootLayout->ResetType(PanelItemType::Vertical);
                rootLayout->data.push_front(std::make_shared<PanelLayout>(newGroup));
                return newGroup;

            case GroupLocation::Bottom:
                rootLayout->ResetType(PanelItemType::Vertical);
                rootLayout->data.push_back(std::make_shared<PanelLayout>(newGroup));
                return newGroup;
            }
            return nullptr;
        }

        // not none
        if (rootLayout->type == PanelItemType::Vertical)
        {
            auto newGroup = std::make_shared<CPanelGroupWindow>(m_panelColorProfile, m_panelCommonContext);

            switch (location)
            {
            case GroupLocation::Top:
                rootLayout->data.push_front(std::make_shared<PanelLayout>(newGroup));
                return newGroup;

            case GroupLocation::Bottom:
                rootLayout->data.push_back(std::make_shared<PanelLayout>(newGroup));
                return newGroup;
            }
        }
        else
        {
            auto newGroup = std::make_shared<CPanelGroupWindow>(m_panelColorProfile, m_panelCommonContext);
            // horizontal
            switch (location)
            {
            case GroupLocation::Left:
                rootLayout->data.push_front(std::make_shared<PanelLayout>(newGroup));
                return newGroup;

            case GroupLocation::Right:
                rootLayout->data.push_back(std::make_shared<PanelLayout>(newGroup));
                return newGroup;
            }
        }

        if (!replaceLayout)
        {
            return nullptr;
        }
        // here we've need to replace root
        auto newRootLayout = std::make_shared<PanelLayout>();
        newRootLayout->data.push_back(rootLayout);
        replaceLayout(newRootLayout);
        return AttachPanelImpl(newRootLayout,
            location,
            nullptr);
    }
    bool CPanelContainerWindow::QueryContextImpl(std::shared_ptr<CPanelGroupWindow> group,
        decltype(PanelLayout::data)::iterator parentIt,
        GroupLocationContext& ctx,
        int level)
    {
        if (level > 50)
        {
            return false;
        }
        auto parent = *parentIt;
        
        for (auto it = parent->data.begin(), it_end = parent->data.end();
            it != it_end;
            ++it)
        {
            if ((*it)->group == group)
            {
                ctx.layout = *it;
                ctx.parentLayout = *parentIt;
                ctx.replaceParentLayout = [=](auto newLayout) {   *parentIt = newLayout; };
                ctx.replaceLayout = [=](auto newLayout) {  *it = newLayout; };
                return true;
            }
            if (QueryContextImpl(group, it, ctx, level+1))
            {
                return true;
            }
        }
        return false;
    }
    bool CPanelContainerWindow::QueryContextImpl(std::shared_ptr<CPanelGroupWindow> group,
        GroupLocationContext& ctx)
    {
        auto rootLayout = m_panelCommonContext->GetRootLayout();
        if (!rootLayout)
        {
            return false;
        }
        if (group == nullptr || rootLayout->group == group)
        {
            // root
            ctx.layout = rootLayout;
            ctx.parentLayout = rootLayout;
            ctx.replaceParentLayout = [=](auto newLayout) {  m_panelCommonContext->SetRootLayout(newLayout); };
            ctx.replaceLayout = ctx.replaceParentLayout;
            return true;
        }
        
        auto it = std::find_if(rootLayout->data.begin(), rootLayout->data.end(), [=](auto& value) {  return value->group == group; });
        if (it != rootLayout->data.end())
        {
            ctx.layout = *it;
            ctx.parentLayout = rootLayout;
            ctx.replaceParentLayout = [=](auto newLayout) {  m_panelCommonContext->SetRootLayout(newLayout); };
            ctx.replaceLayout = [=](auto newLayout) {  *it = newLayout; };
            return true;
        }

        for (auto it = rootLayout->data.begin(), it_end = rootLayout->data.end();
            it != it_end;
            ++it)
        {
            if (QueryContextImpl(group, it, ctx, 0))
            {
                return true;
            }
        }
        return false;
    }
    std::shared_ptr<CPanelGroupWindow> CPanelContainerWindow::AttachNewGroup(std::shared_ptr<CPanelGroupWindow> where,
        GroupLocation location,
        GroupAttachMode mode)
    {
        // check
        GroupLocationContext context;
        if (!QueryContextImpl(where, context))
        {
            return nullptr;
        }

        decltype (context.layout) layout = context.layout;
        decltype (context.replaceLayout) replace = context.replaceLayout;
        if (mode == GroupAttachMode::Sibling)
        {
            layout = context.parentLayout;
            replace = context.replaceParentLayout;
        }
        auto result = AttachPanelImpl(layout, location, replace);
        switch(location)
        {
            case GroupLocation::Left:
            case GroupLocation::Right:
                result->Info().stretchHeight = true;
                break;

            case GroupLocation::Top:
            case GroupLocation::Bottom:
                result->Info().stretchWidth = true;
        }
        if (result)
        {
            ++m_groupsCount;
        }
        return result;
    }
    void CPanelContainerWindow::ConstructChilds()
    {
        m_panelCommonContext->Register(GetPtr_t<CPanelContainerWindow>(this));

        // register child to common context
        std::vector<oui::String::string_type> tags;
        oui::CLayoutIterator iterator;
        iterator.InitStart(m_panelCommonContext->GetRootLayout());
        for (; iterator.MoveNext();)
        {
            auto layout = iterator.GetLayout();
            m_panelCommonContext->Register(layout->group);
        }        
        // and register childs
        for (auto it = m_panelCommonContext->GroupBegin(), it_end = m_panelCommonContext->GroupEnd();
            it != it_end;
            ++it)
        {
            AddChild(*it);
        }
    }
    bool CPanelContainerWindow::HasPanels() const
    {
        return m_groupsCount !=0;
    }

    void CPanelContainerWindow::OnResize()
    {
        if (!HasPanels())
        {
            Parent_type::OnResize();
            return;
        }
        // here render happens
        Rect rect = GetClientRect();
        auto rootLayout = m_panelCommonContext->GetRootLayout();

        oui::RepositionLayout(rootLayout, rect, !m_repositionCacheValid, true);
        m_repositionCacheValid = true;
    }

    Rect CPanelContainerWindow::GetClientRect() const
    {
        if (!HasPanels())
        {
            return Parent_type::GetClientRect();
        }
        return CWindow::GetClientRect();
    }
    void CPanelContainerWindow::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        if (!HasPanels())
        {
            return Parent_type::DoPaint(rect, parameters);
        }
        return CWindow::DoPaint(rect, parameters);
    }
}