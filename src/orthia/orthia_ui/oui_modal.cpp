#include "oui_modal.h"

namespace oui
{

    CBaseModalWindow::CBaseModalWindow()
    {
    }

    void CBaseModalWindow::OnInit(std::shared_ptr<CWindowsPool> pool)
    {
        m_prevFocus = pool->GetFocus();
        Activate();
    }
    void CBaseModalWindow::FinishDialog()
    {
        if (m_dialogFinished)
        {
            return;
        }
        m_dialogFinished = true;

        OnFinishDialog();
        Deactivate();
        auto pool = GetPool();
        if (pool)
        {
            pool->SetFocus(m_prevFocus.lock());
            m_prevFocus.reset();
        }
        Destroy();
    }
    bool CBaseModalWindow::ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext)
    {
        if (!CWindow::ProcessEvent(evt, evtContext))
        {
            if (evt.keyEvent.valid)
            {
                switch (evt.keyEvent.virtualKey)
                {
                case oui::VirtualKey::Escape:
                    FinishDialog();
                    return true;
                }
            }
        }
        // non-popup modal dialogs showdn't allow global hotkeys to flee
        return !IsPopup();
    }


    // CModalWindow
    CModalWindow::CModalWindow()
    {
        // colors for header
        m_panelColorProfile = std::make_shared<PanelColorProfile>();
        QueryDefaultColorProfile(*m_panelColorProfile);

        // other dialog colors
        m_colorProfile = std::make_shared<DialogColorProfile>();
        QueryDefaultColorProfile(*m_colorProfile);

    }
    void CModalWindow::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        Parent_type::DoPaint(rect, parameters);
        PaintTitle(rect, parameters);
    }

    String CModalWindow::m_chunk;
    static const auto g_braceLeft = String::char_type('[');
    static const auto g_braceRight = String::char_type(']');
    static const auto g_closeSign = String::char_type('x');

    void CModalWindow::PaintTitle(const Rect& rect, DrawParameters& parameters)
    {
        auto console = GetConsole();
        if (!console)
        {
            return;
        }
        m_captionRange = Range();
        m_closeRange = Range();

        if (rect.size.width <= 0 || rect.size.height <= 0)
        {
            return;
        }

        const auto absClientRect = GetAbsoluteClientRect(this, rect);
        int symbolsLeft = absClientRect.size.width;
        Point target = absClientRect.position;
        --target.y;
        
        // skip some space from left
        target.x += 3;
        symbolsLeft -= 3;

        // and right
        symbolsLeft -= 5;

        if (!m_caption.native.empty())
        {
            // compose header
            m_chunk.native.clear();
            m_chunk.native.append(1, String::symSpace);
            m_chunk.native.append(1, g_braceLeft);
            m_chunk.native.append(1, g_braceLeft);

            m_chunk.native.append(1, String::symSpace);
            m_chunk.native.append(GetCaption().native);
            m_chunk.native.append(1, String::symSpace);

            m_chunk.native.append(1, g_braceRight);
            m_chunk.native.append(1, g_braceRight);
            m_chunk.native.append(1, String::symSpace);

            {
                PanelCaptionProfile* currentColors = &m_panelColorProfile->normal;
                if (IsActiveOrFocused())
                {
                    currentColors = &m_panelColorProfile->selected;
                }
                if (symbolsLeft > 0)
                {
                    int tableCharsCount = console->GetSymbolsAnalyzer().CutVisibleString(m_chunk.native, symbolsLeft).visibleSize;

                    if (IsMouseOn() &&
                        m_lastMouseMovePoint.y == target.y &&
                        m_lastMouseMovePoint.x >= target.x &&
                        m_lastMouseMovePoint.x < (target.x + tableCharsCount))
                    {
                        currentColors = &m_panelColorProfile->mouseHighlight;
                    }

                    m_captionRange = Range{ target.x, (target.x + tableCharsCount) };
                    parameters.console.PaintText(target,
                        currentColors->text,
                        currentColors->background,
                        m_chunk.native);

                    target.x += tableCharsCount;
                    symbolsLeft -= tableCharsCount;
                }
            }
        }

        // write x cross for close
        if (absClientRect.size.width > 5)
        {
            const int rightCornerX = absClientRect.position.x + absClientRect.size.width;
            target.x = rightCornerX - 5;
            m_chunk.native.clear();
            m_chunk.native.append(1, String::symSpace);
            m_chunk.native.append(1, g_closeSign);
            m_chunk.native.append(1, String::symSpace);

            PanelCaptionProfile* currentColors = &m_panelColorProfile->normal;

            m_closeRange = Range{ target.x, (target.x + (int)m_chunk.native.size()) };

            if (IsMouseOn() &&
                m_lastMouseMovePoint.y == target.y &&
                m_lastMouseMovePoint.x >= m_closeRange.begin &&
                m_lastMouseMovePoint.x < m_closeRange.end)
            {
                currentColors = &m_panelColorProfile->mouseHighlight;
            }

            parameters.console.PaintText(target,
                currentColors->text,
                currentColors->background,
                m_chunk.native);
        }
    }

    bool CModalWindow::Drag_MoveHandler(DragEvent event,
        const Point& initialPoint,
        const Point& currentPoint,
        std::shared_ptr<CWindow> wnd,
        const Rect& initialRect)
    {
        switch (event)
        {
        default:
            return false;

        case DragEvent::Progress:
        case DragEvent::Drop:
        {
            int differenceX = currentPoint.x - initialPoint.x;
            int differenceY = currentPoint.y - initialPoint.y;
            auto newState = initialRect;

            newState.position.x += differenceX;
            newState.position.y += differenceY;

            this->MoveTo(newState.position);
        }
        break;
        case DragEvent::Cancel:
            this->MoveTo(initialRect.position);
        }
        return true;
    }
    bool CModalWindow::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        m_lastMouseMovePoint = evt.mouseEvent.point;

        if (evt.mouseEvent.button == MouseButton::Left)
        {
            if (evt.mouseEvent.state == MouseState::Released)
            {
                if (IsInside(m_closeRange, evt.mouseEvent.point.x))
                {
                    this->FinishDialog();
                    return true;
                }
            }
            // user must be able to move window by dragging its caption
            if (evt.mouseEvent.state == MouseState::Pressed && 
                IsInside(m_captionRange, evt.mouseEvent.point.x) &&
                evt.mouseEvent.point.y == m_position.y)
            {
                // register move handler
                Rect initialRect = GetWndRect();

                RegisterDragEvent(m_lastMouseMovePoint, [this, initialRect = initialRect](DragEvent evt,
                    const Point& initialPoint,
                    const Point& currentPoint,
                    std::shared_ptr<CWindow> wnd) {
                    return Drag_MoveHandler(evt,
                    initialPoint,
                    currentPoint,
                    wnd,
                    initialRect);
                });
            }
        }
        Invalidate(false);
        return true;
    }
    void CModalWindow::OnInit(std::shared_ptr<CWindowsPool> pool)
    {
        m_lastModalWindow = pool->GetModalWindow();
        pool->SetModalWindow(GetPtr());
        pool->SetFocus(GetPtr());
        Parent_type::OnInit(pool);
    }
    void CModalWindow::OnFinishDialog() 
    {
        auto pool = GetPool();
        if (!pool)
        {
            return;
        }
        pool->SetModalWindow(m_lastModalWindow);
        m_lastModalWindow = nullptr;
    }
    void CModalWindow::SetCaption(const String& caption)
    {
        m_caption = caption;
    }
    String CModalWindow::GetCaption() const
    {
        return m_caption;
    }
    std::shared_ptr<DialogColorProfile> CModalWindow::GetColorProfile()
    {
        return m_colorProfile;
    }
    void CModalWindow::Dock()
    {
        auto parent = GetParent();
        if (!parent)
        {
            return;
        }
        const auto parentRect = parent->GetClientRect();
        int xBorder = parentRect.size.width / 6;
        int yBorder = parentRect.size.height / 6;
        Rect rect = parentRect;
        rect.position.x += xBorder;
        rect.position.y += yBorder;
        rect.size.width -= xBorder * 2;
        rect.size.height -= yBorder * 2;
        this->MoveTo(rect.position);
        this->Resize(rect.size);
    }

    // CMessageBoxWindow
    CMessageBoxWindow::CMessageBoxWindow(std::function<String()> getText, std::function<void()> onDestroy)
        :
            m_onDestroy(onDestroy)
    {
        m_fileLabel = std::make_shared<CLabel>(m_colorProfile, getText);
    }
    void CMessageBoxWindow::OnFinishDialog()
    {
        if (m_onDestroy)
        {
            m_onDestroy();
        }
        Parent_type::OnFinishDialog();
    }
    void CMessageBoxWindow::ConstructChilds()
    {
        AddChild(m_fileLabel);
    }
    void CMessageBoxWindow::Resize(const Size& newSize)
    {
        auto size = newSize;
        size.height = 5;
        Parent_type::Resize(size);
    }
    void CMessageBoxWindow::OnResize()
    {
        const auto clientRect = GetClientRect();

        if (clientRect.size.width < 5 || clientRect.size.height < 3)
        {
            Size zeroSize;
            m_fileLabel->Resize(zeroSize);
            return;
        }

        Rect fileEditRect = clientRect;
        fileEditRect.position.x += 2;
        fileEditRect.position.y += 1;
        fileEditRect.size.width -= 4;
        fileEditRect.size.height = 1;

        m_fileLabel->MoveTo(fileEditRect.position);
        m_fileLabel->Resize(fileEditRect.size);
        Parent_type::OnResize();
    }


}