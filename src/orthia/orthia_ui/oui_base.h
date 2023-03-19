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

struct IUnknown;
namespace oui
{

    struct Point
    {
        int x = 0;
        int y = 0;
    };

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
        constexpr size_t FNV_prime = 1099511628211ul;
        constexpr size_t FNV_offset = 14695981039346656037ul;

        size_t hashed = FNV_offset;
        T* p = arr;
        for (size_t i = 0; i < size; ++i, ++p)
        {
            hashed ^= (size_t)*p;
            hashed *= FNV_prime;
        }
        seed ^= hashed;
    }


    inline bool IsLeadByte(wchar_t ch)
    {
        return (unsigned int)ch >= 0xD800 && (unsigned int)ch <= 0xDFFF;
    }
    inline bool IsLeadByte(char ch)
    {
        return (ch & 0xC0) != 0x80;
    }
    inline int CalculateSymbolsCount(const wchar_t* pStart, size_t sizeInWchars, const wchar_t exceptSym_in)
    {
        wchar_t exceptSym = exceptSym_in;
        int charCount = 0;
        const wchar_t* pEnd = pStart + sizeInWchars;
        for (const wchar_t* p = pStart; p < pEnd; ++p)
        {
            wchar_t ch = *p;
            if (ch == exceptSym)
            {
                exceptSym = 0;
                continue;
            }
            ++charCount;
            if (IsLeadByte(ch))
            {
                ++p;
                if (p >= pEnd)
                {
                    break;
                }
            }
        }
        return charCount;
    }

    template<class Type>
    int CalculateSymbolsCount(const Type& str, const wchar_t exceptSym_in)
    {
        return CalculateSymbolsCount(str.c_str(), str.size(), exceptSym_in);
    }

}