//
// Created by yang on 2024/5/2.
//
#include "surface_manager.h"

::SurfaceManager *SurfaceManager::create() {
    return new SurfaceManager();
}

int SurfaceManager::redirect_window_2_surface(Window window, WindAttribute *attr, Atom type) {
    if (count_window(window)) {
        return -1;
    }
    int index = attr->index = get_avilable_index(type);
    log("redirect_window index:%d", index);
    window_attrs[window] = *attr;
    return index;
}

void SurfaceManager::update_window(Window window, WindAttribute attr) {
    WindAttribute *pAttr =  &window_attrs[window];
    if(pAttr){
        pAttr->offset_x = attr.offset_x;
        pAttr->offset_y = attr.offset_y;
        pAttr->width = attr.width;
        pAttr->height = attr.height;
        pAttr->pWin = attr.pWin;
        pAttr->index = attr.index;
        pAttr->window = attr.window;
    }
}

int SurfaceManager::remove_widget(Window window) {
    for (auto &pair: window_attrs) {
        if (pair.second.widget_size == 0 ) {
            continue;
        } else {
            bool update = false;
            size_t size = pair.second.widget_size;
            for (int i = 0; i < pair.second.widget_size; ++i) {
                Widget* widget = &pair.second.widgets[i];
                if (widget->window == window) {
                    update = true;
//                    memset(widget, 0, sizeof(Widget));
//                    widget->window = 0;
//                    widget->texture_id = 0;
                    widget->width = 0;
                    widget->height = 0;
                    widget->offset_x = 0;
                    widget->offset_y = 0;
                    widget->task_to = 0;
                    widget->pWin = NULL;
                    size --;
//                    pair.second.widget_size--;
                }
            }
            if(update){
                Widget *filtered_widgets = (Widget *)malloc(50 * sizeof(Widget));
                size_t index = 0;
                for (size_t i = 0; i < pair.second.widget_size; i++) {
                    Widget* widget = &pair.second.widgets[i];
                    if (widget->window != 0 && widget->width != 0 && widget->height != 0) {
                        filtered_widgets[index] = *widget;
                        index++;
                    }
                    if(widget->discard){
                        free(widget);
                    }
                }
                find_window(pair.first)->widgets = filtered_widgets;
                find_window(pair.first)->widget_size = index;
            }
        }
        LogWindAttribute(pair.first, pair.second);
    }
    return TRUE;
}

WindAttribute* SurfaceManager::find_window(Window window) {
    if(window_attrs.count(window)){
        WindAttribute *attr = &window_attrs[window];
//        log("found attr window:%x", window);
        return attr;
    } else {
        log("not found window:%x", window);
        return NULL;
    }
}

Widget* SurfaceManager::find_widget(Window window) {
    for (auto& pair : window_attrs) {
        for (int i = 0; i < pair.second.widget_size; ++i) {
            if(pair.second.widgets[i].window == window){
                return &pair.second.widgets[i];
            }
        }
    }
    return NULL;
}

int compare_by_level_desc(const void *a, const void *b) {
    WindAttribute *attrA = (WindAttribute *)a;
    WindAttribute *attrB = (WindAttribute *)b;
    return attrB->level - attrA->level;
}

WindAttribute* SurfaceManager::all_window(int * size){
    *size = window_attrs.size();
    WindAttribute* array = new WindAttribute[window_attrs.size()];
    int i = 0 ;
    for (const auto& pair : window_attrs) {
        array[i] =  pair.second;
        i++;
    }
    WindAttribute *sorted_attrs = (WindAttribute *)malloc(*size * sizeof(WindAttribute));
    memcpy(sorted_attrs, array, *size * sizeof(WindAttribute));
    qsort(sorted_attrs, *size, sizeof(WindAttribute), compare_by_level_desc);
    return sorted_attrs;
}

int SurfaceManager::count_window(Window window) {
    return window_attrs.count(window);
}

int SurfaceManager::count_widget(Window window) {
    log("count_widget %x", window);
    for (auto& pair : window_attrs) {
        log("count_widget window:%lx widget size:%d", pair.first, pair.second.widget_size);
        for (int i = 0; i < pair.second.widget_size; ++i) {
            if(pair.second.widgets[i].window == window){
                return TRUE;
            }
        }
    }
    return FALSE;
}


void SurfaceManager::delete_window(Window window) {
    window_attrs.erase(window);
//    auto it = window_attrs.begin();
//    while (it != window_attrs.end()) {
//        if (it->second.window == 0 && it->second.width == 0
//        && it->second.height == 0 && it->second.sfc == 0
//        && it->second.pWin == 0 && it->second.index == 0  ) {
//            it = window_attrs.erase(it);
//        } else {
//            ++it;
//        }
//    }
}


int SurfaceManager::size(){
    return window_attrs.size();
}

void SurfaceManager::traversal_log_window(){
    if(window_attrs.size() == 0){
        log("no window for android");
        return;
    }
    log("traversal_window_attrs>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
    for (const auto& pair : window_attrs) {
        LogWindAttribute(pair.first, pair.second);
    }
    log("traversal_window_attrs<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");

}

void SurfaceManager::LogWindAttribute(Window window, WindAttribute attr) {
    log("======> this is a window xid:%x index:%d w:%.0f h:%.0f x:%.0f y:%.0f  t:%d win:%p s:%p level:%d",
        attr.window,
        attr.index,
        attr.width,
        attr.height,
        attr.offset_x,
        attr.offset_y,
        attr.texture_id,
        attr.pWin,
        attr.sfc,
        attr.level);
    if (attr.widget_size != 0) {
        for (int i = 0; i < attr.widget_size; i++) {
            Widget widget = attr.widgets[i];
            log("==============> this is a widget xid:%x w:%.0f h:%.0f x:%.0f y:%.0f t:%d  s:%p ",
                widget.window,
                widget.width,
                widget.height,
                widget.offset_x,
                widget.offset_y,
                widget.texture_id,
                widget.pWin
            );
        }
    }
}

SurfaceManager::SurfaceManager() {
}

int SurfaceManager::get_avilable_index(Atom type) {
    index_normal ++;
    index_normal %= CAPACITY;
    return index_normal;
}

SurfaceManager::~SurfaceManager() {

}
