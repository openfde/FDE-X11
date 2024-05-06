//
// Created by yang on 2024/5/2.
//

#include "surface_manager.h"


::SurfaceManager *SurfaceManager::create() {
    return new SurfaceManager(10);
}

SurfaceManager::SurfaceManager(int size) :
        max_size_(size) {
}

int SurfaceManager::redirect_window_2_surface(Window window, WindAttribute attr) {
    if (find_window_by_xid(window) >= 0) {
        return -1;
    }
    int index = 0;
    while (index < CAPACITY) {
        if (window_attrs.find(index) == window_attrs.end()) {
            break;
        } else {
            index++;
        }
    }
    attr.index = index;
    window_attrs[index] = attr;
    log("redirect window:%x index:%d", window, index);
    return index;
}

void SurfaceManager::update_window(Window window, WindAttribute attr) {
    int index = find_window_by_xid(window);
    if (index) {
        window_attrs[index] = attr;
    } else {
        redirect_window_2_surface(window, attr);
    }
}

WindAttribute* SurfaceManager::find_window_by_index(int index) {
    return &window_attrs[index];
}

int SurfaceManager::count_window_by_index(int index) {
    return window_attrs.count(index);
}


int SurfaceManager::find_window_by_xid(Window window) {
    for (const auto &pair: window_attrs) {
        WindAttribute attr = pair.second;
        if (attr.window == window) {
            log("find window:%x", window);
            return pair.first;
        }
    }
    log("not find window:%x", window);
    return -1;
}

void SurfaceManager::delete_window(Window window) {
    int index = -1;
    for (const auto &pair: window_attrs) {
        WindAttribute attr = pair.second;
        if (attr.window == window) {
            index = pair.first;
            break;
        }
    }
    if (index != -1) {
        window_attrs.erase(index);
    }
}

void SurfaceManager::traversal_window_func(void (* func)(WindAttribute)){
    for (const auto &pair: window_attrs) {
        func(pair.second);
    }
}

WindAttribute* SurfaceManager::get_surface_array(){
    int index =  window_attrs.size();
    WindAttribute* array = new WindAttribute[index];
    for (const auto& pair : window_attrs) {
        array[index] = pair.second;
    }
    return array;
}

void SurfaceManager::traversal_log_window(){
    if(window_attrs.size() == 0){
        log("no window for android");
        return;
    }
    log("traversal window_attrs--------------->>>>");
    for (const auto& pair : window_attrs) {
        log("window index:%d,  window:%x index:%d w:%.0f h:%.0f x:%.0f y:%.0f \n\t t:%d win:%p s:%p ", pair.first,
            pair.second.window,
            pair.second.index,
            pair.second.width,
            pair.second.height,
            pair.second.offset_x,
            pair.second.offset_y,
            pair.second.texture_id,
            pair.second.pWin,
            pair.second.sfc
        );
    }
}


SurfaceManager::SurfaceManager() {
    SurfaceManager(CAPACITY);
}
