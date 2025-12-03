#pragma once

/*
 *****************************************************************
 *                     String Toolkit Library                    *
 *                                                               *
 * Author: Arash Partow (2002-2020)                              *
 * URL: http://www.partow.net/programming/strtk/index.html       *
 *                                                               *
 * Copyright notice:                                             *
 * Free use of the String Toolkit Library is permitted under the *
 * guidelines and in accordance with the most current version of *
 * the MIT License.                                              *
 * http://www.opensource.org/licenses/MIT                        *
 *                                                               *
 *****************************************************************
*/
#include <utility>
#include <string>

namespace utils
{

    template<class T>
    inline const typename T::value_type* to_ptr(const T& s)
    {
        return &s[0];
    }

    template<class T>
    inline typename T::value_type* to_ptr(T& s)
    {
        return &s[0];
    }
    namespace details
    {
        struct cs_match
        {
            template <typename char_t>
            static inline bool cmp(const char_t c0, const char_t c1)
            {
                return (c0 == c1);
            }
        };

        struct cis_match
        {
            template <typename char_t>
            static inline bool cmp(const char_t c0, const char_t c1)
            {
                return (std::tolower(c0) == std::tolower(c1));
            }
        };

        template <typename Comparator, typename Iterator>
        inline bool match_impl(const Iterator pattern_begin, const Iterator pattern_end,
            const Iterator data_begin, const Iterator data_end,
            const typename std::iterator_traits<Iterator>::value_type& match_zero_or_more,
            const typename std::iterator_traits<Iterator>::value_type& match_zero_or_one)
        {
            Iterator d_itr = data_begin;
            Iterator p_itr = pattern_begin;

            while ((p_itr != pattern_end) && (d_itr != data_end))
            {
                if (match_zero_or_more == *p_itr)
                {
                    while ((p_itr != pattern_end) && (*p_itr == match_zero_or_more || *p_itr == match_zero_or_one))
                    {
                        ++p_itr;
                    }

                    if (p_itr == pattern_end)
                        return true;

                    const typename std::iterator_traits<Iterator>::value_type c = *(p_itr++);

                    while ((d_itr != data_end) && !Comparator::cmp(c, *d_itr))
                    {
                        ++d_itr;
                    }

                    ++d_itr;
                }
                else if ((*p_itr == match_zero_or_one) || Comparator::cmp(*p_itr, *d_itr))
                {
                    ++d_itr;
                    ++p_itr;
                }
                else
                    return false;
            }

            if (d_itr != data_end)
                return false;
            else if (p_itr == pattern_end)
                return true;
            else if ((match_zero_or_more == *p_itr) || (match_zero_or_one == *p_itr))
                ++p_itr;

            return pattern_end == p_itr;
        }
    }

    template <typename Iterator>
    inline bool match(const Iterator pattern_begin, const Iterator pattern_end,
        const Iterator data_begin, const Iterator data_end,
        const typename std::iterator_traits<Iterator>::value_type& match_zero_or_more,
        const typename std::iterator_traits<Iterator>::value_type& match_zero_or_one)
    {
        return details::match_impl<details::cs_match>(pattern_begin, pattern_end,
            data_begin, data_end,
            match_zero_or_more,
            match_zero_or_one);
    }

    inline bool match(const std::string& wild_card,
        const std::string& str)
    {
        /*
           * : Match zero or more character
           ? : Match zero or one character
        */
        return details::match_impl<details::cs_match>(to_ptr(wild_card), to_ptr(wild_card) + wild_card.size(),
            to_ptr(str), to_ptr(str) + str.size(),
            '*',
            '?');
    }

    inline bool match(const std::wstring& wild_card,
        const std::wstring& str)
    {
        /*
           * : Match zero or more character
           ? : Match zero or one character
        */
        return details::match_impl<details::cs_match>(to_ptr(wild_card), to_ptr(wild_card) + wild_card.size(),
            to_ptr(str), to_ptr(str) + str.size(),
            '*',
            '?');
    }


}