#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <set>
#include <unordered_map>

struct IUnknown;
namespace oui
{

    struct Point
    {
        int x = 0;
        int y = 0;
    };

    struct Size
    {
        int width = 0;
        int heigh = 0;
    };

    struct Rect
    {
        Point position;
        Size size;
    };

}