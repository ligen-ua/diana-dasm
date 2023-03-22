#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <set>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <algorithm>
#include <array>

struct IUnknown;
namespace oui
{

    struct Point
    {
        int x = 0;
        int y = 0;
    };
    inline Point operator + (const Point& pt1, const Point& pt2)
    {
        Point res = pt1;
        res.x += pt2.x;
        res.y += pt2.y;
        return res;
    }
    inline bool operator == (const Point& pt1, const Point& pt2)
    {
        return pt1.x == pt2.x && pt1.y == pt2.y;
    }
    struct Size
    {
        int width = 0;
        int height = 0;
    };

    inline bool operator == (const Size& size1, const Size& size2)
    {
        return size1.width == size2.width && size1.height == size2.height;
    }
    struct Rect
    {
        Point position;
        Size size;
    };
    bool IsInside(const Rect& rect, Point& pt);

    class Noncopyable {
    public:
        Noncopyable() = default;
        ~Noncopyable() = default;

    private:
        Noncopyable(const Noncopyable&) = delete;
        Noncopyable& operator=(const Noncopyable&) = delete;
    };

    template <class T>
    inline void hash_combine(std::size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template <class T>
    inline void hash_combine(std::size_t& seed, T * arr, size_t size)
    {
        constexpr unsigned long long FNV_prime = 1099511628211ul;
        constexpr unsigned long long FNV_offset = 14695981039346656037ul;

        unsigned long long hashed = FNV_offset;
        T* p = arr;
        for (size_t i = 0; i < size; ++i, ++p)
        {
            hashed ^= (size_t)*p;
            hashed *= FNV_prime;
        }
        seed ^= (size_t)hashed;
    }


    inline bool IsLeadByte(wchar_t ch)
    {
        return (unsigned int)ch >= 0xD800 && (unsigned int)ch <= 0xDFFF;
    }
    inline bool IsLeadByte(char ch)
    {
        return (ch & 0xC0) != 0x80;
    }
    int CalculateSymbolsCount(const wchar_t* pStart, size_t sizeInWchars, const wchar_t exceptSym_in);

    template<class Type>
    int CalculateSymbolsCount(const Type& str, const wchar_t exceptSym_in)
    {
        return CalculateSymbolsCount(str.c_str(), str.size(), exceptSym_in);
    }

    template<class Type>
    wchar_t QueryFirstSymbol(const Type& str, const wchar_t exceptSym_in)
    {
        return QueryFirstSymbol(str.c_str(), str.size(), exceptSym_in);
    }

}