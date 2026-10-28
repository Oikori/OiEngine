#pragma once

#if defined(_WIN32) && defined(OI_SHARED_LIBRARY)
// 在编译库本身的时候，通过__declspec(dllexport)，告诉编译器，把符号导出到DLL
#if defined(OI_BUILDING_LIBRARY)
#define OI_ENGINE_API __declspec(dllexport)
#else
// 其它项目使用动态库的时候，__declspec(dllimport)告诉编译器，符号来自于DLL
#define OI_ENGINE_API __declspec(dllimport)
#endif
// 通过__attribute__((visibility("default")))，确保符号对外可见
#elif defined(__GNUC__) && defined(OI_SHARED_LIBRARY)
#define OI_ENGINE_API __attribute__((visibility("default")))
#else
#define OI_ENGINE_API
#endif
