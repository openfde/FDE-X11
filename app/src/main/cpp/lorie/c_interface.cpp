//
// Created by yang on 2024/5/3.
//
#include "c_interface.h"
#include "surface_manager.h"

#define CHECKWRAPER(wrap)  \
      if(!wrap){     \
          return;                \
      }

#define CHECKWRAPER_R(wrap)  \
      if(!wrap){     \
          return -1;                \
      }
#define CHECKWRAPER_N(wrap)  \
      if(!wrap){     \
          return NULL;                \
      }

struct SurfaceManagerWrapper{
    SurfaceManager* surfaceManager;
};



SurfaceManagerWrapper* _surface_create_manager(int size){
    SurfaceManagerWrapper* wrapper = new SurfaceManagerWrapper();
    wrapper->surfaceManager = new SurfaceManager(size);
    return wrapper;
}

void _surface_destroy_wrapper(SurfaceManagerWrapper* wrapper){
    delete wrapper->surfaceManager;
    delete wrapper;
}

int _surface_redirect_window(SurfaceManagerWrapper* wrapper, Window window, WindAttribute attr){
    CHECKWRAPER_R(wrapper);
    SurfaceManager* surfaceManager = wrapper->surfaceManager;
    return surfaceManager->redirect_window_2_surface(window, attr);
}

void _surface_delete_window(SurfaceManagerWrapper* wrapper, Window window){
    CHECKWRAPER(wrapper);
    SurfaceManager* surfaceManager = wrapper->surfaceManager;
    surfaceManager->delete_window(window);
}

void _surface_update_window(SurfaceManagerWrapper* wrapper, Window window, WindAttribute attr){
    CHECKWRAPER(wrapper);
    SurfaceManager* surfaceManager = wrapper->surfaceManager;
    surfaceManager->update_window(window, attr);
}

int _surface_count_window(SurfaceManagerWrapper* wrapper, int index){
    CHECKWRAPER_R(wrapper);
    SurfaceManager* surfaceManager = wrapper->surfaceManager;
    return surfaceManager->count_window_by_index(index);
}

void _surface_traversal_window(SurfaceManagerWrapper* wrapper,void (* func)(WindAttribute)){
    CHECKWRAPER(wrapper);
    SurfaceManager* surfaceManager = wrapper->surfaceManager;
    surfaceManager->traversal_window_func((* func));
}

WindAttribute * _surface_get_array_window(SurfaceManagerWrapper* wrapper){
    CHECKWRAPER_N(wrapper);
    SurfaceManager* surfaceManager = wrapper->surfaceManager;
    return surfaceManager->get_surface_array();
}

WindAttribute* _surface_find_window_by_index(SurfaceManagerWrapper* wrapper, int index){
    CHECKWRAPER_N(wrapper);
    SurfaceManager* surfaceManager = wrapper->surfaceManager;
    return surfaceManager->find_window_by_index(index);
}

void _surface_log_traversal_window(SurfaceManagerWrapper* wrapper){
    CHECKWRAPER(wrapper);
    SurfaceManager* surfaceManager = wrapper->surfaceManager;
    surfaceManager->traversal_log_window();
}