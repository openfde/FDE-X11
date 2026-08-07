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

::WindowManager *WindowManager::create(char *export_display, JNIEnv *env, jclass cls, jint width,
                                       jint height, jint density)
{
    staticClass = cls;
    GlobalEnv = env;
    std::string unixstring = "unix:/tmp/.X11-unix/X";
    std::string exportstring(export_display);
    std::string display_str = unixstring + exportstring;
    Display *display = XOpenDisplay(display_str.c_str());
    if (display == nullptr)
    {
        logd("Failed to open X display");
        return nullptr;
    }
    // 2. Construct WindowManager instance.
    return new WindowManager(display, width, height, density);
}

WindowManager::WindowManager(Display *display, jint width, jint height, jint density)
    : display_(display),
        width_(width),
        height_(height),
        density_(density),
      screen_(DefaultScreen(display)),
      root_(DefaultRootWindow(display_)),
      WM_PROTOCOLS(XInternAtom(display_, "WM_PROTOCOLS", false)),
      WM_DELETE_WINDOW(XInternAtom(display_, "WM_DELETE_WINDOW", false)),
      stoped(False)
{
    logd("WindowManager::WindowManager width:%d height:%d density:%d ", width, height, density);
    decorcationview_height = density_ * decorcationview_height / 96;
    system_tray_icon_width = density_ * system_tray_icon_width / 96;
    status_bar_height = density_ * status_bar_height / 96;
    status_bar_icon_width = density_ * status_bar_icon_width / 96;
    offset_right_in_statusbar = density_ * offset_right_in_statusbar / 96;
    navigation_bar_height = density_ * navigation_bar_height / 96;
    logd("WindowManager::WindowManager %d %d %d %d %d", decorcationview_height, system_tray_icon_width,
        status_bar_height, status_bar_icon_width, offset_right_in_statusbar);
    back_window = XCreateSimpleWindow(display_, root_, 0, 0, width_, height_, 0,
                                      BlackPixel(display, screen_), WhitePixel(display, screen_));
    XMapWindow(display, back_window);
    XSetWindowBackground(display, back_window, WhitePixel(display, screen_));
}

static DisplayInfo *
initialize(gboolean replace_wm, Display *display_, Window back_window, Window root_,
           int navigation_bar_height, int status_bar_height)
{

    DisplayInfo *display_info;
    gint i, nscreens, default_screen;

    clientClearFocus(NULL);
    display_info = myDisplayInit(display_);
    display_info->dpy = display_;
    display_info->enable_compositor = compositor;
    nscreens = ScreenCount(display_info->dpy);
    default_screen = DefaultScreen(display_info->dpy);
    logd("nscreens = %d, display_info = %p", nscreens, display_info);

    for (i = 0; i < nscreens; i++)
    {
        ScreenInfo *screen_info;
        screen_info = myScreenInit(display_info, MAIN_EVENT_MASK, i, back_window, root_);
        if (screen_info == NULL)
        {
            loge("Failed to initialize screen %d", i);
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
        screen_info->navigation_bar_height = navigation_bar_height;
        screen_info->status_bar_height = status_bar_height;
        myDisplayAddScreen(display_info, screen_info);

        // setUTF8StringHint(display_info, back_window, NET_WM_NAME, "FDE-XWM");

        setNetSupportedHint(display_info, screen_info->xroot, back_window);
        logd(" display_info  %p   back_window %p ", display_info, back_window);

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
    logd("~WindowManager");
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

bool WindowManager::isNormalWindow(long window)
{
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char *propData = NULL;
    char *atomName = XGetAtomName(display_, _NET_WM_WINDOW_TYPE);
    Atom type = display_info->atoms[NET_WM_WINDOW_TYPE];
    Atom type_nomarl = display_info->atoms[NET_WM_WINDOW_TYPE_NORMAL];
    Atom type_menu = display_info->atoms[NET_WM_WINDOW_TYPE_MENU];
    Atom type_dialog = display_info->atoms[NET_WM_WINDOW_TYPE_DIALOG];
    Atom type_popup = display_info->atoms[NET_WM_WINDOW_TYPE_MENU];
    //    logd("isNormalWindow ? %lx", window);
    if (XGetWindowProperty(display_, window, type, 0, 1024, False, AnyPropertyType,
                           &actualType, &actualFormat, &nItems, &bytesAfter, &propData) ==
        Success)
    {
        //        logd(" actualType = %ld \n", actualType);
        if (actualType == XA_ATOM)
        {
            Atom *atoms = (Atom *)propData;
            for (int i = 0; i < nItems; i++)
            {
                if (atoms[i] == type_nomarl)
                {
                    continue;
                }
                else if (atoms[i] == type_menu || atoms[i] == type_dialog
                         || atoms[i] == type_popup || atoms[i] == _NET_WM_WINDOW_TYPE_TRAY)
                {
                    char *atomValue = XGetAtomName(display_, atoms[i]);
                    //                    logd("%s not normal window %lx \n", atomValue, window);
                    XFree(atomName);
                    return False;
                }
            }
        }
    }
    XFree(propData);
    return True;
}

void syncConfigureRequest(int x, int y, int w, int h, XID window, int isMoving)
{
    logd("syncConfigureRequest %d %d %d %d %lx %d", x, y, w, h, window, isMoving);
    jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                    "syncConfigureRequest", "(IIIIJI)V");
    GlobalEnv->CallStaticVoidMethod(staticClass, method, x, y, w, h, window, isMoving);
}

void updateSystemTrayIcon(jobject icon, XID window, long opcode)
{
    jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                    "updateSystemTrayIcon", "(Landroid/graphics/Bitmap;JJ)V");
    GlobalEnv->CallStaticVoidMethod(staticClass, method, icon, window, opcode);
}

void unmapWindowFromX(Window window, int action, Bool wm_delete)
{
    jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                    "unmapWindowFromX", "(IJJJIII)V");
    GlobalEnv->CallStaticVoidMethod(staticClass, method, 0,
                                    (long) 0, 0L, (long) window, action, wm_delete,
                                    0);
}

void WindowManager::OnCreateNotify(const XCreateWindowEvent &e) {}

void WindowManager::OnDestroyNotify(const XDestroyWindowEvent &ev)
{
    Client *c = myDisplayGetClientFromWindow(display_info, ev.window, SEARCH_WINDOW);
    if (c)
    {
        clientUnframe(c, FALSE);
    }

    if(dock_windows.count(ev.window)){
        Window tray = tray_window_map[ev.window];
        logd("Undock request window: %lx, tray: %lx", ev.window, tray);
        updateSystemTrayIcon(nullptr, tray, SYSTEM_TRAY_UNDOCK);
        tray_window_map.erase(ev.window);
        dock_windows.erase(ev.window);
        XDestroyWindow(display_, tray);
        XFlush(display_);
    }
    dock_windows.erase(ev.window);
}

void WindowManager::OnReparentNotify(const XReparentEvent &e) {
}

bool is_window_visible(Display *display, Window window) {
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, window, &attrs)) {
        return (attrs.map_state == IsViewable);
    }
    return false;
}

void WindowManager::OnMapNotify(const XMapEvent &e)
{
    Client *c;

    logd("OnMapNotify window (0x%lx)", e.window);

    c = myDisplayGetClientFromWindow(display_info, e.window, SEARCH_WINDOW);
    if (c)
    {
        logd("client \"%s\" (0x%lx)", c->name, c->window);
        if(c->frame && is_window_visible(display_, c->frame)){
            loge("XCompositeNameWindowPixmap window:0x%lx", c->frame);
            XCompositeNameWindowPixmap(display_, c->frame);
            XSync(display_, False);
        }
    } else {
        if (e.event == root_ && support_composite)
        {
            loge("XCompositeNameWindowPixmap window:0x%lx", e.window);
            XCompositeNameWindowPixmap(display_, e.window);
            XSync(display_, False);
//            CreateBitmapFromPixmap(GlobalEnv, display_, pixmap);
        }
    }



}

void WindowManager::OnUnmapNotify(const XUnmapEvent &ev)
{
    Client *c;
    ScreenInfo *screen_info;
    logd("OnUnmapNotify window:0x%lx event:0x%lx send:%d", ev.window, ev.event, ev.send_event);
    c = myDisplayGetClientFromWindow(display_info, ev.window, SEARCH_WINDOW);
    if (c) {
        screen_info = c->screen_info;
        if ((ev.event == screen_info->xroot) && (ev.send_event)) {
            if (!FLAG_TEST(c->xfwm_flags, XFWM_FLAG_VISIBLE)) {
                // TRACE ("ICCCM UnmapNotify for \"%s\"", c->name);
                // list_of_windows = clientListTransientOrModal (c);
                // clientPassFocus (screen_info, c, list_of_windows);
//                clientUnframe(c, FALSE);
                // g_list_free (list_of_windows);
            }


//        } else {
//            unmapWindowFromX(c->frame, ACTION_UNMAP,  True);
        } else if ((ev.event == c->frame)) {
            if (c->ignore_unmap) {
                c->ignore_unmap--;
                logd ("ignore_unmap for \"%s\" is now %i", c->name, c->ignore_unmap);
                unmapWindowFromX(c->frame, ACTION_UNMAP, True);
            } else {
                unmapWindowFromX(c->frame, ACTION_DESTORY, False);
            }
        }

    }
    if (dock_windows.count(ev.window)) {
        Window tray = tray_window_map[ev.window];
        logd("Undock request window: %lx, tray: %lx", ev.window, tray);
        updateSystemTrayIcon(nullptr, tray, SYSTEM_TRAY_UNDOCK);
        tray_window_map.erase(ev.window);
        dock_windows.erase(ev.window);
        XDestroyWindow(display_, tray);
        XFlush(display_);
    }

}

void WindowManager::OnConfigureNotify(const XConfigureEvent &e)
{
        logd("OnConfigureNotify window:%lx above:%lx", e.window, e.above);
//    if (clients_.count(e.above))
//    {
//        //        logd("OnConfigureNotify %lx", e.window);
//        configedTopWindow[e.window] = e;
//    }
}

void WindowManager::OnMapRequest(const XMapRequestEvent &e)
{
    Client *c;
    logd("window (0x%lx)", e.window);
    if (e.window == None)
    {
        logd("mapping None ???");
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
        logd("client not found for window %lx", e.window);
        clientFrame(display_info, e.window, FALSE);
    }
}


void WindowManager::OnCirculateRequest(const XCirculateRequestEvent &e)
{
    XCirculateSubwindows(display_, e.parent, e.place);
}

Atom getWindowType(Display *display, Window window) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom *prop = NULL;
    Atom type = None;

    Atom wm_window_type =  XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);

    if (XGetWindowProperty(display, window, wm_window_type,
                           0, 1, False, XA_ATOM,
                           &actual_type, &actual_format,
                           &nitems, &bytes_after,
                           (unsigned char**)&prop) == Success) {

        if (prop && nitems > 0) {
            type = prop[0];
        }

        if (prop) XFree(prop);
    }

    return type;
}

void WindowManager::OnConfigureRequest(const XConfigureRequestEvent &e)
{
    Client *c;
    XWindowChanges changes;
    bool normal = isNormalWindow(e.window);
    changes.x = e.x;
    changes.y = (e.y < decorcationview_height && normal) ? decorcationview_height : e.y;
    changes.width = e.width;
    changes.height = e.height;
    changes.border_width = e.border_width;
    changes.sibling = e.above;
    changes.stack_mode = e.detail;
    unsigned long value_mask = e.value_mask;
    if (e.y < decorcationview_height)
    {
        value_mask = e.value_mask | CWY;
        //        logd("value_mask : %lu", value_mask);
    }

    logd("OnConfigureRequest x:%d y:%d w:%d h:%d border:%d above:%d stack:%d value:%d",
         e.x, e.y, e.width, e.height, e.border_width, e.above, e.detail, value_mask);
    XConfigureRequestEvent *ev = (XConfigureRequestEvent *)&e;

    int isDockWindow = dock_windows.count(ev->window);
    if(isDockWindow){
        int offsetx = (system_tray_icon_width - e.width) / 2;
        int offsety = (system_tray_icon_width - e.height) / 2;
        value_mask = CWWidth | CWHeight | CWX | CWY;
        changes.x = offsetx;
        changes.y = offsety;
    }
    c = myDisplayGetClientFromWindow (display_info, ev->window, SEARCH_WINDOW);
    if (c)
    {
        changes.x = c->x;
        changes.y = c->y;
        logd ("OnConfigureRequest \"%s\" (0x%lx) x:%d y:%d e.x:%d e.y:%d", c->name, c->window, c->x, c->y, e.x, e.y);
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
        logd ("unmanaged OnConfigureRequest for window 0x%lx", ev->window);
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
    logd("OnButtonPress  window:%lx x_root:%d y_root:%d state:0x%X type:%d x:%d y:%d send_event:%d",
        e.window, e.x_root, e.y_root, e.state, e.type, e.x, e.y, e.send_event);
    raiseWindow(e.window);
}

void WindowManager::OnButtonRelease(const XButtonEvent &e) {
    logd("OnButtonRelease  window:%lx x_root:%d y_root:%d state:0x%X type:%d x:%d y:%d send_event:%d",
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
    logd("OnButtonRelease clear passdata.c");
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
    logd("OnMotionNotify  window:%lx x_root:%d y_root:%d state:0x%X type:%d x:%d y:%d send_event:%d",
        e.window, e.x_root, e.y_root, e.state, e.type, e.x, e.y, e.send_event);
    ScreenInfo *screen_info;
    screen_info = myDisplayGetScreenFromWindow(display_info, e.window);
    if (!screen_info || !screen_info->passdata.c)
    {
        return;
    }
    logd("OnMotionNotify_mx:%d my:%d  ox:%d oy:%d ow:%d oh:%d oldw:%d oldh:%d cancel_x:%d cancel_y:%d",
        screen_info->passdata.mx, screen_info->passdata.my,
        screen_info->passdata.ox, screen_info->passdata.oy,
        screen_info->passdata.ow, screen_info->passdata.oh,
        screen_info->passdata.oldw, screen_info->passdata.oldh,
        screen_info->passdata.cancel_x, screen_info->passdata.cancel_y
    );
    Client *c = screen_info->passdata.c;
    logd("OnMotionNotify width:%d height:%d", c->width, c->height);
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
        logd("OnMotionNotify_window:%lx frame:%lx final_x:%d final_y:%d", c->window, c->frame, final_x, final_y);
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

    logd("OnPropertyNotify window:0x%lx Atom:%s", ev->window, XGetAtomName(display_, ev->atom));

    c = myDisplayGetClientFromWindow(display_info, ev->window, SEARCH_WINDOW | SEARCH_WIN_USER_TIME);
    if (c)
    {
        screen_info = c->screen_info;
        if (ev->atom == XA_WM_NORMAL_HINTS)
        {
            logd("client \"%s\" (0x%lx) has received a XA_WM_NORMAL_HINTS notify", c->name, c->window);
            clientGetWMNormalHints(c, TRUE);
        }
        else if ((ev->atom == XA_WM_NAME) ||
                 (ev->atom == display_info->atoms[NET_WM_NAME]) ||
                 (ev->atom == display_info->atoms[WM_CLIENT_MACHINE]))
        {
            logd("client \"%s\" (0x%lx) has received a XA_WM_NAME/NET_WM_NAME/WM_CLIENT_MACHINE notify", c->name, c->window);
            clientUpdateName(c);
        }
        else if (ev->atom == display_info->atoms[MOTIF_WM_HINTS])
        {
            logd("client \"%s\" (0x%lx) has received a MOTIF_WM_HINTS notify", c->name, c->window);
            clientGetMWMHints(c);
            clientApplyMWMHints(c, TRUE);
        }
        else if (ev->atom == XA_WM_HINTS)
        {
            logd("client \"%s\" (0x%lx) has received a XA_WM_HINTS notify", c->name, c->window);

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
            logd("client \"%s\" (0x%lx) has received a WM_PROTOCOLS notify", c->name, c->window);
            clientGetWMProtocols(c);
        }
        else if (ev->atom == display_info->atoms[WM_TRANSIENT_FOR])
        {
            Window w;

            logd("client \"%s\" (0x%lx) has received a WM_TRANSIENT_FOR notify", c->name, c->window);
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
            logd("client \"%s\" (0x%lx) has received a NET_WM_WINDOW_TYPE notify", c->name, c->window);
            clientGetNetWmType(c);
            // frameQueueDraw(c, TRUE);
        }
        else if (ev->atom == display_info->atoms[NET_WM_USER_TIME])
        {
            logd("client \"%s\" (0x%lx) has received a NET_WM_USER_TIME notify", c->name, c->window);
            clientGetUserTime(c);
        }
        else if (ev->atom == display_info->atoms[NET_WM_USER_TIME_WINDOW])
        {
            logd("client \"%s\" (0x%lx) has received a NET_WM_USER_TIME_WINDOW notify", c->name, c->window);
            clientRemoveUserTimeWin(c);
            c->user_time_win = getNetWMUserTimeWindow(display_info, c->window);
            clientAddUserTimeWin(c);
        }
        else if (ev->atom == display_info->atoms[NET_WM_PID])
        {
            logd("client \"%s\" (0x%lx) has received a NET_WM_PID notify", c->name, c->window);
            if (c->pid == 0)
            {
                c->pid = getWindowPID(display_info, c->window);
                logd("client \"%s\" (0x%lx) updated PID = %i", c->name, c->window, c->pid);
            }
        }
        else if (ev->atom == display_info->atoms[NET_WM_WINDOW_OPACITY])
        {
            logd("client \"%s\" (0x%lx) has received a NET_WM_WINDOW_OPACITY notify", c->name, c->window);
            if (!getOpacity(display_info, c->window, &c->opacity))
            {
                c->opacity = NET_WM_OPAQUE;
            }
            // clientSetOpacity(c, c->opacity, 0, 0);
        }
        else if (ev->atom == display_info->atoms[NET_WM_WINDOW_OPACITY_LOCKED])
        {
            logd("client \"%s\" (0x%lx) has received a NET_WM_WINDOW_OPACITY_LOCKED notify", c->name, c->window);
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
        else if (ev->atom == display_info->atoms[NET_WM_STATE])
        {
            logd("client \"%s\" (0x%lx) has received a NET_WM_STATE notify", c->name, c->window);

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

        logd("root has received a NET_DESKTOP_NAMES notify");
        if (getUTF8StringList(display_info, screen_info->xroot, NET_DESKTOP_NAMES, &names, &items))
        {
            // workspaceSetNames(screen_info, names, items);
        }
    }
    else if (ev->atom == display_info->atoms[NET_DESKTOP_LAYOUT])
    {
        logd("root has received a NET_DESKTOP_LAYOUT notify");
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
        loge("Received X error:\n");
        loge("    Request: %d", int(e->request_code));
        if (e->request_code < 120)
        {
            loge(" - %s \n", XRequestCodeToString(e->request_code).c_str());
        }
        loge("    Error code %d: ", int(e->error_code));
        loge(" - %s \n", error_text);
        loge("    Resource ID: %x", e->resourceid);
        loge("    serial ID: %ld", e->serial);
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
        logd("composite_major:%d  composite_minor:%d support_composite:%d", composite_major, composite_minor, support_composite);
        if (wm_detected_)
        {
            loge("Detected another window manager on display %s ", XDisplayString(display_));
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
        logd("top_level_window %x to frame", top_level_windows[i]);
        //        Frame(top_level_windows[i], true);
    }
    //     iii. Free top-level window array.
    XFree(top_level_windows);
    //   e. Ungrab X server.
    XUngrabServer(display_);

    if (CLIPMANAGER_ENABLE)
    {
        owner = XCreateSimpleWindow(display_, root_, -10, -10, 1, 1, 0, 0, 0);
        logd("owner:%x", owner);
        sel = XInternAtom(display_, "CLIPBOARD", False);
        utf8 = XInternAtom(display_, "UTF8_STRING", False);
        XSetSelectionOwner(display_, sel, owner, CurrentTime);
    }

    gboolean replace_wm = FALSE;
    display_info = initialize(replace_wm, display_, back_window, root_, navigation_bar_height,
                              status_bar_height);


        char resource_data[1024];
    snprintf(resource_data, sizeof(resource_data),
             "Xft.dpi:\t%d\n"
             "Xcursor.size:\t%d\n"
             "Xcursor.theme:\tdark-sense\n"
             "Xft.antialias:\t1\n"
             "Xft.hinting:\t1\n"
             "Xft.hintstyle:\thintslight\n"
             "Xft.rgba:\trgb\n"
             "Xft.lcdfilter:\tlcddefault\n",
             density_, density_ / 4);
    // 设置属性
    if (SetRootResourceManager(display_, resource_data) == 0) {
        logd("RESOURCE_MANAGER属性设置成功");
    } else {
        loge("RESOURCE_MANAGER属性设置失败");
    }

    // 2. Main event loop.
    while (!stoped)
    {
        // 1. Get next event.
        XEvent e;
        XNextEvent(display_, &e);
        logd("------Received event: %s", ToString(e).c_str());
        //        logd("type:%d", e.type);
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
                case SelectionNotify:
                if (CLIPMANAGER_ENABLE)
                {
                    OnSelectionNotify(e);
                }
                break;
            case ClientMessage:
                ProcessClientMessage(e);
                // HandleClientMessage(e);
                break;
            default:
                break;
                //                logd("Ignored event");
        }
    }
}

void WindowManager::ProcessClientMessage(XEvent e)
{
    ScreenInfo *screen_info;
    Client *c;
    XClientMessageEvent *ev = (XClientMessageEvent *)&e.xclient;
    logd("ProcessClientMessage window (0x%lx) %s", ev->window, XGetAtomName(display_, ev->message_type));
    if (ev->window == None)
    {
        /* Some do not set the window member, not much we can do without */
        return;
    }
    c = myDisplayGetClientFromWindow (display_info, ev->window, SEARCH_WINDOW);
    if (c)
    {
        logd("format:%d",ev->format)
        if ((ev->message_type == display_info->atoms[WM_CHANGE_STATE]) && (ev->format == 32) && (ev->data.l[0] == IconicState))
        {
            logd("client \"%s\" (0x%lx) has received a WM_CHANGE_STATE event", c->name, c->window);
            if (!FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
            {
                clientWithdraw (c, c->win_workspace, TRUE);
            }
        }
        else if ((ev->message_type == display_info->atoms[NET_WM_DESKTOP]) && (ev->format == 32))
        {
            logd("client \"%s\" (0x%lx) has received a NET_WM_DESKTOP event", c->name, c->window);
            // clientUpdateNetWmDesktop (c, ev);
        }
        else if ((ev->message_type == display_info->atoms[NET_CLOSE_WINDOW]) && (ev->format == 32))
        {
            logd("client \"%s\" (0x%lx) has received a NET_CLOSE_WINDOW event", c->name, c->window);
            clientClose (c);
        }
        else if ((ev->message_type == display_info->atoms[NET_WM_STATE]) && (ev->format == 32))
        {

            Atom a1 = (Atom)ev->data.l[1];
            Atom a2 = (Atom)ev->data.l[2];
            bool fullscreen =
                a1 == display_info->atoms[NET_WM_STATE_FULLSCREEN] ||
                a2 == display_info->atoms[NET_WM_STATE_FULLSCREEN];
            //TODO operation in decoration
            int wm_action = clientUpdateNetState (c, ev);
            if(fullscreen){
                wm_action = WINDOW_ACTION_FULLSCREEN;
            }
            logd("NET_WM_STATE_FULLSCREEN window (0x%lx) a1:%s a2:%s wm_action:%d", ev->window, XGetAtomName(display_, a1),
                XGetAtomName(display_, a2), wm_action);
            if(fullscreen){
                setMaximizedState(ev->window, TRUE);
            } else if(wm_action == WINDOW_ACTION_MAXIMIZED_REMOVE){
                setMaximizedState(ev->window, FALSE);
            } else {
                setMaximizedState(ev->window, TRUE);
            }
            logd("client \"%s\" (0x%lx) has received a NET_WM_STATE event action:%d", c->name, c->window, wm_action);
            jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                            "updateWmStateClient", "(IJ)V");
            GlobalEnv->CallStaticVoidMethod(staticClass, method, wm_action, c->frame);
        }
        else if ((ev->message_type == display_info->atoms[NET_WM_MOVERESIZE]) && (ev->format == 32))
        {
            logd("client \"%s\" (0x%lx) has received a NET_WM_MOVERESIZE event", c->name, c->window);
            //TODO operation in decoration
            clientNetMoveResize (c, ev);
            syncConfigureRequest(0, 0, 0, 0, c->frame, true);
        }
        else if ((ev->message_type == display_info->atoms[NET_MOVERESIZE_WINDOW]) && (ev->format == 32))
        {
            logd("client \"%s\" (0x%lx) has received a NET_MOVERESIZE_WINDOW event", c->name, c->window);
            clientNetMoveResizeWindow (c, ev);
        }
        else if ((ev->message_type == display_info->atoms[NET_ACTIVE_WINDOW]) && (ev->format == 32))
        {
            logd("client \"%s\" (0x%lx) has received a NET_ACTIVE_WINDOW event", c->name, c->window);
            clientHandleNetActiveWindow (c, (guint32) ev->data.l[1], (gboolean) (ev->data.l[0] == 1));
        }
        else if (ev->message_type == display_info->atoms[NET_REQUEST_FRAME_EXTENTS])
        {
            logd("client \"%s\" (0x%lx) has received a NET_REQUEST_FRAME_EXTENTS event", c->name, c->window);
            // setNetFrameExtents (display_info, c->window, frameTop (c), frameLeft (c),
            //  frameRight (c), frameBottom (c));
        }
        else if (ev->message_type == display_info->atoms[NET_WM_FULLSCREEN_MONITORS])
        {
            logd("client \"%s\" (0x%lx) has received a NET_WM_FULLSCREEN_MONITORS event", c->name, c->window);
            // clientSetFullscreenMonitor (c, (gint) ev->data.l[0], (gint) ev->data.l[1],
            //    (gint) ev->data.l[2], (gint) ev->data.l[3]);
        }
        else if ((ev->message_type == display_info->atoms[GTK_SHOW_WINDOW_MENU]) && (ev->format == 32))
        {
            logd("client \"%s\" (0x%lx) has received a GTK_SHOW_WINDOW_MENU event", c->name, c->window);
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

            logd("window (0x%lx) has received a MANAGER event", ev->window);
            selection = (Atom) ev->data.l[1];

            if (myScreenCheckWMAtom (screen_info, selection))
            {
                logd("root has received a WM_Sn selection event");
                display_info->quit = TRUE;
            }
        }
        else if (ev->message_type == display_info->atoms[WM_PROTOCOLS])
        {
            if ((Atom) ev->data.l[0] == display_info->atoms[NET_WM_PING])
            {
                logd("root has received a NET_WM_PING (pong) event\n");
                clientReceiveNetWMPong (screen_info, (guint32) ev->data.l[1]);
            }
        }
        else if(ev->message_type == display_info->atoms[NET_SYSTEM_TRAY_OPCODE])
        {
            if(SYSTEM_TRAY_ENABLE && !system_tray){
                XSetWindowAttributes attributes;
                attributes.background_pixel = 0x80808080;

                system_tray = XCreateWindow(display_, root_,
                    width_ - offset_right_in_statusbar - SYSTEM_TRAY_CAPACITY * system_tray_icon_width, 0,
                    SYSTEM_TRAY_CAPACITY * system_tray_icon_width, system_tray_icon_width,
                              0,
                              CopyFromParent,
                              InputOutput,
                              CopyFromParent,
                              CWBackPixel,
                              &attributes);
//                system_tray = XCreateSimpleWindow(display_, root_,
//                              WIDTH - 254 - SYSTEM_TRAY_CAPACITY * SYSTEM_TRAY_ICON_WIDTH, 0,
//                              SYSTEM_TRAY_CAPACITY * SYSTEM_TRAY_ICON_WIDTH, SYSTEM_TRAY_ICON_WIDTH, 0, 0, 0);
                MotifWmHints hints;
                Atom motif_hints_atom = display_info->atoms[MOTIF_WM_HINTS];
                hints.flags = MWM_HINTS_DECORATIONS;
                hints.functions = 2;
                hints.decorations = 0;
                hints.input_mode = 0;
                hints.status = 0;
                XChangeProperty(display_, system_tray, motif_hints_atom, motif_hints_atom, 32,
                                PropModeReplace, (unsigned char*)&hints, 5);
                Atom prop_atom = display_info->atoms[NET_WM_WINDOW_TYPE];
                Atom type_atom;
                type_atom = _NET_WM_WINDOW_TYPE_TRAY;
                XChangeProperty(display_, system_tray, prop_atom, XA_ATOM, 32,
                                PropModeReplace, (unsigned char*)&type_atom, 1);
                XMapWindow(display_, system_tray);
                    Atom utf8_string = display_info->atoms[UTF8_STRING];
                    Atom net_wm_name = display_info->atoms[NET_WM_NAME];
                    XChangeProperty(display_, system_tray, net_wm_name, utf8_string, 8,
                                    PropModeReplace, (unsigned char*)"x11_tray",
                                    strlen("x11_tray"));
            }
            HandleSystemTrayClientMessage(screen_info, ev);
        }
    }
}

void WindowManager::HandleSystemTrayClientMessage( ScreenInfo *screen_info, XClientMessageEvent *ev) {
    long *data = ev->data.l;
    long timestamp = data[0];
    long opcode = data[1];
    switch (opcode) {
        case SYSTEM_TRAY_REQUEST_DOCK: {
            Window dock_window = data[2];
            logd("Received DOCK request from client. Icon window: %lx  tray window: %lx",
                dock_window, screen_info->systray);
            dock_windows.insert(dock_window);
            size_t dock_icon_count = dock_windows.size();
            if (dock_icon_count) {
                ReparentDockWindow(dock_window);
            }
            break;
        }
        case SYSTEM_TRAY_BEGIN_MESSAGE:
            logd("Begin message: timeout=%ld, length=%ld, id=%ld\n", data[2], data[3], data[4]);
            break;
        case SYSTEM_TRAY_CANCEL_MESSAGE:
            logd("Cancel message: id=%ld\n", data[2]);
            break;
        case SYSTEM_TRAY_UNDOCK: {
            Window window = data[2];
            dock_windows.erase(window);
            if(tray_window_map.count(window)){
                Window tray = tray_window_map[window];
                logd("Undock request window: %lx, tray: %lx", window, tray);
                updateSystemTrayIcon(nullptr, tray, opcode);
                tray_window_map.erase(window);
                XUnmapWindow(display_, tray);
//                XDestroyWindow(display_, window);
                XDestroyWindow(display_, tray);
                XSync(display_, False);
            }
            break;
        }
        default:
            logd("Unknown opcode received: %ld\n", opcode);
            break;
    }
}

char* get_net_wm_name(Display *dpy, Window win_b) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop_value = nullptr;
    Atom prop_atom = XInternAtom(dpy, "_NET_WM_NAME", False);
    char *wm_name = nullptr;

    int result = XGetWindowProperty(dpy, win_b, prop_atom, 0, 1024, False,
                                    XInternAtom(dpy, "UTF8_STRING", False),
                                    &actual_type, &actual_format,
                                    &nitems, &bytes_after, &prop_value);

    if (result == Success && actual_type != None && nitems > 0) {
        wm_name = strdup((char*)prop_value);
    }
    XFree(prop_value);
    return wm_name;
}

void WindowManager::ReparentDockWindow(Window window)
{
    XWindowAttributes attrs;
    char *target_wm_name = nullptr;
    if (XGetWindowAttributes(display_, window, &attrs)) {
        XSetWindowAttributes tray_attr;
        tray_attr.background_pixel = 0xC0C0C0;
        Window tray = XCreateWindow(display_, root_,
                                    width_ - offset_right_in_statusbar - dock_windows.size() * status_bar_icon_width +
                                    (status_bar_icon_width - system_tray_icon_width) / 2,
                                    (status_bar_height - system_tray_icon_width) / 2,
                                    system_tray_icon_width, system_tray_icon_width,
                               0,
                               CopyFromParent,
                               InputOutput,
                               CopyFromParent,
                               CWBackPixel,
                               &tray_attr);
        long event_mask = BASE_EVENT_MASK;
        XSelectInput(display_, window, event_mask);
        target_wm_name = get_net_wm_name(display_, window);
        logd("ReparentDockWindow tray window：%lx  x:(%zu) y:(%d) w:(%d) h:(%d) netwmname:%s",
            tray,(WIDTH - 254 - dock_windows.size() * system_tray_icon_width), 0,
            system_tray_icon_width, system_tray_icon_width, target_wm_name)
        if (target_wm_name) {
            Atom utf8_string = display_info->atoms[UTF8_STRING];
            Atom net_wm_name = display_info->atoms[NET_WM_NAME];
            XChangeProperty(display_, tray, net_wm_name, utf8_string, 8,
                            PropModeReplace, (unsigned char*)target_wm_name,
                            strlen(target_wm_name));
            XFlush(display_);
            XStoreName(display_, tray, target_wm_name);
            free(target_wm_name);
        } else {
            XStoreName(display_, tray, "tray");
        }
        MotifWmHints hints;
        Atom motif_hints_atom = display_info->atoms[MOTIF_WM_HINTS];
        hints.flags = MWM_HINTS_DECORATIONS;
        hints.functions = 0;
        hints.decorations = 0;
        hints.input_mode = 0;
        hints.status = 0;
        XChangeProperty(display_, tray, motif_hints_atom, motif_hints_atom, 32,
                        PropModeReplace, (unsigned char*)&hints, 5);

        Atom prop_atom = display_info->atoms[NET_WM_WINDOW_TYPE];
        Atom type_atom;
        type_atom = _NET_WM_WINDOW_TYPE_TRAY;

        XChangeProperty(display_, tray, prop_atom, XA_ATOM, 32,
                        PropModeReplace, (unsigned char*)&type_atom, 1);
        dock_trays.insert(tray);
//        int offsetx = (SYSTEM_TRAY_ICON_WIDTH - attrs.width) / 2;
//        int offsety = (SYSTEM_TRAY_ICON_WIDTH - attrs.height) / 2;

        XWindowChanges changes;
        changes.width = system_tray_icon_width;
        changes.height = system_tray_icon_width;
        XConfigureWindow(display_, window, CWWidth | CWHeight, &changes);
        XReparentWindow(display_, window, tray,
                        0, 0);
        XMapWindow(display_, window);
        XMapWindow(display_, tray);
        tray_window_map[window] = tray;
        logd("ReparentDockWindow dock window:%lx to tray:%lx  ", window, tray);
    }
}

//jobject WindowManager::GetWindowIcon(Window target_window) {
//    XWindowAttributes attrs;
//    jobject bitmap = nullptr;
//
//    if (!XGetWindowAttributes(display_, target_window, &attrs)) {
//        logd("Window does not exist or cannot be accessed");
//        return bitmap;
//    }
//    Atom net_wm_icon = display_info->atoms[NET_WM_ICON];
//    Atom actual_type;
//    int actual_format;
//    unsigned long nitems;
//    unsigned long bytes_after;
//    unsigned char *data = nullptr;
//
//    int status = XGetWindowProperty(display_, target_window, net_wm_icon,
//                                    0, LONG_MAX, False, XA_CARDINAL,
//                                    &actual_type, &actual_format,
//                                    &nitems, &bytes_after, &data);
//
//    if (status == Success && data && nitems > 0) {
//        logd("Found _NET_WM_ICON property, items: %lu", nitems);
//        bitmap = CreateBitmapFromNetWmIcon(  data, nitems);
//        XFree(data);
//    }
//    return bitmap;
//}

//jobject WindowManager::CreateBitmapFromPixmap(JNIEnv *env, Display *display, Pixmap pixmap) {
//    if (pixmap == None) {
//        return nullptr;
//    }
//
//    XWindowAttributes pix_attrs;
//    if (!XGetWindowAttributes(display, pixmap, &pix_attrs)) {
//        logd("Failed to get pixmap attributes");
//        return nullptr;
//    }
//
//    // 获取 pixmap 数据
//    XImage *image = XGetImage(display, pixmap, 0, 0,
//                              pix_attrs.width, pix_attrs.height,
//                              AllPlanes, ZPixmap);
//
//    if (!image) {
//        logd("Failed to get image from pixmap");
//        return nullptr;
//    }
//
//    jobject bitmap = CreateBitmapFromXImage(env, image);
//    XDestroyImage(image);
//    return bitmap;
//}

// 从 XImage 创建 Bitmap
//jobject WindowManager::CreateBitmapFromXImage(JNIEnv *env, XImage *image) {
//    int width = image->width;
//    int height = image->height;
//
//    jclass bitmap_class = env->FindClass("android/graphics/Bitmap");
//    jmethodID create_bitmap = env->GetStaticMethodID(bitmap_class,
//                                                     "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
//
//    jclass config_class = env->FindClass("android/graphics/Bitmap$Config");
//    jfieldID argb_8888_field = env->GetStaticFieldID(config_class, "ARGB_8888",
//                                                     "Landroid/graphics/Bitmap$Config;");
//    jobject config = env->GetStaticObjectField(config_class, argb_8888_field);
//
//    jobject bitmap = env->CallStaticObjectMethod(bitmap_class, create_bitmap,
//                                                 width, height, config);
//
//    AndroidBitmapInfo info;
//    void *pixels;
//    if (AndroidBitmap_getInfo(env, bitmap, &info) == ANDROID_BITMAP_RESULT_SUCCESS &&
//        AndroidBitmap_lockPixels(env, bitmap, &pixels) == ANDROID_BITMAP_RESULT_SUCCESS) {
//
//        uint32_t *dest = (uint32_t *)pixels;
//
//        for (int y = 0; y < height; y++) {
//            for (int x = 0; x < width; x++) {
//                unsigned long pixel = XGetPixel(image, x, y);
//                uint32_t android_pixel = ConvertPixelToARGB(pixel, image->depth, image->byte_order);
//                dest[y * width + x] = android_pixel;
//            }
//        }
//
//        AndroidBitmap_unlockPixels(env, bitmap);
//    }
//
//    return bitmap;
//}

//uint32_t WindowManager::ConvertPixelToARGB(unsigned long pixel, int depth, int byte_order) {
//    if (depth == 24 || depth == 32) {
//        if (byte_order == LSBFirst) {
//            return ((pixel & 0xFF000000) >> 24) |  // A
//                   ((pixel & 0x00FF0000) >> 8)  |  // R
//                   ((pixel & 0x0000FF00) << 8)  |  // G
//                   ((pixel & 0x000000FF) << 24);   // B
//        } else {
//            return pixel;
//        }
//    } else if (depth == 1) {
//        return pixel ? 0xFFFFFFFF : 0xFF000000;
//    }
//
//    return 0xFF000000; // 默认黑色
//}

//jobject WindowManager::CreateBitmapFromNetWmIcon(unsigned char *data, unsigned long nitems)
//{
//    if (nitems < 2) {
//        logd("Invalid _NET_WM_ICON data");
//        return nullptr;
//    }
//    unsigned long *long_data = (unsigned long *)data;
//    int width = (int)long_data[0];
//    int height = (int)long_data[1];
//    if (nitems < (unsigned long)(2 + width * height)) {
//        logd("Incomplete _NET_WM_ICON data");
//        return nullptr;
//    }
//    logd("Creating bitmap from _NET_WM_ICON: %dx%d", width, height);
//    jclass bitmap_class = GlobalEnv->FindClass("android/graphics/Bitmap");
//    jmethodID create_bitmap = GlobalEnv->GetStaticMethodID(bitmap_class,
//                                                           "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
//    jclass config_class = GlobalEnv->FindClass("android/graphics/Bitmap$Config");
//    jfieldID argb_8888_field = GlobalEnv->GetStaticFieldID(config_class, "ARGB_8888",
//                                                           "Landroid/graphics/Bitmap$Config;");
//    jobject config = GlobalEnv->GetStaticObjectField(config_class, argb_8888_field);
//
//    jobject bitmap = GlobalEnv->CallStaticObjectMethod(bitmap_class, create_bitmap,
//                                                       width, height, config);
//    AndroidBitmapInfo info;
//    void *pixels;
//    if (AndroidBitmap_getInfo(GlobalEnv, bitmap, &info) == ANDROID_BITMAP_RESULT_SUCCESS &&
//        AndroidBitmap_lockPixels(GlobalEnv, bitmap, &pixels) == ANDROID_BITMAP_RESULT_SUCCESS) {
//        uint32_t *dest = (uint32_t *)pixels;
//        unsigned long *src = long_data + 2;
//        for (int y = 0; y < height; y++) {
//            for (int x = 0; x < width; x++) {
//                unsigned long pixel = src[y * width + x];
//                uint32_t android_pixel =
//                        ((pixel & 0xFF000000) >> 24) |  // A
//                        ((pixel & 0x00FF0000) >> 8)  |  // R
//                        ((pixel & 0x0000FF00) << 8)  |  // G
//                        ((pixel & 0x000000FF) << 24);   // B
//                dest[y * width + x] = android_pixel;
//            }
//        }
//        AndroidBitmap_unlockPixels(GlobalEnv, bitmap);
//    }
//    return bitmap;
//}

void WindowManager::HandleClientMessage(XEvent e)
{
    //    logd("HandleClientMessage ---------------------------------type:%s", XGetAtomName(display_, e.xclient.message_type));
    int wm_action = WINDOW_ACTION_UNDEFINED;
    if (e.xclient.message_type == XInternAtom(display_, "WM_CHANGE_STATE", False))
    {
        long target_state = e.xclient.data.l[0];
        if (target_state == NormalState)
        {
            wm_action = WINDOW_ACTION_MINIMIZE_REMOVE;
            //            logd("HandleClientMessage WM_CHANGE_STATE: Restore window to normal state.\n");
        }
        else if (target_state == IconicState)
        {
            wm_action = WINDOW_ACTION_MINIMIZE;
            //            logd("HandleClientMessage WM_CHANGE_STATE: Minimize (iconify) window.\n");
        }
        else
        {
            //            logd("HandleClientMessage WM_CHANGE_STATE with unknown state: %ld\n", target_state);
        }
    }
    else if (e.xclient.message_type == XInternAtom(display_, "WM_PROTOCOLS", False))
    {
        Atom wm_delete_window = XInternAtom(display_, "WM_DELETE_WINDOW", False);
        wm_action = WINDOW_ACTION_DELETE;
        if (e.xclient.data.l[0] == wm_delete_window)
        {
            //            logd("HandleClientMessage WM_PROTOCOLS: Window close request.\n");
        }
        else
        {
            //            logd("HandleClientMessage WM_PROTOCOLS with unknown protocol.\n");
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
                //                logd("HandleClientMessage Maximize Vertically requested.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_VERT;
            }
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False))
            {
                //                logd("HandleClientMessage Maximize Horizontally requested.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_HORZ;
            }
            if (wm_action == WINDOW_ACTION_MAXIMIZED_HORZ + WINDOW_ACTION_MAXIMIZED_VERT)
            {
                wm_action = WINDOW_ACTION_MAXIMIZED;
            }
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False))
            {
                //                logd("HandleClientMessage Minimize requested.\n");
            }
        }
        else if (action == _NET_WM_STATE_REMOVE)
        {
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False))
            {
                //                logd("HandleClientMessage Maximize Vertically removed.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_VERT;
            }
            if (state1 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False) ||
                state2 == XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False))
            {
                //                logd("HandleClientMessage Maximize Horizontally removed.\n");
                wm_action |= WINDOW_ACTION_MAXIMIZED_HORZ;
            }
            if (wm_action == WINDOW_ACTION_MAXIMIZED_HORZ + WINDOW_ACTION_MAXIMIZED_VERT)
            {
                wm_action = WINDOW_ACTION_MAXIMIZED_REMOVE;
            }
            //            logd("HandleClientMessage Remove state1:%s state2:%s", XGetAtomName(display_, state1),  XGetAtomName(display_, state2));
        }
        else if (action == _NET_WM_STATE_TOGGLE)
        {
            //            logd("HandleClientMessage Toggle state1:%s state2:%s", XGetAtomName(display_, state1),  XGetAtomName(display_, state2));
        }
        if (wm_action == WINDOW_ACTION_MAXIMIZED)
        {
        }
        else if (wm_action == WINDOW_ACTION_MAXIMIZED_REMOVE)
        {
        }
    }
    else if (e.xclient.message_type == display_info->atoms[NET_ACTIVE_WINDOW])
    {
        Window active_window = e.xclient.data.l[0];
        //        logd("HandleClientMessage w1:%lx w2:%s w3:%lx", e.xclient.data.l[0], XGetAtomName(display_, e.xclient.data.l[1] ), e.xclient.data.l[2]);
    }
    //    logd("HandleClientMessage final wm_action:%d window:%lx", wm_action, e.xclient.window);
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
    logd("setMaximizedState window:%lx maximized:%d", window, maximized);
    Atom net_wm_state = display_info->atoms[NET_WM_STATE];
    Atom vert_max = display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT];
    Atom horz_max = display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ];
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
            display_info->atoms[NET_WM_ACTION_MOVE],
            display_info->atoms[NET_WM_ACTION_RESIZE],
            display_info->atoms[NET_WM_ACTION_MINIMIZE],
            display_info->atoms[NET_WM_ACTION_SHADE],
            display_info->atoms[NET_WM_ACTION_MAXIMIZE_HORZ],
            display_info->atoms[NET_WM_ACTION_MAXIMIZE_VERT],
            display_info->atoms[NET_WM_ACTION_FULLSCREEN],
            display_info->atoms[NET_WM_ACTION_CHANGE_DESKTOP],
            display_info->atoms[NET_WM_ACTION_CLOSE],
    };
    Atom actions_maximized[] = {
            display_info->atoms[NET_WM_ACTION_MOVE],
            display_info->atoms[NET_WM_ACTION_MINIMIZE],
            display_info->atoms[NET_WM_ACTION_SHADE],
            display_info->atoms[NET_WM_ACTION_CLOSE],
    };
    if (maximized)
    {
        XChangeProperty(
                display_,
                window,
                display_info->atoms[NET_WM_ALLOWED_ACTIONS],
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
                display_info->atoms[NET_WM_ALLOWED_ACTIONS],
                XA_ATOM,
                32,
                PropModeReplace,
                (unsigned char *)actions_normal,
                9);
    }
    XSync(display_, False);

    Client *c;
    c = myDisplayGetClientFromWindow (display_info, window, SEARCH_WINDOW);
    if(c)
    {
        if(maximized)
        {
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
        } else
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
            FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
        }
    }
    return true;
}

void WindowManager::OnSelectionRequest(XEvent e)
{
    XSelectionRequestEvent *sev = (XSelectionRequestEvent *)&e.xselectionrequest;
    logd("OnSelectionRequest start-------->");
    logd("OnSelectionRequest owner:%lx requestor:%lx ", sev->owner, sev->requestor);
    sel = XInternAtom(display_, "CLIPBOARD", False);
    utf8 = display_info->atoms[UTF8_STRING];
    Atom targets = XInternAtom(display_, "TARGETS", False);
    Atom type_qt = XInternAtom(display_, "peony-qt/encoded-uris", False);
    Atom type_texturi = XInternAtom(display_, "text/uri-list", False);
    Atom type_plain = XInternAtom(display_, "text/plain", False);
    Atom type_text = XInternAtom(display_, "TEXT", False);
    Atom type_string = XInternAtom(display_, "STRING", False);
    logd("OnSelectionRequest target:%s property:%s", XGetAtomName(display_, sev->target), XGetAtomName(display_, sev->property));
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
            logd("Sending linux data to window 0x%lx, property '%s'\n", sev->requestor, XGetAtomName(display_, sev->property));
            for (int i = 0; i < selection_property_size; i++)
            {
                logd("   property:%s", XGetAtomName(display_, selection_property_list[i]));
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
            logd("Sending clip text data to window 0x%lx, property '%s' targets & uft8 \n", sev->requestor, XGetAtomName(display_, sev->property));
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
            logd("Sending clip file data to window 0x%lx, property '%s'\n", sev->requestor, XGetAtomName(display_, sev->property));
            for (int i = 0; i < (int)(sizeof(types) / sizeof(Atom)); i++)
            {
                logd("   property:%s", XGetAtomName(display_, types[i]));
            }
        }
        else
        {
            // 不支持的目标类型或没有数据
            logd("Unsupported target or no data available");
            sev->property = None;  // 标记为没有数据
        }
        // 发送SelectionNotify事件
        XSelectionEvent ssev;
        ssev.type = SelectionNotify;
        ssev.requestor = sev->requestor;
        ssev.selection = sev->selection;
        ssev.target = sev->target;

        if (sev->property == None) {
            ssev.property = None;
        } else {
            ssev.property = sev->property;
        }

        ssev.time = sev->time;
        XSendEvent(display_, sev->requestor, 0, NoEventMask, (XEvent *)&ssev);
        XFlush(display_);
    }
    else
    {
        XSelectionEvent ssev;
        char *an;
        an = XGetAtomName(display_, sev->property);
        logd("Sending data to window 0x%lx, property '%s'\n", sev->requestor, an);
        if (!an)
        {
            logd("No data to send to window 0x%lx, property '%s'\n", sev->requestor, an);
            XFree(an);
            XFlush(display_);
            return;
        }
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *data = nullptr;
        XGetWindowProperty(display_, owner, sev->target, 0, (~0L), False, AnyPropertyType,
                           &actual_type, &actual_format, &nitems, &bytes_after, &data);
        logd("data :%s actual_format:%d data:%s nitems:%lu actual_type:%s",
            XGetAtomName(display_, sev->target), actual_format, data, nitems, XGetAtomName(display_, actual_type));
        logd("send property :%s clip_text:%s file_path:%s selection_property_size:%d", XGetAtomName(display_, sev->target), clip_text.c_str(), file_path.c_str(), selection_property_size)
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
            logd("change data to window 0x%lx, property:%s actual_type:%s actual_format:%d data:%s nitems:%d",
                sev->requestor,
                XGetAtomName(display_, sev->property),
                XGetAtomName(display_, actual_type),
                actual_format,
                data,
                nitems)
        }
        logd("Sending data to window 0x%lx, data: '%s'\n", sev->requestor, data);
        ssev.type = SelectionNotify;
        ssev.requestor = sev->requestor;
        ssev.selection = sev->selection;
        ssev.target = sev->target;
        ssev.property = sev->property;
        ssev.time = sev->time;
        XSendEvent(display_, sev->requestor, True, NoEventMask, (XEvent *)&ssev);
        XFlush(display_);
    }
    logd("OnSelectionRequest  end-------->");
}

void WindowManager::OnSelectionClear(XEvent e)
{
    Window request = e.xclient.window;
    XSelectionClearEvent *scev = &e.xselectionclear;
    logd("OnSelectionClear start--------- request:0x:%x  clearowner:0x:%lx selection:%s>\n",
         request, scev->window, XGetAtomName(display_, scev->selection));
    sel = XInternAtom(display_, "CLIPBOARD", False);
    utf8 = XInternAtom(display_, "UTF8_STRING", False);
    Atom target_name = XInternAtom(display_, "TARGETS", False);
    Atom manager_prop_name = XInternAtom(display_, "XSEL_DATA", False);
    XEvent event;
    XConvertSelection(display_, sel, target_name, manager_prop_name, owner, CurrentTime);
//    XConvertSelection(display_, xa_primary, target_name, manager_prop_name, owner, CurrentTime);
    XSelectionEvent *sev;
    for (;;)
    {
        XNextEvent(display_, &event);
        logd(" next event.type:%d\n", event.type);
        switch (event.type)
        {
            case SelectionNotify:
                sev = (XSelectionEvent *)&event.xselection;
                if (sev->property == None)
                {
                    logd("Conversion could not be performed.\n");
                }
                else
                {
                    Atom type, *targets;
                    int di;
                    unsigned long nitems, dul;
                    unsigned char *prop_ret = nullptr;
                    char *an = nullptr;
                    logd("show_targets:\n");
                    XGetWindowProperty(display_, owner, manager_prop_name, 0, 1024 * sizeof(Atom), False, XA_ATOM,
                                       &type, &di, &nitems, &dul, &prop_ret);
                    logd("Targets:  nitems:%lu \n", nitems);
                    targets = (Atom *)prop_ret;
                    selection_property_list = targets;
                    selection_property_size = nitems;
                    for (int index = 0; index < selection_property_size; index++)
                    {
                        logd("type :%s", XGetAtomName(display_, selection_property_list[index]));
                    }
                    for (int index = 0; index < nitems; index++)
                    {
                        an = XGetAtomName(display_, targets[index]);
                        //                        logd("    '%s'\n", an);
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
//    XSetSelectionOwner(display_, xa_primary, owner, CurrentTime);
    logd("OnSelectionClear  end------->\n");
}

void WindowManager::OnSelectionNotify(XEvent event) {
    XSelectionEvent *sev = (XSelectionEvent *)&event.xselection;
    Atom manager_prop_name = XInternAtom(display_, "XSEL_DATA", False);
    if (sev->property == None)
    {
        logd("Conversion could not be performed.\n");
    }
    else
    {
        Atom type, *targets;
        int di;
        unsigned long nitems, dul;
        unsigned char *prop_ret = nullptr;
        char *an = nullptr;
        logd("show_targets:\n");
        XGetWindowProperty(display_, owner, manager_prop_name, 0, 1024 * sizeof(Atom), False, XA_ATOM,
                           &type, &di, &nitems, &dul, &prop_ret);
        logd("Targets:  nitems:%lu \n", nitems);
        targets = (Atom *)prop_ret;
        selection_property_list = targets;
        selection_property_size = nitems;
        for (int index = 0; index < selection_property_size; index++)
        {
            logd("type :%s", XGetAtomName(display_, selection_property_list[index]));
        }
        for (int index = 0; index < nitems; index++)
        {
            an = XGetAtomName(display_, targets[index]);
            //                        logd("    '%s'\n", an);
            if (an)
                XFree(an);
        }
    }
    ConvertAllTarget();
    XSetSelectionOwner(display_, sel, owner, CurrentTime);
//    XSetSelectionOwner(display_, xa_primary, owner, CurrentTime);
    logd("OnSelectionNotify  end------->\n");
}

void WindowManager::ConvertAllTarget()
{
    XEvent event;
    XSelectionEvent *sev;
    bool isText = false;
    bool isFile = false;
    unsigned char *data = nullptr;
    unsigned char *text_data, *file_data = nullptr;
    for (int i = 0; i < selection_property_size; i++)
    {
        logd("show_data:%s\n", XGetAtomName(display_, selection_property_list[i]));
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
                        logd("Conversion could not be performed.\n");
                    }
                    else
                    {
                        Atom actual_type;
                        int actual_format;
                        unsigned long nitems, bytes_after;
                        XGetWindowProperty(display_, owner, selection_property_list[i], 0, (~0L),
                                           False, AnyPropertyType,
                                           &actual_type, &actual_format, &nitems, &bytes_after, &data);
                        if (actual_format == 8)
                        { // 字符串类型
                            logd("actual_type :%s Content of target: %s\n", XGetAtomName(display_, actual_type), data);
                            if (selection_property_list[i] == XInternAtom(display_, "UTF8_STRING", False))
                            {
                                logd("got text_data :%s\n", data);
                                isText = true;
                                text_data = data;
                            }
                            else if (selection_property_list[i] == XInternAtom(display_, "text/uri-list", False)
                            || selection_property_list[i] == XInternAtom(display_, "peony-qt/encoded-uris", False))
                            {
                                logd("got file_data :%s\n", data);
                                isFile = true;
                                file_data = data;
                            }
                        }
                        else
                        {
                            logd("Content of target (binary data or non-8-bit format):\n");
                            for (unsigned long item = 0; item < nitems; item++)
                            {
                                logd("%02x ", data[item]);
                            }
                            logd("\n");
                        }
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
        XFree(data);

    }
    else if (isText)
    {
        UpdateXserverCliptext(reinterpret_cast<const char *>(text_data));
        XFree(data);

    }
}

int WindowManager::moveWindow(long window, int x, int y)
{
    logd("moveWindow %x: x:%d y:%d", window, x, y);
    int ret = XMoveWindow(display_, window, x, y);
    XSync(display_, False);
    return ret;
}

int WindowManager::configureWindow(long window, int x, int y, int w, int h)
{
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
        logd("configureWindow \"%s\" (0x%lx) (%d, %d) %dx%d", c->name, c->window, x, y, w, h);
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING))
        {
            loge("Sorry, but it's not the right time for configure request");
            return False;
        }
        clientMoveResizeWindow (c, &changes, value_mask);
    }
    else
    {
        logd("unmanaged configureWindow for window 0x%lx", window);
        myDisplayErrorTrapPush (display_info);
        ret = XConfigureWindow (display_info->dpy, window, value_mask, &changes);
        myDisplayErrorTrapPopIgnored (display_info);
    }
    XFlush(display_);
    return ret;
}

int WindowManager::setWindowingMode(long frame, long window, int mode)
{
    logd ("setWindowingMode %lx %lx %d", frame, window, mode)
    int ret;
    Client *c;
    c = myDisplayGetClientFromWindow(display_info, window, SEARCH_WINDOW);
    if(c){
        if(mode){
            ret = setMaximizedState(c->window, true);
        } else {
            ret = setMaximizedState(c->window, false);
        }
    }
    XFlush(display_);
    return ret;
}

int WindowManager::resizeWindow(long window, int w, int h)
{
    logd("resizeWindow %lx w:%d h:%d", window, w, h);
    int ret = XResizeWindow(display_, window, w, h);
    XSync(display_, False);
    return ret;
}

int WindowManager::unmapWindow(long window)
{
    logd("unmapWindow %lx ", window);
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
        logd("can't find frame to close window");
        return FALSE;
    }
    clientClose(c);
    return True;
}

int WindowManager::raiseWindow(long window)
{
    logd("raiseWindow %x", window);
    int ret;
    ret =  XRaiseWindow(display_, window);
    Client *c;
    c = myDisplayGetClientFromWindow(display_info, window, SEARCH_FRAME);
    if(c){
        logd("raiseWindow %x", c->window);
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
        logd("no need update clip text");
    }
    else
    {
        clip_text = in_text;
        logd("update clip text :%s", clip_text.c_str());
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
        logd("no need update clip file")
    }
    else
    {
        file_path = in_text;
        logd("update clip file :%s", file_path.c_str())
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
        logd("circulaSubWindows ret:%d", ret);
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
    std::string in_text = text;
    clip_text = in_text;
    logd("update clip text :%s", text)
    if (GlobalEnv && util_is_valid_utf8(text))
    {
        jstring utf = GlobalEnv->NewStringUTF(text);
        jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                        "updateXserverCliptext", "(Ljava/lang/String;)V");
        GlobalEnv->CallStaticVoidMethod(staticClass, method, utf);
    }
}

void WindowManager::UpdateXserverClipFile(const char *text)
{
    std::string in_text = text;
    file_path = in_text;
    logd("update clip file :%s", text)
    if (GlobalEnv && util_is_valid_utf8(text))
    {
        jstring utf = GlobalEnv->NewStringUTF(text);
        jmethodID method = GlobalEnv->GetStaticMethodID(staticClass,
                                                        "updateXserverClipFile", "(Ljava/lang/String;)V");
        GlobalEnv->CallStaticVoidMethod(staticClass, method, utf);
    }
}

int WindowManager::SetRootResourceManager(Display *display, const char *resource_string) {
    Window root = DefaultRootWindow(display);
    Atom resource_manager = XInternAtom(display, "RESOURCE_MANAGER", False);
    if (resource_manager == None) {
        logd("无法获取RESOURCE_MANAGER原子");
        return -1;
    }
    Atom string_atom = XInternAtom(display, "STRING", False);
    if (string_atom == None) {
        logd("无法获取STRING原子");
        return -1;
    }
    logd("设置RESOURCE_MANAGER属性...");
    logd("数据长度: %zu 字节", strlen(resource_string));
    XChangeProperty(display, root,
                    resource_manager,
                    string_atom,
                    8,                // 8位格式
                    PropModeReplace,
                    (unsigned char *)resource_string,
                    strlen(resource_string));
    XSync(display, False);
    return 0;
}
