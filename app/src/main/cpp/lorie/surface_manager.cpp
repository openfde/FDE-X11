//
// Created by yang on 2024/5/2.
//
#include "surface_manager.h"


// 优化后的 printWindAttributeFormatted 函数
static void printWindAttributeFormatted(const WindAttribute* attr, const char* tag) {
    if (attr == nullptr) {
        logd("[%s] WindAttribute is NULL\n", tag);
        return;
    }

    logd("┌─── WindAttribute: %s ───", tag);
    logd("├─ Graphics:");
    logd("│   texture_id: %u, dri_texture_id: %u", attr->texture_id, attr->dri_texture_id);
    logd("│   size: %.1fx%.1f, offset: (%.1f,%.1f)", attr->width, attr->height, attr->offset_x, attr->offset_y);

    logd("├─ DRI Info:");
    logd("│   dri_size: %dx%d, dri_pos: (%d,%d)", attr->dri_w, attr->dri_h, attr->dri_x, attr->dri_y);

    logd("├─ Window IDs:");
    logd("│   window: 0x%lx, child: 0x%lx, frame: 0x%lx",
         static_cast<unsigned long>(attr->window),
         static_cast<unsigned long>(attr->child),
         static_cast<unsigned long>(attr->frame));
    logd("│   pWin: %p, dri_pWin: %p",
         static_cast<void*>(attr->pWin),
         static_cast<void*>(attr->dri_pWin));

    logd("├─ EGL/Widgets:");
    logd("│   EGLSurface: %p", static_cast<void*>(attr->sfc));
    logd("│   widget: %p, widgets: %p (size: %d)",
         static_cast<void*>(attr->widget),
         static_cast<void*>(attr->widgets),
         attr->widget_size);

    logd("├─ Flags & Status:");
    logd("│   discard: %d, level: %d, status: %d", attr->discard, attr->level, attr->status);
    logd("│   system_tray: %s, dock_sent: %s",
         attr->system_tray ? "YES" : "NO",
         attr->dock_sent ? "YES" : "NO");

    logd("├─ Android:");
    logd("│   override_window_type: %d, android_component: %d",
         attr->override_window_type, attr->android_component);

    logd("└────────────────────────────");

}

::SurfaceManager *SurfaceManager::create() {
    return new SurfaceManager();
}

int SurfaceManager::redirect_window_2_surface(Window window, WindAttribute *attr, Atom type) {
    if (count_window(window)) {
        return -1;
    }
    int index = attr->index = get_avilable_index(type);
    logd("redirect_window index:%d", index);
    window_attrs[window] = *attr;
    return index;
}

void SurfaceManager::update_window(Window window, WindAttribute attr) {
    WindAttribute *pAttr =  &window_attrs[window];
    if(pAttr != nullptr){
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
        if (pair.second.widget_size == 0) {
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
                    widget->sfc = NULL;
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
                Widget *filtered_widgets = (Widget *)malloc(10 * sizeof(Widget));
                if(filtered_widgets == NULL){
                    return FALSE;
                }
                size_t index = 0;
                for (size_t i = 0; i < pair.second.widget_size; i++) {
                    Widget* widget = &pair.second.widgets[i];
                    if (!widget->discard && widget->window != 0 && widget->width != 0 && widget->height != 0) {
                        filtered_widgets[index] = *widget;
                        index++;
                    }
                }
                Widget *old_widget = find_window(pair.first)->widgets;
                free(old_widget);
                find_window(pair.first)->widgets = filtered_widgets;
                find_window(pair.first)->widget_size = index;
            }
        }
//        LogWindAttribute(pair.first, pair.second);
    }
    return TRUE;
}

WindAttribute* SurfaceManager::find_window(Window window) {
    if(window_attrs.count(window)){
        WindAttribute *attr = &window_attrs[window];
//        logd("found attr window:%x", window);
        return attr;
    } else {
        logd("not found window:%x", window);
        return NULL;
    }
}

WindAttribute* SurfaceManager::find_window_in_type(Window window, int type) {
    for (auto& pair : window_attrs) {
        const WindAttribute& attr = pair.second;

        if ((type & TYPE_WINDOW) && attr.window == window) {
            return &pair.second;
        }
        if ((type & TYPE_FRAME) && attr.frame == window) {
            return &pair.second;
        }
        if ((type & TYPE_CHILD) && attr.child == window) {
            return &pair.second;
        }
        if ((type & TYPE_LEADER) && attr.prop.leader == window) {
            return &pair.second;
        }
        if ((type & TYPE_PID) && attr.prop.pid == window) {
            return &pair.second;
        }
    }
    return nullptr;
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
    qsort(array, *size, sizeof(WindAttribute), compare_by_level_desc);
    return array;
}

int SurfaceManager::count_window(Window window) {
    return window_attrs.count(window);
}

int SurfaceManager::count_window_in_type(Window window, int type, WindAttribute *ptr) {
    for (auto& pair : window_attrs) {
        if(type == TYPE_ANY){
            if(pair.second.child == window || pair.second.window == window
             || pair.second.frame == window){
                *ptr = pair.second;
                return TRUE;
            }
        } if(type == TYPE_WINDOW){
            if(pair.second.child == window){
                *ptr = pair.second;
                return TRUE;
            }
        } else if(type == TYPE_FRAME){
            if(pair.second.frame == window){
                *ptr = pair.second;
                return TRUE;
            }
        }
    }
    return FALSE;
}

int SurfaceManager::count_widget(Window window) {
    logd("count_widget %x", window);
    for (auto& pair : window_attrs) {
        logd("count_widget window:%lx widget size:%d", pair.first, pair.second.widget_size);
        for (int i = 0; i < pair.second.widget_size; ++i) {
            if(pair.second.widgets[i].window == window){
                return TRUE;
            }
        }
    }
    return FALSE;
}


void SurfaceManager::delete_window(Window window) {
    auto it = window_attrs.find(window);
    if (it != window_attrs.end()) {
        WindAttribute& attr = it->second;
        if (attr.widgets) {
            free(attr.widgets);
            attr.widgets = nullptr;
        }
        window_attrs.erase(it);
    }
}


int SurfaceManager::size(){
    return window_attrs.size();
}

void SurfaceManager::traversal_log_window(){
    if (window_attrs.empty())
    {
        logd("no window for android");
        return;
    }
//    logd("traversal_window_attrs>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
    for (const auto& pair : window_attrs)
    {
//        LogWindAttribute(pair.first, pair.second);
//        printWindAttributeFormatted(&pair.second, "traversal_window_attrs");
    }
//    logd("traversal_window_attrs<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");

}



void SurfaceManager::LogWindAttribute(Window window, const WindAttribute &attr) {
//    const char* str =StructPrinter::toString(attr).c_str();
//    logd("window:%lx attr:%s", window, str)
    logd("======> this is a window xid:%x index:%d w:%.0f h:%.0f x:%.0f y:%.0f  t:%d win:%p s:%p level:%d name:%s leader:%x transient:%x",
        attr.window,
        attr.index,
        attr.width,
        attr.height,
        attr.offset_x,
        attr.offset_y,
        attr.texture_id,
        attr.pWin,
        attr.sfc,
        attr.level,
        attr.prop.net_wm_name,
        attr.prop.leader,
        attr.prop.transient
        )
    if (attr.widget_size != 0)
    {
        for (int i = 0; i < attr.widget_size; i++)
        {
            Widget widget = attr.widgets[i];
            logd("==============> this is a widget xid:%x w:%.0f h:%.0f x:%.0f y:%.0f t:%d  s:%p ",
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


