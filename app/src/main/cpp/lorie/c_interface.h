//
// Created by yang on 2024/5/3.
//

#include "android.h"
#include <stdlib.h>

#ifndef TERMUX_X11_C_INTERFACE_H
#define TERMUX_X11_C_INTERFACE_H

#define WINDOW_TYPE                     "_NET_WM_WINDOW_TYPE"
#define WINDOW_TYPE_NORMAL              "_NET_WM_WINDOW_TYPE_NORMAL"
#define WINDOW_TYPE_MENU                "_NET_WM_WINDOW_TYPE_MENU"
#define WINDOW_TYPE_TOOLTIP             "_NET_WM_WINDOW_TYPE_TOOLTIP"
#define WINDOW_TYPE_COMBO               "_NET_WM_WINDOW_TYPE_COMBO"
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
#define WINDOW_X11_PID                  "_NET_WM_PID"
#define WINDOW_MOTIF_WM_HINTS           "_MOTIF_WM_HINTS"

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

// Motif WM Hints 标志位
#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

// 功能位
#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

// 装饰位
#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZE        (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

// 定义与客户端相同的结构体和常量
#define MWM_HINTS_ELEMENTS 3L

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
} PropMwmHints;

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

int _surface_count_window(ARGE_PWRAP, Window window);

int _surface_count_window_any(ARGE_PWRAP, Window window);

int _surface_count_window_in_type(SurfaceManagerWrapper *wrapper, Window index, int type,
                                  WindAttribute *ptr);
WindAttribute* _surface_find_window_in_type(ARGE_PWRAP,  int type, Window window);

int _surface_count_widget(ARGE_PWRAP, Window index);

void _surface_log_traversal_window(ARGE_PWRAP);


#ifdef __cplusplus
}
#endif
#endif //TERMUX_X11_C_INTERFACE_H
