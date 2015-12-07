/***
 * Demonstrike Core
 */

#pragma once

#include "Common.h"

/* update this every loop to avoid the time() syscall! */
extern SERVER_DECL time_t UNIXTIME;
extern SERVER_DECL tm g_localTime;

namespace RONIN_UTIL
{
    static const char* timeNames[6] = { " seconds, ", " minutes, ", " hours, ", " days, ", " months, ", " years, " };
    static const char * szDayNames[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
    static const char * szMonthNames[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

    ///////////////////////////////////////////////////////////////////////////////
    // String Functions ///////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////
    RONIN_INLINE std::vector<std::string> StrSplit(const std::string &src, const std::string &sep)
    {
        std::string s;
        std::vector<std::string> r;
        for (std::string::const_iterator i = src.begin(); i != src.end(); i++)
        {
            if (sep.find(*i) != std::string::npos)
            {
                if (s.length())
                    r.push_back(s);
                s = "";
            } else s += *i;
        }
        if (s.length())
            r.push_back(s);
        return r;
    }

    RONIN_INLINE time_t convTimePeriod ( uint32 dLength, char dType)
    {
        time_t rawtime = 0;
        if (dLength == 0)
            return rawtime;

        struct tm *ti = localtime( &rawtime );
        switch(dType)
        {
        case 'h': ti->tm_hour += dLength; break;
        case 'd': ti->tm_mday += dLength; break;
        case 'w': ti->tm_mday += 7 * dLength; break;
        case 'm': ti->tm_mon += dLength; break;
        case 'y': ti->tm_year += dLength; break;
        default: ti->tm_min += dLength; break;
        }
        return mktime(ti);
    }

    RONIN_INLINE int32 GetTimePeriodFromString(const char * str)
    {
        char *p = (char*)str;
        std::string number_temp; number_temp.reserve(10);
        uint32 time_to_ban = 0, multiplier, multipliee;
        while(*p != 0)
        {
            // always starts with a number.
            if(!isdigit(*p))
                break;

            number_temp.clear();
            while(isdigit(*p) && *p != 0)
            {
                number_temp += *p;
                ++p;
            }

            // try and find a letter
            if(*p == 0)
                break;

            // check the type
            switch(tolower(*p))
            {
            case 'y': multiplier = TIME_YEAR; break;
            case 'm': multiplier = TIME_MONTH; break;
            case 'd': multiplier = TIME_DAY; break;
            case 'h': multiplier = TIME_HOUR; break;
            default: return -1; break;
            }

            ++p;
            multipliee = atoi(number_temp.c_str());
            time_to_ban += (multiplier * multipliee);
        }

        return time_to_ban;
    }

    RONIN_INLINE std::string ConvertTimeStampToString(uint32 timestamp)
    {
        static int calcVal[5] = { 60, 60, 24, 30, 12};
        int timeVals[6] = { timestamp, 0, 0, 0, 0, 0 };
        for(uint8 i = 0; i < 5; i++)
        {
            if(timeVals[i+1] = timeVals[i]/calcVal[i])
                timeVals[i] -= timeVals[i+1]*calcVal[i];
            else break;
        }

        char szTempBuf[100];
        std::string szResult;
        for(int i = 5; i >= 0; i--)
        {
            sprintf(szTempBuf,"%02u", timeVals[i]);
            szResult += szTempBuf;
            szResult += timeNames[i];
        }
        return szResult;
    }

    RONIN_INLINE std::string ConvertTimeStampToDataTime(uint32 timestamp)
    {
        char szTempBuf[100];
        time_t t = (time_t)timestamp;
        struct tm *pTM = localtime(&t);

        std::string szResult;
        szResult += szDayNames[pTM->tm_wday];
        szResult += ", ";
        sprintf(szTempBuf,"%02u", pTM->tm_mday);
        szResult += szTempBuf;
        szResult += " ";
        szResult += szMonthNames[pTM->tm_mon];
        szResult += " ";
        sprintf(szTempBuf,"%u",pTM->tm_year+1900);
        szResult += szTempBuf;
        szResult += ", ";
        sprintf(szTempBuf,"%02u",pTM->tm_hour);
        szResult += szTempBuf;
        szResult += ":";
        sprintf(szTempBuf,"%02u",pTM->tm_min);
        szResult += szTempBuf;
        szResult += ":";
        sprintf(szTempBuf,"%02u",pTM->tm_sec);
        szResult += szTempBuf;
        return szResult;
    }

    RONIN_INLINE uint32 secsToTimeBitFields(time_t secs)
    {
        tm* time = localtime(&secs);
        uint32 Time = ((time->tm_min << 0) & 0x0000003F); // Minute
        Time |= ((time->tm_hour << 6) & 0x000007C0); // Hour
        Time |= ((time->tm_wday << 11) & 0x00003800); // WeekDay
        Time |= (((time->tm_mday-1) << 14) & 0x000FC000); // MonthDay
        Time |= ((time->tm_mon << 20) & 0x00F00000); // Month
        Time |= (((time->tm_year-100) << 24) & 0x1F000000); // Year
        return Time;
    }

    RONIN_INLINE void reverse_array(uint8 * pointer, size_t count)
    {
        size_t x;
        uint8 * temp = (uint8*)malloc(count);
        memcpy(temp, pointer, count);
        for(x = 0; x < count; ++x)
            pointer[x] = temp[count-x-1];
        free(temp);
    }

    RONIN_INLINE bool FindXinYString(std::string x, std::string y)
    {
        return y.find(x) != std::string::npos;
    }

    RONIN_INLINE void TOLOWER(std::string& str)
    {
        for(size_t i = 0; i < str.length(); ++i)
            str[i] = (char)tolower(str[i]);
    }

    RONIN_INLINE void TOUPPER(std::string& str)
    {
        for(size_t i = 0; i < str.length(); ++i)
            str[i] = (char)toupper(str[i]);
    }

    RONIN_INLINE std::string TOLOWER_RETURN(std::string str)
    {
        std::string newname = str;
        for(size_t i = 0; i < str.length(); ++i)
            newname[i] = (char)tolower(str[i]);

        return newname;
    }

    RONIN_INLINE std::string TOUPPER_RETURN(std::string str)
    {
        std::string newname = str;
        for(size_t i = 0; i < str.length(); ++i)
            newname[i] = (char)toupper(str[i]);
        return newname;
    }

    // returns true if the ip hits the mask, otherwise false
    RONIN_INLINE bool ParseCIDRBan(unsigned int IP, unsigned int Mask, unsigned int MaskBits)
    {
        // CIDR bans are a compacted form of IP / Submask
        // So 192.168.1.0/255.255.255.0 would be 192.168.1.0/24
        // IP's in the 192.168l.1.x range would be hit, others not.
        unsigned char * source_ip = (unsigned char*)&IP;
        unsigned char * mask = (unsigned char*)&Mask;
        int full_bytes = MaskBits / 8;
        int leftover_bits = MaskBits % 8;
        //int byte;

        // sanity checks for the data first
        if( MaskBits > 32 )
            return false;

        // this is the table for comparing leftover bits
        static const unsigned char leftover_bits_compare[9] = {
            0x00,           // 00000000
            0x80,           // 10000000
            0xC0,           // 11000000
            0xE0,           // 11100000
            0xF0,           // 11110000
            0xF8,           // 11111000
            0xFC,           // 11111100
            0xFE,           // 11111110
            0xFF,           // 11111111 - This one isn't used
        };

        // if we have any full bytes, compare them with memcpy
        if( full_bytes > 0 )
        {
            if( memcmp( source_ip, mask, full_bytes ) != 0 )
                return false;
        }

        // compare the left over bits
        if( leftover_bits > 0 )
        {
            if( ( source_ip[full_bytes] & leftover_bits_compare[leftover_bits] ) !=
                ( mask[full_bytes] & leftover_bits_compare[leftover_bits] ) )
            {
                // one of the bits does not match
                return false;
            }
        }

        // all of the bits match that were testable
        return true;
    }

    RONIN_INLINE uint MakeIP(const char * str)
    {
        unsigned int bytes[4];
        unsigned int res;
        if( sscanf(str, "%u.%u.%u.%u", &bytes[0], &bytes[1], &bytes[2], &bytes[3]) != 4 )
            return 0;

        res = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
        return res;
    }

    template<typename T> RONIN_INLINE T FirstBitValue(T value)
    {
        assert(sizeof(T)<=8); // Limit to 8 bytes
        if(value)
        {   // for each byte we have 8 bit stacks
            for(T i = 0; i < sizeof(T)*8; i++)
                if(value & (T(1)<<i))
                    return i;
        } return static_cast<T>(NULL);
    }
};