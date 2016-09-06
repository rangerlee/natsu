/*
 * This file is part of the format and 
 * licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 *
 * Copyright (c) 2013 rangerlee (rangerlee@foxmail.com)
 * Latest version available at: http://git.oschina.net/rangerlee/format.git 
 * 
 * The aim of the project is to simplify string operation of format.
 * More information can get from README.md 
 * 
 */
 
#ifndef FMT_FORMAT
#define FMT_FORMAT

#define FMT_VER 0.1

#include <cstdarg>
#include <string>

#ifdef _MSC_VER
#define FMT_MSVC
#undef FMT_GNUC
#include <windows.h>
#else
#ifdef __GNUC__
#define FMT_GNUC
#undef FMT_MSVC
#endif
#endif

#if !defined(FMT_MSVC) && !defined(FMT_GNUC)
#error "Unsupport BuildSystem Now"
#endif

/** \brief stl format
 *
 * \param fmt string
 * \param va_list
 * \return std::string
 *
 */
inline std::string format(const char* fmt, va_list& valist)
{
    std::string result;

#if defined(FMT_MSVC)
    size_t len = _vscprintf(fmt, valist) + 1;
    result.resize(len);
    _vsnprintf_s(&result[0], result.size(), len, fmt, valist);
#endif

#if defined(FMT_GNUC)
    char *buf = 0;
    int len = vasprintf(&buf, fmt, valist);

    if (!buf)
        return result;

    if (len > 0)
        result.append(buf,len);

    free(buf);
#endif

    return result;
}

/** \brief stl format
 *
 * \param string reference
 * \param fmt string
 * \param va_list
 * \return std::string
 *
 */
inline std::string format(std::string& result, const char* fmt, va_list& valist)
{
#if defined(FMT_MSVC)
    size_t len = _vscprintf(fmt, valist) + 1;
    result.resize(len);
    _vsnprintf_s(&result[0], result.size(), len, fmt, valist);
#endif

#if defined(FMT_GNUC)
    char *buf = 0;
    int len = vasprintf(&buf, fmt, valist);

    if (!buf)
        return result;

    if (len > 0)
    {
		result.clear();
		result.append(buf,len);
	}

    free(buf);
#endif

    return result;
}

/** \brief stl format
 *
 * \param fmt string
 * \param arg list
 * \return std::string
 *
 */
inline std::string format(const char* fmt, ...)
{
    std::string result;
    if(fmt)
    {
        va_list marker;
        va_start(marker, fmt);
        format(result, fmt, marker);
        va_end(marker);
    }

    return result;
}

/** \brief stl format
 *
 * \param string reference
 * \param fmt string
 * \param arg list
 * \return std::string
 *
 */
inline std::string format(std::string& result, const char* fmt, ...)
{
    if(fmt)
    {
        va_list marker;
        va_start(marker, fmt);
        format(result, fmt, marker);
        va_end(marker);
    }

    return result;
}

#endif
