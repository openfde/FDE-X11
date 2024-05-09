//
// Created by yang on 2024/5/3.
//
#include "c_interface.h"
#include "surface_manager.h"

struct SurfaceManagerWrapper{
    SurfaceManager* surfaceManager;
};

SurfaceManagerWrapper* _surface_create_manager(int size){
    ARGE_PWRAP = new SurfaceManagerWrapper();
    wrapper->surfaceManager = new SurfaceManager(size);
    return wrapper;
}

void _surface_destroy_wrapper(ARGE_PWRAP){
    delete wrapper->surfaceManager;
    delete wrapper;
}

int _surface_redirect_window(ARGE_PWRAP, Window window, WindAttribute * attr){
    CHECKWRAPER_R(wrapper);
    GET_PWRAP;
    return surfaceManager->redirect_window_2_surface(window, attr);
}

void _surface_delete_window(ARGE_PWRAP, Window window){
    CHECKWRAPER(wrapper);
    GET_PWRAP;
    surfaceManager->delete_window(window);
}

void _surface_update_window(ARGE_PWRAP, Window window, WindAttribute attr){
    CHECKWRAPER(wrapper);
    GET_PWRAP;
    surfaceManager->update_window(window, attr);
}

WindAttribute* _surface_all_window(ARGE_PWRAP, int * size){
    CHECKWRAPER_N(wrapper);
    GET_PWRAP;
    return surfaceManager->all_window(size);
}

int _surface_count_window(ARGE_PWRAP, Window window){
    CHECKWRAPER_R(wrapper);
    GET_PWRAP;
    return surfaceManager->count_window(window);
}


void _surface_traversal_window(ARGE_PWRAP,void (* func)(WindAttribute)){
    CHECKWRAPER(wrapper);
    GET_PWRAP;
    surfaceManager->traversal_window_func((* func));
}


WindAttribute* _surface_find_window(ARGE_PWRAP, Window window){
    CHECKWRAPER_N(wrapper);
    GET_PWRAP;
    return surfaceManager->find_window(window);
}

int _surface_size(ARGE_PWRAP){
    CHECKWRAPER_R(wrapper);
    GET_PWRAP;
    return surfaceManager->size();
}

void _surface_log_traversal_window(ARGE_PWRAP){
    CHECKWRAPER(wrapper);
    GET_PWRAP;
    surfaceManager->traversal_log_window();
}