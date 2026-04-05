#include "orthia_utils.h"
#include <sys/stat.h>
#include "utf8rewind.h"
#include "orthia_files.h"

namespace orthia
{

PlatformString_type Downcase(const PlatformString_type & str)
{
    int32_t errors = UTF8_ERR_NONE;
    std::vector<char> res((str.size()+1)*2);
    size_t resSize = utf8tolower(str.c_str(), str.size(),
                                 &res.front(), res.size() -1,
                                 UTF8_LOCALE_DEFAULT, &errors);
    if (errors != UTF8_ERR_NONE)
    {
        return str;
    }
    return PlatformString_type(res.begin(), res.begin() + resSize);
}

long long UnixTimeToFileTime(time_t unixTime)
{
    return (long long)(unixTime) * 10000000LL + 116444736000000000LL;
}
time_t FileTimeToUnixTime(long long winTime)
{
    const long long unixTimeStart = 0x019DB1DED53E8000LL; //January 1, 1970 (start of Unix epoch) in "ticks"
    const long long ticksPerSecond = 10000000LL; //a tick is 100ns
    return (time_t)((winTime - unixTimeStart) / ticksPerSecond);
}
long long GetUtcTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    return UnixTimeToFileTime(ts.tv_sec) + ts.tv_nsec/100LL;
}
void GetUtcTime(orthia::WinSystemTime_type * pTime)
{
    long long winTime = GetUtcTime();
    ConvertFileTimeToSystemTime(winTime, pTime);
}
bool ConvertFileTimeToSystemTime(long long fileTime, orthia::WinSystemTime_type * pTime)
{
    if (fileTime <= 0)
    {
        pTime->wYear = 1601;
        pTime->wMonth = 1;
        pTime->wDayOfWeek = 1;
        pTime->wDay = 1;
        pTime->wHour = 0;
        pTime->wMinute = 0;
        pTime->wSecond = 0;
        pTime->wMilliseconds = 0;
        return true;
    }
    if (fileTime == 0x7FFFFFFFFFFFFFFFLL)
    {
        pTime->wYear = 30828;
        pTime->wMonth = 9;
        pTime->wDayOfWeek = 4;
        pTime->wDay = 14;
        pTime->wHour = 2;
        pTime->wMinute = 48;
        pTime->wSecond = 5;
        pTime->wMilliseconds = 477;
        return true;
    }

    time_t unixTime = FileTimeToUnixTime(fileTime);
    struct tm timeStruct = {0,};
    gmtime_r(&unixTime, &timeStruct);

    pTime->wYear = timeStruct.tm_year + 1900;
    pTime->wMonth = timeStruct.tm_mon + 1;
    pTime->wDayOfWeek = timeStruct.tm_wday;
    pTime->wDay = timeStruct.tm_mday;
    pTime->wHour = timeStruct.tm_hour;
    pTime->wMinute = timeStruct.tm_min;
    pTime->wSecond = timeStruct.tm_sec;
    pTime->wMilliseconds = ((fileTime / 10000LL) % 1000LL);
    return true;
}

static
long long DaysAndFractionToTime(unsigned int ElapsedDays, unsigned int Milliseconds)
{
    //
    //  Calculate the exact number of milliseconds in the elapsed days.
    //

    long long Temp = 86400000LL * (long long)ElapsedDays; // ConvertDaysToMilliseconds(ElapsedDays);

    //
    //  Convert milliseconds to a large integer
    //

    long long Temp2 = Milliseconds;

    //
    //  add milliseconds to the whole day milliseconds
    //

    Temp = Temp + Temp2;

    //
    //  Finally convert the milliseconds to 100ns resolution
    //

    return Temp*10000LL;
}

#define NumberOfLeapYears(YEARS) (                    \
    ((YEARS) / 4) - ((YEARS) / 100) + ((YEARS) / 400) \
    )
//
#define ElapsedYearsToDays(YEARS) (            \
    ((YEARS) * 365) + NumberOfLeapYears(YEARS) \
    )
//
#define IsLeapYear(YEARS) (                        \
    (((YEARS) % 400 == 0) ||                       \
     ((YEARS) % 100 != 0) && ((YEARS) % 4 == 0)) ? \
        1                                       \
    :                                              \
        0                                      \
    )
//

const unsigned short LeapYearDaysPrecedingMonth[13] = {
    0,                                 // January
    31,                                // February
    31+29,                             // March
    31+29+31,                          // April
    31+29+31+30,                       // May
    31+29+31+30+31,                    // June
    31+29+31+30+31+30,                 // July
    31+29+31+30+31+30+31,              // August
    31+29+31+30+31+30+31+31,           // September
    31+29+31+30+31+30+31+31+30,        // October
    31+29+31+30+31+30+31+31+30+31,     // November
    31+29+31+30+31+30+31+31+30+31+30,  // December
    31+29+31+30+31+30+31+31+30+31+30+31};

const unsigned short NormalYearDaysPrecedingMonth[13] = {
0,                                 // January
31,                                // February
31+28,                             // March
31+28+31,                          // April
31+28+31+30,                       // May
31+28+31+30+31,                    // June
31+28+31+30+31+30,                 // July
31+28+31+30+31+30+31,              // August
31+28+31+30+31+30+31+31,           // September
31+28+31+30+31+30+31+31+30,        // October
31+28+31+30+31+30+31+31+30+31,     // November
31+28+31+30+31+30+31+31+30+31+30,  // December
31+28+31+30+31+30+31+31+30+31+30+31};

long long ConvertSystemTimeToFileTime(const orthia::WinSystemTime_type * pTime)
{
    unsigned int Year;
    unsigned int Month;
    unsigned int Day;
    unsigned int Hour;
    unsigned int Minute;
    unsigned int Second;
    unsigned int Milliseconds;

    unsigned int ElapsedDays;
    unsigned int ElapsedMilliseconds;

    //
    //  Load the time field elements into local variables.  This should
    //  ensure that the compiler will only load the input elements
    //  once, even if there are alias problems.  It will also make
    //  everything (except the year) zero based.  We cannot zero base the
    //  year because then we can't recognize cases where we're given a year
    //  before 1601.
    //

    Year         = pTime->wYear;
    Month        = pTime->wMonth - 1;
    Day          = pTime->wDay - 1;
    Hour         = pTime->wHour;
    Minute       = pTime->wMinute;
    Second       = pTime->wSecond;
    Milliseconds = pTime->wMilliseconds;

    //
    //  Check that the time field input variable contains
    //  proper values.
    //

    //
    //  Year 30827 check: Time (in 100ns units) is stored in a
    //  64-bit integer, rooted at 1/1/1601.
    //
    //  2^63 / (10^7 * 86400) = 10675199 days
    //  10675199 / 146097 = 73 400-year chunks, 10118 days
    //  10118 / 1461 = 6 4-year chunks, 1352 days
    //  1352 / 365 = 3 years, some residual days
    //  1600 + 73*400 + 6*4 + 3 = 30827 is last year fully
    //  supported.
    //
    //  I'm guessing it's undesirable to support part of the
    //  year 30828.
    //

    if ((pTime->wMonth < 1)                      ||
        (pTime->wDay < 1)                        ||
        (Year < 1601)                                ||
        (Year > 30827)                               ||
        (Month > 11)                                 ||
        ((unsigned int)Day >= 32) ||
        (Hour > 23)                                  ||
        (Minute > 59)                                ||
        (Second > 60)                                ||
        (Milliseconds > 999))
    {
        return 0;
    }

    //
    //  Compute the total number of elapsed days represented by the
    //  input time field variable
    //

    ElapsedDays = ElapsedYearsToDays( Year - 1601 );

    if (IsLeapYear( Year - 1600 )) {

        ElapsedDays += LeapYearDaysPrecedingMonth[ Month ];

    } else {

        ElapsedDays += NormalYearDaysPrecedingMonth[ Month ];

    }

    ElapsedDays += Day;

    //
    //  Now compute the total number of milliseconds in the fractional
    //  part of the day
    //

    ElapsedMilliseconds = (((Hour*60) + Minute)*60 + Second)*1000 + Milliseconds;

    //
    //  Given the elapsed days and milliseconds we can now build
    //  the output time variable
    //

    return DaysAndFractionToTime( ElapsedDays, ElapsedMilliseconds);
}

bool UtcTimeToLocal(const orthia::WinSystemTime_type & utcTime, orthia::WinSystemTime_type * pLocalTime)
{
    long long fileTime = ConvertSystemTimeToFileTime(&utcTime);

    time_t unixTime = FileTimeToUnixTime(fileTime);
    struct tm timeStruct = {0,};
    localtime_r(&unixTime, &timeStruct);

    pLocalTime->wYear = timeStruct.tm_year + 1900;
    pLocalTime->wMonth = timeStruct.tm_mon + 1;
    pLocalTime->wDayOfWeek = timeStruct.tm_wday;
    pLocalTime->wDay = timeStruct.tm_mday;
    pLocalTime->wHour = timeStruct.tm_hour;
    pLocalTime->wMinute = timeStruct.tm_min;
    pLocalTime->wSecond = timeStruct.tm_sec;
    pLocalTime->wMilliseconds = ((fileTime / 10000LL) % 1000LL);
    return true;
}

std::string SystemTimeToISO8601(const orthia::WinSystemTime_type & st)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer)/sizeof(buffer[0]),
        "%4i-%02i-%02iT%02i:%02i:%02i.%03iZ",
        (int)st.wYear,
        (int)st.wMonth,
        (int)st.wDay,
        (int)st.wHour,
        (int)st.wMinute,
        (int)st.wSecond,
        (int)st.wMilliseconds);
    return buffer;
}
std::string UTCTimeToISO8601(long long timeMS)
{
    orthia::WinSystemTime_type winTime = {0,};
    if (!ConvertFileTimeToSystemTime(timeMS, &winTime))
    {
        throw std::runtime_error("Can't convert time");
    }
    return SystemTimeToISO8601(winTime);
}

bool IsFileExist(const std::string & fileName)
{
    struct stat buffer;
    return (stat(fileName.c_str(), &buffer) == 0);
}

static char g_hexChars[] = "0123456789abcdef";

PlatformString_type ToHexString(const char * pArray, 
                         size_t size)
{
    std::string res;
    res.reserve(size*2);
    for(size_t i = 0; i < size; ++i)
    {
        unsigned char item = (unsigned char )(pArray[i]);
        res.push_back(g_hexChars[item >> 4]);
        res.push_back(g_hexChars[item &0xF]);
    }
    return res;
}

Address_type GetSizeOfFile(const PlatformString_type & fullFileName)
{
    CFile file;
    file.OpenExistingRead(fullFileName);
    return file.GetSize();
}

}