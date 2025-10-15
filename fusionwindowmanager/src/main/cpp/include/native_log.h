// native_log.h
#ifndef NATIVE_LOG_H
#define NATIVE_LOG_H

#include <android/log.h>
#include <string.h> // for strrchr

// 提取文件名（不带路径）
#ifdef __FILE_NAME__ // 如果编译器支持 __FILE_NAME__（Clang/GCC）
    #define __FILENAME__ __FILE_NAME__
#else
    #define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

// 日志开关（1=启用，0=禁用）
#ifndef PRINT_LOG
    #define PRINT_LOG 0
#endif

// 通用日志宏（自动处理有无格式化参数）
#define log(level, fmt, ...) \
    if (PRINT_LOG) { \
        if (strchr(fmt, '%') == NULL) { \
            __android_log_print(level, "native_" __FILENAME__, "[%s] %s", __func__, fmt); \
        } else { \
            __android_log_print(level, "native_" __FILENAME__, "[%s] " fmt, __func__, ##__VA_ARGS__); \
        } \
    }

// 不同级别的日志宏
#define logd(...) log(ANDROID_LOG_DEBUG, __VA_ARGS__)    // DEBUG
#define logerror(...) log(ANDROID_LOG_ERROR, __VA_ARGS__)    // ERROR
#define logi(...) log(ANDROID_LOG_INFO, __VA_ARGS__)     // INFO
#define logw(...) log(ANDROID_LOG_WARN, __VA_ARGS__)     // WARN
#define logv(...) log(ANDROID_LOG_VERBOSE, __VA_ARGS__)  // VERBOSE

#endif // NATIVE_LOG_H