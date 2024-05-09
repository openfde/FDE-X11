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

int SurfaceManager::redirect_window_2_surface(Window window, WindAttribute *attr) {
    if (count_window(window)) {
        return -1;
    }
    int index = attr->index = get_avilable_index();
    window_attrs[window] = *attr;
    log("redirect_window index:%d", index);
    return index;
}

void SurfaceManager::update_window(Window window, WindAttribute attr) {
    window_attrs[window] = attr;
}

WindAttribute* SurfaceManager::find_window(Window window) {
    return &window_attrs[window];
}

WindAttribute* SurfaceManager::all_window(int * size){
    *size = window_attrs.size();
    WindAttribute* array = new WindAttribute[window_attrs.size()];
    int i = 0 ;
    for (const auto& pair : window_attrs) {
       array[i] =  pair.second;
       i++;
    }
    return array;
}

int SurfaceManager::count_window(Window window) {
    return window_attrs.count(window);
}


void SurfaceManager::delete_window(Window window) {
    window_attrs.erase(window);
}

void SurfaceManager::traversal_window_func(void (* func)(WindAttribute)){
    for (const auto &pair: window_attrs) {
        func(pair.second);
    }
}

int SurfaceManager::size(){
    return window_attrs.size();
}

void SurfaceManager::traversal_log_window(){
    if(window_attrs.size() == 0){
        log("no window for android");
        return;
    }
    log("traversal_window_attrs--------------->>>>");
    for (const auto& pair : window_attrs) {
        log("\t window key:%x,  window:%x index:%d w:%.0f h:%.0f x:%.0f y:%.0f \n\t\t t:%d win:%p s:%p ", pair.first,
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

int SurfaceManager::get_avilable_index() {
    int pInt[10];
    auto it = window_attrs.begin();
    while(it != window_attrs.end()) {
        pInt[it->second.index] = 1;
        it ++;
    }
    for ( int i = 1; i <= CAPACITY ; i ++ ){
        if(pInt[i] == 0){
            return i;
        }
    }
    log("index out of CAPACITY");
    return CAPACITY;
}
