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

::WindowManager *WindowManager::create(char *export_display, JNIEnv *env, jclass cls)
{
    staticClass = cls;
    GlobalEnv = env;
    std::string unixstring = "unix:/tmp/.X11-unix/X";
    std::string exportstring(export_display);
    std::string display_str = unixstring + exportstring;
    Display *display = XOpenDisplay(display_str.c_str());
    if (display == nullptr)
    {
        log("Failed to open X display");
        return nullptr;
    }
    // 2. Construct WindowManager instance.
    return new WindowManager(display);
}

WindowManager::WindowManager(Display *display)
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

static DisplayInfo *
initialize(gboolean replace_wm, Display *display_, Window back_window, Window root_)
{

    DisplayInfo *display_info;
    gint i, nscreens, default_screen;

    clientClearFocus(NULL);
    display_info = myDisplayInit(display_);
    display_info->dpy = display_;
    display_info->enable_compositor = compositor;
    nscreens = ScreenCount(display_info->dpy);
    default_screen = DefaultScreen(display_info->dpy);
    log("nscreens = %d, display_info = %p", nscreens, display_info);

    for (i = 0; i < nscreens; i++)
    {
        ScreenInfo *screen_info;
        screen_info = myScreenInit(display_info, MAIN_EVENT_MASK, i, back_window, root_);
        if (screen_info == NULL)
        {
            log("Failed to initialize screen %d", i);
            continue;
        }
        if (i == default_screen)
        {
            display_info->screens = g_slist_prepend(display_info->screens, screen_info);
            display_info->nb_screens++;
        }
        else
        {
            g_slist_append(display_info->screens, screen_info);
        }
        screen_info->xfwm4_win = back_window;
        myDisplayAddScreen(display_info, screen_info);

        // setUTF8StringHint(display_info, back_window, NET_WM_NAME, "FDE-XWM");

        setNetSupportedHint(display_info, screen_info->xroot, back_window);
        log(" display_info  %p   back_window %p ", display_info, back_window);

        // setNetDesktopInfo(display_info, screen_info->xroot, screen_info->current_ws,
                        //   screen_info->width,
                        //   screen_info->height);
        XSetInputFocus(display_info->dpy, back_window, RevertToPointerRoot, CurrentTime);
        // XSync(display_info->dpy, FALSE);
        clientFrameAll(screen_info);
    }
    return display_info;
}

int WindowManager::OnWMDetected(Display *display, XErrorEvent *e)
{
    // In the case of an already running window manager, the error code from
    // XSelectInput is BadAccess. We don't expect this handler to receive any
    // other errors.
    CHECK_EQ(static_cast<int>(e->error_code), BadAccess);
    // Set flag.
    //    wm_detected_ = true;
    // The return value is ignored.
    return 0;
}

WindowManager::~WindowManager()
{
    log("~WindowManager");
    for (auto &pair : clients_)
    {
        XDestroyWindow(display_, pair.second);
    }
    if (owner)
    {
        XDestroyWindow(display_, owner);
    }
    XCloseDisplay(display_);
}

void WindowManager::Frame(Window w, bool was_created_before_window_manager)
{
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
    if (was_created_before_window_manager)
    {
        if (x_window_attrs.override_redirect ||
            x_window_attrs.map_state != IsViewable)
        {
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
        0, 0); // Offset of client window within frame.
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
    XConfigureWindow(display_, frame, 15, &change_values);
    change_values.x = 1; // x_window_attrs.x;
    change_values.y = 1; // x_window_attrs.y;
    change_values.width = x_window_attrs.width;
    change_values.height = x_window_attrs.height;
    XConfigureWindow(display_, w, 15, &change_values);
    XMapWindow(display_, frame);
    XMapWindow(display_, w);
    Atom normal_type = XInternAtom(display_, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    setWindowType(frame, normal_type);

    window_under_frames.insert(w);
    frames.insert(frame);
    log("Framed_ window %x reparent to frame %x", w, frame);
}

bool WindowManager::isNormalWindow(long window)
{
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
        Success)
    {
        //        log(" actualType = %ld \n", actualType);
        if (actualType == XA_ATOM)
        {
            Atom *atoms = (Atom *)propData;
            for (int i = 0; i < nItems; i++)
            {
                if (atoms[i] == type_nomarl)
                {
                    continue;
                }
                else if (atoms[i] == type_menu || atoms[i] == type_dialog || atoms[i] == type_popup)
                {
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

bool WindowManager::isInFrameMap(long window)
{
    auto it = frames.find(window);
    if (it != frames.end())
    {
        log("isInFrameMap %x", window);
        return True;
    }
    return False;
}

void WindowManager::OnCreateNotify(const XCreateWindowEvent &e) {}

void WindowManager::OnDestroyNotify(const XDestroyWindowEvent &e)
{
    Client *c = myDisplayGetClientFromWindow(display_info, e.window, SEARCH_WINDOW);
    if (c)
    {
        clientUnframe(c, FALSE);
    }
}

void WindowManager::OnReparentNotify(const XReparentEvent &e) {}


// 检查窗口是否可见
bool is_window_visible(Display *display, Window window) {
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, window, &attrs)) {
        return (attrs.map_state == IsViewable);
    }
    return false;
}

// 检查窗口是否为顶层窗口
bool is_top_level_window(Display *display, Window window) {
    Window root, parent;
    Window *children;
    unsigned int nchildren;

    // 获取窗口的父窗口
    if (XQueryTree(display, window, &root, &parent, &children, &nchildren)) {
        // 如果父窗口是根窗口，那么这是一个顶层窗口
        if (parent == root) {
            XFree(children);
            return true;
        }

        // 检查是否有临时提示（如对话框）
        Window transient_for;
        if (XGetTransientForHint(display, window, &transient_for)) {
            // 如果有临时提示，也认为是顶层窗口
            XFree(children);
            return true;
        }

        XFree(children);
    }

    return false;
}

void WindowManager::OnMapNotify(const XMapEvent &e)
{
    Client *c;

    log("OnMapNotify window (0x%lx)", e.window);

    c = myDisplayGetClientFromWindow(display_info, e.window, SEARCH_WINDOW);
    if (c)
    {
        log("client \"%s\" (0x%lx)", c->name, c->window);
//        if (FLAG_TEST(c->xfwm_flags, XFWM_FLAG_MAP_PENDING))
//        {
//            FLAG_UNSET(c->xfwm_flags, XFWM_FLAG_MAP_PENDING);
//        }
//
//        Atom type;
//        int format;
//        unsigned long nitems, after;
//        unsigned char *data = NULL;
//        Atom classAtom = XInternAtom(display_, "_NET_WM_NAME", False);
//        XGetWindowProperty(display_, e.window, classAtom, 0, (~0L), False,
//                            AnyPropertyType, &type, &format, &nitems, &after, &data);
//
//        if(data){
//            log("_NET_WM_NAME %s classAtom %lu", data, classAtom);
//        }
        if(c->frame && is_window_visible(display_, c->frame)){
            log("XCompositeNameWindowPixmap window:0x%lx", c->frame);
            XCompositeNameWindowPixmap(display_, c->frame);
            XSync(display_, False);
        }
    } else {
        if (e.event == root_ && support_composite)
        {
            log("XCompositeNameWindowPixmap window:0x%lx", e.window);
            XCompositeNameWindowPixmap(display_, e.window);
            XSync(display_, False);
        }
    }



}

void WindowManager::OnUnmapNotify(const XUnmapEvent &ev)
{
    Client *c;
    ScreenInfo *screen_info;
    log("OnUnmapNotify window:0x%lx event:0x%lx send:%d", ev.window, ev.event, ev.send_event);
    c = myDisplayGetClientFromWindow(display_info, ev.window, SEARCH_WINDOW);
    if (c)
    {
        screen_info = c->screen_info;
        if ((ev.event == screen_info->xroot) && (ev.send_event))
        {
            if (!FLAG_TEST(c->xfwm_flags, XFWM_FLAG_VISIBLE))
            {
                // TRACE ("ICCCM UnmapNotify for \"%s\"", c->name);
                // list_of_windows = clientListTransientOrModal (c);
                // clientPassFocus (screen_info, c, list_of_windows);
                clientUnframe(c, FALSE);
                // g_list_free (list_of_windows);
            }
        }
    }
}

void WindowManager::Unframe(Window w)
{
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
        0, 0); // Offset of client window within root.
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
    log("Unframed window %lu frame %x", w, frame);
}

void WindowManager::OnConfigureNotify(const XConfigureEvent &e)
{
    //    log("OnConfigureNotify window:%lx above:%lx", e.window, e.above);
    if (clients_.count(e.above))
    {
        //        log("OnConfigureNotify %lx", e.window);
        configedTopWindow[e.window] = e;
    }
}

void WindowManager::OnMapRequest(const XMapRequestEvent &e)
{
    Client *c;
    log("window (0x%lx)", e.window);
    if (e.window == None)
    {
        log("mapping None ???");
    }
    c = myDisplayGetClientFromWindow(display_info, e.window, SEARCH_WINDOW);
    if (c)
    {
        ScreenInfo *screen_info = c->screen_info;

        if (FLAG_TEST(c->xfwm_flags, XFWM_FLAG_MAP_PENDING))
        {
            // TRACE ("ignoring MapRequest on window (0x%lx)", ev->window);
        }
        if (FLAG_TEST(c->xfwm_flags, XFWM_FLAG_WAS_SHOWN))
        {
            //  clientClearAllShowDesktop (screen_info);
        }
        clientShow(c, TRUE);

        if (FLAG_TEST(c->flags, CLIENT_FLAG_STICKY) ||
            (c->win_workspace == screen_info->current_ws))
        {
            // TODO FDE
            //  clientFocusNew(c);
        }
    }
    else
    {
        log("client not found for window %lx", e.window);
        clientFrame(display_info, e.window, FALSE);
    }
}

void syncConfigureRequest(int x, int y, int w, int h, XID window, int isMoving)
{
    jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                    "syncConfigureRequest", "(IIIIJI)V");
    GlobalEnv->CallStaticVoidMethod(staticClass, method, x, y, w, h, window, isMoving);
}

void WindowManager::OnCirculateRequest(const XCirculateRequestEvent &e)
{
    XCirculateSubwindows(display_, e.parent, e.place);
}

void WindowManager::OnConfigureRequest(const XConfigureRequestEvent &e)
{
    Client *c;
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
    if (e.y < DECORCATIONVIEW_HEIGHT)
    {
        value_mask = e.value_mask | (1 << 1);
        //        log("value_mask : %lu", value_mask);
    }
    loge("OnConfigureRequest x:%d y:%d w:%d h:%d border:%d above:%d stack:%d value:%d",
         e.x, e.y, e.width, e.height, e.border_width, e.above, e.detail, value_mask);
    XConfigureRequestEvent *ev = (XConfigureRequestEvent *)&e;
    c = myDisplayGetClientFromWindow (display_info, ev->window, SEARCH_WINDOW);
    if (c)
    {
        changes.x = c->x;
        changes.y = c->y;
        log ("OnConfigureRequest \"%s\" (0x%lx) x:%d y:%d e.x:%d e.y:%d", c->name, c->window, c->x, c->y, e.x, e.y);
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING))
        {
            /* Sorry, but it's not the right time for configure request */
            return ;
        }
        // clientAdjustCoordGravity (c, c->gravity, &wc, &ev->value_mask);
        clientMoveResizeWindow (c, &changes, value_mask);
    }
    else
    {
        log ("unmanaged OnConfigureRequest for window 0x%lx", ev->window);
        myDisplayErrorTrapPush (display_info);
        XConfigureWindow (display_info->dpy, ev->window, value_mask, &changes);
        myDisplayErrorTrapPopIgnored (display_info);
    }

    if ((value_mask & CWX || value_mask & CWY || value_mask & CWWidth || value_mask & CWHeight)
        && c
    )
    {
        syncConfigureRequest(changes.x, changes.y, changes.width,
                             changes.height, c->frame, 2);
    }
}

void WindowManager::OnButtonPress(const XButtonEvent &e)
{
    CHECK(clients_.count(e.window));
    const Window frame = clients_[e.window];
    log("OnButtonPress  window:%lx x_root:%d y_root:%d state:0x%X type:%d x:%d y:%d send_event:%d",
         e.window, e.x_root, e.y_root, e.state, e.type, e.x, e.y, e.send_event);

    // // 1. Save initial cursor position.
    // drag_start_pos_ = Position<int>(e.x_root, e.y_root);

    // // 2. Save initial window info.
    // Window returned_root;
    // int x, y;
    // unsigned width, height, border_width, depth;
    // XGetGeometry(
    //     display_,
    //     frame,
    //     &returned_root,
    //     &x, &y,
    //     &width, &height,
    //     &border_width,
    //     &depth);
    // drag_start_frame_pos_ = Position<int>(x, y);
    // drag_start_frame_size_ = Size<int>(width, height);

    // // 3. Raise clicked window to top.
    // XRaiseWindow(display_, frame);
    raiseWindow(e.window);
}

void WindowManager::OnButtonRelease(const XButtonEvent &e) {
    log("OnButtonRelease  window:%lx x_root:%d y_root:%d state:0x%X type:%d x:%d y:%d send_event:%d",
        e.window, e.x_root, e.y_root, e.state, e.type, e.x, e.y, e.send_event);
    ScreenInfo *screen_info;
    screen_info = myDisplayGetScreenFromWindow(display_info, e.window);
    if (!screen_info) {
        return;
    }
    myScreenUngrabKeyboard(screen_info, myDisplayGetCurrentTime(display_info));
    // myScreenUngrabPointer(screen_info, myDisplayGetCurrentTime(display_info));
    XUngrabPointer (display_, myDisplayGetCurrentTime(display_info));
    screen_info->passdata.c = NULL;
    log("OnButtonRelease clear passdata.c");
    Client *c;
    c = myDisplayGetClientFromWindow (display_info, e.window, SEARCH_WINDOW);
    if(isTaskMoving && c){
        isTaskMoving = FALSE;
        syncConfigureRequest(0, 0, 0, 0, c->frame, isTaskMoving);
    }
}

void WindowManager::OnMotionNotify(const XMotionEvent &e)
{
    // CHECK(clients_.count(e.window));
    const Window frame = clients_[e.window];
    const Position<int> drag_pos(e.x_root, e.y_root);
    const Vector2D<int> delta = drag_pos - drag_start_pos_;
    log("OnMotionNotify  window:%lx x_root:%d y_root:%d state:0x%X type:%d x:%d y:%d send_event:%d",
         e.window, e.x_root, e.y_root, e.state, e.type, e.x, e.y, e.send_event);
    ScreenInfo *screen_info;
    screen_info = myDisplayGetScreenFromWindow(display_info, e.window);
    if (!screen_info || !screen_info->passdata.c)
    {
        return; 
    }
    log("OnMotionNotify_mx:%d my:%d  ox:%d oy:%d ow:%d oh:%d oldw:%d oldh:%d cancel_x:%d cancel_y:%d",
        screen_info->passdata.mx, screen_info->passdata.my, 
        screen_info->passdata.ox, screen_info->passdata.oy,
        screen_info->passdata.ow, screen_info->passdata.oh,
        screen_info->passdata.oldw, screen_info->passdata.oldh,
        screen_info->passdata.cancel_x, screen_info->passdata.cancel_y
    );
    Client *c = screen_info->passdata.c;
    log("OnMotionNotify width:%d height:%d", c->width, c->height);
    if (e.state & Button1Mask)
    {
        int origin_x =  screen_info->passdata.ox;
        int origin_y =  screen_info->passdata.oy;
        int final_x = origin_x + (e.x_root - screen_info->passdata.mx);
        int final_y = origin_y + (e.y_root - screen_info->passdata.my);
        c->x = final_x;
        c->y = final_y;
        XWindowChanges changes;
        changes.x = final_x;
        changes.y = final_y;
        changes.width = c->width;
        changes.height = c->height;
        unsigned long value_mask = CWX | CWY ;
        log("OnMotionNotify_window:%lx frame:%lx final_x:%d final_y:%d", c->window, c->frame, final_x, final_y);
        clientMoveResizeWindow (c, &changes, value_mask);
        if ((value_mask & CWX || value_mask & CWY || value_mask & CWWidth || value_mask & CWHeight))
        {
            isTaskMoving = TRUE;
            syncConfigureRequest(changes.x, changes.y, changes.width,
                                 changes.height, c->frame, isTaskMoving);
        }
    }
    else if (e.state & Button3Mask)
    {
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

void WindowManager::OnKeyPress(const XKeyEvent &e)
{
    if ((e.state & Mod1Mask) &&
        (e.keycode == XKeysymToKeycode(display_, XK_F4)))
    {
        // alt + f4: Close window.
        //
        // There are two ways to tell an X window to close. The first is to send it
        // a message of type WM_PROTOCOLS and value WM_DELETE_WINDOW. If the client
        // has not explicitly marked itself as supporting this more civilized
        // behavior (using XSetWMProtocols()), we kill it with XKillClient().
        Atom *supported_protocols;
        int num_supported_protocols;
        if (XGetWMProtocols(display_,
                            e.window,
                            &supported_protocols,
                            &num_supported_protocols) &&
            (::std::find(supported_protocols,
                         supported_protocols + num_supported_protocols,
                         WM_DELETE_WINDOW) !=
             supported_protocols + num_supported_protocols))
        {
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
        }
        else
        {
            //            LOG(INFO) << "Killing window " << e.window;
            XKillClient(display_, e.window);
        }
    }
    else if ((e.state & Mod1Mask) &&
             (e.keycode == XKeysymToKeycode(display_, XK_Tab)))
    {
        // alt + tab: Switch window.
        // 1. Find next window.
        auto i = clients_.find(e.window);
        CHECK(i != clients_.end());
        ++i;
        if (i == clients_.end())
        {
            i = clients_.begin();
        }
        // 2. Raise and set focus.
        XRaiseWindow(display_, i->second);
        XSetInputFocus(display_, i->first, RevertToPointerRoot, CurrentTime);
    }
}

void WindowManager::OnKeyRelease(const XKeyEvent &e) {}

void WindowManager::OnPropertyNotify(XEvent e)
{
    XPropertyEvent *ev = (XPropertyEvent *)&e.xproperty;

    ScreenInfo *screen_info;
    Client *c;

    log("OnPropertyNotify window:0x%lx Atom:%s", ev->window, XGetAtomName(display_, ev->atom));

    c = myDisplayGetClientFromWindow(display_info, ev->window, SEARCH_WINDOW | SEARCH_WIN_USER_TIME);
    if (c)
    {
        screen_info = c->screen_info;
        if (ev->atom == XA_WM_NORMAL_HINTS)
        {
            log("client \"%s\" (0x%lx) has received a XA_WM_NORMAL_HINTS notify", c->name, c->window);
            clientGetWMNormalHints(c, TRUE);
        }
        else if ((ev->atom == XA_WM_NAME) ||
                 (ev->atom == display_info->atoms[NET_WM_NAME]) ||
                 (ev->atom == display_info->atoms[WM_CLIENT_MACHINE]))
        {
            log("client \"%s\" (0x%lx) has received a XA_WM_NAME/NET_WM_NAME/WM_CLIENT_MACHINE notify", c->name, c->window);
            clientUpdateName(c);
        }
        else if (ev->atom == display_info->atoms[MOTIF_WM_HINTS])
        {
            log("client \"%s\" (0x%lx) has received a MOTIF_WM_HINTS notify", c->name, c->window);
            clientGetMWMHints(c);
            clientApplyMWMHints(c, TRUE);
        }
        else if (ev->atom == XA_WM_HINTS)
        {
            log("client \"%s\" (0x%lx) has received a XA_WM_HINTS notify", c->name, c->window);

            /* Free previous wmhints if any */
            if (c->wmhints)
            {
                XFree(c->wmhints);
            }

            myDisplayErrorTrapPush(display_info);
            c->wmhints = XGetWMHints(display_info->dpy, c->window);
            myDisplayErrorTrapPopIgnored(display_info);

            if (c->wmhints)
            {
                if (c->wmhints->flags & WindowGroupHint)
                {
                    c->group_leader = c->wmhints->window_group;
                }
                // if ((c->wmhints->flags & IconPixmapHint) && (screen_info->params->show_app_icon))
                // {
                    // clientUpdateIcon(c);
                // }
                if (HINTS_ACCEPT_INPUT(c->wmhints))
                {
                    FLAG_SET(c->wm_flags, WM_FLAG_INPUT);
                }
                else
                {
                    FLAG_UNSET(c->wm_flags, WM_FLAG_INPUT);
                }
            }
            // clientUpdateUrgency(c);
        }
        else if (ev->atom == display_info->atoms[WM_PROTOCOLS])
        {
            log("client \"%s\" (0x%lx) has received a WM_PROTOCOLS notify", c->name, c->window);
            clientGetWMProtocols(c);
        }
        else if (ev->atom == display_info->atoms[WM_TRANSIENT_FOR])
        {
            Window w;

            log("client \"%s\" (0x%lx) has received a WM_TRANSIENT_FOR notify", c->name, c->window);
            c->transient_for = None;
            getTransientFor(display_info, c->screen_info->xroot, c->window, &w);
            // if (clientCheckTransientWindow(c, w))
            // {
            //     c->transient_for = w;
            // }
            /* Recompute window type as it may have changed */
            clientWindowType(c);
        }
        else if (ev->atom == display_info->atoms[NET_WM_WINDOW_TYPE])
        {
            log("client \"%s\" (0x%lx) has received a NET_WM_WINDOW_TYPE notify", c->name, c->window);
            clientGetNetWmType(c);
            // frameQueueDraw(c, TRUE);
        }
        else if (ev->atom == display_info->atoms[NET_WM_USER_TIME])
        {
            log("client \"%s\" (0x%lx) has received a NET_WM_USER_TIME notify", c->name, c->window);
            clientGetUserTime(c);
        }
        else if (ev->atom == display_info->atoms[NET_WM_USER_TIME_WINDOW])
        {
            log("client \"%s\" (0x%lx) has received a NET_WM_USER_TIME_WINDOW notify", c->name, c->window);
            clientRemoveUserTimeWin(c);
            c->user_time_win = getNetWMUserTimeWindow(display_info, c->window);
            clientAddUserTimeWin(c);
        }
        else if (ev->atom == display_info->atoms[NET_WM_PID])
        {
            log("client \"%s\" (0x%lx) has received a NET_WM_PID notify", c->name, c->window);
            if (c->pid == 0)
            {
                c->pid = getWindowPID(display_info, c->window);
                log("client \"%s\" (0x%lx) updated PID = %i", c->name, c->window, c->pid);
            }
        }
        else if (ev->atom == display_info->atoms[NET_WM_WINDOW_OPACITY])
        {
            log("client \"%s\" (0x%lx) has received a NET_WM_WINDOW_OPACITY notify", c->name, c->window);
            if (!getOpacity(display_info, c->window, &c->opacity))
            {
                c->opacity = NET_WM_OPAQUE;
            }
            // clientSetOpacity(c, c->opacity, 0, 0);
        }
        else if (ev->atom == display_info->atoms[NET_WM_WINDOW_OPACITY_LOCKED])
        {
            log("client \"%s\" (0x%lx) has received a NET_WM_WINDOW_OPACITY_LOCKED notify", c->name, c->window);
            if (getOpacityLock(display_info, c->window))
            {
                FLAG_SET(c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED);
            }
            else
            {
                FLAG_UNSET(c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED);
            }
        }
         else if ((ev->atom == display_info->atoms[NET_WM_ICON]))
        //           (ev->atom == display_info->atoms[KWM_WIN_ICON])))
         {
            // clientUpdateIcon(c);
         }
        return;
    }

    screen_info = myDisplayGetScreenFromWindow(display_info, ev->window);
    if (!screen_info)
    {
        return;
    }

    if (ev->atom == display_info->atoms[NET_DESKTOP_NAMES])
    {
        gchar **names;
        guint items;

        log("root has received a NET_DESKTOP_NAMES notify");
        if (getUTF8StringList(display_info, screen_info->xroot, NET_DESKTOP_NAMES, &names, &items))
        {
            // workspaceSetNames(screen_info, names, items);
        }
    }
    else if (ev->atom == display_info->atoms[NET_DESKTOP_LAYOUT])
    {
        log("root has received a NET_DESKTOP_LAYOUT notify");
        getDesktopLayout(display_info, screen_info->xroot, screen_info->workspace_count, &screen_info->desktop_layout);
        // placeSidewalks(screen_info, screen_info->params->wrap_workspaces);
    }

}

int WindowManager::OnXError(Display *display, XErrorEvent *e)
{
    if (PRINT_XERROR)
    {
        const int MAX_ERROR_TEXT_LENGTH = 1024;
        char error_text[MAX_ERROR_TEXT_LENGTH];
        XGetErrorText(display, e->error_code, error_text, sizeof(error_text));
        log("Received X error:\n");
        log("    Request: %d", int(e->request_code));
        if (e->request_code < 120)
        {
            log(" - %s \n", XRequestCodeToString(e->request_code).c_str());
        }
        log("    Error code %d: ", int(e->error_code));
        log(" - %s \n", error_text);
        log("    Resource ID: %x", e->resourceid);
        log("    serial ID: %ld", e->serial);
    }
    return 0;
}

void WindowManager::Run()
{
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
        if (composite_major > 0 || composite_minor > 2)
        {
            support_composite = true;
            initCompositor();
        }
        log("composite_major:%d  composite_minor:%d support_composite:%d", composite_major, composite_minor, support_composite);
        if (wm_detected_)
        {
            log("Detected another window manager on display %s ", XDisplayString(display_));
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
    Window *top_level_windows;
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
    for (unsigned int i = 0; i < num_top_level_windows; ++i)
    {
        log("top_level_window %x to frame", top_level_windows[i]);
        //        Frame(top_level_windows[i], true);
    }
    //     iii. Free top-level window array.
    XFree(top_level_windows);
    //   e. Ungrab X server.
    XUngrabServer(display_);

    if (CLIPMANAGER_ENABLE)
    {
        owner = XCreateSimpleWindow(display_, root_, -10, -10, 1, 1, 0, 0, 0);
        log("owner:%x", owner);
        sel = XInternAtom(display_, "CLIPBOARD", False);
        utf8 = XInternAtom(display_, "UTF8_STRING", False);
        XSetSelectionOwner(display_, sel, owner, CurrentTime);
    }

    gboolean replace_wm = FALSE;
    display_info = initialize(replace_wm, display_, back_window, root_);

    // 2. Main event loop.
    while (!stoped)
    {
        // 1. Get next event.
        XEvent e;
        XNextEvent(display_, &e);
        // log("------Received event: %s", ToString(e).c_str());
        //        log("type:%d", e.type);
        // 2. Dispatch event.
        switch (e.type)
        {
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
        case ButtonPress:
            OnButtonPress(e.xbutton);
            break;
        case ButtonRelease:
            OnButtonRelease(e.xbutton);
            break;
        case MotionNotify:
            // Skip any already pending motion events.
            while (XCheckTypedWindowEvent(
                display_, e.xmotion.window, MotionNotify, &e))
            {
            }
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
            if (CLIPMANAGER_ENABLE)
            {
                OnSelectionClear(e);
            }
            break;
        case SelectionRequest:
            if (CLIPMANAGER_ENABLE)
            {
                OnSelectionRequest(e);
            }
            break;
        case ClientMessage:
            ProcessClientMessage(e);
            // HandleClientMessage(e);
            break;
        default:
            break;
            //                log("Ignored event");
        }
    }
}

void WindowManager::ProcessClientMessage(XEvent e)
{
    ScreenInfo *screen_info;
    Client *c;
    XClientMessageEvent *ev = (XClientMessageEvent *)&e.xclient;
    log("ProcessClientMessage window (0x%lx) %s", ev->window, XGetAtomName(display_, ev->message_type));
    if (ev->window == None)
    {
        /* Some do not set the window member, not much we can do without */
        return;
    }
    c = myDisplayGetClientFromWindow (display_info, ev->window, SEARCH_WINDOW);
    if (c)
    {
        log("format:%d",ev->format)
        if ((ev->message_type == display_info->atoms[WM_CHANGE_STATE]) && (ev->format == 32) && (ev->data.l[0] == IconicState))
        {
            log("client \"%s\" (0x%lx) has received a WM_CHANGE_STATE event", c->name, c->window);
            if (!FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
            {
                clientWithdraw (c, c->win_workspace, TRUE);
            }
        }
        else if ((ev->message_type == display_info->atoms[NET_WM_DESKTOP]) && (ev->format == 32))
        {
            log("client \"%s\" (0x%lx) has received a NET_WM_DESKTOP event", c->name, c->window);
            // clientUpdateNetWmDesktop (c, ev);
        }
        else if ((ev->message_type == display_info->atoms[NET_CLOSE_WINDOW]) && (ev->format == 32))
        {
            log("client \"%s\" (0x%lx) has received a NET_CLOSE_WINDOW event", c->name, c->window);
            clientClose (c);
        }
        else if ((ev->message_type == display_info->atoms[NET_WM_STATE]) && (ev->format == 32))
        {
            //TODO operation in decoration
            log("client \"%s\" (0x%lx) has received a NET_WM_STATE event", c->name, c->window);
            int wm_action = clientUpdateNetState (c, ev);
            jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                    "updateWmStateClient", "(IJ)V");
            GlobalEnv->CallStaticVoidMethod(staticClass, method, wm_action, c->frame);
        }
        else if ((ev->message_type == display_info->atoms[NET_WM_MOVERESIZE]) && (ev->format == 32))
        {
            log("client \"%s\" (0x%lx) has received a NET_WM_MOVERESIZE event", c->name, c->window);
            //TODO operation in decoration
            clientNetMoveResize (c, ev);
        }
        else if ((ev->message_type == display_info->atoms[NET_MOVERESIZE_WINDOW]) && (ev->format == 32))
        {
            log("client \"%s\" (0x%lx) has received a NET_MOVERESIZE_WINDOW event", c->name, c->window);
            clientNetMoveResizeWindow (c, ev);
        }
        else if ((ev->message_type == display_info->atoms[NET_ACTIVE_WINDOW]) && (ev->format == 32))
        {
            log("client \"%s\" (0x%lx) has received a NET_ACTIVE_WINDOW event", c->name, c->window);
            clientHandleNetActiveWindow (c, (guint32) ev->data.l[1], (gboolean) (ev->data.l[0] == 1));
        }
        else if (ev->message_type == display_info->atoms[NET_REQUEST_FRAME_EXTENTS])
        {
            log("client \"%s\" (0x%lx) has received a NET_REQUEST_FRAME_EXTENTS event", c->name, c->window);
            // setNetFrameExtents (display_info, c->window, frameTop (c), frameLeft (c),
                                                        //  frameRight (c), frameBottom (c));
        }
        else if (ev->message_type == display_info->atoms[NET_WM_FULLSCREEN_MONITORS])
        {
            log("client \"%s\" (0x%lx) has received a NET_WM_FULLSCREEN_MONITORS event", c->name, c->window);
            // clientSetFullscreenMonitor (c, (gint) ev->data.l[0], (gint) ev->data.l[1],
                                        //    (gint) ev->data.l[2], (gint) ev->data.l[3]);
        }
        else if ((ev->message_type == display_info->atoms[GTK_SHOW_WINDOW_MENU]) && (ev->format == 32))
        {
            log("client \"%s\" (0x%lx) has received a GTK_SHOW_WINDOW_MENU event", c->name, c->window);
            // show_window_menu (c, (gint) ev->data.l[1], (gint) ev->data.l[2], Button3, (Time) myDisplayGetCurrentTime (display_info), TRUE);
        }
    }
    else
    {
        screen_info = myDisplayGetScreenFromWindow (display_info, ev->window);
        if (!screen_info)
        {
            return;
        }

        else if ((ev->message_type == display_info->atoms[MANAGER]) && (ev->format == 32))
        {
            Atom selection;

            log("window (0x%lx) has received a MANAGER event", ev->window);
            selection = (Atom) ev->data.l[1];

            if (myScreenCheckWMAtom (screen_info, selection))
            {
                log("root has received a WM_Sn selection event");
                display_info->quit = TRUE;
            }
        }
        else if (ev->message_type == display_info->atoms[WM_PROTOCOLS])
        {
            if ((Atom) ev->data.l[0] == display_info->atoms[NET_WM_PING])
            {
                log("root has received a NET_WM_PING (pong) event\n");
                clientReceiveNetWMPong (screen_info, (guint32) ev->data.l[1]);
            }
        }
    }
}

void WindowManager::HandleClientMessage(XEvent e)
{
    //    log("HandleClientMessage ---------------------------------type:%s", XGetAtomName(display_, e.xclient.message_type));
    int wm_action = WINDOW_ACTION_UNDEFINED;
    if (e.xclient.message_type == XInternAtom(display_, "WM_CHANGE_STATE", False))
    {
        long target_state = e.xclient.data.l[0];
        if (target_state == NormalState)
        {
            wm_action = WINDOW_ACTION_MINIMIZE_REMOVE;
            //            log("HandleClientMessage WM_CHANGE_STATE: Restore window to normal state.\n");
        }
        else if (target_state == IconicState)
        {
            wm_action = WINDOW_ACTION_MINIMIZE;
            //            log("HandleClientMessage WM_CHANGE_STATE: Minimize (iconify) window.\n");
        }
        else
        {
            //            log("HandleClientMessage WM_CHANGE_STATE with unknown state: %ld\n", target_state);
        }
    }
    else if (e.xclient.message_type == XInternAtom(display_, "WM_PROTOCOLS", False))
    {
        Atom wm_delete_window = XInternAtom(display_, "WM_DELETE_WINDOW", False);
        wm_action = WINDOW_ACTION_DELETE;
        if (e.xclient.data.l[0] == wm_delete_window)
        {
            //            log("HandleClientMessage WM_PROTOCOLS: Window close request.\n");
        }
        else
        {
            //            log("HandleClientMessage WM_PROTOCOLS with unknown protocol.\n");
        }
    }
    else if (e.xclient.message_type == XInternAtom(display_, "_NET_WM_STATE", False))
    {
        long action = e.xclient.data.l[0];
        long state1 = e.xclient.data.l[1];
        long state2 = e.xclient.data.l[2];
        if (action == _NET_WM_STATE_ADD)
        {
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False))
            {
                //                log("HandleClientMessage Maximize Vertically requested.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_VERT;
            }
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False))
            {
                //                log("HandleClientMessage Maximize Horizontally requested.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_HORZ;
            }
            if (wm_action == WINDOW_ACTION_MAXIMIZED_HORZ + WINDOW_ACTION_MAXIMIZED_VERT)
            {
                wm_action = WINDOW_ACTION_MAXIMIZED;
            }
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False))
            {
                //                log("HandleClientMessage Minimize requested.\n");
            }
        }
        else if (action == _NET_WM_STATE_REMOVE)
        {
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False))
            {
                //                log("HandleClientMessage Maximize Vertically removed.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_VERT;
            }
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False))
            {
                //                log("HandleClientMessage Maximize Horizontally removed.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_HORZ;
            }
            if (wm_action == WINDOW_ACTION_MAXIMIZED_HORZ + WINDOW_ACTION_MAXIMIZED_VERT)
            {
                wm_action = WINDOW_ACTION_MAXIMIZED_REMOVE;
            }
            //            log("HandleClientMessage Remove state1:%s state2:%s", XGetAtomName(display_, state1),  XGetAtomName(display_, state2));
        }
        else if (action == _NET_WM_STATE_TOGGLE)
        {
            //            log("HandleClientMessage Toggle state1:%s state2:%s", XGetAtomName(display_, state1),  XGetAtomName(display_, state2));
        }
        if (wm_action == WINDOW_ACTION_MAXIMIZED)
        {
            // setMaximizedState(e.xclient.window, true);
        }
        else if (wm_action == WINDOW_ACTION_MAXIMIZED_REMOVE)
        {
            // setMaximizedState(e.xclient.window, false);
        }
    }
    else if (e.xclient.message_type == XInternAtom(display_, "_NET_ACTIVE_WINDOW", False))
    {
        Window active_window = e.xclient.data.l[0];
        //        log("HandleClientMessage w1:%lx w2:%s w3:%lx", e.xclient.data.l[0], XGetAtomName(display_, e.xclient.data.l[1] ), e.xclient.data.l[2]);
    }
    //    log("HandleClientMessage final wm_action:%d window:%lx", wm_action, e.xclient.window);
    jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                    "updateWmStateClient", "(IJ)V");
    GlobalEnv->CallStaticVoidMethod(staticClass, method, wm_action, e.xclient.window);
}

void WindowManager::setWindowType(Window window, Atom type)
{
    Atom window_type = XInternAtom(display_, "_NET_WM_WINDOW_TYPE", False);
    XChangeProperty(display_, window, window_type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&type, 1);
}

int WindowManager::setMaximizedState(Window window, Bool maximized)
{
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
        &type, &format, &num_items, &bytes_after, &data);
    std::vector<Atom> current_atoms;
    if (status == Success && data)
    {
        Atom *atoms = reinterpret_cast<Atom *>(data);
        current_atoms.assign(atoms, atoms + num_items);
        XFree(data);
    }
    std::vector<Atom> new_atoms;
    if (maximized)
    {
        new_atoms = current_atoms;
        bool has_vert = false, has_horz = false;
        for (Atom atom : current_atoms)
        {
            if (atom == vert_max)
                has_vert = true;
            if (atom == horz_max)
                has_horz = true;
        }
        if (!has_vert)
            new_atoms.push_back(vert_max);
        if (!has_horz)
            new_atoms.push_back(horz_max);
    }
    else
    {
        for (Atom atom : current_atoms)
        {
            if (atom != vert_max && atom != horz_max)
            {
                new_atoms.push_back(atom);
            }
        }
    }
    XChangeProperty(
        display_, window, net_wm_state,
        XA_ATOM, 32, PropModeReplace,
        reinterpret_cast<unsigned char *>(new_atoms.data()), new_atoms.size());
    Atom actions_normal[] = {
        XInternAtom(display_, "_NET_WM_ACTION_MOVE", False),
        XInternAtom(display_, "_NET_WM_ACTION_RESIZE", False),
        XInternAtom(display_, "_NET_WM_ACTION_MINIMIZE", False),
        XInternAtom(display_, "_NET_WM_ACTION_SHADE", False),
        XInternAtom(display_, "_NET_WM_ACTION_MAXIMIZE_HORZ", False),
        XInternAtom(display_, "_NET_WM_ACTION_MAXIMIZE_VERT", False),
        XInternAtom(display_, "_NET_WM_ACTION_FULLSCREEN", False),
        XInternAtom(display_, "_NET_WM_ACTION_CHANGE_DESKTOP", False),
        XInternAtom(display_, "_NET_WM_ACTION_CLOSE", False)};
    Atom actions_maximized[] = {
        XInternAtom(display_, "_NET_WM_ACTION_MOVE", False),
        XInternAtom(display_, "_NET_WM_ACTION_MINIMIZE", False),
        XInternAtom(display_, "_NET_WM_ACTION_SHADE", False),
        XInternAtom(display_, "_NET_WM_ACTION_CLOSE", False)
        // 注意：移除了 RESIZE、MAXIMIZE_HORZ、MAXIMIZE_VERT，因为窗口已经最大化
    };
    if (maximized)
    {
        XChangeProperty(
            display_,
            window,
            XInternAtom(display_, "_NET_WM_ALLOWED_ACTIONS", False),
            XA_ATOM,
            32,
            PropModeReplace,
            (unsigned char *)actions_maximized,
            4);
    }
    else
    {
        XChangeProperty(
            display_,
            window,
            XInternAtom(display_, "_NET_WM_ALLOWED_ACTIONS", False),
            XA_ATOM,
            32,
            PropModeReplace,
            (unsigned char *)actions_normal,
            9);
    }
    XSync(display_, False);
    return true;
}

void WindowManager::OnSelectionRequest(XEvent e)
{
    XSelectionRequestEvent *sev = (XSelectionRequestEvent *)&e.xselectionrequest;
//    log("OnSelectionRequest start-------->");
//    log("OnSelectionRequest owner:%lx requestor:%lx ", sev->owner, sev->requestor);
    sel = XInternAtom(display_, "CLIPBOARD", False);
    utf8 = XInternAtom(display_, "UTF8_STRING", False);
    Atom targets = XInternAtom(display_, "TARGETS", False);
    Atom type_qt = XInternAtom(display_, "peony-qt/encoded-uris", False);
    Atom type_texturi = XInternAtom(display_, "text/uri-list", False);
    Atom type_plain = XInternAtom(display_, "text/plain", False);
    Atom type_text = XInternAtom(display_, "TEXT", False);
    Atom type_string = XInternAtom(display_, "STRING", False);
//    log("OnSelectionRequest target:%s property:%s", XGetAtomName(display_, sev->target), XGetAtomName(display_, sev->property));
    if (sev->target == targets)
    {
        if (selection_property_size != 0)
        {
            XChangeProperty(display_,
                            sev->requestor,
                            sev->property,
                            XA_ATOM,
                            32, PropModeReplace, (unsigned char *)selection_property_list,
                            selection_property_size);
//            log("Sending linux data to window 0x%lx, property '%s'\n", sev->requestor, XGetAtomName(display_, sev->property));
            for (int i = 0; i < selection_property_size; i++)
            {
//                log("   property:%s", XGetAtomName(display_, selection_property_list[i]));
            }
        }
        else if (!clip_text.empty())
        {
            Atom types[2] = {targets, utf8};
            XChangeProperty(display_,
                            sev->requestor,
                            sev->property,
                            XA_ATOM,
                            32, PropModeReplace, (unsigned char *)types,
                            (int)(sizeof(types) / sizeof(Atom)));
//            log("Sending clip text data to window 0x%lx, property '%s' targets & uft8 \n", sev->requestor, XGetAtomName(display_, sev->property));
        }
        else if (!file_path.empty())
        {
            Atom types[6] = {type_qt, type_texturi, type_plain, type_text, type_string, utf8};
            XChangeProperty(display_,
                            sev->requestor,
                            sev->property,
                            XA_ATOM,
                            32, PropModeReplace, (unsigned char *)types,
                            (int)(sizeof(types) / sizeof(Atom)));
//            log("Sending clip file data to window 0x%lx, property '%s'\n", sev->requestor, XGetAtomName(display_, sev->property));
            for (int i = 0; i < (int)(sizeof(types) / sizeof(Atom)); i++)
            {
//                log("   property:%s", XGetAtomName(display_, types[i]));
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
    }
    else
    {
        XSelectionEvent ssev;
        char *an;
        an = XGetAtomName(display_, sev->property);
//        log("Sending data to window 0x%lx, property '%s'\n", sev->requestor, an);
        if (!an)
        {
//            log("No data to send to window 0x%lx, property '%s'\n", sev->requestor, an);
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
//        log("data :%s actual_format:%d data:%s nitems:%lu actual_type:%s",
//            XGetAtomName(display_, sev->target), actual_format, data, nitems, XGetAtomName(display_, actual_type));
//        log("send property :%s clip_text:%s file_path:%s selection_property_size:%d", XGetAtomName(display_, sev->target), clip_text.c_str(), file_path.c_str(), selection_property_size)
        if (selection_property_size == 0)
        {
            if (!clip_text.empty())
            {
                unsigned char *text = (unsigned char *)clip_text.c_str();
                XChangeProperty(display_, sev->requestor, sev->property, utf8, 8, PropModeReplace,
                                text, clip_text.length());
            }
            else if (!file_path.empty())
            {
                if (type_qt == sev->target || type_texturi == sev->target || type_plain == sev->target || type_text == sev->target || type_string == sev->target || utf8 == sev->target)
                {
                    unsigned char *text = (unsigned char *)file_path.c_str();
                    XChangeProperty(display_, sev->requestor, sev->property, sev->target, 8,
                                    PropModeReplace,
                                    text, file_path.length());
                }
                else if (sev->target == XInternAtom(display_, "peony-qt/is-cut", False))
                {
                    XChangeProperty(display_, sev->requestor, sev->property, sev->target, 8,
                                    PropModeReplace,
                                    reinterpret_cast<const unsigned char *>((char *)"false"), 5);
                }
            }
        }
        else
        {
            XChangeProperty(display_, sev->requestor, sev->property, actual_type, actual_format,
                            PropModeReplace,
                            data, nitems);
//            log("change data to window 0x%lx, property:%s actual_type:%s actual_format:%d data:%s nitems:%d",
//                sev->requestor,
//                XGetAtomName(display_, sev->property),
//                XGetAtomName(display_, actual_type),
//                actual_format,
//                data,
//                nitems)
        }
//        log("Sending data to window 0x%lx, data: '%s'\n", sev->requestor, data);
        ssev.type = SelectionNotify;
        ssev.requestor = sev->requestor;
        ssev.selection = sev->selection;
        ssev.target = sev->target;
        ssev.property = sev->property;
        ssev.time = sev->time;
        XSendEvent(display_, sev->requestor, True, NoEventMask, (XEvent *)&ssev);
        XFlush(display_);
    }
//    log("OnSelectionRequest  end-------->");
}

void WindowManager::OnSelectionClear(XEvent e)
{
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
            sev = (XSelectionEvent *)&event.xselection;
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
                XGetWindowProperty(display_, owner, manager_prop_name, 0, 1024 * sizeof(Atom), False, XA_ATOM,
                                   &type, &di, &nitems, &dul, &prop_ret);
                log("Targets:  nitems:%lu \n", nitems);
                targets = (Atom *)prop_ret;
                selection_property_list = targets;
                selection_property_size = nitems;
                for (int index = 0; index < selection_property_size; index++)
                {
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

void WindowManager::ConvertAllTarget()
{
    XEvent event;
    XSelectionEvent *sev;
    bool isText = false;
    bool isFile = false;
    unsigned char *text_data, *file_data = nullptr;
    for (int i = 0; i < selection_property_size; i++)
    {
        log("show_data:%s\n", XGetAtomName(display_, selection_property_list[i]));
        XConvertSelection(display_, sel, selection_property_list[i], selection_property_list[i], owner, CurrentTime);
        for (;;)
        {
            XNextEvent(display_, &event);
            switch (event.type)
            {
            case SelectionNotify:
                sev = (XSelectionEvent *)&event.xselection;
                if (sev->property == None)
                {
                    log("Conversion could not be performed.\n");
                }
                else
                {
                    Atom actual_type;
                    int actual_format;
                    unsigned long nitems, bytes_after;
                    unsigned char *data = nullptr;
                    XGetWindowProperty(display_, owner, selection_property_list[i], 0, (~0L),
                                       False, AnyPropertyType,
                                       &actual_type, &actual_format, &nitems, &bytes_after, &data);
                    if (actual_format == 8)
                    { // 字符串类型
                        log("actual_type :%s Content of target: %s\n", XGetAtomName(display_, actual_type), data);
                        if (selection_property_list[i] == utf8)
                        {
                            isText = true;
                            text_data = data;
                        }
                        else if (selection_property_list[i] == XInternAtom(display_, "text/uri-list", False) || selection_property_list[i] == XInternAtom(display_, "peony-qt/encoded-uris", False))
                        {
                            isFile = true;
                            file_data = data;
                        }
                    }
                    else
                    {
                        log("Content of target (binary data or non-8-bit format):\n");
                        for (unsigned long item = 0; item < nitems; item++)
                        {
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
    if (isFile)
    {
        UpdateXserverClipFile(reinterpret_cast<const char *>(file_data));
    }
    else if (isText)
    {
        UpdateXserverCliptext(reinterpret_cast<const char *>(text_data));
    }
}

int WindowManager::moveWindow(long window, int x, int y)
{
    log("moveWindow %x: x:%d y:%d", window, x, y);
    int ret = XMoveWindow(display_, window, x, y);
    XSync(display_, False);
    return ret;
}

int WindowManager::configureWindow(long window, int x, int y, int w, int h)
{
    // setMaximizedState(window, false);
    Client *c;
    XWindowChanges changes;
    changes.x = x;
    changes.y = y;
    changes.width = w;
    changes.height = h;
    unsigned long value_mask = CWX | CWY | CWWidth | CWHeight;
    int ret;

    c = myDisplayGetClientFromWindow (display_info, window, SEARCH_FRAME);
    if (c)
    {
        log ("configureWindow \"%s\" (0x%lx) (%d, %d) %dx%d", c->name, c->window, x, y, w, h);
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING))
        {
            log ("Sorry, but it's not the right time for configure request");
            return False;
        }
        clientMoveResizeWindow (c, &changes, value_mask);
    }
    else
    {
        log ("unmanaged configureWindow for window 0x%lx", window);
        myDisplayErrorTrapPush (display_info);
        ret = XConfigureWindow (display_info->dpy, window, value_mask, &changes);
        myDisplayErrorTrapPopIgnored (display_info);
    }
    XFlush(display_);
    return ret;
}

int WindowManager::resizeWindow(long window, int w, int h)
{
    log("resizeWindow %x w:%d h:%d", window, w, h);
    int ret = XResizeWindow(display_, window, w, h);
    XSync(display_, False);
    return ret;
}

int WindowManager::unmapWindow(long window)
{
    log("unmapWindow %x ", window);
    int ret = False;
    ret = XUnmapWindow(display_, window);
    for (const auto &pair : clients_)
    {
        if (pair.second == window)
        {
            ret = XUnmapWindow(display_, pair.first);
        }
    }
    XSync(display_, False);
    return ret;
}

int WindowManager::mapWindow(long window)
{
    int ret = False;
    ret = XMapWindow(display_, window);
    XSync(display_, False);
    return ret;
}

int WindowManager::closeWindow(long frame)
{
    Client *c;
    c = myDisplayGetClientFromWindow (display_info, frame, SEARCH_FRAME);
    if(!c){
        log ("can't find frame to close window");
        return FALSE;
    }
    clientClose(c);
    return True;
}

int WindowManager::raiseWindow(long window)
{
    log("raiseWindow %x", window);
    int ret;
    ret =  XRaiseWindow(display_, window);
    Client *c;
    c = myDisplayGetClientFromWindow(display_info, window, SEARCH_FRAME);
    if(c){
        log("raiseWindow %x", c->window);
        XRaiseWindow(display_, c->window);
        XSetInputFocus(display_, c->window, RevertToPointerRoot, CurrentTime);
        clientShow(c, TRUE);
    }
    XSync(display_, false);
    return ret;
}

void WindowManager::initCompositor()
{
    XCompositeRedirectSubwindows(display_, root_, CompositeRedirectAutomatic);
    XSync(display_, false);
}

jint WindowManager::sendClipText(const char *string)
{
    std::string in_text = string;
    if (clip_text == in_text)
    {
        log("no need update clip text");
    }
    else
    {
        clip_text = in_text;
        log("update clip text :%s", clip_text.c_str());
    }
    selection_property_size = 0;
    file_path.clear();
    XSetSelectionOwner(display_, sel, owner, CurrentTime);
    XFlush(display_);
    return True;
}

void WindowManager::setClipData(char *text, char *path)
{
    if (strlen(text))
    {
        sendClipText(text);
    }
    else if (strlen(path))
    {
        sendClipFile(path);
    }
}

jint WindowManager::sendClipFile(const char *string)
{
    std::string in_text = string;
    if (file_path == in_text)
    {
        log("no need update clip file")
    }
    else
    {
        file_path = in_text;
        log("update clip file :%s", file_path.c_str())
    }
    selection_property_size = 0;
    clip_text.clear();
    XSetSelectionOwner(display_, sel, owner, CurrentTime);
    XFlush(display_);
    return True;
}

jint WindowManager::circulaSubWindows(jlong window, jboolean lowest)
{
    int ret;
    if (lowest)
    {
        ret = XCirculateSubwindows(display_, window, LowerHighest);
        log("circulaSubWindows ret:%d", ret);
    }
    else
    {
        ret = XCirculateSubwindowsUp(display_, window);
    }
    XSync(display_, False);
    return ret;
}

void WindowManager::UpdateXserverCliptext(const char *text)
{
    if (GlobalEnv && is_valid_utf8(text))
    {
        jstring utf = GlobalEnv->NewStringUTF(text);
        jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                        "updateXserverCliptext", "(Ljava/lang/String;)V");
        GlobalEnv->CallStaticVoidMethod(staticClass, method, utf);
    }
}

void WindowManager::UpdateXserverClipFile(const char *text)
{
    if (GlobalEnv && is_valid_utf8(text))
    {
        jstring utf = GlobalEnv->NewStringUTF(text);
        jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                        "updateXserverClipFile", "(Ljava/lang/String;)V");
        GlobalEnv->CallStaticVoidMethod(staticClass, method, utf);
    }
}
