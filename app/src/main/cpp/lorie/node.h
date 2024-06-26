
#ifndef NODE_H
#define NODE_H
#include <stdlib.h>
#include <X11/X.h>
#include <jni.h>
#include <android/log.h>
#include <window.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
/**
 * struct window/attribute/property for x window to android window
 */
typedef struct {
    Window window;
    Window transient;           //WM_TRANSIENT_FOR          WINDOW
    Window leader;              //WM_CLIENT_LEADER          WINDOW
    Atom window_type;           //_NET_WM_WINDOW_TYPE       ATOM
    const char * net_wm_name;   //_NET_WM_NAME              UTF8_STRING
    const char * wm_name;       //WM_NAME                  STRING
    const char * wm_class;       //WM_CLASS                  STRING
    Bool support_wm_delete;     //WM_PROTOCOLS      WM_DELETE_WINDOW
    jobject icon;               //_NET_WM_ICON
} WindProperty;

typedef struct {
    GLuint texture_id;
    float width, height;
    float offset_x, offset_y;
    Window task_to, window;
    WindowPtr pWin;
} Widget;

typedef struct {
    GLuint texture_id;
    float width, height;
    float offset_x, offset_y;
    int index;
    Window window, child;
    WindowPtr pWin;
    EGLSurface sfc;
    Widget widget , *widgets;
    int widget_size ;
    int discard;
    WindProperty aProperty;
} WindAttribute;

typedef struct WindowNode {
    WindAttribute data;
    struct WindowNode* next;
} WindowNode;


#endif // NODE_H
