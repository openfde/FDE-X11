//
// Created by yang on 2024/5/2.
//

#ifndef TERMUX_X11_SURFACE_MANAGER_H
#define TERMUX_X11_SURFACE_MANAGER_H

#include "window.h"
#include "X11/X.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <android/log.h>
#include <set>
#include <stdlib.h>
#include <X11/X.h>
#include <jni.h>
#include <android/log.h>
#include <window.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "node.h"
#include <map>
#include "lorie.h"
#include <memory>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <android/log.h>
#define log(...) __android_log_print(ANDROID_LOG_DEBUG, "huyang_sm", __VA_ARGS__)

#define CAPACITY 10

class SurfaceManager {
public:
    static ::SurfaceManager* create();
    ~SurfaceManager();
    SurfaceManager(int size);
    SurfaceManager();
    int redirect_window_2_surface(Window window, WindAttribute *attr); //add
    void delete_window(Window window);                                //delete
    void update_window(Window window, WindAttribute attr);            //update
    WindAttribute* find_window(Window window);                              //query
    WindAttribute* all_window(int * size);
    void traversal_window_func(void (* func)(WindAttribute));         //traversal
    int count_window(Window index);
    void traversal_log_window();
    int size();

private:
    int max_size_;
    std::map<Window, WindAttribute> window_attrs;
    int get_avilable_index();
};


#endif //TERMUX_X11_SURFACE_MANAGER_H
