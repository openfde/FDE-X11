#pragma once
#include <jni.h>
#include <android/hardware_buffer.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "screenint.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

typedef struct SurfaceRes {
    int id;
    jobject surface;
    ANativeWindow *psf;
    float width, height;
    float offset_x, offset_y;
    WindowPtr pWin;
    Window window;
} SurfaceRes ;

typedef struct SurfaceResNode {
    SurfaceRes data;
    struct SurfaceRes* next;
} SurfaceResNode;

//SurfaceResNode* surface_create(SurfaceRes data);
//
//void surface_append(SurfaceResNode** head, SurfaceRes data);
//
//SurfaceResNode* surface_search(SurfaceResNode* head, WindowPtr key);
//
//SurfaceResNode* surface_get_at_position(SurfaceResNode* head, int position);
//
//SurfaceResNode* surface_get_at_index(SurfaceResNode* head, int index);


#ifndef maybe_unused
#define maybe_unused __attribute__((__unused__))
#endif

// X server is already linked to mesa so linking to Android's GLESv2 will confuse the linker.
// That is a reason why we should compile renderer as separate hared library with its own dependencies.
// In that case part of X server's api is unavailable,
// so we should pass addresses to all needed functions to the renderer lib.
typedef void (*renderer_message_func_type) (int type, int verb, const char *format, ...);
maybe_unused void renderer_message_func(renderer_message_func_type function);

maybe_unused int renderer_init(JNIEnv* env, int* legacy_drawing, uint8_t* flip);
maybe_unused void renderer_set_buffer(JNIEnv* env, AHardwareBuffer* buffer);
maybe_unused void renderer_set_window(JNIEnv* env, jobject surface, AHardwareBuffer* buffer);
maybe_unused void renderer_set_window_each(JNIEnv* env, SurfaceRes* res, AHardwareBuffer* buffer);
maybe_unused void renderer_set_window_init(JNIEnv* env, AHardwareBuffer* buffer);
maybe_unused int renderer_should_redraw(void);
maybe_unused int renderer_redraw(JNIEnv* env, uint8_t flip, bool empty);
maybe_unused int renderer_redraw_traversal_1(JNIEnv* env, uint8_t flip, int index, Window window, bool empty);
maybe_unused jobject android_icon_convert_bitmap(int* data, int width, int height);
maybe_unused void renderer_print_fps(float millis);

maybe_unused void renderer_update_root(int w, int h, void* data, uint8_t flip);
maybe_unused void renderer_update_texture(int x, int y, int w, int h, void *data, uint8_t flip, Window window);
//maybe_unused void renderer_update_widget_texture(int x, int y, int w, int h, void *data, uint8_t flip, Widget * widget);
maybe_unused GLuint renderer_gen_bind_texture(int x, int y, int w, int h, void* data, uint8_t flip);
maybe_unused void renderer_update_cursor(int w, int h, int xhot, int yhot, void* data);
maybe_unused void renderer_set_cursor_coordinates(int x, int y);

#define AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM 5 // Stands to HAL_PIXEL_FORMAT_BGRA_8888