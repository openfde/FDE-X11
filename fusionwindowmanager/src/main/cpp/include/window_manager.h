//
// Created by yang on 2024/4/18.
//

#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "util.hpp"
#include <android/log.h>
#include <set>
#include <jni.h>
#include <X11/Xatom.h>
#include <android/bitmap.h>
#include "native_log.h"

extern "C"
{
#include "glib.h"
#include "client.h"
#include "screen.h"
#include "netwm.h"
#include "hints.h"
#include "display.h"
}

static JavaVM *jniVM = nullptr;
static JNIEnv *GlobalEnv = nullptr;
static jobject bitmap = nullptr;
static jclass staticClass = nullptr;

#define WIDTH 1920
#define HEIGHT 1080

#define BASE_EVENT_MASK            \
    SubstructureNotifyMask |       \
        StructureNotifyMask |      \
        SubstructureRedirectMask | \
        ButtonPressMask |          \
        ButtonReleaseMask |        \
        KeyPressMask |             \
        KeyReleaseMask |           \
        FocusChangeMask |          \
        PropertyChangeMask |       \
        ColormapChangeMask |       \
        PointerMotionMask 

#define MAIN_EVENT_MASK BASE_EVENT_MASK | ExposureMask

static gboolean compositor = TRUE;
static vblankMode vblank_mode = VBLANK_AUTO;

#define PRINT_XERROR 0
#define CHECK(condition)         \
    if (condition)               \
    {                            \
        logd("#condition fatal"); \
    }
#define CHECK_EQ(val1, val2) \
    if (val1 != val2)        \
    {                        \
        logd("not equal");    \
    }
#define CLIPMANAGER_ENABLE 1
#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

#define SYSTEM_TRAY_ENABLE 0
#define SYSTEM_TRAY_CAPACITY 10

#define ACTION_UNMAP 1
#define ACTION_DESTORY 2
#define ACTION_DISMISS 3


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

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2
#define SYSTEM_TRAY_UNDOCK          3

#define MWM_HINTS_DECORATIONS    (1L << 1)

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
} MotifWmHints;


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
const Atom _NET_WM_WINDOW_TYPE_TRAY = 1000;

class WindowManager
{
public:
    static ::WindowManager *create(char *string, JNIEnv *env, jclass cls, jint width, jint height, jint dpi);
    ~WindowManager();
    WindowManager(Display *display, jint i, jint i1, jint i2);
    void Run();
    int configureWindow(long window, int x, int y, int w, int h);
    int setWindowingMode(long frame, long window, int mode);
    int moveWindow(long window, int x, int y);
    int resizeWindow(long window, int x, int y);
    int closeWindow(long window);
    int unmapWindow(long window);
    int mapWindow(long window);
    int raiseWindow(long window);
    bool isNormalWindow(long window);
    void initCompositor();
    int stoped = False;
    jint sendClipText(const char *pJstring);
    jint sendClipFile(const char *pJstring);
    jint circulaSubWindows(jlong window, jboolean lowest);
    void setClipData(char *text, char *path);

private:
    // 缓存常用Atom值
//    struct AtomsCache {
//        Atom atom_wm_protocols;
//        Atom atom_wm_delete_window;
//        Atom atom_net_wm_window_type;
//        Atom atom_net_wm_window_type_normal;
//        Atom atom_net_wm_window_type_menu;
//        Atom atom_net_wm_window_type_dialog;
//        Atom atom_net_wm_window_type_popup_menu;
//    } atoms_cache_;

    Display *display_;
    DisplayInfo *display_info;
    const Window root_;
    int screen_;
    Window back_window;
    int width_ = 1920;
    int height_ = 1080;
    int density_ = 96;
    int decorcationview_height  = 42;
    Window system_tray = 0;
    int system_tray_icon_width = 18;
    int status_bar_height = 24;
    int status_bar_icon_width = 30;
    int offset_right_in_statusbar = 254;
    int navigation_bar_height = 68;

    // Event handlers.
    void OnCreateNotify(const XCreateWindowEvent &e);
    void OnDestroyNotify(const XDestroyWindowEvent &e);
    void OnReparentNotify(const XReparentEvent &e);
    void OnMapNotify(const XMapEvent &e);
    void OnUnmapNotify(const XUnmapEvent &e);
    void OnConfigureNotify(const XConfigureEvent &e);
    void OnMapRequest(const XMapRequestEvent &e);
    void OnConfigureRequest(const XConfigureRequestEvent &e);
    void OnCirculateRequest(const XCirculateRequestEvent &e);
    void OnButtonPress(const XButtonEvent &e);
    void OnButtonRelease(const XButtonEvent &e);
    void OnMotionNotify(const XMotionEvent &e);
    void OnKeyPress(const XKeyEvent &e);
    void OnKeyRelease(const XKeyEvent &e);

    // Xlib error handler. It must be static as its address is passed to Xlib.
    static int OnXError(Display *display, XErrorEvent *e);
    // Xlib error handler used to determine whether another window manager is
    // running. It is set as the error handler right before selecting substructure
    // redirection mask on the root window, so it is invoked if and only if
    // another window manager is running. It must be static as its address is
    // passed to Xlib.
    static int OnWMDetected(Display *display, XErrorEvent *e);
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
    std::set<Window> dock_windows;
    std::set<Window> dock_trays;

    ::std::unordered_map<Window, Window> tray_window_map;
    ::std::unordered_map<Window, XConfigureEvent> configedTopWindow;
    Window owner;
    Atom sel, utf8;
    std::string clip_text;
    std::string file_path;
    Atom *selection_property_list;
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

    void ProcessClientMessage(XEvent event);

    int setMaximizedState(Window window, Bool maximized);

    void UpdateXserverClipFile(const char *data);

    void setWindowType(Window window, Atom type);

    int isTaskMoving;

    void HandleSystemTrayClientMessage( ScreenInfo *screen_info, XClientMessageEvent *ev);

    jobject GetWindowIcon(Window id);

    jobject CreateBitmapFromNetWmIcon(unsigned char *data, unsigned long nitems);

    jobject CreateBitmapFromXImage(JNIEnv *env, XImage *image);

    uint32_t ConvertPixelToARGB(unsigned long pixel, int depth, int byte_order);

    jobject CreateBitmapFromPixmap(JNIEnv *env, Display *display, Pixmap pixmap);

    void ReparentDockWindow(Window window);

    int SetRootResourceManager(Display *display, const char *resource_string);
};
