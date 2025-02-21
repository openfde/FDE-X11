//
// Created by yang on 2024/4/18.
//

#include "X11/Xlib.h"
#include "X11/X.h"

#include <stdio.h>
#include <stdlib.h>
#include "X11/Xutil.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "util.hpp"
#include <android/log.h>
#include <set>
#include <jni.h>
//#include "ewmh_icccm.h"
#include <X11/Xatom.h>
static JavaVM *jniVM = NULL;
static JNIEnv *GlobalEnv = NULL;
static jobject bitmap = NULL;
static jclass staticClass = NULL;

#define WIDTH  1920
#define HEIGHT 1080
#define DECORCATIONVIEW_HEIGHT 42
#define BASE_EVENT_MASK \
    SubstructureNotifyMask|\
    StructureNotifyMask|\
    SubstructureRedirectMask|\
    ButtonPressMask|\
    ButtonReleaseMask|\
    KeyPressMask|\
    KeyReleaseMask|\
    FocusChangeMask|\
    PropertyChangeMask|\
    ColormapChangeMask

#define PRINT_LOG 1
#define log(...) if(PRINT_LOG){ __android_log_print(ANDROID_LOG_DEBUG, "native_wm", __VA_ARGS__);}
#define loge(...) if(PRINT_LOG){__android_log_print(ANDROID_LOG_ERROR, "native_wm", __VA_ARGS__);}
#define PRINT_XERROR 0
#define CHECK(condition)  if(condition){   log("#condition fatal");}
#define CHECK_EQ(val1, val2)  if(val1 != val2){  log("not equal"); }
#define CLIPMANAGER_ENABLE 1
#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

#define _NET_WM_STATE_FULLSCREEN 1
#define _NET_WM_STATE_ABOVE 3
#define _NET_WM_STATE_BELOW 4
#define _NET_WM_STATE_DEMANDS_ATTENTION 5

#define WINDOW_ACTION_UNDEFINED 0
#define WINDOW_ACTION_MAXIMIZED 1000
#define WINDOW_ACTION_MAXIMIZED_REMOVE 1001
#define WINDOW_ACTION_MINIMIZE 1003
#define WINDOW_ACTION_MINIMIZE_REMOVE 1004
#define WINDOW_ACTION_MAXIMIZED_HORZ 1
#define WINDOW_ACTION_MAXIMIZED_VERT 2
#define WINDOW_ACTION_DELETE 1007


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




class WindowManager  {
public:
    static ::WindowManager *create( char *string, JNIEnv *env, jclass cls);
    ~WindowManager();
    WindowManager(Display *display);
    void Run();
    int configureWindow(long window, int x, int y, int w, int h);
    int moveWindow(long window, int x, int y);
    int resizeWindow(long window, int x, int y);
    int closeWindow(long window);
    int unmapWindow(long window);
    int mapWindow(long window);
    int raiseWindow(long window);
    bool isNormalWindow(long window);
    bool isInFrameMap(long window);
    void initCompositor();
    int stoped = False;
    jint sendClipText(const char *pJstring);

    jint circulaSubWindows(jlong window, jboolean lowest);

private:

    // Handle to the underlying Xlib Display struct.
    Display* display_;
    // Handle to root window.
    const Window root_;
    // Frames a top-level window.
    void Frame(Window w, bool was_created_before_window_manager);
    // Unframes a client window.
    void Unframe(Window w);
    int screen_;
    Window back_window;

    // Event handlers.
    void OnCreateNotify(const XCreateWindowEvent& e);
    void OnDestroyNotify(const XDestroyWindowEvent& e);
    void OnReparentNotify(const XReparentEvent& e);
    void OnMapNotify(const XMapEvent& e);
    void OnUnmapNotify(const XUnmapEvent& e);
    void OnConfigureNotify(const XConfigureEvent& e);
    void OnMapRequest(const XMapRequestEvent& e);
    void OnConfigureRequest(const XConfigureRequestEvent& e);
    void OnButtonPress(const XButtonEvent& e);
    void OnButtonRelease(const XButtonEvent& e);
    void OnMotionNotify(const XMotionEvent& e);
    void OnKeyPress(const XKeyEvent& e);
    void OnKeyRelease(const XKeyEvent& e);

    // Xlib error handler. It must be static as its address is passed to Xlib.
    static int OnXError(Display* display, XErrorEvent* e);
    // Xlib error handler used to determine whether another window manager is
    // running. It is set as the error handler right before selecting substructure
    // redirection mask on the root window, so it is invoked if and only if
    // another window manager is running. It must be static as its address is
    // passed to Xlib.
    static int OnWMDetected(Display* display, XErrorEvent* e);
    // Whether an existing window manager has been detected. Set by OnWMDetected,
    // and hence must be static.
    static bool wm_detected_;
    // A mutex for protecting wm_detected_. It's not strictly speaking needed as
    // this program is single threaded, but better safe than sorry.
    static ::std::mutex wm_detected_mutex_;

    static bool support_composite;

    // Maps top-level windows to their frame windows.
    ::std::unordered_map<Window, Window> clients_;

    // The cursor position at the start of a window move/resize.
    Position<int> drag_start_pos_;
    // The position of the affected window at the start of a window
    // move/resize.
    Position<int> drag_start_frame_pos_;
    // The size of the affected window at the start of a window move/resize.
    Size<int> drag_start_frame_size_;

    std::set<Window> window_under_frames;
    std::set<Window> frames;
    std::set<Window> named_windows;

    ::std::unordered_map<Window, XConfigureEvent> configedTopWindow;
    Window owner;
    Atom sel, utf8;
    std::string clip_text;
    Atom * selection_property_list ;
    int selection_property_size = 0;

    // selection_property_size
    const Atom WM_PROTOCOLS;
    const Atom WM_DELETE_WINDOW;

    void OnPropertyNotify(XEvent event);

    void OnSelectionClear(XEvent event);

    void OnSelectionRequest(XEvent event);

    void ConvertAllTarget();

    void UpdateXserverCliptext(const char *data);

    void HandleClientMessage(XEvent event);
};


