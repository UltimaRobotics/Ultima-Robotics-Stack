
#ifndef LOGGING_MACROS_HPP
#define LOGGING_MACROS_HPP

#include <iostream>

#ifdef VERBOSE_LOGG
    #define LOG_INFO(msg) std::cout << msg
    #define LOG_ERROR(msg) std::cerr << msg
#else
    #define LOG_INFO(msg) do {} while(0)
    #define LOG_ERROR(msg) do {} while(0)
#endif

#endif // LOGGING_MACROS_HPP
