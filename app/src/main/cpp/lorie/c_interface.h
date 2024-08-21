//
// Created by yang on 2024/5/3.
//

#include "node.h"

#ifndef TERMUX_X11_C_INTERFACE_H
#define TERMUX_X11_C_INTERFACE_H

#define WINDOW_TYPE                     "_NET_WM_WINDOW_TYPE"
#define WINDOW_TYPE_NORMAL              "_NET_WM_WINDOW_TYPE_NORMAL"
#define WINDOW_TYPE_MENU                "_NET_WM_WINDOW_TYPE_MENU"
#define WINDOW_TYPE_TOOLTIP             "_NET_WM_WINDOW_TYPE_TOOLTIP"
#define WINDOW_TYPE_DIALOG              "_NET_WM_WINDOW_TYPE_DIALOG"
#define WINDOW_TYPE_POPUP               "_NET_WM_WINDOW_TYPE_POPUP_MENU"
#define WINDOW_TYPE_UTILITY             "_NET_WM_WINDOW_TYPE_UTILITY"
#define WINDWO_TRANSIENT_FOR            "WM_TRANSIENT_FOR"
#define NET_WINDOW_NAME                 "_NET_WM_NAME"
#define WINDOW_CLIENT_LEADER            "WM_CLIENT_LEADER"
#define WINDOW_CLASS                    "WM_CLASS"
#define WINDOW_NAME                     "WM_NAME"
#define WINDOW_ICON                     "_NET_WM_ICON"
#define WINDOW_PROTOCOLS                "WM_PROTOCOLS"
#define WINDOW_DELETE_WINDOW            "WM_DELETE_WINDOW"

#define ACTION_UNMAP 1
#define ACTION_DESTORY 2
#define ACTION_DISMISS 3

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

Widget* _surface_find_widget(ARGE_PWRAP, Window window);

int _surface_remove_widget(ARGE_PWRAP, Window window);

WindAttribute* _surface_all_window(ARGE_PWRAP, int * size);

SurfaceManagerWrapper* _surface_create_manager();

int _surface_redirect_window(SurfaceManagerWrapper *wrapper, Window window, WindAttribute *attr,
                             Atom i);

void _surface_delete_window(ARGE_PWRAP, Window window);

int _surface_count_window(ARGE_PWRAP, Window index);

int _surface_count_widget(ARGE_PWRAP, Window index);

void _surface_log_traversal_window(ARGE_PWRAP);


#ifdef __cplusplus
}
#endif
#endif //TERMUX_X11_C_INTERFACE_H
