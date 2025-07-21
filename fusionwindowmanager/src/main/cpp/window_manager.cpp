//
// Created by yang on 2024/4/18.
//

#include "include/window_manager.h"
#include "include/X11/keysymdef.h"
#include "include/X11/extensions/Xcomposite.h"


using ::std::max;
using ::std::mutex;
using ::std::string;
using ::std::unique_ptr;

bool WindowManager::wm_detected_;
bool WindowManager::support_composite;
mutex WindowManager::wm_detected_mutex_;


::WindowManager *WindowManager::create(char *export_display, JNIEnv * env, jclass cls) {
    staticClass = cls;
    GlobalEnv = env;
    std::string unixstring = "unix:/tmp/.X11-unix/X";
    std::string exportstring(export_display);
    std::string display_str = unixstring + exportstring;
    Display* display = XOpenDisplay(display_str.c_str());
    if (display == nullptr) {
        log("Failed to open X display");
        return nullptr;
    }
    // 2. Construct WindowManager instance.
    return new WindowManager(display);
}


WindowManager::WindowManager(Display* display)
        : display_(display),
          screen_(DefaultScreen(display)),
          root_(DefaultRootWindow(display_)),
          WM_PROTOCOLS(XInternAtom(display_, "WM_PROTOCOLS", false)),
          WM_DELETE_WINDOW(XInternAtom(display_, "WM_DELETE_WINDOW", false)),
          stoped(False)
{
    back_window = XCreateSimpleWindow(display_, root_, 0, 0, WIDTH, HEIGHT, 0,
                                      BlackPixel(display, screen_), WhitePixel(display, screen_));
    XMapWindow(display, back_window);
    XSetWindowBackground(display, back_window, WhitePixel(display, screen_));
}


int WindowManager::OnWMDetected(Display* display, XErrorEvent* e) {
    // In the case of an already running window manager, the error code from
    // XSelectInput is BadAccess. We don't expect this handler to receive any
    // other errors.
    CHECK_EQ(static_cast<int>(e->error_code), BadAccess);
    // Set flag.
//    wm_detected_ = true;
    // The return value is ignored.
    return 0;
}


WindowManager::~WindowManager() {
    log("~WindowManager");
    for (auto& pair : clients_) {
        XDestroyWindow(display_, pair.second);
    }
    if (owner) {
        XDestroyWindow(display_, owner);
    }
    XCloseDisplay(display_);
}


void WindowManager::Frame(Window w, bool was_created_before_window_manager) {
    log("Frame_ %x", w);
    // Visual properties of the frame to create.
    const unsigned int BORDER_WIDTH = 0;
    const unsigned long BORDER_COLOR = 0xffffff;
    const unsigned long BG_COLOR = 0xffffff;
    // We shouldn't be framing windows we've already framed.
    CHECK(!clients_.count(w))

    // 1. Retrieve attributes of window to frame.
    XWindowAttributes x_window_attrs;
    CHECK(!XGetWindowAttributes(display_, w, &x_window_attrs));

    // 2. If window was created before window manager started, we should frame
    // it only if it is visible and doesn't set override_redirect.
    if (was_created_before_window_manager) {
        if (x_window_attrs.override_redirect ||
            x_window_attrs.map_state != IsViewable) {
            return;
        }
    }

    // 3. Create frame.
    const Window frame = XCreateSimpleWindow(
            display_,
            root_,
            x_window_attrs.x,
            x_window_attrs.y,
            x_window_attrs.width,
            x_window_attrs.height,
            BORDER_WIDTH,
            BORDER_COLOR,
            BG_COLOR);
    // 4. Select events on frame.
    XSelectInput(
            display_,
            frame,
            BASE_EVENT_MASK);
    // 5. Add client to save set, so that it will be restored and kept alive if we
    // crash.
    XAddToSaveSet(display_, w);
    // 6. Reparent client window.
    XReparentWindow(
            display_,
            w,
            frame,
            0, 0);  // Offset of client window within frame.
    // 7. Map frame.
    // 8. Save frame handle.
    clients_[w] = frame;
    // 9. Grab universal window management actions on client window.
    //   a. Move windows with alt + left button.
    XGrabButton(
            display_,
            Button1,
            Mod1Mask,
            w,
            false,
            ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
            GrabModeAsync,
            GrabModeAsync,
            None,
            None);
    //   b. Resize windows with alt + right button.
    XGrabButton(
            display_,
            Button3,
            Mod1Mask,
            w,
            false,
            ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
            GrabModeAsync,
            GrabModeAsync,
            None,
            None);
    //   c. Kill windows with alt + f4.
    XGrabKey(
            display_,
            XKeysymToKeycode(display_, XK_F4),
            Mod1Mask,
            w,
            false,
            GrabModeAsync,
            GrabModeAsync);
    //   d. Switch windows with alt + tab.
    XGrabKey(
            display_,
            XKeysymToKeycode(display_, XK_Tab),
            Mod1Mask,
            w,
            false,
            GrabModeAsync,
            GrabModeAsync);
    XWindowChanges change_values;
    change_values.x = x_window_attrs.x;
    change_values.y = x_window_attrs.y;
    change_values.width = x_window_attrs.width;
    change_values.height = x_window_attrs.height;
    // myDisplayErrorTrapPush (display_info);
    XConfigureWindow (display_, frame, 15, &change_values);
    change_values.x = 1;//x_window_attrs.x;
    change_values.y = 1;//x_window_attrs.y;
    change_values.width = x_window_attrs.width;
    change_values.height = x_window_attrs.height;
    XConfigureWindow (display_, w, 15, &change_values);
    XMapWindow (display_, frame);
    XMapWindow (display_, w);
    Atom normal_type = XInternAtom(display_, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    setWindowType(frame, normal_type);

    window_under_frames.insert(w);
    frames.insert(frame);
    log("Framed_ window %x reparent to frame %x" ,w , frame);
}

bool WindowManager::isNormalWindow(long window) {
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char *propData = NULL;
    char *atomName = XGetAtomName(display_, _NET_WM_WINDOW_TYPE);
    Atom type = XInternAtom(display_, "_NET_WM_WINDOW_TYPE", False);
    Atom type_nomarl = XInternAtom(display_, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    Atom type_menu = XInternAtom(display_, "_NET_WM_WINDOW_TYPE_MENU", False);
    Atom type_dialog = XInternAtom(display_, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    Atom type_popup = XInternAtom(display_, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
//    log("isNormalWindow ? %lx", window);
    if (XGetWindowProperty(display_, window, type, 0, 1024, False, AnyPropertyType,
                           &actualType, &actualFormat, &nItems, &bytesAfter, &propData) ==
        Success) {
//        log(" actualType = %ld \n", actualType);
        if (actualType == XA_ATOM) {
            Atom *atoms = (Atom *) propData;
            for (int i = 0; i < nItems; i++) {
                if (atoms[i] == type_nomarl) {
                    continue;
                } else if (atoms[i] == type_menu
                           || atoms[i] == type_dialog
                           || atoms[i] == type_popup
                        ) {
                    char *atomValue = XGetAtomName(display_, atoms[i]);
//                    log("%s not normal window %lx \n", atomValue, window);
                    XFree(atomName);
                    return False;
                }
            }
        }
    }
    XFree(propData);
    return True;
}

bool WindowManager::isInFrameMap(long window ){
    auto it = frames.find(window);
    if (it != frames.end()) {
        log("isInFrameMap %x", window);
        return True;
    }
    return False;
}

void WindowManager::OnCreateNotify(const XCreateWindowEvent& e) {}

void WindowManager::OnDestroyNotify(const XDestroyWindowEvent& e) {
    frames.erase(e.window);
    window_under_frames.erase(e.window);
    clients_.erase(e.window);
}

void WindowManager::OnReparentNotify(const XReparentEvent& e) {}

void WindowManager::OnMapNotify(const XMapEvent &e) {
    if(e.event == root_ && support_composite){
        Atom type;
        int format;
        unsigned long nitems, after;
        unsigned char *data = NULL;
        Atom classAtom = XInternAtom(display_, "WM_CLASS", False);
        XGetWindowProperty(display_, e.window, classAtom, 0, 1024, False,
                           XA_STRING, &type, &format, &nitems, &after, &data);
        if(data){
            log("WM_CLASS %s", data);
        }
        if (data && strstr((char *)data, "Simcenter STAR-CCM+")) {
            log("redirect star!!");
//            XCompositeRedirectWindow(display_, e.window, CompositeRedirectManual);
            XCompositeNameWindowPixmap(display_, e.window);
//        named_windows.insert(e.window);
            XSync(display_, False);


        } else {
//            XCompositeRedirectWindow(display_, e.window, CompositeRedirectAutomatic);
            XCompositeNameWindowPixmap(display_, e.window);
//        named_windows.insert(e.window);
            XSync(display_, False);
        }
    }
}

void WindowManager::OnUnmapNotify(const XUnmapEvent& e) {
    if (!clients_.count(e.window)) {
//        log("Ignore UnmapNotify for non-client window %lu",e.window);
        return;
    }
    if (e.event == root_) {
        return;
    }
}

void WindowManager::Unframe(Window w) {
    CHECK(clients_.count(w));
    log("Unframe %x", w);
    // We reverse the steps taken in Frame().
    const Window frame = clients_[w];
    unsigned long serial = NextRequest(display_);
    log(" serial1:%ld", serial);
    // 1. Unmap frame.
    XUnmapWindow(display_, frame);
    serial = NextRequest(display_);
    log(" serial2:%ld", serial);
    // 2. Reparent client window.
    XReparentWindow(
            display_,
            w,
            root_,
            0, 0);  // Offset of client window within root.
    serial = NextRequest(display_);
    log(" serial3:%ld", serial);
    // 3. Remove client window from save set, as it is now unrelated to us.
    XRemoveFromSaveSet(display_, w);
    serial = NextRequest(display_);
    log(" serial4:%ld", serial);
    // 4. Destroy frame.
    XDestroyWindow(display_, frame);
    serial = NextRequest(display_);
    log(" serial5:%ld", serial);
    // 5. Drop reference to frame handle.
    clients_.erase(w);
    log("Unframed window %lu frame %x" , w , frame);
}

void WindowManager::OnConfigureNotify(const XConfigureEvent& e) {
//    log("OnConfigureNotify window:%lx above:%lx", e.window, e.above);
    if(clients_.count(e.above)){
//        log("OnConfigureNotify %lx", e.window);
        configedTopWindow[e.window] = e;
    }
}

void WindowManager::OnMapRequest(const XMapRequestEvent& e) {
    // 1. Frame or re-frame window.
//    Frame(e.window, false);
    // 2. Actually map window.
    Atom type;
    int format;
    unsigned long nitems, after;
    unsigned char *data = NULL;
    Atom classAtom = XInternAtom(display_, "WM_CLASS", False);
    XGetWindowProperty(display_, e.window, classAtom, 0, 1024, False,
                       XA_STRING, &type, &format, &nitems, &after, &data);
    if (data &&  strstr((char *)data, "Simcenter STAR-CCM+")) {
        Frame(e.window, false);
//        XWindowAttributes windowAttr;
//        XGetWindowAttributes(display_, e.window, &windowAttr);
//        Window window = e.window;
//        XSetWindowAttributes attributes;
//        attributes.bit_gravity = NorthWestGravity;
//        attributes.win_gravity = NorthWestGravity;
//        Window frame = XCreateWindow (display_, root_, 0, 0, 1, 1, 0,
//                                      24, InputOutput, windowAttr.visual, CWWinGravity|CWBitGravity, &attributes);
//        XReparentWindow (display_, window, frame, 0, 0);
//        XWindowChanges change_values;
//        change_values.x = 187;
//        change_values.y = 97;
//        change_values.width = 1536;
//        change_values.height = 831;
//        // myDisplayErrorTrapPush (display_info);
//        XConfigureWindow (display_, frame, 15, &change_values);
//        change_values.x = 0;
//        change_values.y = 0;
//        change_values.width = 800;
//        change_values.height = 600;
//        XConfigureWindow (display_, window, 15, &change_values);
//        XMapWindow (display_, frame);
//        XMapWindow (display_, window);
//        Atom normal_type = XInternAtom(display_, "_NET_WM_WINDOW_TYPE_NORMAL", False);
//        setWindowType(frame, normal_type);
    } else {
        XMapWindow(display_, e.window);
    }
    XFlush(display_);

}

void syncConfigureRequest(int x, int y, int w, int h, XID window){
    jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                    "syncConfigureRequest", "(IIIIJ)V");
    GlobalEnv->CallStaticVoidMethod(staticClass, method, x,y,w,h, window);
}



void WindowManager::OnCirculateRequest(const XCirculateRequestEvent& e){
    XCirculateSubwindows(display_, e.parent, e.place);
}

void WindowManager::OnConfigureRequest(const XConfigureRequestEvent& e) {
    XWindowChanges changes;
    bool normal = isNormalWindow(e.window);
    changes.x = e.x;
    changes.y = (e.y < DECORCATIONVIEW_HEIGHT && normal) ? DECORCATIONVIEW_HEIGHT : e.y;
    changes.width = e.width;
    changes.height = e.height;
    changes.border_width = e.border_width;
    changes.sibling = e.above;
    changes.stack_mode = e.detail;
    unsigned long value_mask = e.value_mask;
    if(e.y < DECORCATIONVIEW_HEIGHT) {
        value_mask  = e.value_mask | (1 << 1);
//        log("value_mask : %lu", value_mask);
    }
    loge("configurerequest x:%d y:%d w:%d h:%d border:%d above:%d stack:%d value:%d", e.x, e.y, e.width, e.height, e.border_width, e.above, e.detail, value_mask);
    if (clients_.count(e.window)) {
        const Window frame = clients_[e.window];
        XConfigureWindow(display_, frame, value_mask, &changes);
        log("Resize_ frame %lx  to %s x.y %s value_mask:%lu " , frame, Size<int>(e.width, e.height).ToString().c_str()
        ,Size<int>(changes.x, changes.y).ToString().c_str(), value_mask);
    } else {
        XConfigureWindow(display_, e.window, value_mask, &changes);
        log("Resize_ %lx to %s x.y %s value_mask:%lu " , e.window , Size<int>(e.width, e.height).ToString().c_str()
        ,Size<int>(changes.x, changes.y).ToString().c_str(), value_mask);
    }
    XSync(display_, False);
    if (value_mask & CWX || value_mask & CWY || value_mask & CWWidth || value_mask & CWHeight) {
        syncConfigureRequest(changes.x, changes.y, changes.width,  changes.height, e.window);
    }
}

void WindowManager::OnButtonPress(const XButtonEvent &e) {
    CHECK(clients_.count(e.window));
    const Window frame = clients_[e.window];

    // 1. Save initial cursor position.
    drag_start_pos_ = Position<int>(e.x_root, e.y_root);

    // 2. Save initial window info.
    Window returned_root;
    int x, y;
    unsigned width, height, border_width, depth;
    XGetGeometry(
            display_,
            frame,
            &returned_root,
            &x, &y,
            &width, &height,
            &border_width,
            &depth);
    drag_start_frame_pos_ = Position<int>(x, y);
    drag_start_frame_size_ = Size<int>(width, height);

    // 3. Raise clicked window to top.
    XRaiseWindow(display_, frame);
}

void WindowManager::OnButtonRelease(const XButtonEvent& e) {}

void WindowManager::OnMotionNotify(const XMotionEvent& e) {
    CHECK(clients_.count(e.window));
    const Window frame = clients_[e.window];
    const Position<int> drag_pos(e.x_root, e.y_root);
    const Vector2D<int> delta = drag_pos - drag_start_pos_;

    if (e.state & Button1Mask ) {
        // alt + left button: Move window.
        const Position<int> dest_frame_pos = drag_start_frame_pos_ + delta;
        XMoveWindow(
                display_,
                frame,
                dest_frame_pos.x, dest_frame_pos.y);
    } else if (e.state & Button3Mask) {
        // alt + right button: Resize window.
        // Window dimensions cannot be negative.
        const Vector2D<int> size_delta(
                max(delta.x, -drag_start_frame_size_.width),
                max(delta.y, -drag_start_frame_size_.height));
        const Size<int> dest_frame_size = drag_start_frame_size_ + size_delta;
        // 1. Resize frame.
        XResizeWindow(
                display_,
                frame,
                dest_frame_size.width, dest_frame_size.height);
        // 2. Resize client window.
        XResizeWindow(
                display_,
                e.window,
                dest_frame_size.width, dest_frame_size.height);
    }
}

void WindowManager::OnKeyPress(const XKeyEvent& e) {
    if ((e.state & Mod1Mask) &&
        (e.keycode == XKeysymToKeycode(display_, XK_F4))) {
        // alt + f4: Close window.
        //
        // There are two ways to tell an X window to close. The first is to send it
        // a message of type WM_PROTOCOLS and value WM_DELETE_WINDOW. If the client
        // has not explicitly marked itself as supporting this more civilized
        // behavior (using XSetWMProtocols()), we kill it with XKillClient().
        Atom* supported_protocols;
        int num_supported_protocols;
        if (XGetWMProtocols(display_,
                            e.window,
                            &supported_protocols,
                            &num_supported_protocols) &&
            (::std::find(supported_protocols,
                         supported_protocols + num_supported_protocols,
                         WM_DELETE_WINDOW) !=
             supported_protocols + num_supported_protocols)) {
//            LOG(INFO) << "Gracefully deleting window " << e.window;
            // 1. Construct message.
            XEvent msg;
            memset(&msg, 0, sizeof(msg));
            msg.xclient.type = ClientMessage;
            msg.xclient.message_type = WM_PROTOCOLS;
            msg.xclient.window = e.window;
            msg.xclient.format = 32;
            msg.xclient.data.l[0] = WM_DELETE_WINDOW;
            // 2. Send message to window to be closed.
            XSendEvent(display_, e.window, false, 0, &msg);
        } else {
//            LOG(INFO) << "Killing window " << e.window;
            XKillClient(display_, e.window);
        }
    } else if ((e.state & Mod1Mask) &&
               (e.keycode == XKeysymToKeycode(display_, XK_Tab))) {
        // alt + tab: Switch window.
        // 1. Find next window.
        auto i = clients_.find(e.window);
        CHECK(i != clients_.end());
        ++i;
        if (i == clients_.end()) {
            i = clients_.begin();
        }
        // 2. Raise and set focus.
        XRaiseWindow(display_, i->second);
        XSetInputFocus(display_, i->first, RevertToPointerRoot, CurrentTime);
    }
}

void WindowManager::OnKeyRelease(const XKeyEvent& e) {}

void WindowManager::OnPropertyNotify(XEvent e) {}

int WindowManager::OnXError(Display* display, XErrorEvent* e) {
    if(PRINT_XERROR){
        const int MAX_ERROR_TEXT_LENGTH = 1024;
        char error_text[MAX_ERROR_TEXT_LENGTH];
        XGetErrorText(display, e->error_code, error_text, sizeof(error_text));
        log("Received X error:\n");
        log("    Request: %d", int(e->request_code));
        if(e->request_code < 120){
            log(" - %s \n", XRequestCodeToString(e->request_code).c_str());
        }
        log("    Error code %d: " , int(e->error_code));
        log(" - %s \n", error_text );
        log("    Resource ID: %x",e->resourceid);
        log("    serial ID: %ld",e->serial);
    }
    return 0;

}

void WindowManager::Run() {
    // 1. Initialization.
    //   a. Select events on root window. Use a special error handler so we can
    //   exit gracefully if another window manager is already running.

    {
        ::std::lock_guard<mutex> lock(wm_detected_mutex_);
        wm_detected_ = false;
        XSetErrorHandler(&WindowManager::OnWMDetected);
        XSelectInput(
                display_,
                root_,
                BASE_EVENT_MASK);
        XSync(display_, false);
        int composite_major = 0, composite_minor = 0;
        XCompositeQueryVersion(display_, &composite_major, &composite_minor);
        log("composite_major:%d  composite_minor:%d", composite_major, composite_minor);
        if(composite_major > 0 || composite_minor > 2){
            support_composite = true;
            initCompositor();
        }
        if (wm_detected_) {
            log("Detected another window manager on display %s "
            ,XDisplayString(display_));
            return;
        }
    }
    //   b. Set error handler.
    XSetErrorHandler(&WindowManager::OnXError);
    //   c. Grab X server to prevent windows from changing under us.
    XGrabServer(display_);
    //   d. Reparent existing top-level windows.
    //     i. Query existing top-level windows.
    Window returned_root, returned_parent;
    Window* top_level_windows;
    unsigned int num_top_level_windows;
    CHECK(XQueryTree(
            display_,
            root_,
            &returned_root,
            &returned_parent,
            &top_level_windows,
            &num_top_level_windows));
    CHECK_EQ(returned_root, root_);
    //     ii. Frame each top-level window.
    for (unsigned int i = 0; i < num_top_level_windows; ++i) {
        log("top_level_window %x to frame", top_level_windows[i]);
//        Frame(top_level_windows[i], true);
    }
    //     iii. Free top-level window array.
    XFree(top_level_windows);
    //   e. Ungrab X server.
    XUngrabServer(display_);

    if(CLIPMANAGER_ENABLE){
        owner = XCreateSimpleWindow(display_, root_, -10, -10, 1, 1, 0, 0, 0);
        log("owner:%x", owner);
        sel = XInternAtom(display_, "CLIPBOARD", False);
        utf8 = XInternAtom(display_, "UTF8_STRING", False);
        XSetSelectionOwner(display_, sel, owner, CurrentTime);
    }

    // 2. Main event loop.
    while (!stoped) {
        // 1. Get next event.
        XEvent e;
        XNextEvent(display_, &e);
        log("------Received event: %s",ToString(e).c_str());
//        log("type:%d", e.type);
        // 2. Dispatch event.
        switch (e.type) {
            case CreateNotify:
                OnCreateNotify(e.xcreatewindow);
                break;
            case DestroyNotify:
                OnDestroyNotify(e.xdestroywindow);
                break;
            case ReparentNotify:
                OnReparentNotify(e.xreparent);
                break;
            case MapNotify:
                OnMapNotify(e.xmap);
                break;
            case UnmapNotify:
                OnUnmapNotify(e.xunmap);
                break;
            case ConfigureNotify:
                OnConfigureNotify(e.xconfigure);
                break;
            case MapRequest:
                OnMapRequest(e.xmaprequest);
                break;
            case ConfigureRequest:
                OnConfigureRequest(e.xconfigurerequest);
                break;
            case CirculateRequest:
                OnCirculateRequest(e.xcirculaterequest);
                break;
//            case MapRequest:
//            case ConfigureRequest:
//            case CirculateRequest:
//                XAllowEvents(display_, ReplayPointer, CurrentTime);
//                break;
            case ButtonPress:
                OnButtonPress(e.xbutton);
                break;
            case ButtonRelease:
                OnButtonRelease(e.xbutton);
                break;
            case MotionNotify:
                // Skip any already pending motion events.
                while (XCheckTypedWindowEvent(
                        display_, e.xmotion.window, MotionNotify, &e)) {}
                OnMotionNotify(e.xmotion);
                break;
            case KeyPress:
                OnKeyPress(e.xkey);
                break;
            case KeyRelease:
                OnKeyRelease(e.xkey);
                break;
            case PropertyNotify:
                OnPropertyNotify(e);
                break;
            case SelectionClear:
                if(CLIPMANAGER_ENABLE){
                    OnSelectionClear(e);
                }
                break;
            case SelectionRequest:
                if(CLIPMANAGER_ENABLE){
                    OnSelectionRequest(e);
                }
                break;
            case ClientMessage:
                HandleClientMessage(e);
                break;
            default:
                break;
//                log("Ignored event");
        }
    }
}

void WindowManager::HandleClientMessage(XEvent e) {
//    log("HandleClientMessage ---------------------------------type:%s", XGetAtomName(display_, e.xclient.message_type));
    int wm_action = WINDOW_ACTION_UNDEFINED;
    if (e.xclient.message_type == XInternAtom(display_, "WM_CHANGE_STATE", False)) {
        long target_state = e.xclient.data.l[0];
        if (target_state == NormalState) {
            wm_action = WINDOW_ACTION_MINIMIZE_REMOVE;
//            log("HandleClientMessage WM_CHANGE_STATE: Restore window to normal state.\n");
        } else if (target_state == IconicState) {
            wm_action = WINDOW_ACTION_MINIMIZE;
//            log("HandleClientMessage WM_CHANGE_STATE: Minimize (iconify) window.\n");
        } else {
//            log("HandleClientMessage WM_CHANGE_STATE with unknown state: %ld\n", target_state);
        }
    } else if (e.xclient.message_type == XInternAtom(display_, "WM_PROTOCOLS", False)) {
        Atom wm_delete_window = XInternAtom(display_, "WM_DELETE_WINDOW", False);
        wm_action = WINDOW_ACTION_DELETE;
        if (e.xclient.data.l[0] == wm_delete_window) {
//            log("HandleClientMessage WM_PROTOCOLS: Window close request.\n");
        } else {
//            log("HandleClientMessage WM_PROTOCOLS with unknown protocol.\n");
        }
    } else if (e.xclient.message_type == XInternAtom(display_, "_NET_WM_STATE", False)) {
        long action = e.xclient.data.l[0];
        long state1 = e.xclient.data.l[1];
        long state2 = e.xclient.data.l[2];
        if (action == _NET_WM_STATE_ADD) {
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False)) {
//                log("HandleClientMessage Maximize Vertically requested.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_VERT;
            }
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False)) {
//                log("HandleClientMessage Maximize Horizontally requested.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_HORZ;
            }
            if(wm_action == WINDOW_ACTION_MAXIMIZED_HORZ + WINDOW_ACTION_MAXIMIZED_VERT){
                wm_action = WINDOW_ACTION_MAXIMIZED;
            }
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False)) {
//                log("HandleClientMessage Minimize requested.\n");
            }
        } else if (action == _NET_WM_STATE_REMOVE) {
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False)) {
//                log("HandleClientMessage Maximize Vertically removed.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_VERT;
            }
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False)) {
//                log("HandleClientMessage Maximize Horizontally removed.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_HORZ;
            }
            if(wm_action == WINDOW_ACTION_MAXIMIZED_HORZ + WINDOW_ACTION_MAXIMIZED_VERT){
                wm_action = WINDOW_ACTION_MAXIMIZED_REMOVE;
            }
//            log("HandleClientMessage Remove state1:%s state2:%s", XGetAtomName(display_, state1),  XGetAtomName(display_, state2));
        } else if (action == _NET_WM_STATE_TOGGLE) {
//            log("HandleClientMessage Toggle state1:%s state2:%s", XGetAtomName(display_, state1),  XGetAtomName(display_, state2));
        }
        if(wm_action == WINDOW_ACTION_MAXIMIZED) {
            setMaximizedState(e.xclient.window, true);
        } else if(wm_action == WINDOW_ACTION_MAXIMIZED_REMOVE){
            setMaximizedState(e.xclient.window, false);
        }

    } else if(e.xclient.message_type == XInternAtom(display_, "_NET_ACTIVE_WINDOW", False)){
        Window active_window = e.xclient.data.l[0];
//        log("HandleClientMessage w1:%lx w2:%s w3:%lx", e.xclient.data.l[0], XGetAtomName(display_, e.xclient.data.l[1] ), e.xclient.data.l[2]);
    }
//    log("HandleClientMessage final wm_action:%d window:%lx", wm_action, e.xclient.window);
    jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                    "updateWmStateClient", "(IJ)V");
    GlobalEnv->CallStaticVoidMethod(staticClass, method, wm_action, e.xclient.window);
}


void WindowManager::setWindowType(Window window, Atom type) {
    Atom window_type = XInternAtom(display_, "_NET_WM_WINDOW_TYPE", False);
    XChangeProperty(display_, window, window_type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&type, 1);
}


int WindowManager::setMaximizedState(Window window, Bool maximized) {
    log("setMaximizedState window:%lx maximized:%d", window, maximized);
    Atom net_wm_state = XInternAtom(display_, "_NET_WM_STATE", False);
    Atom vert_max = XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom horz_max = XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom type;
    int format;
    unsigned long num_items, bytes_after;
    unsigned char *data = nullptr;
    int status = XGetWindowProperty(
            display_, window, net_wm_state,
            0, 1024, False, XA_ATOM,
            &type, &format, &num_items, &bytes_after, &data
    );
    std::vector<Atom> current_atoms;
    if (status == Success && data) {
        Atom *atoms = reinterpret_cast<Atom*>(data);
        current_atoms.assign(atoms, atoms + num_items);
        XFree(data);
    }
    std::vector<Atom> new_atoms;
    if (maximized) {
        new_atoms = current_atoms;
        bool has_vert = false, has_horz = false;
        for (Atom atom : current_atoms) {
            if (atom == vert_max) has_vert = true;
            if (atom == horz_max) has_horz = true;
        }
        if (!has_vert) new_atoms.push_back(vert_max);
        if (!has_horz) new_atoms.push_back(horz_max);
    } else {
        for (Atom atom : current_atoms) {
            if (atom != vert_max && atom != horz_max) {
                new_atoms.push_back(atom);
            }
        }
    }
    XChangeProperty(
            display_, window, net_wm_state,
            XA_ATOM, 32, PropModeReplace,
            reinterpret_cast<unsigned char*>(new_atoms.data()), new_atoms.size()
    );
    Atom actions_normal[] = {
            XInternAtom(display_, "_NET_WM_ACTION_MOVE", False),
            XInternAtom(display_, "_NET_WM_ACTION_RESIZE", False),
            XInternAtom(display_, "_NET_WM_ACTION_MINIMIZE", False),
            XInternAtom(display_, "_NET_WM_ACTION_SHADE", False),
            XInternAtom(display_, "_NET_WM_ACTION_MAXIMIZE_HORZ", False),
            XInternAtom(display_, "_NET_WM_ACTION_MAXIMIZE_VERT", False),
            XInternAtom(display_, "_NET_WM_ACTION_FULLSCREEN", False),
            XInternAtom(display_, "_NET_WM_ACTION_CHANGE_DESKTOP", False),
            XInternAtom(display_, "_NET_WM_ACTION_CLOSE", False)
    };
    Atom actions_maximized[] = {
            XInternAtom(display_, "_NET_WM_ACTION_MOVE", False),
            XInternAtom(display_, "_NET_WM_ACTION_MINIMIZE", False),
            XInternAtom(display_, "_NET_WM_ACTION_SHADE", False),
            XInternAtom(display_, "_NET_WM_ACTION_CLOSE", False)
            // 注意：移除了 RESIZE、MAXIMIZE_HORZ、MAXIMIZE_VERT，因为窗口已经最大化
    };
    if (maximized) {
        XChangeProperty(
                display_,
                window,
                XInternAtom(display_, "_NET_WM_ALLOWED_ACTIONS", False),
                XA_ATOM,
                32,
                PropModeReplace,
                (unsigned char *)actions_maximized,
                4
        );
    } else {
        XChangeProperty(
                display_,
                window,
                XInternAtom(display_, "_NET_WM_ALLOWED_ACTIONS", False),
                XA_ATOM,
                32,
                PropModeReplace,
                (unsigned char *)actions_normal,
                9
        );
    }
//    XSizeHints hints;
//    hints.flags = PPosition | PWinGravity;  // 设置位置和重力
//    hints.x = 0;      // x 坐标
//    hints.y = 0;      // y 坐标
//    hints.win_gravity = StaticGravity;  // 重力方式（Static=1）
//
//    // 设置 WM_NORMAL_HINTS
//    XSetWMNormalHints(display_, window, &hints);


    XSync(display_, False);
    return true;
}

void WindowManager::OnSelectionRequest(XEvent e) {
    XSelectionRequestEvent *sev = (XSelectionRequestEvent*)&e.xselectionrequest;
    log("OnSelectionRequest start-------->");
    log("OnSelectionRequest owner:%lx requestor:%lx ", sev->owner, sev->requestor);
    sel = XInternAtom(display_, "CLIPBOARD", False);
    utf8 = XInternAtom(display_, "UTF8_STRING", False);
    Atom targets = XInternAtom(display_, "TARGETS", False);
    Atom type_qt = XInternAtom(display_, "peony-qt/encoded-uris", False);
    Atom type_texturi = XInternAtom(display_, "text/uri-list", False);
    Atom type_plain = XInternAtom(display_, "text/plain", False);
    Atom type_text = XInternAtom(display_, "TEXT", False);
    Atom type_string = XInternAtom(display_, "STRING", False);
    log("OnSelectionRequest target:%s property:%s", XGetAtomName(display_, sev->target), XGetAtomName(display_, sev->property));
    if(sev->target == targets){
        if(selection_property_size != 0){
            XChangeProperty(display_,
                            sev->requestor,
                            sev->property,
                            XA_ATOM,
                            32, PropModeReplace, (unsigned char *) selection_property_list,
                            selection_property_size
            );
            log("Sending linux data to window 0x%lx, property '%s'\n", sev->requestor, XGetAtomName(display_, sev->property));
            for(int i = 0; i < selection_property_size; i ++ ){
                log("   property:%s", XGetAtomName(display_, selection_property_list[i]));
            }
        } else if(!clip_text.empty()){
            Atom types[2] = { targets, utf8 };
            XChangeProperty(display_,
                            sev->requestor,
                            sev->property,
                            XA_ATOM,
                            32, PropModeReplace, (unsigned char *) types,
                            (int) (sizeof(types) / sizeof(Atom))
            );
            log("Sending clip text data to window 0x%lx, property '%s' targets & uft8 \n", sev->requestor, XGetAtomName(display_, sev->property));
        } else if (!file_path.empty()){
            Atom types[6] = { type_qt, type_texturi, type_plain, type_text, type_string, utf8};
            XChangeProperty(display_,
                            sev->requestor,
                            sev->property,
                            XA_ATOM,
                            32, PropModeReplace, (unsigned char *) types,
                            (int) (sizeof(types) / sizeof(Atom))
            );
            log("Sending clip file data to window 0x%lx, property '%s'\n", sev->requestor, XGetAtomName(display_, sev->property));
            for(int i = 0; i < (int) (sizeof(types) / sizeof(Atom)); i ++ ){
                log("   property:%s", XGetAtomName(display_, types[i]));
            }
        }
        XSelectionEvent ssev;
        ssev.type = SelectionNotify;
        ssev.requestor = sev->requestor;
        ssev.selection = sev->selection;
        ssev.target = sev->target;
        ssev.property = sev->property;
        ssev.time = sev->time;
        XSendEvent(display_, sev->requestor, 0, NoEventMask, (XEvent *)&ssev);
        XFlush(display_);
    } else {
        XSelectionEvent ssev;
        char *an;
        an = XGetAtomName(display_, sev->property);
        log("Sending data to window 0x%lx, property '%s'\n", sev->requestor, an);
        if (!an){
            log("No data to send to window 0x%lx, property '%s'\n", sev->requestor, an);
            XFree(an);
            XFlush(display_);
            return;
        }
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *data = NULL;
        XGetWindowProperty(display_, owner, sev->target, 0, (~0L), False, AnyPropertyType,
                           &actual_type, &actual_format, &nitems, &bytes_after, &data);
        log("data :%s actual_format:%d data:%s nitems:%lu actual_type:%s",
            XGetAtomName(display_, sev->target), actual_format, data, nitems, XGetAtomName(display_, actual_type));
        log("send property :%s clip_text:%s file_path:%s selection_property_size:%d", XGetAtomName(display_, sev->target), clip_text.c_str()
        , file_path.c_str(), selection_property_size)
        if(selection_property_size == 0 ){
            if(!clip_text.empty()){
                unsigned char * text = (unsigned char *)clip_text.c_str();
                XChangeProperty(display_, sev->requestor, sev->property, utf8, 8, PropModeReplace,
                                text, clip_text.length());
            } else if(!file_path.empty()){
                if(type_qt == sev->target || type_texturi == sev->target || type_plain == sev->target
                   || type_text == sev->target  || type_string == sev->target  || utf8 == sev->target
                        ){
                    unsigned char * text = (unsigned char *)file_path.c_str();
                    XChangeProperty(display_, sev->requestor, sev->property, sev->target, 8,
                                    PropModeReplace,
                                    text, file_path.length());
                } else if(sev->target == XInternAtom(display_, "peony-qt/is-cut", False)){
                    XChangeProperty(display_, sev->requestor, sev->property, sev->target, 8,
                                    PropModeReplace,
                                    reinterpret_cast<const unsigned char *>((char *) "false"), 5);
                }
            }
        }  else {
            XChangeProperty(display_, sev->requestor, sev->property, actual_type, actual_format,
                            PropModeReplace,
                            data, nitems);
            log("change data to window 0x%lx, property:%s actual_type:%s actual_format:%d data:%s nitems:%d",
                sev->requestor,
                XGetAtomName(display_, sev->property),
                XGetAtomName(display_, actual_type),
                actual_format,
                data,
                nitems)
        }
        log("Sending data to window 0x%lx, data: '%s'\n", sev->requestor, data);
        ssev.type = SelectionNotify;
        ssev.requestor = sev->requestor;
        ssev.selection = sev->selection;
        ssev.target = sev->target;
        ssev.property = sev->property;
        ssev.time = sev->time;
        XSendEvent(display_, sev->requestor, True, NoEventMask, (XEvent *)&ssev);
        XFlush(display_);
    }
    log("OnSelectionRequest  end-------->");
}


void WindowManager::OnSelectionClear(XEvent e) {
    Window request = e.xclient.window;
    log("OnSelectionClear start--------- request:0x:%x>\n", request);
    sel = XInternAtom(display_, "CLIPBOARD", False);
    utf8 = XInternAtom(display_, "UTF8_STRING", False);
    Atom target_name = XInternAtom(display_, "TARGETS", False);
    Atom manager_prop_name = XInternAtom(display_, "XSEL_DATA", False);
    XEvent event;
    XConvertSelection(display_, sel, target_name, manager_prop_name, owner, CurrentTime);
    XSelectionEvent *sev;
    for (;;)
    {
        XNextEvent(display_, &event);
        switch (event.type)
        {
            case SelectionNotify:
                sev = (XSelectionEvent*)&event.xselection;
                if (sev->property == None)
                {
                    log("Conversion could not be performed.\n");
                }
                else
                {
                    Atom type, *targets;
                    int di;
                    unsigned long nitems, dul;
                    unsigned char *prop_ret = NULL;
                    char *an = NULL;
                    log("show_targets:\n");
                    XGetWindowProperty(display_, owner, manager_prop_name, 0, 1024 * sizeof (Atom), False, XA_ATOM,
                                       &type, &di, &nitems, &dul, &prop_ret);
                    log("Targets:  nitems:%lu \n", nitems);
                    targets = (Atom *)prop_ret;
                    selection_property_list = targets;
                    selection_property_size = nitems;
                    for(int index = 0; index < selection_property_size ; index ++){
                        log("type :%s", XGetAtomName(display_, selection_property_list[index]));
                    }
                    for (int index = 0; index < nitems; index++)
                    {
                        an = XGetAtomName(display_, targets[index]);
//                        log("    '%s'\n", an);
                        if (an)
                            XFree(an);
                    }
                }
                break;
            default:
                break;
        }
        break;
    }
    ConvertAllTarget();
    XSetSelectionOwner(display_, sel, owner, CurrentTime);
    log("OnSelectionClear  end------->\n");
}

void WindowManager::ConvertAllTarget() {
    XEvent event;
    XSelectionEvent *sev;
    bool isText = false;
    bool isFile = false;
    unsigned char *text_data, *file_data = nullptr;
    for (int i = 0; i < selection_property_size; i++) {
        log("show_data:%s\n", XGetAtomName(display_, selection_property_list[i]));
        XConvertSelection(display_, sel, selection_property_list[i], selection_property_list[i], owner, CurrentTime);
        for (;;) {
            XNextEvent(display_, &event);
            switch (event.type) {
                case SelectionNotify:
                    sev = (XSelectionEvent *) &event.xselection;
                    if (sev->property == None) {
                        log("Conversion could not be performed.\n");
                    } else {
                        Atom actual_type;
                        int actual_format;
                        unsigned long nitems, bytes_after;
                        unsigned char *data = nullptr;
                        XGetWindowProperty(display_, owner, selection_property_list[i], 0, (~0L),
                                           False, AnyPropertyType,
                                           &actual_type, &actual_format, &nitems, &bytes_after, &data);
                        if (actual_format == 8) {  // 字符串类型
                            log("actual_type :%s Content of target: %s\n", XGetAtomName(display_, actual_type), data);
                            if(selection_property_list[i] == utf8){
                                isText = true;
                                text_data = data;
                            }else if(selection_property_list[i] == XInternAtom(display_, "text/uri-list", False)
                                     || selection_property_list[i] == XInternAtom(display_, "peony-qt/encoded-uris", False)
                                    ){
                                isFile = true;
                                file_data = data;
                            }
                        }  else {
                            log("Content of target (binary data or non-8-bit format):\n");
                            for (unsigned long item = 0; item < nitems; item++) {
                                log("%02x ", data[item]);
                            }
                            log("\n");
                        }
                        XFree(data);
                    }
                    break;
                default:
                    break;
            }
            break;
        }
    }
    if(isFile) {
        UpdateXserverClipFile(reinterpret_cast<const char *>(file_data));
    } else if(isText){
        UpdateXserverCliptext(reinterpret_cast<const char *>(text_data));
    }
}


int WindowManager::moveWindow(long window, int x, int y) {
    log("moveWindow %x: x:%d y:%d", window, x, y);
    int ret = XMoveWindow(display_, window, x, y);
    XSync(display_, False);
    return ret;
}


int WindowManager::configureWindow(long window, int x, int y, int w, int h) {
    setMaximizedState(window, false);
    XWindowChanges changes;
    changes.x = x;
    changes.y = y;
    changes.width = w;
    changes.height = h;
    unsigned long value_mask = CWX| CWY |CWWidth | CWHeight  ;
    int ret;
//    value_mask = 76;
    if (isInFrameMap(window)) {
        log("configureWindow_ frame %lx  to %s x.y %s value_mask:%lu ", window,
            Size<int>(w, h).ToString().c_str(), Size<int>(changes.x, changes.y).ToString().c_str(),
            value_mask);
        ret = XConfigureWindow(display_, window, value_mask, &changes);
        for (const auto& pair : clients_) {
            if (pair.second == window) {
                changes.x = 0;
                changes.y = 0;
                ret = XConfigureWindow(display_, pair.first, value_mask, &changes);
            }
        }
    } else {
        log("configureWindow_ %lx to %s x.y %s value_mask:%lu ", window,
            Size<int>(w, h).ToString().c_str(), Size<int>(changes.x, changes.y).ToString().c_str(),
            value_mask);
        ret = XConfigureWindow(display_, window, value_mask, &changes);
    }

//    XEvent ev;
//    ev.type = Expose;
//    ev.xexpose.window = window;
//    XSendEvent(display_, window, False, ExposureMask, &ev);
//    XSync(display_, False);

//    XClientMessageEvent event = {
//            .type = ClientMessage,
//            .window = (Window)window,
//            .message_type = XInternAtom(display_, "WM_CHANGE_STATE", False),
//            .format = 32,
//            .data.l[0] = 3  // IconicState
//    };
//    XSendEvent(display_, root_, False, SubstructureRedirectMask, (XEvent*)&event);
//    XMapWindow(display_, window);



    Atom wm_state = XInternAtom(display_, "WM_STATE", False);

    // 准备要设置的值
    // data[0] = 窗口状态 (1 = Normal)
    // data[1] = 图标窗口 ID (0xe1336880)
    long state_data[2] = {1, 0};

    // 修改窗口属性
    XChangeProperty(
            display_,            // 显示连接
            window,      // 目标窗口
            wm_state,          // 属性: WM_STATE
            wm_state,          // 类型: WM_STATE (32位整数)
            32,                // 格式: 32位
            PropModeReplace,    // 模式: 替换现有值
            (unsigned char *)state_data, // 数据
            2                  // 元素数量 (2个 long 值)
    );
    XFlush(display_);
    return ret;
}

int WindowManager::resizeWindow(long window, int w, int h) {
    log("resizeWindow %x w:%d h:%d", window, w, h);
    int  ret = XResizeWindow(display_, window, w, h);
    XSync(display_, False);
    return ret;
}

int WindowManager::unmapWindow(long window){
    int ret = False;
    ret = XUnmapWindow(display_, window);
    for (const auto& pair : clients_) {
        if (pair.second == window) {
            ret = XUnmapWindow(display_, pair.first);
        }
    }
    XSync(display_, False);
    return ret;
}

int WindowManager::mapWindow(long window){
    int ret = False;
    ret = XMapWindow(display_, window);
    XSync(display_, False);
    return ret;
}

int WindowManager::closeWindow(long window) {
    Atom* supported = nullptr;
    int num_supported = 0;
    if (!XGetWMProtocols(display_, window, &supported, &num_supported)) {
        log("closeWindow failed to get protocols for window:%x", window);
        return -1; // 返回错误
    }

    log("closeWindow window:%x num_supported:%d", window, num_supported);
    for (int i = 0; i < num_supported; ++i) {
        log("  Supported protocol: %s", XGetAtomName(display_, supported[i]));
    }

    int ret = -1;
    if (num_supported > 0 && supported[0] == XInternAtom(display_, "WM_DELETE_WINDOW", False)) {
        log("closeWindow supported WM_DELETE_WINDOW");
        XEvent msg;
        memset(&msg, 0, sizeof(msg));
        msg.xclient.type = ClientMessage;
        msg.xclient.message_type = XInternAtom(display_, "WM_PROTOCOLS", False);
        msg.xclient.window = window;
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = XInternAtom(display_, "WM_DELETE_WINDOW", False);
        msg.xclient.data.l[1] = CurrentTime;
        ret = XSendEvent(display_, window, false, 0, &msg);
    } else {
        log("closeWindow not supported, killing client");
        ret = XKillClient(display_, window);
    }

    XSync(display_, False);
    XFree(supported); // 释放支持的协议列表
    return ret;
}

int WindowManager::raiseWindow(long window) {
    log("raiseWindow %x", window);
    int ret = XRaiseWindow(display_, window);
    XSetInputFocus(display_, window, RevertToPointerRoot, CurrentTime);
    XSync(display_, false);
    return ret;
}

void WindowManager::initCompositor() {
    XCompositeRedirectSubwindows(display_, root_, CompositeRedirectAutomatic);
    XSync(display_, false);
}

jint WindowManager::sendClipText(const char *string) {
    std::string in_text = string;
    if (clip_text == in_text) {
        log("no need update clip text");
    } else {
        clip_text = in_text;
        log("update clip text :%s", clip_text.c_str());
    }
    selection_property_size = 0;
    file_path.clear();
    XSetSelectionOwner(display_, sel, owner, CurrentTime);
    XFlush(display_);
    return True;
}


void WindowManager::setClipData(char *text, char *path) {
    if(strlen(text)){
        sendClipText(text);
    } else if(strlen(path)){
        sendClipFile(path);
    }
}


jint WindowManager::sendClipFile(const char *string) {
    std::string in_text = string;
    if (file_path == in_text) {
        log("no need update clip file")
    } else {
        file_path = in_text;
        log("update clip file :%s", file_path.c_str())
    }
    selection_property_size = 0;
    clip_text.clear();
    XSetSelectionOwner(display_, sel, owner, CurrentTime);
    XFlush(display_);
    return True;
}

jint WindowManager::circulaSubWindows(jlong window, jboolean lowest) {
    int ret;
    if(lowest){
        ret = XCirculateSubwindows(display_, window, LowerHighest);
        log("circulaSubWindows ret:%d", ret);
    } else {
        ret = XCirculateSubwindowsUp(display_, window);
    }
    XSync(display_, False);
    return ret;
}

void WindowManager::UpdateXserverCliptext(const char *text) {
    if(GlobalEnv && is_valid_utf8(text)){
        jstring utf = GlobalEnv->NewStringUTF(text);
        jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                        "updateXserverCliptext", "(Ljava/lang/String;)V");
        GlobalEnv->CallStaticVoidMethod(staticClass, method, utf);
    }
}

void WindowManager::UpdateXserverClipFile(const char *text){
    if(GlobalEnv && is_valid_utf8(text)){
        jstring utf = GlobalEnv->NewStringUTF(text);
        jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                        "updateXserverClipFile", "(Ljava/lang/String;)V");
        GlobalEnv->CallStaticVoidMethod(staticClass, method, utf);
    }
}





