/**************************************************
 * Adding a standard DEBUG to the project 
 * Author: Andy Young
 * Date: September 15, 2025
 **************************************************/

#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>

// This is the preprocessor check.
// If the compiler flag -DDEBUG_MODE is set, DEBUG_LOG becomes a real print statement.
#ifdef DEBUG_MODE
    #define DEBUG_LOG(x) do { std::cout << "--> DEBUG: " << x << std::endl; } while (0)
#else
    // Otherwise, DEBUG_LOG becomes nothing. The compiler will remove it.
    #define DEBUG_LOG(x) do {} while (0)
#endif

#endif // DEBUG_H