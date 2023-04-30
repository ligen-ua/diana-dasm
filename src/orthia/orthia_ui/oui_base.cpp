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

}