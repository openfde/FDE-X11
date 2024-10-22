#include <jni.h>
#include <string>
#include "include/xcb/xcb.h"
#include "include/X11/Xlib.h"
#include <android/log.h>
#include "include/window_manager.h"
WindowManager *window_manager;
extern JavaVM *jniVM;
extern JNIEnv *GlobalEnv;
extern jobject bitmap;
extern jclass staticClass;


JNIEXPORT void JNICALL createXWindow(JNIEnv * env, jobject obj)
{
    setenv("DISPLAY", ":0", 1);
    Display *display;
    Window root, window;
    XEvent event;
    int screen;

    // 连接到 X 服务器
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        log("Cannot open display\n");
        exit(1);
    }

    // 获取默认屏幕和根窗口
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    // 创建窗口
    window = XCreateSimpleWindow(display, root, 200, 200, 640, 480, 1,
                                 BlackPixel(display, screen),
                                 WhitePixel(display, screen));
    // 设置窗口标题
    XStoreName(display, window, "Simple Window Manager");
    // 显示窗口
    XMapWindow(display, window);

    // 处理窗口事件
    while (1) {
        XNextEvent(display, &event);
        switch (event.type) {
            case ClientMessage:
                // 窗口关闭事件，退出程序
                // if (event.xclient.data.l[0] == WM_DELETE_WINDOW) {
                //     XCloseDisplay(display);
                //     return 0;
                // }
                break;
            default:
                break;
        }
    }
}

JNIEXPORT jint JNICALL connect2Server(JNIEnv * env, jobject obj, jstring display){
    jboolean isCopy = false;
    char* export_display = const_cast<char *>(env->GetStringUTFChars(display, &isCopy));
    jclass js  = static_cast<jclass>(env->NewGlobalRef(obj));
    setenv("DISPLAY", export_display, 1);
    window_manager = WindowManager::create(export_display, env, js);
    if(!window_manager){
        log("Failed to initialize window manager.");
        return False;
    }
    window_manager->Run();
    return True;
}

JNIEXPORT jint JNICALL moveWindow(JNIEnv * env, jobject obj, jlong ptr, jint x, jint y){
    if(!window_manager){
        log("Failed to initialize window manager.");
        return False;
    }
    return window_manager->moveWindow(ptr, x, y);
}

JNIEXPORT jint JNICALL configureWindow(JNIEnv * env, jobject obj, jlong wid, jint x, jint y, jint w, jint h){
    if(!window_manager){
        log("Failed to initialize window manager.");
        return False;
    }
    return window_manager->configureWindow(wid, x, y, w, h);
}

JNIEXPORT jint JNICALL resizeWindow(JNIEnv * env, jobject obj, jlong ptr, jint x, jint y){
    if(!window_manager){
        log("Failed to initialize window manager.");
        return False;
    }
    return window_manager->resizeWindow(ptr, x, y);
}

JNIEXPORT jint JNICALL closeWindow(JNIEnv * env, jobject obj, jlong xid){
    if(!window_manager){
        log("Failed to initialize window manager.");
        return False;
    }
    return window_manager->closeWindow(xid);
}

JNIEXPORT jint JNICALL raiseWindow(JNIEnv * env, jobject obj, jlong ptr){
    if(!window_manager){
        log("Failed to initialize window manager.");
        return False;
    }
    return window_manager->raiseWindow(ptr);
}


JNIEXPORT jint JNICALL circulaSubWindows(JNIEnv * env, jobject obj, jlong window, jboolean lowest){
    if(!window_manager){
        log("Failed to initialize window manager.");
        return False;
    }
    return window_manager->circulaSubWindows(window, lowest);
}

JNIEXPORT jint JNICALL sendClipText(JNIEnv * env, jobject obj, jstring string){
    if(!window_manager){
        log("Failed to initialize window manager.");
        return False;
    }
    jboolean isCopy = false;
    const char* cliptext = env->GetStringUTFChars(string, &isCopy);
    return window_manager->sendClipText(cliptext);
}

JNIEXPORT jint JNICALL disconnect2Server(JNIEnv * env, jobject obj){
    if(window_manager){
        log("disconnect2Server");
        window_manager->stoped = True;
        delete window_manager;
        window_manager = NULL;
    }
    return True;
}

static JNINativeMethod method_table[] = {
        {"createXWindow","()V", (void *) createXWindow},
        {"connect2Server", "(Ljava/lang/String;)I", (void *) connect2Server},
        {"moveWindow","(JII)I", (void *) moveWindow},
        {"configureWindow","(JIIII)I", (void *) configureWindow},
        {"resizeWindow","(JII)I", (void *) resizeWindow},
        {"closeWindow","(J)I", (void *) closeWindow},
        {"raiseWindow","(J)I", (void *) raiseWindow},
        {"circulaSubWindows","(JZ)I", (void *) circulaSubWindows},
        {"sendClipText","(Ljava/lang/String;)I", (void *) sendClipText},
        {"disconnect2Server","()I", (void *) disconnect2Server},
};


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void *reserved){
    JNIEnv *env = NULL;
    if (vm->AttachCurrentThread(&env,NULL) == JNI_OK){
        jclass clazz = env->FindClass("com/fde/fusionwindowmanager/WindowManager");
        if (clazz == NULL){
            return JNI_ERR;
        }
        if (env->RegisterNatives(clazz, method_table, sizeof(method_table)/ sizeof(method_table[0]))==JNI_OK) {
            return JNI_VERSION_1_6;
        }
    }
    return JNI_ERR;
}
