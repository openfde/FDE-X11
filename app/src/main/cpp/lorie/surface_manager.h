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
    int redirect_window_2_surface(Window window, WindAttribute attr); //add
    void delete_window(Window window);                                //delete
    void update_window(Window window, WindAttribute attr);            //update
    WindAttribute* find_window_by_index(int index);                              //query
    void traversal_window_func(void (* func)(WindAttribute));         //traversal
    WindAttribute* get_surface_array();
    int count_window_by_index(int index);
    void traversal_log_window();

private:
    int find_window_by_xid(Window window);
    int max_size_;
    std::map<int, WindAttribute> window_attrs;
};


#endif //TERMUX_X11_SURFACE_MANAGER_H
