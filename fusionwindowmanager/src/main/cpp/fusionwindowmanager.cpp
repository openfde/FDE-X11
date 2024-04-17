#include <jni.h>
#include <string>
#include "include/xcb/xcb.h"
#include "include/X11/Xlib.h"

#include <android/log.h>
#define log(...) __android_log_print(ANDROID_LOG_DEBUG, "huyang_native", __VA_ARGS__)

extern "C" JNIEXPORT jstring JNICALL
Java_com_fde_fusionwindowmanager_NativeLib_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    setenv("DISPLAY", ":0", 1);
    Display *display;
    Window root, window;
    XEvent event;
    int screen;

    // 连接到 X 服务器
    display = XOpenDisplay(NULL);
    if (display == NULL) {
//        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    // 获取默认屏幕和根窗口
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    // 创建窗口
    window = XCreateSimpleWindow(display, root, 200, 200, 640, 480, 1,
                                 BlackPixel(display, screen),
                                 WhitePixel(display, screen));

    // 设置窗口标题
    XStoreName(display, window, "Simple Window Manager");

    // 显示窗口
    XMapWindow(display, window);

    // 处理窗口事件
    while (1) {
        XNextEvent(display, &event);
        switch (event.type) {
            case ClientMessage:
                // 窗口关闭事件，退出程序
                // if (event.xclient.data.l[0] == WM_DELETE_WINDOW) {
                //     XCloseDisplay(display);
                //     return 0;
                // }
                break;
            default:
                break;
        }
    }
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}