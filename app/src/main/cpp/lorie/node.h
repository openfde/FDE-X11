
#include <stdlib.h>
#include <X11/X.h>
#include <jni.h>
#include <android/log.h>
#include <window.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>

typedef struct {
    GLuint texture_id;
    float width, height;
    float offset_x, offset_y;
    int index;
    Window window;
    WindowPtr pWin;
    EGLSurface sfc;
} WindAttribute;

typedef struct WindowNode {
    WindAttribute data;
    struct WindowNode* next;
} WindowNode;


WindowNode* node_create(WindAttribute data);

void node_insert_at_begin(WindowNode** head, WindAttribute data);

void node_append(WindowNode** head, WindAttribute data);

void node_delete(WindowNode** head, WindAttribute key);

WindowNode* node_search(WindowNode* head, WindowPtr key);

WindowNode* node_get_at_position(WindowNode* head, int position);

int node_get_length(WindowNode* head);

void node_replace_at_position(WindowNode* head, int position, WindAttribute newData);

WindowNode* node_get_at_index(WindowNode* head, int index);

int node_get_max_index(WindowNode* head);
