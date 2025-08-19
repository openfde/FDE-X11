#ifndef NATIVE_LOG_H
#define NATIVE_LOG_H

#include <android/log.h>
#include <string.h> // for strrchr, strchr

// 提取文件名（不带路径）
#ifdef __FILE_NAME__ // 如果编译器支持 __FILE_NAME__（Clang/GCC）
    #define __FILENAME__ __FILE_NAME__
#else
    #define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

// 日志开关（1=启用，0=禁用）
#ifndef PRINT_LOG
    #define PRINT_LOG 1
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
#define logd(fmt, ...) log(ANDROID_LOG_DEBUG, fmt, ##__VA_ARGS__)    // DEBUG
#define logerror(fmt, ...) log(ANDROID_LOG_ERROR, fmt, ##__VA_ARGS__) // ERROR
#define logi(fmt, ...) log(ANDROID_LOG_INFO, fmt, ##__VA_ARGS__)     // INFO
#define logw(fmt, ...) log(ANDROID_LOG_WARN, fmt, ##__VA_ARGS__)     // WARN
#define logv(fmt, ...) log(ANDROID_LOG_VERBOSE, fmt, ##__VA_ARGS__)  // VERBOSE

#endif // NATIVE_LOG_H