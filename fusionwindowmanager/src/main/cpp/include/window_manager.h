//
// Created by yang on 2024/4/18.
//

#ifndef TERMUX_X11_WINDOW_MANAGER_H
#define TERMUX_X11_WINDOW_MANAGER_H
#include "include/X11/Xlib.h"
#include "include/X11/X.h"
#include <android/log.h>
#define log(...) __android_log_print(ANDROID_LOG_DEBUG, "huyang_native", __VA_ARGS__)


class WindowManager  {
public:
    static WindowManager create();
    ~WindowManager();
    void Run();

private:
    WindowManager(Display *display);
    Display  *display;
    const Window root_;
};


#endif //TERMUX_X11_WINDOW_MANAGER_H
