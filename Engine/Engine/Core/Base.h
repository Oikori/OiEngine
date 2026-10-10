#pragma once

// Platform detection using predefined macros
#ifdef _WIN32
    /* Windows x64/x86 */
    #ifdef _WIN64
        /* Windows x64  */
        #define OI_PLATFORM_WINDOWS
    #else
        /* Windows x86 */
        #error "x86 Builds are not supported!"
    #endif
#elif defined(__APPLE__) || defined(__MACH__)
    #include <TargetConditionals.h>
    /* TARGET_OS_MAC exists on all the platforms
     * so we must check all of them (in this order)
     * to ensure that we're running on MAC
     * and not some other Apple platform */
    #if TARGET_IPHONE_SIMULATOR == 1
        #error "IOS simulator is not supported!"
    #elif TARGET_OS_IPHONE == 1
        #define OI_PLATFORM_IOS
        #error "IOS is not supported!"
    #elif TARGET_OS_MAC == 1
        #define OI_PLATFORM_MACOS
        #error "MacOS is not supported!"
    #else
        #error "Unknown Apple platform!"
    #endif
/* We also have to check __ANDROID__ before __linux__
 * since android is based on the linux kernel
 * it has __linux__ defined */
#elif defined(__ANDROID__)
    #define OI_PLATFORM_ANDROID
    #error "Android is not supported!"
#elif defined(__linux__)
    #define OI_PLATFORM_LINUX
    #error "Linux is not supported!"
#else
    /* Unknown compiler/platform */
    #error "Unknown platform!"
#endif

#if defined(_WIN32) && defined(OI_SHARED_LIBRARY)
// 在编译库本身的时候，通过__declspec(dllexport)，告诉编译器，把符号导出到DLL
#if defined(OI_BUILDING_LIBRARY)
#define OI_API __declspec(dllexport)
#else
// 其它项目使用动态库的时候，__declspec(dllimport)告诉编译器，符号来自于DLL
#define OI_API __declspec(dllimport)
#endif
// 通过__attribute__((visibility("default")))，确保符号对外可见
#elif defined(__GNUC__) && defined(OI_SHARED_LIBRARY)
#define OI_API __attribute__((visibility("default")))
#else
#define OI_API
#endif

#ifdef OI_DEBUG
    #if defined(OI_PLATFORM_WINDOWS)
        #define OI_ASSERT(check, ...) do { if(!(check)) { __debugbreak(); } } while(0)
    #elif defined(OI_PLATFORM_LINUX)
        #include <signal.h>
        #define OI_ASSERT(check, ...) do { if(!(check)) { raise(SIGTRAP); } } while(0)
    #else
        #error "Platform doesn't support debugbreak yet!"
    #endif
#else
    #define OI_ASSERT(check, ...)
#endif

#define OI_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

#include <memory>

#include "Log.h"

namespace OiEngine {
    template<typename T>
    using Scope = std::unique_ptr<T>;
    template<typename T, typename ... Args>
    constexpr Scope<T> CreateScope(Args&& ... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    using Ref = std::shared_ptr<T>;
    template<typename T, typename ... Args>
    constexpr Ref<T> CreateRef(Args&& ... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
}
