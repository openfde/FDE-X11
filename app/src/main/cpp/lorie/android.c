#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma ide diagnostic ignored "bugprone-reserved-identifier"
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
#define __USE_GNU
#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <jni.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <libgen.h>
#include <globals.h>
#include <xkbsrv.h>
#include <errno.h>
#include <wchar.h>
#include <inpututils.h>
#include <randrstr.h>
#include "renderer.h"
#include "lorie.h"
#include "android.h"
#include "c_interface.h"
#include <propertyst.h>
#include <string.h>
#include <X11/Xatom.h>
#include <android/bitmap.h>
#include <signal.h>
#include <arpa/inet.h>
#include "native_log.h"

Bool LOG_ENABLE;
//Bool GL_CHECK_ERROR = FALSE;
//#define ANDROID_LOG_ENABLE 0
//#define PRINT_LOG (ANDROID_LOG_ENABLE)
//#define log(prio, ...) if(PRINT_LOG){__android_log_print(ANDROID_LOG_ ## prio, "native_android", __VA_ARGS__);}
const Atom _NET_WM_WINDOW_TYPE = 267;
const Atom _NET_WM_WINDOW_TYPE_COMBO = 268;
const Atom _NET_WM_WINDOW_TYPE_DIALOG = 269;
const Atom _NET_WM_WINDOW_TYPE_DND = 270;
const Atom _NET_WM_WINDOW_TYPE_DROPDOWN_MENU = 271;
const Atom _NET_WM_WINDOW_TYPE_MENU = 272;
const Atom _NET_WM_WINDOW_TYPE_NORMAL = 273;
const Atom _NET_WM_WINDOW_TYPE_POPUP_MENU = 274;
const Atom _NET_WM_WINDOW_TYPE_TOOLTIP = 275;
const Atom _NET_WM_WINDOW_TYPE_UTILITY = 276;

static int argc = 0;
static char **argv = NULL;
int conn_fd = -1;
extern DeviceIntPtr lorieMouse, lorieMouseRelative, lorieTouch, lorieKeyboard;
extern ScreenPtr pScreenPtr;
char *xtrans_unix_path_x11 = NULL;
char *xtrans_unix_dir_x11 = NULL;
static jclass JavaCmdEntryPointClass;
static JavaVM *jniVM = NULL;
extern struct SurfaceManagerWrapper *sfWraper;
Window focusWindow = -1;
static int window_top_level = 0;
extern DevPrivateKeyRec FDETexturePrivateKey;
extern DevPrivateKeyRec FDEWindowTexturePrivateKey;

extern int ucs2keysym(long ucs);

extern int clientNum;

#define CHECK_WITH_PROP      if(!pWin){loge( "LOG_PROPERTIES pWin null");return;}\
                             if(!pWin->optional){loge( "LOG_PROPERTIES optional null");return;}\
                             if(!pWin->optional->userProps){loge( "LOG_PROPERTIES userProps null");return;}
#define CHECK_CHILD     pWin = pWin->firstChild; CHECK_WITH_PROP
#define STRCPY   char * atom_value = (char *)calloc(pProper->size + 1, sizeof(char));strncpy(atom_value, propData, pProper->size);
#define STRING_EQUAL(str1, str2) (strcmp((str1), (str2)) == 0 ? 1 : 0)

static inline JNIEnv *GetJavaEnv(void) {
    if (!jniVM) {
        return NULL;
    }
    JNIEnv *ret = NULL;
    (*jniVM)->GetEnv(jniVM, (void **) &ret, JNI_VERSION_1_6);
    return ret;
}

void android_update_texture(Window window) {
//    loge( "android_update_texture window:%x", window);
    WindAttribute *attr = _surface_find_window(sfWraper, window);
    if (!attr) {
        loge( "android_update_texture not find window:%x", window);
        return;
    }
    if(!attr->pWin->viewable){
        loge("window:%lx no need update texture", window)
        return;
    }
    PixmapPtr pixmap = (PixmapPtr) (*pScreenPtr->GetWindowPixmap)(attr->pWin);
    TexturePrivRecPtr ptr = dixLookupPrivate(&attr->pWin->devPrivates, &FDEWindowTexturePrivateKey);
    GLuint texture_id = 0;
    if (ptr) {
        texture_id = ptr->texture;
//            loge( "android_update_texture texture:%x", ptr->texture);
    }

    renderer_update_texture(pixmap->screen_x, pixmap->screen_y, pixmap->drawable.width,
                            pixmap->drawable.height, pixmap->devPrivate.ptr, 0, window,
                            texture_id);
}

void android_update_widget_texture(Widget *widget) {
    PixmapPtr pixmap = (PixmapPtr) (*pScreenPtr->GetWindowPixmap)(widget->pWin);
//    loge( "android_update_texture pixmap:%x", pixmap->drawable.id)
    TexturePrivRecPtr ptr = dixLookupPrivate(&widget->pWin->devPrivates, &FDEWindowTexturePrivateKey);
    GLuint texture_id = 0;
    if (ptr) {
        texture_id = ptr->texture;
//        loge( "android_update_texture texture:%x", ptr->texture);
    }
//    loge( "android_update_widget_texture window:%x pixmap:%x", widget->window, pixmap->drawable.id);
    renderer_update_widget_texture(pixmap->screen_x, pixmap->screen_y, pixmap->drawable.width,
                                   pixmap->drawable.height, pixmap->devPrivate.ptr, 1, widget,
                                   texture_id);
//    _surface_log_traversal_window(sfWraper);
}

//void android_destroy_window(Window window) {
//    logd( "android_destroy_window %x", window);
//    WindAttribute attribute = {0};
//    if (_surface_count_window_in_type(sfWraper, window, TYPE_WINDOW, &attribute)) {
//        logd( "destroy_activity type window %x", window);
//        WindAttribute *attr = _surface_find_window(sfWraper, attribute.frame);
//        attr->discard = 1;
//        android_destroy_activity(attr->index, attr->pWin, attr->window, ACTION_DESTORY,
//                                 attr->prop.support_wm_delete);
//        renderer_release_window(GetJavaEnv(), attr->window);
//        _surface_delete_window(sfWraper, attr->window);
//        glDeleteTextures(1, &attr->texture_id);
//    } else if (_surface_count_window_in_type(sfWraper, window, TYPE_ANY, &attribute)) {
//        logd( "destroy_activity type any %x", window);
//        attribute.discard = 1;
//        android_destroy_activity(attribute.index, attribute.pWin, attribute.window, ACTION_DESTORY,
//                                 attribute.prop.support_wm_delete);
//        renderer_release_window(GetJavaEnv(), attribute.window);
//        _surface_delete_window(sfWraper, attribute.window);
//        glDeleteTextures(1, &attribute.texture_id);
//    } else if (_surface_count_widget(sfWraper, window)) {
//        logd( "destroy widget");
//        Widget *widget = _surface_find_widget(sfWraper, window);
//        widget->discard = 1;
//        glDeleteTextures(1, &widget->texture_id);
//        renderer_release_window(GetJavaEnv(), window);
//        _surface_remove_widget(sfWraper, window);
//    }
//}

void android_destroy_window(Window window) {
    logd( "android_destroy_window %x", window);
    _surface_log_traversal_window(sfWraper);
    WindAttribute attribute = {0};
    if(_surface_count_window_in_type(sfWraper, window, TYPE_ANY, &attribute))
    {
        logd( "destroy attribute:%0x", window);
        if(attribute.android_component == ANDROID_COMPONENT_VIEW){
            logd( "destroy view window:%x", attribute.window);
            android_destroy_view(0, attribute.pWin, attribute.prop.transient, attribute.window, ACTION_DISMISS);
            attribute.discard = 1;
            glDeleteTextures(1, &attribute.texture_id);
            renderer_release_window(GetJavaEnv(), window);
            _surface_delete_window(sfWraper, attribute.window);
        } else {
            logd( "destroy activity window:%x", attribute.frame);
            android_destroy_activity(attribute.index, attribute.pWin, attribute.window,
                                     ACTION_DESTORY,
                                     attribute.prop.support_wm_delete);
//            android_destroy_window(attribute.window);
            glDeleteTextures(1, &attribute.texture_id);
            renderer_release_window(GetJavaEnv(), attribute.window);
            _surface_delete_window(sfWraper, attribute.window);
        }

    }
    else if (_surface_count_widget(sfWraper, window))
    {
        logd( "unmap widget:%0x", window);
        Widget *widget = _surface_find_widget(sfWraper, window);
        if (!widget->inbounds) {
            android_destroy_view(0, widget->pWin, widget->task_to, widget->window, ACTION_DISMISS);
        }
        widget->discard = 1;
        glDeleteTextures(1, &widget->texture_id);
        renderer_release_window(GetJavaEnv(), window);
        _surface_remove_widget(sfWraper, window);
    }
}

void android_unmap_window(Window window) {
    logd( "%x", window);
    _surface_log_traversal_window(sfWraper);
    WindAttribute attribute = {0};
    if(_surface_count_window_in_type(sfWraper, window, TYPE_ANY, &attribute))
    {
        logd( "unmap attribute:%0x", window);
        if(attribute.android_component == ANDROID_COMPONENT_VIEW){
            logd( "unmap view window:%x", attribute.window);
            android_destroy_view(0, attribute.pWin, attribute.prop.transient, attribute.window, ACTION_DISMISS);
            attribute.discard = 1;
            glDeleteTextures(1, &attribute.texture_id);
            renderer_release_window(GetJavaEnv(), window);
            _surface_delete_window(sfWraper, attribute.window);
        } else {
            if(attribute.prop.net_wm_name && STRING_EQUAL("WPS文字", attribute.prop.net_wm_name))
            {
                logd( "unmap activity window:%x", attribute.prop.net_wm_name);
//                            android_destroy_activity(attribute.index, attribute.pWin, attribute.window,
//                                     ACTION_DESTORY,
//                                     attribute.prop.support_wm_delete);
//                android_destroy_window(attribute.window);
//                glDeleteTextures(1, &attribute.texture_id);
//                renderer_release_window(GetJavaEnv(), attribute.window);
//                _surface_delete_window(sfWraper, attribute.window);
            }
        }

    }
    else if (_surface_count_widget(sfWraper, window))
    {
        logd( "unmap widget:%0x", window);
        Widget *widget = _surface_find_widget(sfWraper, window);
        if (!widget->inbounds) {
            android_destroy_view(0, widget->pWin, widget->task_to, widget->window, ACTION_DISMISS);
        }
        widget->discard = 1;
        glDeleteTextures(1, &widget->texture_id);
        renderer_release_window(GetJavaEnv(), window);
        _surface_remove_widget(sfWraper, window);
    }
}

//void android_redirect_window_1(WindowPtr pWin) {
//    logd( "%lx", pWin->drawable.id)
//    //fill some properties
//    int redirect = pWin->overrideRedirect;
//    bool intransient_bounds = false;
//    Window taskTo = 0;
//    // get real property (name leader transient)
//    WindProperty aProperty;
//    memset(&aProperty, 0, sizeof(WindProperty));
//
//    //got a tray window
//    property_get(pWin, &aProperty);
//    if (pWin->firstChild && !redirect && aProperty.window_type != _WM_WINDOW_TYPE_SYSTRAY) {
//        if (!pWin->firstChild->overrideRedirect) {
//            property_get(pWin->firstChild, &aProperty);
//        } else {
//            property_get(pWin->firstChild->nextSib, &aProperty);
//        }
//    } else {
//        property_get(pWin, &aProperty);
//    }
//    _surface_log_traversal_window(sfWraper);
//    Atom win_type = aProperty.window_type;
//    if (aProperty.transient != 0) {
//        WindAttribute *attr = _surface_find_window(sfWraper, aProperty.transient);
//        if (attr) {
//            taskTo = attr->window;
//            intransient_bounds = util_check_window_bounds(pWin, attr);
//        }
//    }
//
//    loge( "%x redirect:%d atom:%d transient:%x, "
//               "taskTo:%x inbounds:%d mapped:%d clientNum:%d prop.window_type %d",
//        pWin->drawable.id, redirect, win_type, aProperty.transient, taskTo,
//        intransient_bounds, pWin->mapped, clientNum, aProperty.window_type);
//    //TODO revert from steam
//
//
//    if (redirect) {
//        if (taskTo != 0) {
//            taskTo = focusWindow;
//        }
//        if (_surface_count_window(sfWraper, taskTo)) {
//            android_redirect_widget(pWin, aProperty, taskTo);
//            property_cleanup(&aProperty);
//            return;
//        } else {
//            aProperty.window_type = _WM_WINDOW_TYPE_SYSTIP;
//        }
//    }
//
//
//    if (_surface_count_window(sfWraper, pWin->drawable.id)) {
//        logd( "already redirect_window window:%lx", pWin->drawable.id)
//        WindAttribute *attr = _surface_find_window(sfWraper, pWin->drawable.id);
////        if( attr->frame) {
////            android_create_or_map_window(*attr, attr->prop, taskTo, intransient_bounds, true);
////        } else {
//        JNIEnv *JavaEnv = GetJavaEnv();
//        if (JavaEnv && JavaCmdEntryPointClass) {
//            jmethodID method = (*JavaEnv)->GetStaticMethodID(JavaEnv, JavaCmdEntryPointClass,
//                                                             "xserverMapWindow",
//                                                             "(J)V");
//            (*JavaEnv)->CallStaticVoidMethod(JavaEnv, JavaCmdEntryPointClass,
//                                             method, (long) pWin->drawable.id);
//
////            }
//        }
//
////        property_cleanup(&prop);
//        return;
//    } else if (!redirect || aProperty.window_type == _WM_WINDOW_TYPE_SYSTIP) {
//        PixmapPtr pixmap = (*pScreenPtr->GetWindowPixmap)(pWin);
//        int x = pWin->drawable.x;
//        int y = pWin->drawable.y;
//        int w = pixmap->drawable.width;
//        int h = pixmap->drawable.height;
//        GLuint tid = renderer_gen_bind_texture(x, y, w, h, pixmap->devPrivate.ptr, 0);
//        WindAttribute windAttribute = {
//                .offset_x = x,
//                .offset_y = y,
//                .width = w,
//                .height = h,
//                .pWin = pWin,
//                .window = pWin->drawable.id,
//                .texture_id = tid,
//                .widget_size = 0,
////                .prop = prop
//        };
//        property_win_copy(&windAttribute.prop, &aProperty);  // ✅ 深拷贝
//        if (pWin->firstChild) {
//            windAttribute.child = pWin->firstChild->drawable.id;
//            windAttribute.frame = pWin->drawable.id;
//        }
//        logd("%x to redirect", pWin->drawable.id)
////        printWindAttribute(&windAttribute);
//        _surface_redirect_window(sfWraper, pWin->drawable.id, &windAttribute, win_type);
//        android_create_or_map_window(windAttribute, aProperty, taskTo, intransient_bounds, true);
//        _surface_log_traversal_window(sfWraper);
////        property_cleanup(&prop);
//        return;
//    }
//}

/**
 * // start to redirect
// 1. 检查是否已经重定向
if (already_redirected) {
    // 如果是：映射窗口
    map_window();
} else {
    // 如果否：准备创建 Android 组件
    // ready to create android component

    // 2. 选择正确的类型
    // choose the right type

    // 3. 决定类型
    // decide the type

    // 检查是否覆盖重定向
    if (override_redirect == yes) {
        // 检查窗口瞬态是否已恢复
        if (window_transient_for_is_resumed == yes) {
            // 如果是：广播到窗口 (原生/native)
            broadcast_to_window();
            // 这是 Java 层的操作
            // java
        } else {
            // 如果否：继续到创建系统视图
            // go to create a system view

            // 检查类型是否大于等于 1000
            if (type >= 1000) {
                // 如果类型 >= 1000 (帧窗口)
                // frame window
                // 创建活动 (Activity)
                create_a_activity();
            } else {
                // 如果类型 < 1000 (非帧窗口)
                // non-frame window
                // 创建系统视图 (System View)
                create_a_system_view();

                // 在 Android 视图系统中
                // in android view system

                // 决定视图类型...
                decide_view_type_...();
            }
        }
    } else {
        // 检查类型是否大于等于 1000
        if (type >= 1000) {
            // 如果类型 >= 1000 (帧窗口)
            // frame window
            // 创建活动 (Activity)
            create_a_activity();
        } else {
            // 如果类型 < 1000 (非帧窗口)
            // non-frame window
            // 创建系统视图 (System View)
            create_a_system_view();

            // 在 Android 视图系统中
            // in android view system

            // 决定视图类型...
            decide_view_type_...();
        }
    }
}
 */
void android_redirect_window(WindowPtr pWin) {
    //already redirect to android
    if(_surface_count_window_any(sfWraper, pWin->drawable.id)){
        logd( "already redirect_window window:%lx", pWin->drawable.id)
        WindAttribute *attr = _surface_find_window(sfWraper, pWin->drawable.id);
        JNIEnv *JavaEnv = GetJavaEnv();
        if (JavaEnv && JavaCmdEntryPointClass) {
            jmethodID method = (*JavaEnv)->GetStaticMethodID(JavaEnv, JavaCmdEntryPointClass, "xserverMapWindow", "(J)V");
            (*JavaEnv)->CallStaticVoidMethod(JavaEnv, JavaCmdEntryPointClass,  method, (long) pWin->drawable.id);
        }
        return;
    }

    //lunch activity
    PropertyPtr pProp;
    int rc = property_lookup(&pProp, pWin, XA_WM_NAME);
    char *wm_name = property_copy_data(pProp->data, pProp->size);
    if(rc &&  wm_name && STRING_EQUAL(wm_name, "android_frame")){
        logd( "ready to redirect_activity %lx", pWin->drawable.id)
        logd( "redirect_window frame %lx should lunch activity", pWin->drawable.id)
        logd( "     get property from its first child", pWin->drawable.id)
        WindowPtr p;
        if (!pWin->firstChild->overrideRedirect) {
            p = pWin->firstChild;
        } else {
            p = pWin->firstChild->nextSib;
        }
        WindAttribute *attr = android_create_attr(pWin, p);
        attr->android_component = ANDROID_COMPONENT_ACTIVITY;
        _surface_redirect_window(sfWraper, pWin->drawable.id, attr, attr->prop.window_type);
        android_create_or_map_window(*attr, attr->prop, 0, false, true);
        return;
    }

    logd( "ready to redirect_view %lx", pWin->drawable.id)
    //lunch system view
    PropertyPtr pType;
    rc = property_lookup_string(&pType, pWin, WINDOW_TYPE);
    if(rc){
        ATOM type = ((ATOM*)(pType->data))[0];
        logd( "window type %d ", type)
        if(rc && type >= _WM_WINDOW_TYPE_SYSTRAY){
            logd( "redirect_view %lx for systemtray ", pWin->drawable.id)
            WindAttribute *attr = android_create_attr(pWin, pWin);
            attr->android_component = ANDROID_COMPONENT_VIEW;
            attr->override_window_type = _WM_WINDOW_TYPE_SYSTRAY;
            _surface_redirect_window(sfWraper, pWin->drawable.id, attr, attr->prop.window_type);
            android_create_or_map_window(*attr, attr->prop, 0, false, true);
            return;
        }
    }
    if(pWin->overrideRedirect){
        WindProperty windProperty;
        WindAttribute* attr;
        memset(&windProperty, 0, sizeof(WindProperty));
        property_get(pWin, &windProperty);
        logd( "redirect_view %x for overrideRedirect transient:%x leader:%x, pid:%ld, windowtype:%d",
            pWin->drawable.id, windProperty.transient, windProperty.leader, windProperty.pid, windProperty.window_type)
        _surface_log_traversal_window(sfWraper);
        if(windProperty.transient){
            attr = _surface_find_window_in_type(sfWraper, TYPE_ANY , windProperty.transient);
            if(attr)
            {
                logd( "redirect_view %x for transient found attr:%x", pWin->drawable.id, attr->window)
            }
        } else if(windProperty.leader){
            attr = _surface_find_window_in_type(sfWraper, TYPE_ANY , windProperty.leader);
            if(attr)
            {
                logd( "redirect_view %x for leader found attr:%x", pWin->drawable.id, attr->window)
            }
        } else if(focusWindow){
            attr = _surface_find_window_in_type(sfWraper, TYPE_ANY , focusWindow);
            if(attr)
            {
                logd( "redirect_view %x for focusWindow found attr:%x", pWin->drawable.id, attr->window)
            }
        } else if (windProperty.pid) {
            attr = _surface_find_window_in_type(sfWraper, TYPE_ANY , windProperty.pid);
            if(attr)
            {
                logd( "redirect_view %x for pid found attr:%x", pWin->drawable.id, attr->window)
            }
        }
        if(attr && attr->status >= ANDROID_STATUS_FOCUSED && attr->android_component > ANDROID_COMPONENT_VIEW){
            logd( "redirect_widget  %x should create view for widget", pWin->drawable.id)
            android_redirect_widget(pWin, windProperty, attr->window);
            property_cleanup(&windProperty);
            return;
        } else {
            logd( "transient window not resumed redirect_widget  %lx should create view for attr", pWin->drawable.id)
            WindAttribute *create_attr = android_create_attr(pWin, pWin);
            create_attr->override_window_type = _WM_WINDOW_TYPE_SYSTIP;
            create_attr->android_component = ANDROID_COMPONENT_VIEW;
            _surface_redirect_window(sfWraper, pWin->drawable.id, create_attr, create_attr->override_window_type);
            android_create_or_map_window(*create_attr, create_attr->prop, 0, false, true);
            return;
        }
    }
}

WindAttribute *android_create_attr(WindowPtr pWin, WindowPtr pPropWin) {
    WindProperty windProperty;
    Window taskTo = 0;
    memset(&windProperty, 0, sizeof(WindProperty));
    property_get(pPropWin, &windProperty);
    if(windProperty.net_wm_name && STRING_EQUAL("WPS文字", windProperty.net_wm_name))
    {
        windProperty.support_motif = 1;
    }
    PixmapPtr pixmap = (*pScreenPtr->GetWindowPixmap)(pWin);
    int x = pWin->drawable.x;
    int y = pWin->drawable.y;
    int w = pixmap->drawable.width;
    int h = pixmap->drawable.height;
    logd( " %lx x:%d y:%d w:%d h:%d", pWin->drawable.id, x, y, w, h)
    GLuint tid = renderer_gen_bind_texture(x, y, w, h, pixmap->devPrivate.ptr, 0);
    WindAttribute *windAttribute = (WindAttribute *)malloc(sizeof(WindAttribute));
    if (!windAttribute) {
        loge( "Failed to allocate WindAttribute");
        return NULL;
    }
    memset(windAttribute, 0, sizeof(WindAttribute));
    windAttribute->offset_x = x;
    windAttribute->offset_y = y;
    windAttribute->width = w;
    windAttribute->height = h;
    windAttribute->pWin = pWin;
    windAttribute->window = pWin->drawable.id;
    windAttribute->texture_id = tid;
    windAttribute->widget_size = 0;
    windAttribute->discard = 0;
    windAttribute->status = 0;
    windAttribute->level = 0;
    property_win_copy(&windAttribute->prop, &windProperty);
    if (pWin->firstChild) {
        windAttribute->child = pWin->firstChild->drawable.id;
        windAttribute->frame = pWin->drawable.id;
    }
    logd( "%lx redirect:%d x:%d y:%d w:%d h:%d atom:%d transient:%lx, "
               "taskTo:%lx mapped:%d clientNum:%d prop.window_type %d",
        pWin->drawable.id, x, y, w, h, pWin->overrideRedirect, windProperty.window_type,
        windProperty.transient, taskTo, pWin->mapped, clientNum, windProperty.window_type);

    return windAttribute;
}

//update some effect property and do sth if need , eg. window icon
void property_update_android(WindowPtr pWin, Atom prop, ClientPtr client) {
    WindAttribute attribute = {0};
    if (!_surface_count_window_in_type(sfWraper, pWin->drawable.id,
                                       TYPE_WINDOW, &attribute)) {
        return;
    }
    CHECK_WITH_PROP;
    if (STRING_EQUAL(NameForAtom(prop), WINDOW_ICON)) {
        loge("window:%lx name:%s", pWin->drawable.id, NameForAtom(prop))
        PropertyPtr pProp;
        int rc = dixLookupProperty(&pProp, pWin, prop, client,
                                   DixReadAccess);
        if (rc == Success) {
            unsigned char *propData = pProp->data;
            int *icon_data = (int *) propData;
            int width = *icon_data;
            int height = *(icon_data + 1);
            int *imageData = (int *) (icon_data + 2);
            android_icon_update(imageData, width, height, (long) pWin->drawable.id);
        }
    }
}

void android_icon_update(int *data, int width, int height, long window) {
    if (!data || width <= 0 || height <= 0) {
        loge( "Invalid input parameters: data=%p, width=%d, height=%d", data, width, height);
        return;
    }
    JNIEnv *env = GetJavaEnv();
    if (!env) {
        loge( "Failed to get JavaEnv");
        return;
    }
    int dataLength = width * height;
    jintArray javaData = (*env)->NewIntArray(env, dataLength);
    if (!javaData) {
        loge( "Failed to create jintArray");
        return;
    }
    (*env)->SetIntArrayRegion(env, javaData, 0, dataLength, data);
    if (!JavaCmdEntryPointClass) {
        loge( "Failed to find class com/fde/x11/Xserver");
        return;
    }
    jmethodID mid = (*env)->GetStaticMethodID(env, JavaCmdEntryPointClass, "createBitmapFromNative",
                                              "([IIIJ)V");
    if (!mid) {
        loge( "Failed to get method ID for createBitmapFromNative");
        (*env)->DeleteLocalRef(env, javaData);
        return;
    }
    (*env)->CallStaticVoidMethod(env, JavaCmdEntryPointClass, mid, javaData, width, height, window);
    (*env)->DeleteLocalRef(env, javaData);
}

bool util_check_bounds(int x, int y, int w, int h, int x1, int y1, int w1, int h1) {
    logd( "x:%d y:%d w:%d h:%d x1:%d y1:%d w1:%d h1:%d ",
        x, y, w, h, x1, y1, w1, h1);
    if (x < x1 || y < y1 || (x + w) > (x1 + w1) || (y + h) > (y1 + h1)) {
        return FALSE;
    }
    return TRUE;
}

bool util_check_window_bounds(WindowPtr pWindow, WindAttribute *attr) {
    int x = pWindow->drawable.x;
    int y = pWindow->drawable.y;
    int w = pWindow->drawable.width;
    int h = pWindow->drawable.height;
    int x1 = attr->offset_x;
    int y1 = attr->offset_y;
    int w1 = attr->width;
    int h1 = attr->height;
    bool inbound = util_check_bounds(x, y, w, h, x1, y1, w1, h1);
    logd( "inbound:%d", inbound);
    return inbound;
}

void android_redirect_widget(WindowPtr pWin, WindProperty prop, Window window) {
    PixmapPtr pixmap = (*pScreenPtr->GetWindowPixmap)(pWin);
    WindAttribute *attr = _surface_find_window(sfWraper, window);
//    loge( "window:%lx taskto:%lx", pWin->drawable.id, window);
    if (attr) {
        GLuint id = renderer_gen_bind_texture(pWin->drawable.x, pWin->drawable.y,
                                              pixmap->drawable.width,
                                              pixmap->drawable.height, pixmap->devPrivate.ptr, 0);
        bool inBound =
                util_check_window_bounds(pWin, attr) && prop.window_type != _NET_WM_WINDOW_TYPE_DND;
        Widget widget = {
                .texture_id = id,
                .offset_x = pWin->drawable.x,
                .offset_y = pWin->drawable.y,
                .width = pWin->drawable.width,
                .height = pWin->drawable.height,
                .window = pWin->drawable.id,
                .pWin = pWin,
                .task_to = window,
                .inbounds = inBound
        };
        if (!attr->widgets) {
            attr->widgets = malloc(sizeof(Widget) * 10);
        }

        if (attr->widgets == NULL) {
            loge( "widget malloc failed")
        }
        attr->widgets[attr->widget_size] = widget;
        attr->widget_size++;
//        loge( "android_redirect_widget texture:%d", id);
        if (!inBound) {
            android_create_view(widget, prop, window, inBound);
        }
    }
}

void android_create_view(Widget widget, WindProperty aProperty, Window taskTo, bool inbound) {
    logd( "window:%lx wm_name:%s net_wm_name:%s inbound:%d",
        widget.window, aProperty.wm_name, aProperty.net_wm_name, inbound);
    JNIEnv *JavaEnv = GetJavaEnv();
    if (JavaEnv && JavaCmdEntryPointClass) {
        logd( "ready to create view");
        Window aWindow = aProperty.window;
        Window aTransient = aProperty.transient;
        Window aLeader = aProperty.leader;
        int aType = aProperty.window_type;
        jstring wm_name = NULL, net_wm_name = NULL, wm_class = NULL;
        if (util_is_valid_utf8(aProperty.net_wm_name)) {
            net_wm_name = (*JavaEnv)->NewStringUTF(JavaEnv, aProperty.net_wm_name);
        }
        if (util_is_valid_utf8(aProperty.wm_name)) {
            wm_name = (*JavaEnv)->NewStringUTF(JavaEnv, aProperty.wm_name);
        }
        if (util_is_valid_utf8(aProperty.wm_class)) {
            wm_class = (*JavaEnv)->NewStringUTF(JavaEnv, aProperty.wm_class);
        }
        int offsetX = widget.pWin->drawable.x;
        int offsetY = widget.pWin->drawable.y;
        int width = widget.pWin->drawable.width;
        int height = widget.pWin->drawable.height;
//        int index = widget.index;
        WindowPtr windowPtr = widget.pWin;
        Window window = widget.window;
        jmethodID method = (*JavaEnv)->GetStaticMethodID(JavaEnv, JavaCmdEntryPointClass,
                                                         "startOrUpdateWindow",
                                                         "(JJJILjava/lang/String;Ljava/lang/String;IIIIIJJJIILandroid/graphics/Bitmap;ZIZJZ)V");
        (*JavaEnv)->CallStaticVoidMethod(JavaEnv, JavaCmdEntryPointClass, method,
                                         aWindow, aTransient, aLeader, aType, wm_class,
                                         net_wm_name == NULL ? wm_name : net_wm_name,
                                         offsetX, offsetY, width, height, 0,
                                         (long) windowPtr, (long) window, (long) taskTo,
                                         aProperty.support_wm_delete, aProperty.support_motif,
                                         aProperty.icon ? aProperty.icon : NULL, inbound, clientNum,
                                         false, aWindow, true);
    }
}

void android_create_or_map_window(WindAttribute attribute, WindProperty prop, Window taskTo, bool inbound, bool create) {
    logd( "window:%x wm_name:%s net_wm_name:%s inbound:%d",
        attribute.window, prop.wm_name, prop.net_wm_name, inbound);
    JNIEnv *JavaEnv = GetJavaEnv();
    if (JavaEnv && JavaCmdEntryPointClass) {
        logd( "ready to create window %x", attribute.window);
        Window aid = attribute.window;
        Window aTransient = prop.transient;
        Window aLeader = prop.leader;
        int aType = attribute.override_window_type ? attribute.override_window_type :prop.window_type;
        jstring wm_name = NULL, net_wm_name = NULL, wm_class = NULL;
        if (util_is_valid_utf8(prop.net_wm_name)) {
            net_wm_name = (*JavaEnv)->NewStringUTF(JavaEnv, prop.net_wm_name);
        }
        if (util_is_valid_utf8(prop.wm_name)) {
            wm_name = (*JavaEnv)->NewStringUTF(JavaEnv, prop.wm_name);
        }
        if (util_is_valid_utf8(prop.wm_class)) {
            wm_class = (*JavaEnv)->NewStringUTF(JavaEnv, prop.wm_class);
        }
        int offsetX = attribute.pWin->drawable.x;
        int offsetY = attribute.pWin->drawable.y;
        int width = attribute.pWin->drawable.width;
        int height = attribute.pWin->drawable.height;
        int index = attribute.index;
        WindowPtr windowPtr = attribute.pWin;
//        Window window = attribute.window;
        jmethodID method = (*JavaEnv)->GetStaticMethodID(JavaEnv, JavaCmdEntryPointClass,
                                                         "startOrUpdateWindow",
                                                         "(JJJILjava/lang/String;Ljava/lang/String;IIIIIJJJIILandroid/graphics/Bitmap;ZIZJZ)V");
        (*JavaEnv)->CallStaticVoidMethod(JavaEnv, JavaCmdEntryPointClass, method,
                                         (long) aid, (long) aTransient, (long) aLeader, aType,
                                         wm_class, net_wm_name == NULL ? wm_name : net_wm_name,
                                         offsetX, offsetY, width, height, index,
                                         (long) windowPtr, (long) aid, (long) taskTo,
                                         prop.support_wm_delete, prop.support_motif,
                                         prop.icon ? prop.icon : NULL, inbound, clientNum,
                                         true, (long) attribute.child, create);
//        free(prop.net_wm_name);
//        free(prop.wm_class);
//        free(prop.wm_name);
    }
}

void android_configure_window(WindowPtr pWin, short x, short y, short w, short h) {
//    _surface_log_traversal_window(sfWraper);
//    WindAttribute *attr = _surface_find_window(sfWraper, pWin->drawable.id);
    logd( "rediret:%d pWin:%lx x:%d y:%d w:%d h:%d",
        pWin->overrideRedirect, pWin->drawable.id, x, y, w, h)
    if (!pWin->overrideRedirect) {
        return;
    }
    JNIEnv *JavaEnv = GetJavaEnv();
    if (JavaEnv && JavaCmdEntryPointClass) {
        jmethodID method = (*JavaEnv)->GetStaticMethodID(JavaEnv, JavaCmdEntryPointClass,
                                                         "configureWidget", "(JIIII)V");
        (*JavaEnv)->CallStaticVoidMethod(JavaEnv, JavaCmdEntryPointClass, method,
                                         (long) pWin->drawable.id,
                                         x, y, w, h);
    }
}

void android_destroy_activity(int index, WindowPtr pWin, Window window, int action, Bool wm_delete) {
    logd( "index:%d action:%d window:%lx clientNum:%d", index, action,
        window, clientNum);
    JNIEnv *JavaEnv = GetJavaEnv();
    if (JavaEnv && JavaCmdEntryPointClass) {
        jmethodID method = (*JavaEnv)->GetStaticMethodID(JavaEnv, JavaCmdEntryPointClass,
                                                         "closeOrDestroyWindow", "(IJJJIII)V");
        (*JavaEnv)->CallStaticVoidMethod(JavaEnv, JavaCmdEntryPointClass, method, index,
                                         (long) pWin, 0L, (long) window, action, wm_delete,
                                         clientNum);
    }
}

void android_destroy_view(int index, WindowPtr pWin, Window task_to, Window window, int action) {
    logd( "index%d task_to:%p window:%lx", index, task_to, window);
    JNIEnv *JavaEnv = GetJavaEnv();
    if (JavaEnv && JavaCmdEntryPointClass) {
        jmethodID method = (*JavaEnv)->GetStaticMethodID(JavaEnv, JavaCmdEntryPointClass,
                                                         "closeOrDestroyWindow", "(IJJJIII)V");
        (*JavaEnv)->CallStaticVoidMethod(JavaEnv, JavaCmdEntryPointClass, method, index,
                                         (long) pWin, (long) task_to, (long) window, action, 0,
                                         clientNum);
    }
}

void android_update_cursor(int w, int h, int xhot, int yhot, void *data) {
    JNIEnv *JavaEnv = GetJavaEnv();
    (*jniVM)->GetEnv(jniVM, (void **) &JavaEnv, JNI_VERSION_1_6);
    if (JavaEnv && JavaCmdEntryPointClass) {
        jobject cursor_icon = property_icon_convert_bitmap(data, w, h);
        jmethodID method = (*JavaEnv)->GetStaticMethodID(JavaEnv, JavaCmdEntryPointClass,
                                                         "updateCursor",
                                                         "(Landroid/graphics/Bitmap;II)V");
        (*JavaEnv)->CallStaticVoidMethod(JavaEnv, JavaCmdEntryPointClass, method, cursor_icon, xhot,
                                         yhot);
    }
}

static void *startServer(unused void *cookie) {
    lorieSetVM((JavaVM *) cookie);
    char *envp[] = {NULL};
    exit(dix_main(argc, (char **) argv, envp));
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    jniVM = vm;
    sfWraper = _surface_create_manager();
    return JNI_VERSION_1_6;
}

JNIEXPORT jboolean JNICALL
Java_com_fde_x11_Xserver_start(JNIEnv *env, unused jobject thiz, jobjectArray args, jboolean logEnable) {
//    JavaVM *vm = NULL;
    // execv's argv array is a bit incompatible with Java's String[], so we do some converting here...
    argc = (*env)->GetArrayLength(env, args) + 1; // Leading executable path
    argv = (char **) calloc(argc, sizeof(char *));
    LOG_ENABLE = logEnable;

    JNIEnv *JavaEnv = env;
    JavaCmdEntryPointClass = (*JavaEnv)->NewGlobalRef(JavaEnv, thiz);
    setenv("XKB_CONFIG_ROOT", "/data/data/com.fde.x11/files/xkb/", 1);

    argv[0] = (char *) "FDEX11";
    for (int i = 1; i < argc; i++) {
        jstring js = (jstring) ((*env)->GetObjectArrayElement(env, args, i - 1));
        const char *pjc = (*env)->GetStringUTFChars(env, js, JNI_FALSE);
        argv[i] = (char *) calloc(strlen(pjc) + 1,
                                  sizeof(char)); //Extra char for the terminating NULL
        strcpy((char *) argv[i], pjc);
        (*env)->ReleaseStringUTFChars(env, js, pjc);
    }

    {
        cpu_set_t mask;
        long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);

        for (int i = num_cpus / 2; i < num_cpus; i++)
            CPU_SET(i, &mask);

        if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1)
            loge( "Failed to set process affinity: %s", strerror(errno));
    }

    if (getenv("TERMUX_X11_DEBUG") && !fork()) {
        // Printing logs of local logcat.
        char pid[32] = {0};
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        sprintf(pid, "%d", getppid());
        execlp("logcat", "logcat", "--pid", pid, NULL);
    }

    // adb sets TMPDIR to /data/local/tmp which is pretty useless.
//    if (!strcmp("/data/local/tmp", getenv("TMPDIR") ?: ""))
    unsetenv("TMPDIR");

    if (!getenv("TMPDIR")) {
        if (access("/tmp", F_OK) == 0)
            setenv("TMPDIR", "/tmp", 1);
        else if (access("/data/data/com.termux/files/usr/tmp", F_OK) == 0)
            setenv("TMPDIR", "/data/data/com.termux/files/usr/tmp", 1);
    }


    if (!getenv("TMPDIR")) {
        char *error = (char *) "$TMPDIR is not set. Normally it is pointing to /tmp of a container.";
        loge( "%s", error);
        dprintf(2, "%s\n", error);
        return JNI_FALSE;
    }

    {
        char *tmp = getenv("TMPDIR");
        char cwd[1024] = {0};

        if (!getcwd(cwd, sizeof(cwd)) || access(cwd, F_OK) != 0)
            chdir(tmp);
        asprintf(&xtrans_unix_path_x11, "%s/.X11-unix/X", tmp);
        asprintf(&xtrans_unix_dir_x11, "%s/.X11-unix/", tmp);

    }


    {
        const char *root_dir = dirname(getenv("TMPDIR"));
        const char *pathes[] = {
                "/etc/X11/fonts", "/usr/share/fonts/X11", "/share/fonts", NULL
        };
        for (int i = 0; pathes[i]; i++) {
            char current_path[1024] = {0};
            snprintf(current_path, sizeof(current_path), "%s%s", root_dir, pathes[i]);
            if (access(current_path, F_OK) == 0) {
                char default_font_path[4096] = {0};
                snprintf(default_font_path, sizeof(default_font_path),
                         "%s/misc,%s/TTF,%s/OTF,%s/Type1,%s/100dpi,%s/75dpi",
                         current_path, current_path, current_path, current_path, current_path,
                         current_path);
                defaultFontPath = strdup(default_font_path);
                break;
            }
        }
    }

    if (!getenv("XKB_CONFIG_ROOT")) {
        // chroot case
        const char *root_dir = dirname(getenv("TMPDIR"));
        char current_path[1024] = {0};
        snprintf(current_path, sizeof(current_path), "%s/usr/share/X11/xkb", root_dir);
        if (access(current_path, F_OK) == 0)
            setenv("XKB_CONFIG_ROOT", current_path, 1);
    }
    if (!getenv("XKB_CONFIG_ROOT")) {
        // proot case
        if (access("/usr/share/X11/xkb", F_OK) == 0)
            setenv("XKB_CONFIG_ROOT", "/usr/share/X11/xkb", 1);
            // Termux case
        else if (access("/data/data/com.termux/files/usr/share/X11/xkb", F_OK) == 0)
            setenv("XKB_CONFIG_ROOT", "/data/data/com.termux/files/usr/share/X11/xkb", 1);
    }

    if (!getenv("XKB_CONFIG_ROOT")) {
        char *error = (char *) "$XKB_CONFIG_ROOT is not set. Normally it is pointing to /usr/share/X11/xkb of a container.";
        loge( "%s", error);
        dprintf(2, "%s\n", error);
        return JNI_FALSE;
    }

    XkbBaseDirectory = getenv("XKB_CONFIG_ROOT");
    if (access(XkbBaseDirectory, F_OK) != 0) {
        loge( "%s is unaccessible: %s\n", XkbBaseDirectory, strerror(errno));
        printf("%s is unaccessible: %s\n", XkbBaseDirectory, strerror(errno));
        return JNI_FALSE;
    }

    char *xkb_root = getenv("XKB_CONFIG_ROOT");

    JavaVM *vm = NULL;
    pthread_t t = 0;
    if ((*env)->GetJavaVM(env, &vm) != JNI_OK) {
        loge( "GetJavaVM fail")
        return JNI_TRUE;
    }
    logd( "vm address is %p ", vm)
    if (vm == NULL) {
        loge( "VM isNULL")
    }
    logd( "t address is %p ", &t)
    if (pthread_create(&t, NULL, startServer, vm) != 0) {
        logd( "t address is %p ", &t)
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_fde_x11_Xserver_windowChanged(JNIEnv *env, unused jobject cls, jobject surface, jfloat offsetX, jfloat offsetY, jfloat width, jfloat height, jint index, jlong windowPtr, jlong window) {
    jobject sfc = surface ? (*env)->NewGlobalRef(env, surface) : NULL;
    logd( "windowChanged index:%d surface:%p", index, sfc);
    SurfaceRes *res = (SurfaceRes *) malloc(sizeof(SurfaceRes));
    res->id = (int) index;
    res->surface = sfc;
    res->offset_x = (int) offsetX;
    res->offset_y = (int) offsetY;
    res->width = (int) width;
    res->height = (int) height;
    res->pWin = (WindowPtr) windowPtr;
    res->window = window;
    if(res->width == -1 && res->height == -1){
        WindAttribute *attr =  _surface_find_window(sfWraper, res->window);
        if(attr){
            attr->discard = 1;
//            glDeleteTextures(1, &attr->texture_id);
            renderer_release_window(GetJavaEnv(), attr->window);
            _surface_delete_window(sfWraper, attr->window);
        }
        return;
    }
    QueueWorkProc(lorieChangeWindow, NULL, res);
}

void handleLorieEvents(int fd, maybe_unused int ready, maybe_unused void *data) {
    ValuatorMask mask;
    lorieEvent e = {0};
    valuator_mask_zero(&mask);

    if (ready & X_NOTIFY_ERROR) {
//        RemoveNotifyFd(fd);
        InputThreadUnregisterDev(fd);
        close(fd);
        conn_fd = -1;
        lorieEnableClipboardSync(FALSE);
        return;
    }
//    __android_log_print(ANDROID_LOG_ERROR, "native_android",
//                        "handleLorieEvents: %d ", fd);
    if (read(fd, &e, sizeof(e)) == sizeof(e)) {
        switch (e.type) {
            case EVENT_SCREEN_SIZE:
                logd( "tx11-request", "window changed: %d %d",
                                    e.screenSize.width, e.screenSize.height);
                lorieConfigureNotify(e.screenSize.width, e.screenSize.height,
                                     e.screenSize.framerate);
                break;
            case EVENT_TOUCH: {
                double x, y;
                DDXTouchPointInfoPtr touch = TouchFindByDDXID(lorieTouch, e.touch.id, FALSE);
                loge( "EVENT_TOUCH ");
                x = (float) e.touch.x * 0xFFFF /
                    (float) pScreenPtr->GetScreenPixmap(pScreenPtr)->drawable.width;
                y = (float) e.touch.y * 0xFFFF /
                    (float) pScreenPtr->GetScreenPixmap(pScreenPtr)->drawable.height;

                // Avoid duplicating events
                if (touch && touch->active) {
                    double oldx, oldy;
                    if (e.touch.type == XI_TouchUpdate &&
                        valuator_mask_fetch_double(touch->valuators, 0, &oldx) &&
                        valuator_mask_fetch_double(touch->valuators, 1, &oldy) &&
                        oldx == x && oldy == y)
                        break;
                }

                // Sometimes activity part does not send XI_TouchBegin and sends only XI_TouchUpdate.
                if (e.touch.type == XI_TouchUpdate && (!touch || !touch->active))
                    e.touch.type = XI_TouchBegin;

                if (e.touch.type == XI_TouchEnd && (!touch || !touch->active))
                    break;

                __android_log_print(ANDROID_LOG_ERROR, "tx11-request", "touch event: %d %d %d %d",
                                    e.touch.type, e.touch.id, e.touch.x, e.touch.y);
                valuator_mask_set_double(&mask, 0, x);
                valuator_mask_set_double(&mask, 1, y);
//                loge( "EVENT_TOUCH button %d x:%.0f y:%.0f", e.mouse.detail, e.mouse.x,
//                    e.mouse.y);
                QueueTouchEvents(lorieTouch, e.touch.type, e.touch.id, 0, &mask);
                break;
            }
            case EVENT_MOUSE: {
                int flags;
                loge( "EVENT_MOUSE button %d x:%.0f y:%.0f, down:%d", e.mouse.detail,
                    e.mouse.x,
                    e.mouse.y, e.mouse.down);
                switch (e.mouse.detail) {
                    case 0: // BUTTON_UNDEFINED
                        if (e.mouse.relative) {
                            valuator_mask_set_double(&mask, 0, (double) e.mouse.x);
                            valuator_mask_set_double(&mask, 1, (double) e.mouse.y);
                            QueuePointerEvents(lorieMouseRelative, MotionNotify, 0,
                                               POINTER_RELATIVE | POINTER_ACCELERATE, &mask);
                        } else {
                            flags = POINTER_ABSOLUTE | POINTER_SCREEN | POINTER_NORAW;
                            valuator_mask_set_double(&mask, 0, (double) e.mouse.x);
                            valuator_mask_set_double(&mask, 1, (double) e.mouse.y);
                            QueuePointerEvents(lorieMouse, MotionNotify, 0, flags, &mask);
                        }
                        break;
                    case 1: // BUTTON_LEFT
                    case 2: // BUTTON_MIDDLE
                    case 3: // BUTTON_RIGHT
                        QueuePointerEvents(e.mouse.relative ? lorieMouseRelative : lorieMouse,
                                           e.mouse.down ? ButtonPress : ButtonRelease,
                                           e.mouse.detail, 0, &mask);
                        break;
                    case 4: // BUTTON_SCROLL
                        if (e.mouse.x) {
                            valuator_mask_zero(&mask);
                            valuator_mask_set_double(&mask, 2, (double) e.mouse.x / 120);
                            QueuePointerEvents(lorieMouseRelative, MotionNotify, 0,
                                               POINTER_RELATIVE, &mask);
                        }
                        if (e.mouse.y) {
                            valuator_mask_zero(&mask);
                            valuator_mask_set_double(&mask, 3, (double) e.mouse.y / 120);
                            QueuePointerEvents(lorieMouseRelative, MotionNotify, 0,
                                               POINTER_RELATIVE, &mask);
                        }
                        break;
                }
                break;
            }
            case EVENT_KEY:
                QueueKeyboardEvents(lorieKeyboard, e.key.state ? KeyPress : KeyRelease, e.key.key);
                break;
            case EVENT_UNICODE: {
                int ks = ucs2keysym((long) e.unicode.code);
                __android_log_print(ANDROID_LOG_DEBUG, "LorieNative", "Trying to input keysym %d\n",
                                    ks);
                lorieKeysymKeyboardEvent(ks, TRUE);
                lorieKeysymKeyboardEvent(ks, FALSE);
                break;
            }
            case EVENT_CLIPBOARD_SYNC:
                lorieEnableClipboardSync(e.clipboardSync.enable);
                break;
            case EVENT_CLIPBOARD_TEXT:
//                updateClipText(e.cliptext.text);
                break;
        }
    }
}

int util_is_valid_utf8(const char *string) {
    if (!string)
        return 0;

    const unsigned char *bytes = (const unsigned char *) string;
    while (*bytes) {
        if ((bytes[0] == 0x99) ||
            (bytes[0] == 0xC0 || bytes[0] == 0xC1) ||
            (bytes[0] >= 0xF5))
            return 0;

        // More checks for UTF-8 validity
        if ((bytes[0] & 0x80) == 0x00) {
            // ASCII byte
            bytes += 1;
        } else if ((bytes[0] & 0xE0) == 0xC0) {
            // 2-byte sequence
            if ((bytes[1] & 0xC0) != 0x80)
                return 0;
            bytes += 2;
        } else if ((bytes[0] & 0xF0) == 0xE0) {
            // 3-byte sequence
            if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80)
                return 0;
            bytes += 3;
        } else if ((bytes[0] & 0xF8) == 0xF0) {
            // 4-byte sequence
            if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80)
                return 0;
            bytes += 4;
        } else {
            return 0;
        }
    }
    return 1;
}

void lorieSendClipboardData(const char *data) {
    if (data && conn_fd != -1) {
        logd( "lorieSendClipboardData data:%s", data);
        write(conn_fd, data, strlen(data));
        JNIEnv *JavaEnv = GetJavaEnv();
        if (JavaEnv && JavaCmdEntryPointClass && util_is_valid_utf8(data)) {
            jstring cliptext = (*JavaEnv)->NewStringUTF(JavaEnv, data);
            jmethodID method = (*JavaEnv)->GetStaticMethodID(JavaEnv, JavaCmdEntryPointClass,
                                                             "updateXserverCliptext",
                                                             "(Ljava/lang/String;)V");
            (*JavaEnv)->CallStaticVoidMethod(JavaEnv, JavaCmdEntryPointClass, method, cliptext);
        }
    }
}

static Bool addFd(unused ClientPtr pClient, void *closure) {
//    SetNotifyFd((int) (int64_t) closure, handleLorieEvents, X_NOTIFY_READ, NULL);
    InputThreadRegisterDev((int) (int64_t) closure, handleLorieEvents, NULL);
    conn_fd = (int) (int64_t) closure;
    return TRUE;
}

JNIEXPORT jobject JNICALL
Java_com_fde_x11_Xserver_getXConnection(JNIEnv *env, unused jobject cls) {
    if (conn_fd == -1) {
        int client[2];
        jclass ParcelFileDescriptorClass = (*env)->FindClass(env,
                                                             "android/os/ParcelFileDescriptor");
        jmethodID adoptFd = (*env)->GetStaticMethodID(env, ParcelFileDescriptorClass, "adoptFd",
                                                      "(I)Landroid/os/ParcelFileDescriptor;");
        socketpair(AF_UNIX, SOCK_STREAM, 0, client);
        fcntl(client[0], F_SETFL, fcntl(client[0], F_GETFL, 0) | O_NONBLOCK);
//        __android_log_print(ANDROID_LOG_ERROR, "native_android",
//                            "getXConnection: conn_fd:%d fd[0]%d fd[1]%d", conn_fd, client[0], client[1]);
        QueueWorkProc(addFd, NULL, (void *) (int64_t) client[1]);
        return (*env)->CallStaticObjectMethod(env, ParcelFileDescriptorClass, adoptFd, client[0]);
    } else {
        return NULL;
    }
}

void *logcatThread(void *arg) {
    char buffer[4096];
    size_t len;
    while ((len = read((int) (int64_t) arg, buffer, 4096)) >= 0)
        write(2, buffer, len);
    close((int) (int64_t) arg);
    return NULL;
}

JNIEXPORT jobject JNICALL
Java_com_fde_x11_Xserver_getLogcatOutput(JNIEnv *env, unused jobject cls) {
    jclass ParcelFileDescriptorClass = (*env)->FindClass(env, "android/os/ParcelFileDescriptor");
    jmethodID adoptFd = (*env)->GetStaticMethodID(env, ParcelFileDescriptorClass, "adoptFd",
                                                  "(I)Landroid/os/ParcelFileDescriptor;");
    const char *debug = getenv("TERMUX_X11_DEBUG");
    if (debug && !strcmp(debug, "1")) {
        pthread_t t;
        int p[2];
        pipe(p);
        fchmod(p[1], 0777);
        pthread_create(&t, NULL, logcatThread, (void *) (uint64_t) p[0]);
        return (*env)->CallStaticObjectMethod(env, ParcelFileDescriptorClass, adoptFd, p[1]);
    }
    return NULL;
}

JNIEXPORT jboolean JNICALL
Java_com_fde_x11_Xserver_connected(__unused JNIEnv *env, __unused jclass clazz) {
    return conn_fd != -1;
}

static inline void checkConnection(JNIEnv *env) {
    int retval, b = 0;

    if (conn_fd == -1)
        return;

    if ((retval = recv(conn_fd, &b, 1, MSG_PEEK)) <= 0 && errno != EAGAIN) {
        logd( "recv %d %s", retval, strerror(errno));
        jclass cls = (*env)->FindClass(env, "com/fde/x11/Xserver");
        jmethodID method = !cls ? NULL : (*env)->GetStaticMethodID(env, cls, "requestConnection",
                                                                   "()V");
        if (method)
            (*env)->CallStaticVoidMethod(env, cls, method);

        close(conn_fd);
        conn_fd = -1;
    }
}

JNIEXPORT void JNICALL
Java_com_fde_x11_LorieView_connect(unused JNIEnv *env, unused jobject cls, jint fd) {
    conn_fd = fd;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    checkConnection(env);
    logd( "XCB connection is successfull");
}

static char clipboard[1024 * 1024] = {0};

JNIEXPORT void JNICALL
Java_com_fde_x11_LorieView_sendWindowChange(unused JNIEnv *env, unused jobject cls, jint width, jint height, jint framerate) {
    if (conn_fd != -1) {
        lorieEvent e = {.screenSize = {.t = EVENT_SCREEN_SIZE, .width = width, .height = height, .framerate = framerate}};
        write(conn_fd, &e, sizeof(e));
        checkConnection(env);
    }
}

JNIEXPORT void JNICALL
Java_com_fde_x11_LorieView_sendMouseEvent(unused JNIEnv *env, unused jobject cls, jfloat x,jfloat y, jint which_button, jboolean button_down,jboolean relative, jint index) {
    if (conn_fd != -1) {
        __android_log_print(ANDROID_LOG_ERROR, "native_android",
                            "lorieview sendmouseevent: x:%.0f y:%.0f", x, y);
        loge( "lorieview sendmouseevent x:%.0f y:%.0f detail:%d down:%d", x, y, which_button,
            button_down);
        lorieEvent e = {.mouse = {.t = EVENT_MOUSE, .x = x, .y = y, .detail = which_button, .down = button_down, .relative = relative}};
        write(conn_fd, &e, sizeof(e));
        checkConnection(env);
    }
}

JNIEXPORT void JNICALL
Java_com_fde_x11_LorieView_sendTouchEvent(unused JNIEnv *env, unused jobject cls, jint action,jint id, jint x, jint y) {
    if (conn_fd != -1 && action != -1) {
        lorieEvent e = {.touch = {.t = EVENT_TOUCH, .type = action, .id = id, .x = x, .y = y}};
        write(conn_fd, &e, sizeof(e));
        checkConnection(env);
    }
}

JNIEXPORT jboolean JNICALL
Java_com_fde_x11_LorieView_sendKeyEvent(unused JNIEnv *env, unused jobject cls, jint scan_code, jint key_code, jboolean key_down) {
    if (conn_fd != -1) {
        int code = (scan_code) ?: android_to_linux_keycode[key_code];
        logd( "Sending key: %d (%d %d %d)", code + 8, scan_code, key_code, key_down);
        lorieEvent e = {.key = {.t = EVENT_KEY, .key = code + 8, .state = key_down}};
        write(conn_fd, &e, sizeof(e));
        checkConnection(env);
    }
    return true;
}

JNIEXPORT void JNICALL
Java_com_fde_x11_LorieView_sendTextEvent(JNIEnv *env, unused jobject thiz, jbyteArray text) {
    if (conn_fd != -1 && text) {
        jsize length = (*env)->GetArrayLength(env, text);
        jbyte *str = (*env)->GetByteArrayElements(env, text, JNI_FALSE);
        char *p = (char *) str;
        mbstate_t state = {0};
        logd( "Parsing text: %.*s", length, str);

        while (*p) {
            wchar_t wc;
            size_t len = mbrtowc(&wc, p, MB_CUR_MAX, &state);

            if (len == (size_t) -1 || len == (size_t) -2) {
                loge( "Invalid UTF-8 sequence encountered");
                break;
            }

            if (len == 0)
                break;

            logd( "Sending unicode event: %lc (U+%X)", wc, wc);
            lorieEvent e = {.unicode = {.t = EVENT_UNICODE, .code = wc}};
            write(conn_fd, &e, sizeof(e));
            p += len;
            if (p - (char *) str >= length)
                break;
            usleep(30000);
        }

        (*env)->ReleaseByteArrayElements(env, text, str, JNI_ABORT);
        checkConnection(env);
    }
}

JNIEXPORT void JNICALL
Java_com_fde_x11_LorieView_sendUnicodeEvent(JNIEnv *env, unused jobject thiz, jint code) {
    if (conn_fd != -1) {
        logd( "Sending unicode event: %lc (U+%X)", code, code);
        lorieEvent e = {.unicode = {.t = EVENT_UNICODE, .code = code}};
        write(conn_fd, &e, sizeof(e));
        checkConnection(env);
    }
}

void abort(void) {
    _exit(134);
}

void exit(int code) {
    _exit(code);
}

#if 1

JNIEXPORT void JNICALL
Java_com_fde_x11_Xserver_tellFocusWindow(JNIEnv *env, jobject thiz, jlong window) {
    logd( "tellFocusWindow window:%lx", window);
    focusWindow = window;
    if (_surface_count_window(sfWraper, window)) {
        WindAttribute *attr = _surface_find_window(sfWraper, window);
        window_top_level++;
        window_top_level %= LEVEL_MAX;
        attr->level = window_top_level;
    }
}

JNIEXPORT void JNICALL
Java_com_fde_x11_Xserver_sendMouseEvent(JNIEnv *env, jobject thiz, jfloat x, jfloat y,jint which_button, jboolean button_down, jboolean relative, jint index) {
//    logd( "MouseEvent x:%.0f y:%.0f detail:%d  down:%s relative:%d", x, y, which_button,
//        button_down == 1 ? "true" : "false", relative);
    lorieEvent e = {.mouse = {.t = EVENT_MOUSE, .x = x, .y = y, .detail = which_button, .down = button_down, .relative = relative}};
    ValuatorMask mask;
    valuator_mask_zero(&mask);
    int flags;
    switch (e.mouse.detail) {
        case 0: // BUTTON_UNDEFINED
            if (e.mouse.relative) {
                valuator_mask_set_double(&mask, 0, (double) e.mouse.x);
                valuator_mask_set_double(&mask, 1, (double) e.mouse.y);
                QueuePointerEvents(lorieMouseRelative, MotionNotify, 0,
                                   POINTER_RELATIVE | POINTER_ACCELERATE, &mask);
            } else {
                flags = POINTER_ABSOLUTE | POINTER_SCREEN | POINTER_NORAW;
                valuator_mask_set_double(&mask, 0, (double) e.mouse.x);
                valuator_mask_set_double(&mask, 1, (double) e.mouse.y);
                QueuePointerEvents(lorieMouse, MotionNotify, 0, flags, &mask);
            }
            break;
        case 1: // BUTTON_LEFT
        case 2: // BUTTON_MIDDLE
        case 3: // BUTTON_RIGHT
            QueuePointerEvents(e.mouse.relative ? lorieMouseRelative : lorieMouse,
                               e.mouse.down ? ButtonPress : ButtonRelease,
                               e.mouse.detail, 0, &mask);
            break;
        case 4: // BUTTON_SCROLL
            if (e.mouse.x) {
                valuator_mask_zero(&mask);
                valuator_mask_set_double(&mask, 2, (double) e.mouse.x / 120);
                QueuePointerEvents(lorieMouseRelative, MotionNotify, 0,
                                   POINTER_RELATIVE, &mask);
            }
            if (e.mouse.y) {
                valuator_mask_zero(&mask);
                valuator_mask_set_double(&mask, 3, (double) e.mouse.y / 120);
                QueuePointerEvents(lorieMouseRelative, MotionNotify, 0,
                                   POINTER_RELATIVE, &mask);
            }
            break;
        default:
            break;
    }

}

void property_cleanup(WindProperty *prop) {
    if (!prop) return;

    if (prop->net_wm_name) {
        free((char *) prop->net_wm_name);
        prop->net_wm_name = NULL;
    }
    if (prop->wm_name) {
        free((char *) prop->wm_name);
        prop->wm_name = NULL;
    }
    if (prop->wm_class) {
        free((char *) prop->wm_class);
        prop->wm_class = NULL;
    }
    // 重置其他字段
    prop->window = 0;
    prop->transient = 0;
    prop->leader = 0;
    prop->window_type = 0;
    prop->support_wm_delete = FALSE;
    prop->support_motif = FALSE;
    prop->icon = NULL;
    prop->pid = 0;
}

void property_win_copy(WindProperty *dest, const WindProperty *src) {
    if (!dest || !src) return;
    property_cleanup(dest);
    dest->window = src->window;
    dest->transient = src->transient;
    dest->leader = src->leader;
    dest->window_type = src->window_type;
    dest->support_wm_delete = src->support_wm_delete;
    dest->support_motif = src->support_motif;
    dest->icon = src->icon;  // 注意：jobject 可能需要特殊处理
    dest->pid = src->pid;
    if (src->net_wm_name) {
        dest->net_wm_name = strdup(src->net_wm_name);
    }
    if (src->wm_name) {
        dest->wm_name = strdup(src->wm_name);
    }
    if (src->wm_class) {
        dest->wm_class = strdup(src->wm_class);
    }
}

int property_lookup(PropertyPtr *result, WindowPtr pWin, Atom name)
{
    int rc = FALSE;
    PropertyPtr pProp;
    for (pProp = wUserProps(pWin); pProp; pProp = pProp->next)
    {
        logd( "检查属性: pProp=%p, propertyName=%s, 目标name=%s",
            pProp, NameForAtom(pProp->propertyName), NameForAtom(name));
        if (pProp->propertyName == name)
        {
            *result = pProp;
            rc = TRUE;
            break;
        }
    }
    return rc;
}

int property_lookup_string(PropertyPtr *result, WindowPtr pWin, char* name)
{
    int rc = FALSE;
    PropertyPtr pProp;
    for (pProp = wUserProps(pWin); pProp; pProp = pProp->next)
    {
        if (STRING_EQUAL(NameForAtom(pProp->propertyName), name))
        {
            *result = pProp;
            rc = TRUE;
            break;
        }
    }
    return rc;
}

void property_get(WindowPtr pWin, WindProperty *prop) {
    CHECK_WITH_PROP;
    PropertyPtr pProper = pWin->optional->userProps;
    unsigned char *propData;
    prop->window = pWin->drawable.id;
    bool overrideRedirect = pWin->overrideRedirect;
//    loge( "prop start================================>");
//    loge( "prop window:%lx realized:%d", pWin->drawable.id, pWin->realized);
//    loge( "prop window:%lx overrideRedirect:%d", pWin->drawable.id, overrideRedirect);
    while (pProper) {
        ATOM name = pProper->propertyName;
        propData = pProper->data;
//        loge( "GET property NAME:%s", NameForAtom(name))
        if (STRING_EQUAL(NameForAtom(name), WINDOW_TYPE)) {
            Atom *atoms = (Atom *) propData;
            for (int i = 0; i < pProper->size; i++) {
                char *type = NameForAtom(atoms[i]);
//                loge( "prop window:%lx type:%s atom:%d", pWin->drawable.id, type, atoms[i]);
                if (atoms[i] == _WM_WINDOW_TYPE_SYSTRAY) {
                    prop->window_type = _WM_WINDOW_TYPE_SYSTRAY;
                    break;
                }
                if (STRING_EQUAL(NameForAtom(atoms[i]), WINDOW_TYPE_NORMAL)) {
                    prop->window_type = _NET_WM_WINDOW_TYPE_NORMAL;
                    if (!overrideRedirect) {
                        break;
                    } else {
                        prop->window_type = _NET_WM_WINDOW_TYPE_MENU;
                    }
                } else if (STRING_EQUAL(NameForAtom(atoms[i]), WINDOW_TYPE_DIALOG)) {
                    prop->window_type = _NET_WM_WINDOW_TYPE_DIALOG;
                    if (!overrideRedirect) {
                        break;
                    } else {
                        prop->window_type = _NET_WM_WINDOW_TYPE_MENU;
                    }
                } else if (STRING_EQUAL(NameForAtom(atoms[i]), WINDOW_TYPE_UTILITY)) {
                    prop->window_type = _NET_WM_WINDOW_TYPE_UTILITY;
                    if (overrideRedirect) {
                        break;
                    }
                } else if (STRING_EQUAL(NameForAtom(atoms[i]), WINDOW_TYPE_POPUP)) {
                    prop->window_type = _NET_WM_WINDOW_TYPE_POPUP_MENU;
                    if (overrideRedirect) {
                        break;
                    }
                } else if (STRING_EQUAL(NameForAtom(atoms[i]), WINDOW_TYPE_MENU)) {
                    prop->window_type = _NET_WM_WINDOW_TYPE_MENU;
                    if (overrideRedirect) {
                        break;
                    }
                } else if (STRING_EQUAL(NameForAtom(atoms[i]), WINDOW_TYPE_TOOLTIP)) {
                    prop->window_type = _NET_WM_WINDOW_TYPE_TOOLTIP;
                    if (overrideRedirect) {
                        break;
                    }
                } else if (STRING_EQUAL(NameForAtom(atoms[i]), WINDOW_TYPE_COMBO)) {
                    prop->window_type = _NET_WM_WINDOW_TYPE_COMBO;
                    if (overrideRedirect) {
                        break;
                    }
                } else {
                    prop->window_type = atoms[i];
                }
//                loge( "get_window_property window:%lx type:%d", prop->window, prop->window_type)
            }
        } else if (STRING_EQUAL(NameForAtom(name), WINDWO_TRANSIENT_FOR)) {
            prop->transient = ((Window *) propData)[0];
            // loge( "prop window:%x transient:%x", pWin->drawable.id, prop->transient);
        } else if (STRING_EQUAL(NameForAtom(name), WINDOW_CLIENT_LEADER)) {
            prop->leader = ((Window *) propData)[0];
//             loge( "prop window:%x leader:%x", pWin->drawable.id, prop->leader);
        } else if (STRING_EQUAL(NameForAtom(name), NET_WINDOW_NAME)) {
            char *name_copy = property_copy_data(propData, pProper->size);
            prop->net_wm_name = name_copy;
//             loge( "prop_window:%x net_wm_name:%s", pWin->drawable.id, prop->net_wm_name);
        } else if (STRING_EQUAL(NameForAtom(name), WINDOW_CLASS)) {
            char *name_copy = property_copy_data(propData, pProper->size);
            prop->wm_class = name_copy;
//              loge( "prop_window:%x wm_class:%s", pWin->drawable.id, prop->wm_class);
        } else if (STRING_EQUAL(NameForAtom(name), WINDOW_NAME)) {
            char *name_copy = property_copy_data(propData, pProper->size);
            prop->wm_name = name_copy;
//             loge( "prop_window:%x wm_name:%s", pWin->drawable.id, prop->wm_name);
        } else if (STRING_EQUAL(NameForAtom(name), WINDOW_ICON)) {
            int *icon_data = (int *) propData;
            int width = *icon_data;
            int height = *(icon_data + 1);
            int *imageData = (int *) (icon_data + 2);
            prop->icon = property_icon_convert_bitmap(imageData, width, height);
        } else if (STRING_EQUAL(NameForAtom(name), WINDOW_PROTOCOLS)) {
            Atom *atoms = (Atom *) propData;
            for (int i = 0; i < pProper->size; i++) {
                if (STRING_EQUAL(NameForAtom(atoms[i]), WINDOW_DELETE_WINDOW)) {
                    prop->support_wm_delete = TRUE;
                }
                // loge( "prop window:%x protocol:%s", pWin->drawable.id, NameForAtom(atoms[i]));
            }
        } else if (STRING_EQUAL(NameForAtom(name), WINDOW_X11_PID)) {
            long pid = (propData[0]);
            prop->pid = *propData;
//            loge( "prop window:%x pid:%d size:%d format:%d pid:%ld",
//            pWin->drawable.id, prop->pid, pProper->size, pProper->format, pid);
            //TODO revert from steam
//        } else if(STRING_EQUAL(NameForAtom(name), "STEAM_GAME")) {
//            prop->window_type = _NET_WM_WINDOW_TYPE_NORMAL;
        } else if (STRING_EQUAL(NameForAtom(name), WINDOW_MOTIF_WM_HINTS)) {
//            unsigned long *data = ((unsigned long *) propData);
            prop->support_motif = property_get_motif_hints(name, (uint32_t *) pProper->data,
                                                           pProper->size,
                                                           pProper->format);
        }
        pProper = pProper->next;
    }
//    if(prop->window_type == 0){
//        prop->window_type = _NET_WM_WINDOW_TYPE_NORMAL;
//    }
    // loge( "prop end================================>");
}

char *property_copy_data(const char *propData, int size) {
    if (!propData || size <= 0) return NULL;

    char *atom_value = (char *) calloc(size + 1, sizeof(char));
    if (!atom_value) return NULL;

    strncpy(atom_value, propData, size);
    atom_value[size] = '\0';
    return atom_value;
}

int property_get_motif_hints(Atom name, uint32_t *data, unsigned long nitems, uint32_t format) {
    if (STRING_EQUAL(NameForAtom(name), WINDOW_MOTIF_WM_HINTS)) {
        if (nitems >= MWM_HINTS_ELEMENTS) {
            PropMwmHints hints;
            hints.flags = data[0];
            hints.functions = data[1];
            hints.decorations = data[2];
            logd( "- Flags: 0x%lx, Functions: 0x%lx, Decorations: 0x%lx\n",
                hints.flags, hints.functions, hints.decorations);
            if (hints.flags & MWM_HINTS_FUNCTIONS) {
                logd( "Functions hint present\n");
                if (hints.functions & MWM_FUNC_ALL) {
                    logd( "All functions enabled\n");
                }
                if (hints.functions & MWM_FUNC_RESIZE) {
                    logd( "Resize function enabled\n");
                }
                if (hints.functions & MWM_FUNC_MOVE) {
                    logd( "Move function enabled\n");
                }
                if (hints.functions & MWM_FUNC_MINIMIZE) {
                    logd( "Minimize function enabled\n");
                }
                if (hints.functions & MWM_FUNC_MAXIMIZE) {
                    logd( "Maxmize function enabled\n");
                }
                if (hints.functions & MWM_FUNC_CLOSE) {
                    logd( "Close function enabled\n");
                }
            }

            if (hints.flags & MWM_HINTS_DECORATIONS) {
                logd( "Decorations hint present\n");
                if (hints.decorations & MWM_DECOR_ALL) {
                    logd( "All decorations enabled\n");
                }
                if (hints.decorations & MWM_DECOR_BORDER) {
                    logd( "Border decoration enabled\n");
                }
                if (hints.decorations & MWM_DECOR_RESIZE) {
                    logd( "Resize decoration enabled\n");
                }
                if (hints.decorations & MWM_DECOR_TITLE) {
                    logd( "Title decoration enabled\n");
                }
                if (hints.decorations & MWM_DECOR_MENU) {
                    logd( "Menu decoration enabled\n");
                }
                if (hints.decorations & MWM_DECOR_MINIMIZE) {
                    logd( "Minimize decoration enabled\n");
                }
                if (hints.decorations & MWM_DECOR_MAXIMIZE) {
                    logd( "Maxmize decoration enabled\n");
                }
                if (
                        (hints.decorations & MWM_DECOR_ALL)
                        || (hints.decorations & MWM_DECOR_TITLE)
                        || (hints.decorations & MWM_DECOR_MENU)
                        || (hints.decorations & MWM_DECOR_MINIMIZE)
                        || (hints.decorations & MWM_DECOR_MAXIMIZE)
                        ) {
                    return 0;
                }
            }
        } else {
            logd( "Insufficient data for MOTIF_WM_HINTS: got %lu, need %ld\n",
                nitems, MWM_HINTS_ELEMENTS);
        }
    }
    return 1;
}

jobject property_icon_convert_bitmap(int *data, int width, int height) {
    if (!data || width <= 0 || height <= 0) {
        loge( "Invalid input parameters: data=%p, width=%d, height=%d", data, width, height);
        return NULL;
    }
//    logd( "CONVERT_ICON width:%d height:%d", width, height);
    JNIEnv *JavaEnv = GetJavaEnv();
    if (!JavaEnv) {
        loge( "Failed to get JavaEnv");
        return NULL;
    }
    jclass bitmapClass = (*JavaEnv)->FindClass(JavaEnv, "android/graphics/Bitmap");
    if (!bitmapClass) {
        loge( "Failed to find class android/graphics/Bitmap");
        return NULL;
    }
    jmethodID createBitmapMethod = (*JavaEnv)->GetStaticMethodID(JavaEnv, bitmapClass,
                                                                 "createBitmap",
                                                                 "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    if (!createBitmapMethod) {
        loge( "Failed to get method ID for createBitmap");
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapClass);
        return NULL;
    }
    jstring configName = (*JavaEnv)->NewStringUTF(JavaEnv, "ARGB_8888");
    jclass bitmapConfigClass = (*JavaEnv)->FindClass(JavaEnv, "android/graphics/Bitmap$Config");
    if (!bitmapConfigClass) {
        loge( "Failed to find class android/graphics/Bitmap$Config");
        (*JavaEnv)->DeleteLocalRef(JavaEnv, configName);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapClass);
        return NULL;
    }

    jmethodID valueOfMethod = (*JavaEnv)->GetStaticMethodID(JavaEnv, bitmapConfigClass, "valueOf",
                                                            "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;");
    if (!valueOfMethod) {
        loge( "Failed to get method ID for valueOf");
        (*JavaEnv)->DeleteLocalRef(JavaEnv, configName);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapConfigClass);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapClass);
        return NULL;
    }

    jobject bitmapConfig = (*JavaEnv)->CallStaticObjectMethod(JavaEnv, bitmapConfigClass,
                                                              valueOfMethod, configName);
    if (!bitmapConfig) {
        loge( "Failed to create Bitmap$Config object");
        (*JavaEnv)->DeleteLocalRef(JavaEnv, configName);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapConfigClass);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapClass);
        return NULL;
    }

    jobject bitmap = (*JavaEnv)->CallStaticObjectMethod(JavaEnv, bitmapClass, createBitmapMethod,
                                                        width, height, bitmapConfig);
    if (!bitmap) {
        loge( "Failed to create Bitmap object");
        (*JavaEnv)->DeleteLocalRef(JavaEnv, configName);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapConfigClass);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapConfig);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapClass);
        return NULL;
    }

    void *bitmapPixels;
    if (AndroidBitmap_lockPixels(JavaEnv, bitmap, &bitmapPixels) < 0) {
        loge( "Failed to lock bitmap pixels");
        (*JavaEnv)->DeleteLocalRef(JavaEnv, configName);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapConfigClass);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapConfig);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapClass);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmap);
        return NULL;
    }

    if (!bitmapPixels) {
        loge( "Bitmap pixels pointer is NULL");
        AndroidBitmap_unlockPixels(JavaEnv, bitmap);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, configName);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapConfigClass);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapConfig);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapClass);
        (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmap);
        return NULL;
    }

    uint32_t *src = (uint32_t *) data;
    uint32_t *dst = (uint32_t *) bitmapPixels;
    for (unsigned long i = 0; i < (unsigned long) (width * height); i++) {
        uint32_t pixel = src[i];
        uint8_t alpha = (pixel >> 24) & 0xFF;
        uint8_t red = (pixel >> 16) & 0xFF;
        uint8_t green = (pixel >> 8) & 0xFF;
        uint8_t blue = pixel & 0xFF;
        dst[i] = (alpha << 24) | (blue << 16) | (green << 8) | red;  // ABGR
    }

    AndroidBitmap_unlockPixels(JavaEnv, bitmap);

    (*JavaEnv)->DeleteLocalRef(JavaEnv, configName);
    (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapConfigClass);
    (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapConfig);
    (*JavaEnv)->DeleteLocalRef(JavaEnv, bitmapClass);

    logd( " success width: %d, height: %d", width, height)
    return bitmap;
}
#endif




