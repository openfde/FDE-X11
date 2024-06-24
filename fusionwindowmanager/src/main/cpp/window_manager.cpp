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


::WindowManager *WindowManager::create(const char *export_display, JNIEnv * env, jclass cls) {
    Display* display = XOpenDisplay(export_display);
    if (display == nullptr) {
        log("Failed to open X display");
        return nullptr;
    }
    // 2. Construct WindowManager instance.
    return new WindowManager(display);
}


WindowManager::WindowManager(Display* display)
        : display_(display),
          root_(DefaultRootWindow(display_)),
          WM_PROTOCOLS(XInternAtom(display_, "WM_PROTOCOLS", false)),
          WM_DELETE_WINDOW(XInternAtom(display_, "WM_DELETE_WINDOW", false)),
          stoped(False) {
}


int WindowManager::OnWMDetected(Display* display, XErrorEvent* e) {
    // In the case of an already running window manager, the error code from
    // XSelectInput is BadAccess. We don't expect this handler to receive any
    // other errors.
    CHECK_EQ(static_cast<int>(e->error_code), BadAccess);
    // Set flag.
    wm_detected_ = true;
    // The return value is ignored.
    return 0;
}


WindowManager::~WindowManager() {
    log("~WindowManager");
    XCloseDisplay(display_);
}


void WindowManager::Frame(Window w, bool was_created_before_window_manager) {
    log("Frame_ %x", w);
    // Visual properties of the frame to create.
    const unsigned int BORDER_WIDTH = 1;
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
    XMapWindow(display_, frame);
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
    log("isNormalWindow ? %lx", window);
    if (XGetWindowProperty(display_, window, type, 0, 1024, False, AnyPropertyType,
                           &actualType, &actualFormat, &nItems, &bytesAfter, &propData) ==
        Success) {
        log(" actualType = %ld \n", actualType);
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
                    log("%s not normal window %lx \n", atomValue, window);
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
}

void WindowManager::OnReparentNotify(const XReparentEvent& e) {}

void WindowManager::OnMapNotify(const XMapEvent &e) {
    if(e.event == root_ && support_composite){
        XCompositeNameWindowPixmap(display_, e.window);
        named_windows.insert(e.window);
        XSync(display_, False);
    }
}

void WindowManager::OnUnmapNotify(const XUnmapEvent& e) {
    // If the window is a client window we manage, unframe it upon UnmapNotify. We
    // need the check because we will receive an UnmapNotify event for a frame
    // window we just destroyed ourselves.
    if (!clients_.count(e.window)) {
        log("Ignore UnmapNotify for non-client window %lu",e.window);
        return;
    }
    // Ignore event if it is triggered by reparenting a window that was mapped
    // before the window manager started.
    //
    // Since we receive UnmapNotify events from the SubstructureNotify mask, the
    // event attribute specifies the parent window of the window that was
    // unmapped. This means that an UnmapNotify event from a normal client window
    // should have this attribute set to a frame window we maintain. Only an
    // UnmapNotify event triggered by reparenting a pre-existing window will have
    // this attribute set to the root window.
    if (e.event == root_) {
        log("Ignore UnmapNotify for reparented pre-existing window %lu", e.window);
        return;
    }
//    Unframe(e.window);
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
    XMapWindow(display_, e.window);

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
        log("value_mask : %lu", value_mask);
    }
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

    owner = XCreateSimpleWindow(display_, root_, -10, -10, 1, 1, 0, 0, 0);
    log("owner:%x", owner);
    sel = XInternAtom(display_, "CLIPBOARD", False);
//    utf8 = XInternAtom(display_, "UTF8_STRING", False);
    XSetSelectionOwner(display_, sel, owner, CurrentTime);

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
                OnSelectionClear(e);
                break;
            case SelectionRequest:
                OnSelectionRequest(e);
                break;
            default:
                break;
                log("Ignored event");
        }
    }
}


void WindowManager::OnSelectionRequest(XEvent e) {
    XSelectionRequestEvent *sev = (XSelectionRequestEvent*)&e.xselectionrequest;
    log("OnSelectionRequest owner:%lx requestor:%lx ", sev->owner, sev->requestor);
    if(sev->requestor == root_){
        return;
    }
    sel = XInternAtom(display_, "CLIPBOARD", False);
    utf8 = XInternAtom(display_, "UTF8_STRING", False);

    Atom targets = XInternAtom(display_, "TARGETS", False);
    if(sev->target == targets){
        Atom types[2] = { targets, utf8 };
        XChangeProperty(display_,
                        sev->requestor,
                        sev->property,
                        XA_ATOM,
                        32, PropModeReplace, (unsigned char *) types,
                        (int) (sizeof(types) / sizeof(Atom))
        );
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
        time_t now_tm;
        char *now, *an;
        now_tm = time(NULL);
        now = ctime(&now_tm);
        an = XGetAtomName(display_, sev->property);
        log("Sending data to window 0x%lx, property '%s'\n", sev->requestor, an);
        if (an)
            XFree(an);
        XChangeProperty(display_, sev->requestor, sev->property, utf8, 8, PropModeReplace,
                        (unsigned char *)cliptext, strlen(cliptext));
        ssev.type = SelectionNotify;
        ssev.requestor = sev->requestor;
        ssev.selection = sev->selection;
        ssev.target = sev->target;
        ssev.property = sev->property;
        ssev.time = sev->time;
        XSendEvent(display_, sev->requestor, True, NoEventMask, (XEvent *)&ssev);
        XFlush(display_);
    }
}

void WindowManager::OnSelectionClear(XEvent e) {
    sel = XInternAtom(display_, "CLIPBOARD", False);
    utf8 = XInternAtom(display_, "UTF8_STRING", False);

    char *result;
    unsigned long ressize, restail;
    int resbits;
    Atom bufid = XInternAtom(display_, "CLIPBOARD", False),
            fmtid = XInternAtom(display_, "UTF8_STRING", False),
            propid = XInternAtom(display_, "XSEL_DATA", False),
            incrid = XInternAtom(display_, "INCR", False);
    XEvent event;
    XConvertSelection(display_, bufid, fmtid, propid, owner, CurrentTime);
    do {
        XNextEvent(display_, &event);
    } while (event.type != SelectionNotify || event.xselection.selection != bufid);

    if (event.xselection.property)
    {
        XGetWindowProperty(display_, owner, propid, 0, LONG_MAX/4, False, AnyPropertyType,
                           &fmtid, &resbits, &ressize, &restail, (unsigned char**)&result);
        if (fmtid == incrid){
            log("Buffer is too large and INCR reading is not implemented yet.\n");
        } else {
            cliptext = (char*)malloc(strlen(result) + 1);
            if (cliptext != NULL) {
                strcpy(cliptext, result);
            }
            log("%.*s \n", (int)ressize, result);
        }
        XFree(result);
    }
    log("OnSelectionClear:\n");
    XSetSelectionOwner(display_, sel, owner, CurrentTime);
}


int WindowManager::moveWindow(long window, int x, int y) {
    log("moveWindow %x: x:%d y:%d", window, x, y);
    int ret = XMoveWindow(display_, window, x, y);
    XSync(display_, False);
    return ret;
}

int WindowManager::configureWindow(long window, int x, int y, int w, int h) {
    log("configureWindow %x: x:%d y:%d w:%d h:%d", window, x, y, w, h);
    XWindowChanges changes;
    changes.x = x;
    changes.y = y;
    changes.width = w;
    changes.height = h;
    unsigned long value_mask = CWX | CWY | CWWidth | CWHeight ;
    int ret;
    if (isInFrameMap(window)) {
        log("configureWindow_ frame %lx  to %s x.y %s value_mask:%lu ", window,
            Size<int>(w, h).ToString().c_str(), Size<int>(changes.x, changes.y).ToString().c_str(),
            value_mask);
        ret = XConfigureWindow(display_, window, value_mask, &changes);
        XSync(display_, False);
    } else {
        log("configureWindow_ %lx to %s x.y %s value_mask:%lu ", window,
            Size<int>(w, h).ToString().c_str(), Size<int>(changes.x, changes.y).ToString().c_str(),
            value_mask);
        ret = XConfigureWindow(display_, window, value_mask, &changes);
        XSync(display_, False);
    }
    return ret;
}

int WindowManager::resizeWindow(long window, int w, int h) {
    log("resizeWindow %x w:%d h:%d", window, w, h);
    int  ret = XResizeWindow(display_, window, w, h);
    XSync(display_, False);
    return ret;
}

int WindowManager::closeWindow(long window) {
    Atom* supported;
    int num_supported;
    XGetWMProtocols(display_, window, &supported, &num_supported);
    log("closeWindow window:%x supported:%d num_supported:%d", window, supported, num_supported);
    int ret;
    if(supported) {
        log("closeWindow supported1");
        XEvent msg;
        memset(&msg, 0, sizeof(msg));
        msg.xclient.type = ClientMessage;
        msg.xclient.message_type = WM_PROTOCOLS;
        msg.xclient.window = window;
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = WM_DELETE_WINDOW;
        ret = XSendEvent(display_, window, false, 0, &msg);
    } else {
        log("closeWindow not supported");
        ret = XKillClient(display_, window);
    }
    XSync(display_, False);
    return ret;
}

int WindowManager::raiseWindow(long window) {
    log("raiseWindow %x", window);
    int ret = XRaiseWindow(display_, window);
    XSetInputFocus(display_, window, RevertToPointerRoot, CurrentTime);
    XSync(display_, False);
    return ret;
}

void WindowManager::initCompositor() {
    XCompositeRedirectSubwindows(display_, root_, CompositeRedirectAutomatic);
    XSync(display_, false);
}

jint WindowManager::sendClipText(const char *string) {
    cliptext = (char*)malloc(strlen(string) + 1);
    if (cliptext != NULL) {
        strcpy(cliptext, string);
    }
    return True;
}



