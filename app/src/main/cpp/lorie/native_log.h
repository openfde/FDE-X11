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

#define log(level, fmt, ...) \
    if (APP_LOG) { \
        if (strchr(fmt, '%') == NULL) { \
            __android_log_print(level, "cc_" __FILENAME__, "[%s] %s", __func__, fmt); \
        } else { \
            __android_log_print(level, "cc_" __FILENAME__, "[%s] " fmt, __func__, ##__VA_ARGS__); \
        } \
    }

#ifdef APP_LOG
    #define logd(...) log(ANDROID_LOG_DEBUG, __VA_ARGS__)    // DEBUG
    #define logi(...) log(ANDROID_LOG_INFO, __VA_ARGS__)     // INFO
    #define logw(...) log(ANDROID_LOG_WARN, __VA_ARGS__)     // WARN
    #define logv(...) log(ANDROID_LOG_VERBOSE, __VA_ARGS__)  // VERBOSE
#else
    #define logd(...)
    #define logi(...)
    #define logw(...)
    #define logv(...)
#endif

#define loge(...) log(ANDROID_LOG_ERROR, __VA_ARGS__)    // ERROR

#endif // NATIVE_LOG_H