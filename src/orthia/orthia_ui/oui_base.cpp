#include "oui_base.h"

namespace oui
{
    bool IsInside(const Range& range, int value)
    {
        return value >= range.begin && value < range.end;
    }
    bool IsInside(const Rect& rect, Point& pt)
    {
        if (pt.x < rect.position.x)
            return false;
        if (pt.y < rect.position.y)
            return false;
        int xend = rect.position.x + rect.size.width;
        int yend = rect.position.y + rect.size.height;
        if (pt.x >= xend)
        {
            return false;
        }
        if (pt.y >= yend)
        {
            return false;
        }
        return true;
    }

    std::string ToStringA(LogFlags flags)
    {
        switch (flags)
        {
        case LogFlags::Debug:
            return "Debug";
        case LogFlags::Info:
            return "Info";
        case LogFlags::Warning:
            return "Warning";
        case LogFlags::Error:
            return "Error";
        default:
            return "Unknown";
        }
    }
    std::wstring ToStringW(LogFlags flags)
    {
        switch (flags)
        {
        case LogFlags::Debug:
            return L"Debug";
        case LogFlags::Info:
            return L"Info";
        case LogFlags::Warning:
            return L"Warning";
        case LogFlags::Error:
            return L"Error";
        default:
            return L"Unknown";
        }
    }

}