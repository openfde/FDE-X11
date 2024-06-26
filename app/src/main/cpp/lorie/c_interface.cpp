//
// Created by yang on 2024/5/3.
//
#include "c_interface.h"
#include "surface_manager.h"

struct SurfaceManagerWrapper{
    SurfaceManager* surfaceManager;
};

SurfaceManagerWrapper* _surface_create_manager(){
    ARGE_PWRAP = new SurfaceManagerWrapper();
    wrapper->surfaceManager = new SurfaceManager();
    return wrapper;
}

void _surface_destroy_wrapper(ARGE_PWRAP){
    delete wrapper->surfaceManager;
    delete wrapper;
}

int _surface_redirect_window(SurfaceManagerWrapper *wrapper, Window window, WindAttribute *attr,
                             Atom atom) {
    CHECKWRAPER_R(wrapper);
    GET_PWRAP;
    return surfaceManager->redirect_window_2_surface(window, attr, atom);
}

void _surface_delete_window(ARGE_PWRAP, Window window){
    CHECKWRAPER(wrapper);
    GET_PWRAP;
    surfaceManager->delete_window(window);
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

int _surface_count_widget(ARGE_PWRAP, Window window){
    CHECKWRAPER_R(wrapper);
    GET_PWRAP;
    return surfaceManager->count_widget(window);
}

WindAttribute* _surface_find_window(ARGE_PWRAP, Window window){
    CHECKWRAPER_N(wrapper);
    GET_PWRAP;
    return surfaceManager->find_window(window);
}

int _surface_remove_widget(ARGE_PWRAP, Window window){
    CHECKWRAPER_R(wrapper);
    GET_PWRAP;
    return surfaceManager->remove_widget(window);
}

void _surface_log_traversal_window(ARGE_PWRAP){
    CHECKWRAPER(wrapper);
    GET_PWRAP;
    surfaceManager->traversal_log_window();
}