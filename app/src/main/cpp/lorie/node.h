
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


WindowNode* node_create(WindAttribute data);

void node_insert_at_begin(WindowNode** head, WindAttribute data);

void node_append(WindowNode** head, WindAttribute data);

void node_delete(WindowNode** head, WindAttribute key);

void node_delete_by_window(WindowNode **head, Window key);

WindowNode* node_search(WindowNode* head, Window key);

WindowNode* node_get_at_position(WindowNode* head, int position);

int node_get_length(WindowNode* head);

void node_replace_at_position(WindowNode* head, int position, WindAttribute newData);

WindowNode* node_get_at_index(WindowNode* head, int index);

int node_get_max_index(WindowNode* head);



#define MAX_KEYS 100

typedef struct {
    int key;
    void* value;
} KeyValuePair;

typedef struct {
    KeyValuePair pairs[MAX_KEYS];
    int size;
} SurfaceMap;


void putVoid(SurfaceMap* map, int key, void* value);


void* get(SurfaceMap* map, int key);

void freeMap(SurfaceMap* map);

#endif // NODE_H
