//
// Created by yang on 2024/4/18.
//

#include "include/window_manager.h"
#include "include/X11/keysymdef.h"
#include "include/X11/extensions/Xcomposite.h"
#include <set>


using ::std::max;
using ::std::mutex;
using ::std::string;
using ::std::unique_ptr;

bool WindowManager::wm_detected_;
bool WindowManager::support_composite;
mutex WindowManager::wm_detected_mutex_;
Window frame_window;
Window top_level_debug;
std::set<Window> window_under_frames;
std::set<Window> frames;
std::set<Window> named_windows;

::WindowManager *WindowManager::create() {
    Display* display = XOpenDisplay(":1");
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
    log("may Frame %x", w);
    // Visual properties of the frame to create.
    const unsigned int BORDER_WIDTH = 3;
    const unsigned long BORDER_COLOR = 0xff0000;
    const unsigned long BG_COLOR = 0x0000ff;
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
            SubstructureRedirectMask | SubstructureNotifyMask);
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
    frame_window = frame;
    top_level_debug = w;
    window_under_frames.insert(w);
    frames.insert(frame);
    log("Framed window %x reparent to frame %x" ,w , frame);
}


void WindowManager::OnCreateNotify(const XCreateWindowEvent& e) {}

void WindowManager::OnDestroyNotify(const XDestroyWindowEvent& e) {
    frames.erase(e.window);
    window_under_frames.erase(e.window);
}

void WindowManager::OnReparentNotify(const XReparentEvent& e) {}

void WindowManager::OnMapNotify(const XMapEvent& e) {
    auto it = frames.find(e.window);
    if (it != frames.end()) {
        log("Name window %x", e.window);
        XCompositeNameWindowPixmap(display_, e.window);
        named_windows.insert(e.window);
    }
    XSync(display_, False);
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

    Unframe(e.window);
}

void WindowManager::Unframe(Window w) {
    CHECK(clients_.count(w));
    log("Unframe %x", w);
    // We reverse the steps taken in Frame().
    const Window frame = clients_[w];
    // 1. Unmap frame.
    XUnmapWindow(display_, frame);
    // 2. Reparent client window.
    XReparentWindow(
            display_,
            w,
            root_,
            0, 0);  // Offset of client window within root.
    // 3. Remove client window from save set, as it is now unrelated to us.
    XRemoveFromSaveSet(display_, w);
    // 4. Destroy frame.
    XDestroyWindow(display_, frame);
    // 5. Drop reference to frame handle.
    clients_.erase(w);

    log("Unframed window %lu frame %lu" , w , frame);
}

void WindowManager::OnConfigureNotify(const XConfigureEvent& e) {
//    auto it = frames.find(e.window);
//    if (it != frames.end()) {
//        XCompositeNameWindowPixmap(display_, e.window);
//        named_windows.insert(e.window);
//    }
//    XSync(display_, False);
}

void WindowManager::OnMapRequest(const XMapRequestEvent& e) {
    // 1. Frame or re-frame window.
    Frame(e.window, false);
    // 2. Actually map window.
    XMapWindow(display_, e.window);
}

void WindowManager::OnConfigureRequest(const XConfigureRequestEvent& e) {
    XWindowChanges changes;
    changes.x = e.x;
    changes.y = e.y;
    changes.width = e.width;
    changes.height = e.height;
    changes.border_width = e.border_width;
    changes.sibling = e.above;
    changes.stack_mode = e.detail;
    if (clients_.count(e.window)) {
        const Window frame = clients_[e.window];
        XConfigureWindow(display_, frame, e.value_mask, &changes);
        log("Resize %x  to %s ", frame, Size<int>(e.width, e.height).ToString().c_str());
    } else {
        XConfigureWindow(display_, e.window, e.value_mask, &changes);
        log("Resize %x to %s" , e.window , Size<int>(e.width, e.height).ToString().c_str());
    }
//    XCompositeRedirectSubwindows (display_, root_, CompositeRedirectAutomatic);
//    XCompositeNameWindowPixmap(display_, frame_window);
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

int WindowManager::OnXError(Display* display, XErrorEvent* e) {
    const int MAX_ERROR_TEXT_LENGTH = 1024;
    char error_text[MAX_ERROR_TEXT_LENGTH];
    XGetErrorText(display, e->error_code, error_text, sizeof(error_text));
    log("Received X error:\n");
    log("    Request: %d", int(e->request_code));
    log(" - %s \n", XRequestCodeToString(e->request_code).c_str());
    log("    Error code %d: " , int(e->error_code));
    log(" - %s \n", error_text );
    log("    Resource ID: %d",e->resourceid);
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
                SubstructureRedirectMask | SubstructureNotifyMask);
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
        Frame(top_level_windows[i], true);
    }
    //     iii. Free top-level window array.
    XFree(top_level_windows);
    //   e. Ungrab X server.
    XUngrabServer(display_);

    // 2. Main event loop.
    while (!stoped) {
        // 1. Get next event.
        XEvent e;
        XNextEvent(display_, &e);
        log("------Received event: %s",ToString(e).c_str());

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
            default:
                log("Ignored event");
        }
    }
}

int WindowManager::moveWindow(long ptr, int x, int y) {
    ptr = frame_window;
    Window returned_root, returned_parent;
    Window* top_level_windows;
    unsigned int num_top_level_windows;
    XQueryTree(
            display_,
            root_,
            &returned_root,
            &returned_parent,
            &top_level_windows,
            &num_top_level_windows);
    log("moveWindow %x %lu", ptr, ptr);
    int ret;
//    for (unsigned int i = 0; i < num_top_level_windows; ++i) {
//        ret = XMoveWindow(display_, top_level_windows[i], x, y);
//    }
    ret = XMoveWindow(display_, ptr, x, y);
    XSync(display_, False);
    return ret;
}

int WindowManager::resizeWindow(long ptr, int x, int y) {
    ptr = frame_window;
    Window returned_root, returned_parent;
    Window* top_level_windows;
    unsigned int num_top_level_windows;
    XQueryTree(
            display_,
            root_,
            &returned_root,
            &returned_parent,
            &top_level_windows,
            &num_top_level_windows);
    log("resizeWindow %x %lu", ptr, ptr);
    int ret;
//    for (unsigned int i = 0; i < num_top_level_windows; ++i) {
//     ret = XResizeWindow(display_, top_level_windows[i], x, y);
//    }
    ret = XResizeWindow(display_, ptr, x, y);
    ret = XResizeWindow(display_, top_level_debug, x, y);
    XSync(display_, False);
    return ret;
}

int WindowManager::closeWindow(long ptr) {
    Window returned_root, returned_parent;
    Window* top_level_windows;
    unsigned int num_top_level_windows;
    XQueryTree(
            display_,
            root_,
            &returned_root,
            &returned_parent,
            &top_level_windows,
            &num_top_level_windows);
    ptr = top_level_debug;
//    for (unsigned int i = 0; i < num_top_level_windows; ++i) {
//    }
    log("closeWindow %x %lu", ptr, ptr);
    int ret = XKillClient(display_, ptr);
    XSync(display_, False);
    return ret;
}

int WindowManager::raiseWindow(long ptr) {
    ptr = frame_window;
    log("raiseWindow %x %lu", ptr, ptr);
    int ret = XRaiseWindow(display_, frame_window);
    XSync(display_, False);
    return ret;
}

void WindowManager::initCompositor() {
    XCompositeRedirectSubwindows(display_, root_, CompositeRedirectAutomatic);
    XSync(display_, false);
}