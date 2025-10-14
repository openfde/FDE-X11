#pragma once

#ifndef NODE_H
#define NODE_H
#include <stdlib.h>
#include <X11/X.h>
#include <jni.h>
#include <android/log.h>
#include <window.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "property.h"

#define LEVEL_MAX 32536;

/**
 * struct window/attribute/property for x window to android window
 */
typedef struct {
    Window window;
    Window transient;           //WM_TRANSIENT_FOR          WINDOW
    Window leader;              //WM_CLIENT_LEADER          WINDOW
    Atom window_type;           //_NET_WM_WINDOW_TYPE       ATOM
    const char * net_wm_name;   //_NET_WM_NAME              UTF8_STRING
    const char * wm_name;       //WM_NAME                   STRING
    const char * wm_class;      //WM_CLASS                 STRING
    Bool support_wm_delete;     //WM_PROTOCOLS              WM_DELETE_WINDOW
    Bool support_motif;
    jobject icon;               //_NET_WM_ICON
    unsigned long pid;          //_NET_WM_PID               CARDINAL
} WindProperty;

typedef struct {
    GLuint texture_id;
    float width, height;
    float offset_x, offset_y;
    Window task_to, window;
    WindowPtr pWin;
    bool inbounds;
    EGLSurface sfc;
    int discard;
    int status;
    int override_window_type;
} Widget;

typedef struct {
    GLuint texture_id;
    GLuint dri_texture_id;
    float width, height;
    float offset_x, offset_y;
    int dri_w, dri_h;
    int dri_x, dri_y;
    int index;
    Window window, child, frame;
    WindowPtr pWin;
    WindowPtr dri_pWin;
    EGLSurface sfc;
    Widget *widgets;
    void *widget;
    int widget_size ;
    int discard;
    WindProperty prop;
    int level;
    Bool system_tray;
    Bool dock_sent;
    int status;
    int override_window_type;
    int android_component;
} WindAttribute;

typedef struct WindowNode {
    WindAttribute data;
    struct WindowNode* next;
} WindowNode;

typedef enum {
    EVENT_SCREEN_SIZE,
    EVENT_TOUCH,
    EVENT_MOUSE,
    EVENT_KEY,
    EVENT_UNICODE,
    EVENT_CLIPBOARD_SYNC,
    EVENT_CLIPBOARD_TEXT
} eventType;
typedef union {
    uint8_t type;
    struct {
        uint8_t t;
        uint16_t width, height, framerate;
    } screenSize;
    struct {
        uint8_t t;
        uint16_t type, id, x, y;
    } touch;
    struct {
        uint8_t t;
        float x, y;
        uint8_t detail, down, relative;
    } mouse;
    struct {
        uint8_t t;
        uint16_t key;
        uint8_t state;
    } key;
    struct {
        uint8_t t;
        uint32_t code;
    } unicode;
    struct {
        uint8_t t;
        uint8_t enable;
    } clipboardSync;
    struct {
        uint8_t t;
        const char *text;
    } cliptext;
} lorieEvent;

#define TYPE_WINDOW     1 << 0
#define TYPE_FRAME      1 << 1
#define TYPE_LEADER     1 << 2
#define TYPE_CHILD      1 << 3

#define TYPE_ANY  (TYPE_WINDOW|TYPE_FRAME|TYPE_LEADER|TYPE_CHILD)
//HARD CODE NOW
#define OBLIQUE_CROSS_WIDTH 7
#define CURSOR_MOVE_XHOT_R 20
#define CURSOR_MOVE_YHOT_R 13
#define CURSOR_MOVE_XHOT_B 11
#define CURSOR_MOVE_YHOT_B 20
#define CURSOR_MOVE_XHOT_T 12
#define CURSOR_MOVE_YHOT_T 3
#define CURSOR_MOVE_XHOT_L 2
#define CURSOR_MOVE_YHOT_L 11
#define CURSOR_MOVE_WIDTH_WPS 11
#define _WM_WINDOW_TYPE_SYSTRAY 1000
#define _WM_WINDOW_TYPE_SYSTIP 1001

#define ANDROID_STATUS_CREATED 0
#define ANDROID_STATUS_STARTED 1
#define ANDROID_STATUS_PAUSED 2
#define ANDROID_STATUS_STOPED 3
#define ANDROID_STATUS_DESTRYED 4
#define ANDROID_STATUS_FOCUSED 5
#define ANDROID_STATUS_RESUMED 6

#define ANDROID_COMPONENT_VIEW 0
#define ANDROID_COMPONENT_ACTIVITY 1
#define ANDROID_COMPONENT_DIALOG 2

void lorieKeysymKeyboardEvent(KeySym keysym, int down);

void android_create_or_map_window(WindAttribute attribute,
                                  WindProperty prop, Window taskTo, bool inbound, bool create);

void android_create_view(Widget widget, WindProperty aProperty, Window taskTo, bool inbound);

void android_destroy_window(Window window);

void android_unmap_window(Window window);

void android_destroy_activity(int index, WindowPtr pWin, Window window, int action, Bool wm_delete);

void android_destroy_view(int index, WindowPtr pWin, Window task_to, Window window, int action);

void android_redirect_widget(WindowPtr pWindow, WindProperty prop, Window window);

void android_configure_window(WindowPtr pWindow, short x, short y, short w, short h);

void android_icon_update(int *pInt, int width, int height, long i);

void android_update_texture(Window window);

void android_update_widget_texture(Widget *widget);

WindAttribute *android_create_attr(WindowPtr pWindow, WindowPtr pPropWin);

int util_is_valid_utf8(const char *string);

bool util_check_window_bounds(WindowPtr pWindow, WindAttribute *attr);

bool util_check_bounds(int x, int y, int w, int h, int x1, int y1, int w1, int h1);

jobject property_icon_convert_bitmap(int* data, int width, int height);

void property_cleanup(WindProperty *prop);

void property_win_copy(WindProperty *dest, const WindProperty *src);

char *property_copy_data(const char *propData, int size);

void property_get(WindowPtr pWin, WindProperty *prop);

int property_lookup(PropertyPtr *result, WindowPtr pWin, Atom name);

int property_lookup_string(PropertyPtr *result, WindowPtr pWin, char* name);

int property_get_motif_hints(Atom name, uint32_t *data, unsigned long nitems, uint32_t format);
#endif // NODE_H
