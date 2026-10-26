
#ifndef __STRING_UTILITY_H__
#define __STRING_UTILITY_H__

#include <string>
#include <algorithm>
#include <type_traits>

inline std::string Trim(const std::string& val)
{
    static const char* TRIM_STR = " \01\02\03\04\05\06\a\b\t\n\v\f\r\x0E\x0F\x7F\x10\x11\x12\x13"
                                  "\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F";
    std::string::size_type pos = val.find_first_not_of(TRIM_STR);
    std::string::size_type lastpos = val.find_last_not_of(TRIM_STR);
    if (pos == lastpos)
    {
        if (pos == std::string::npos)
        {
            return val;
        }
        else
        {
            return val.substr(pos, 1);
        }
    }
    else
    {
        std::string::size_type len = lastpos - pos + 1;
        return val.substr(pos, len);
    }
}
// Drops leading and trailing spaces and control characters, as the std::string version does.
inline ttstr Trim(const ttstr& val)
{
    auto blank = [](tjs_char c) {
        auto u = static_cast<std::make_unsigned<tjs_char>::type>(c); // UTF-8 bytes are negative
        return u <= 0x20 || u == 0x7F;
    };
    const tjs_char* begin = val.c_str();
    const tjs_char* end = begin + val.length();
    while (begin < end && blank(*begin))
        begin++;
    while (end > begin && blank(end[-1]))
        end--;
    return ttstr(begin, (int)(end - begin));
}

#endif // __STRING_UTILITY_H__
