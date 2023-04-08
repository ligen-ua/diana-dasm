#include "oui_containers.h"

namespace oui
{
    static const auto g_left = String::char_type('<');
    static const auto g_right = String::char_type('>');
    static const auto g_braceLeft = String::char_type('[');
    static const auto g_braceRight = String::char_type(']');

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
    void CPanelGroupWindow::ConstuctChilds()
    {
        m_panelCommonContext->Register(GetPtr_t<CPanelGroupWindow>(this));

        if (m_child)
        {
            AddChild(m_child);
        }
        
        // first panel should be visible
        bool visiblePanel = true;
        for (auto& panel : m_panels)
        {
            AddChild(panel);
            panel->SetVisible(visiblePanel);
            visiblePanel = false;
        }
    }
    void CPanelGroupWindow::AddPanel(std::shared_ptr<CPanelWindow> panel,
        const PanelInfo& info)
    {
        m_fixedWidth = std::max(info.fixedWidth, m_fixedWidth);
        m_fixedHeight = std::max(info.fixedHeight, m_fixedHeight);
        m_panels.push_back(panel);
    }
    bool CPanelGroupWindow::HasPanels() const
    {
        return !m_panels.empty();
    }
    Rect CPanelGroupWindow::GetClientRect() const
    {
        Rect rect = CWindow::GetClientRect();
        return rect;
    }

    void CPanelGroupWindow::ReserveTitleSpace(CWindow* wnd, Point& position, Size& size)
    {
        if (wnd != m_child.get())
        {
            // reserve space for title
            ++position.y;
            --size.height;
        }
    }
    void CPanelGroupWindow::AdjustHorizontally(const Rect& clientRect, CWindow* left, CWindow* right, int leftWidth, int rightWidth)
    {
        const int availableWidth = clientRect.size.width;

        auto leftSize = left->GetSize();
        auto readyLeftSize = leftSize;
        readyLeftSize.height = clientRect.size.height;

        auto rightSize = right->GetSize();
        auto readyRightSize = rightSize;
        readyRightSize.height = clientRect.size.height;

        if (leftWidth)
        {
            if (leftWidth <= availableWidth)
            {
                // enough space
                readyLeftSize.width = leftWidth;
            }
            else
            {
                readyLeftSize.width = availableWidth;
            }
            // adjust flexible space
            readyRightSize.width = availableWidth - readyLeftSize.width;
        }
        else
        if (rightWidth)
        {
            if (rightWidth <= availableWidth)
            {
                // enough space
                readyRightSize.width = rightWidth;
            }
            else
            {
                readyRightSize.width = availableWidth;
            }
            // adjust flexible space
            readyLeftSize.width = availableWidth - readyRightSize.width;
        }

        auto leftPos = clientRect.position;
        ReserveTitleSpace(left, leftPos, readyLeftSize);
        left->MoveTo(leftPos);
        left->Resize(readyLeftSize);

        Point rightPosition = { clientRect.position.x + readyLeftSize.width, clientRect.position.y};
        ReserveTitleSpace(right, rightPosition, readyRightSize);
        right->MoveTo(rightPosition);
        right->Resize(readyRightSize);
    }
    void CPanelGroupWindow::AdjustVertically(const Rect& clientRect, CWindow* top, CWindow* bottom, int topHeight, int bottomHeight)
    {
        const int availableHeight = clientRect.size.height;
        
        auto topSize = top->GetSize();
        auto readyTopSize = topSize;
        readyTopSize.width = clientRect.size.width;

        auto bottomSize = bottom->GetSize();
        auto readyBottomSize = bottomSize;
        readyBottomSize.width = clientRect.size.width;

        if (topHeight)
        {
            if (topHeight <= availableHeight)
            {
                // enough space
                readyTopSize.height = topHeight;
            }
            else
            {
                readyTopSize.height = availableHeight;
            }
            // adjust flexible space
            readyBottomSize.height = availableHeight - readyTopSize.height;
        }
        else
        if (bottomHeight)
        {
            if (bottomHeight <= availableHeight)
            {
                // enough space
                readyBottomSize.height = bottomHeight;
            }
            else
            {
                readyBottomSize.height = availableHeight;
            }
            // adjust flexible space
            readyTopSize.height = availableHeight - readyBottomSize.height;
        }

        auto topPos = clientRect.position;
        ReserveTitleSpace(top, topPos, readyTopSize);
        top->MoveTo(topPos);
        top->Resize(readyTopSize);

        Point bottomPosition = { topPos.x, topPos.y + readyTopSize.height };
        ReserveTitleSpace(bottom, bottomPosition, readyBottomSize);
        bottom->MoveTo(bottomPosition);
        bottom->Resize(readyBottomSize);
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

        // check corner cases
        if (m_panels.empty())
        {
            if (m_child)
            {
                m_child->MoveTo(clientRect.position);
                m_child->Resize(clientRect.size);
            }
            return;
        }

        // got some panel
        auto activePanel = GetActivePanel();
        if (m_childOrintation == PanelOrientation::None)
        {
            Rect clientRectWithTitle = clientRect;
            ++clientRectWithTitle.position.y;
            --clientRectWithTitle.size.height;
            activePanel->MoveTo(clientRectWithTitle.position);
            activePanel->Resize(clientRectWithTitle.size);
            return;
        }

        switch (m_childOrintation)
        {
        case PanelOrientation::Left:
        {
            m_child->m_drawLeftBorder = false;
            m_drawLeftBorder = true;
            AdjustHorizontally(clientRect, m_child.get(), activePanel.get(), m_child->m_fixedWidth, 0);
            break;
        }
        case PanelOrientation::Right:
        {
            m_child->m_drawLeftBorder = true;
            m_drawLeftBorder = false;
            AdjustHorizontally(clientRect, activePanel.get(), m_child.get(), 0, m_child->m_fixedWidth);
            break;
        }
        case PanelOrientation::Top:
            AdjustVertically(clientRect, m_child.get(), activePanel.get(), m_child->m_fixedHeight, 0);
            break;

        case PanelOrientation::Bottom:
            AdjustVertically(clientRect, activePanel.get(), m_child.get(), 0, m_child->m_fixedHeight);
            break;
        }
    }
    void CPanelGroupWindow::ApplyState(const ResizeState& state)
    {
        auto newState = state;
        if (newState.fixedHeight <= 0)
        {
            newState.fixedHeight = 1;
        }
        if (newState.fixedWidth <= 0)
        {
            newState.fixedWidth = 1;
        }
        state.resizeTarget->m_fixedHeight = newState.fixedHeight;
        state.resizeTarget->m_fixedWidth = newState.fixedWidth;
        
        state.applyTarget->ForceResize();
    }

    bool CPanelGroupWindow::ResizeState::SaveState()
    {
        if (resizeTarget && applyTarget)
        {
            fixedWidth = resizeTarget->m_fixedWidth;
            fixedHeight = resizeTarget->m_fixedHeight;
            return true;
        }
        return false;
    }

    CPanelGroupWindow::ResizeState CPanelGroupWindow::GetHeaderResizeState()
    {
        CPanelGroupWindow::ResizeState result;

        // trivial case
        if (m_childOrintation == PanelOrientation::Top)
        {
            // [child]
            // [  me ]
            if (m_fixedHeight || m_fixedWidth)
            {
                result.resizeTarget = GetPtr_t<CPanelGroupWindow>(this);
                result.applyTarget = result.resizeTarget;
            }
            else
            {
                result.resizeTarget = m_child;
                result.applyTarget = GetPtr_t<CPanelGroupWindow>(this);
            }
            return result;
        }

        // scan for parent at the top
        auto ptr = GetPtr_t<CPanelGroupWindow>(this);
        if (!ptr)
        {
            return result;
        }

        bool firstLevel = true;
        std::shared_ptr<CPanelGroupWindow> lastSameLevel;
        if (ptr->m_fixedWidth || ptr->m_fixedHeight)
        {
            lastSameLevel = ptr;
        }
        for (; ;)
        {
            auto parent = GetParent_t<CPanelGroupWindow>(ptr);
            if (!parent || parent->m_childOrintation == PanelOrientation::None)
            {
                // none found
                break;
            }
            if (firstLevel && (parent->m_childOrintation == PanelOrientation::Left ||
                     parent->m_childOrintation == PanelOrientation::Right))
            {
                if (parent->m_fixedWidth || parent->m_fixedHeight)
                {
                    lastSameLevel = parent;
                }
            }
            else
            {
                firstLevel = false;
                if (parent->m_childOrintation == PanelOrientation::Bottom)
                {
                    // found window above
                    if (parent->m_fixedHeight || parent->m_fixedWidth)
                    {
                        result.resizeTarget = parent;
                        result.applyTarget = parent;
                        return result;
                    }
                    break;
                }
            }
            ptr = parent;
        }
        if (lastSameLevel)
        {
            result.resizeTarget = lastSameLevel;
            result.applyTarget = GetParent_t<CPanelGroupWindow>(lastSameLevel);
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
                int differenceY = initialPoint.y - currentPoint.y;
                auto newState = originalState;
                newState.fixedHeight += differenceY;
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
            if (evt.keyEvent.virtualKey == VirtualKey::Tab && !evt.keyState.HasModifiers())
            {
                m_panelCommonContext->ActivateNextGroup(GetPtr_t<CPanelGroupWindow>(this));
                return true;
            }
            if (evt.keyEvent.virtualKey == VirtualKey::Tab &&
                (evt.keyState.state & (evt.keyState.AnyShift | evt.keyState.AnyCtrl)) !=0)
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
        }
        return CWindow::ProcessEvent(evt, evtContext);
    }
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

        PanelCaptionProfile* profile = &m_panelColorProfile->normal;

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

    bool CPanelContainerWindow::AddPanel(const std::vector<PanelOrientation>& location, 
        std::shared_ptr<CPanelWindow> panel,
        const PanelInfo& info)
    {
        if (!m_rootGroup)
        {
            m_rootGroup = std::make_shared<CPanelGroupWindow>(m_panelColorProfile, m_panelCommonContext);
        }

        auto group = m_rootGroup;
        // traverse the binary tree here
        for (auto it = location.begin(), it_end = location.end(); it != it_end; ++it)
        {
            auto& loc = *it;
            if (!group->m_child)
            {
                group->m_childOrintation = loc;
                if (it + 1 == it_end)
                {
                    group->m_child = std::make_shared<CPanelGroupWindow>(m_panelColorProfile, m_panelCommonContext);
                }
                else
                {
                    return false;
                }
            }
            if (group->m_childOrintation != loc)
            {
                auto currentOrientation = group->m_childOrintation;
                auto currentChild = group->m_child;
                group->m_childOrintation = loc;
                group->m_child = std::make_shared<CPanelGroupWindow>(m_panelColorProfile, m_panelCommonContext);
                group->m_child->m_childOrintation = currentOrientation;
                group->m_child->m_child = currentChild;
                group->AddPanel(panel, info);
                return true;
            }
            group = group->m_child;
        }
        group->AddPanel(panel, info);
        return true;
    }
    void CPanelContainerWindow::ConstuctChilds()
    {
        if (m_rootGroup)
        {
            AddChild(m_rootGroup);
        }
    }
    bool CPanelContainerWindow::HasPanels() const
    {
        return !m_childs.empty();
    }
    void CPanelContainerWindow::OnResize()
    {
        if (!HasPanels())
        {
            Parent_type::OnResize();
            return;
        }
        Rect rect = GetClientRect();
        m_rootGroup->Resize(rect.size);
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