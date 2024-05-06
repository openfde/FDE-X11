//
// Created by yang on 2024/5/3.
//

#include "node.h"

#ifndef TERMUX_X11_C_INTERFACE_H
#define TERMUX_X11_C_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SurfaceManagerWrapper SurfaceManagerWrapper;

WindAttribute* _surface_find_window_by_index(SurfaceManagerWrapper* wrapper, int index);

SurfaceManagerWrapper* _surface_create_manager(int size);

void _surface_destroy_wrapper(SurfaceManagerWrapper* wrapper);

int _surface_redirect_window(SurfaceManagerWrapper* wrapper, Window window, WindAttribute attr);

void _surface_delete_window(SurfaceManagerWrapper* wrapper, Window window);

void _surface_update_window(SurfaceManagerWrapper* wrapper, Window window, WindAttribute attr);

int _surface_count_window(SurfaceManagerWrapper* wrapper, int index);

void _surface_traversal_window(SurfaceManagerWrapper* wrapper, void (* func)(WindAttribute));

WindAttribute * _surface_get_array_window(SurfaceManagerWrapper* wrapper);

void _surface_log_traversal_window(SurfaceManagerWrapper* wrapper);

#ifdef __cplusplus
}
#endif
#endif //TERMUX_X11_C_INTERFACE_H
