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

    struct Size
    {
        int width = 0;
        int height = 0;
    };

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

}