//
// Created by yang on 2024/5/3.
//

#include "node.h"

#ifndef TERMUX_X11_C_INTERFACE_H
#define TERMUX_X11_C_INTERFACE_H

#define ARGE_PWRAP SurfaceManagerWrapper* wrapper
#define GET_PWRAP  SurfaceManager* surfaceManager = wrapper->surfaceManager;
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


#ifdef __cplusplus
extern "C" {
#endif

typedef struct SurfaceManagerWrapper SurfaceManagerWrapper;

WindAttribute* _surface_find_window(ARGE_PWRAP, Window window);

WindAttribute* _surface_all_window(ARGE_PWRAP, int * size);

SurfaceManagerWrapper* _surface_create_manager(int size);

void _surface_destroy_wrapper(ARGE_PWRAP);

int _surface_redirect_window(ARGE_PWRAP, Window window, WindAttribute *attr);

void _surface_delete_window(ARGE_PWRAP, Window window);

void _surface_update_window(ARGE_PWRAP, Window window, WindAttribute attr);

int _surface_count_window(ARGE_PWRAP, Window index);

void _surface_traversal_window(ARGE_PWRAP, void (* func)(WindAttribute));

void _surface_log_traversal_window(ARGE_PWRAP);

int _surface_size(ARGE_PWRAP);

#ifdef __cplusplus
}
#endif
#endif //TERMUX_X11_C_INTERFACE_H
