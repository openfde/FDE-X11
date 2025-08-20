
#include <stdio.h>
#include "X11/X.h"
#include "X11/Xlib.h"
#include "X11/Xutil.h"
#include "X11/cursorfont.h"
#include "X11/extensions/shape.h"

#include "screen.h"
#include "display.h"
#include "client.h"
#include "native_log.h"

#ifndef MAX_HOSTNAME_LENGTH
#define MAX_HOSTNAME_LENGTH 512
#endif /* MAX_HOSTNAME_LENGTH */


static gboolean
myDisplayInitAtoms (DisplayInfo *display_info)
{
    static const char *atom_names[] = {
        "COMPOSITING_MANAGER",
        "_GTK_FRAME_EXTENTS",
        "_GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED",
        "_GTK_SHOW_WINDOW_MENU",
        "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR",
        "KWM_WIN_ICON",
        "_MOTIF_WM_HINTS",
        "_MOTIF_WM_INFO",
        "_NET_ACTIVE_WINDOW",
        "_NET_CLIENT_LIST",
        "_NET_CLIENT_LIST_STACKING",
        "_NET_CLOSE_WINDOW",
        "_NET_CURRENT_DESKTOP",
        "_NET_DESKTOP_GEOMETRY",
        "_NET_DESKTOP_LAYOUT",
        "_NET_DESKTOP_NAMES",
        "_NET_DESKTOP_VIEWPORT",
        "_NET_FRAME_EXTENTS",
        "_NET_MOVERESIZE_WINDOW",
        "_NET_NUMBER_OF_DESKTOPS",
        "_NET_REQUEST_FRAME_EXTENTS",
        "_NET_SHOWING_DESKTOP",
        "_NET_STARTUP_ID",
        "_NET_SUPPORTED",
        "_NET_SUPPORTING_WM_CHECK",
        "_NET_SYSTEM_TRAY_OPCODE",
        "_NET_WM_ACTION_ABOVE",
        "_NET_WM_ACTION_BELOW",
        "_NET_WM_ACTION_CHANGE_DESKTOP",
        "_NET_WM_ACTION_CLOSE",
        "_NET_WM_ACTION_FULLSCREEN",
        "_NET_WM_ACTION_MAXIMIZE_HORZ",
        "_NET_WM_ACTION_MAXIMIZE_VERT",
        "_NET_WM_ACTION_MINIMIZE",
        "_NET_WM_ACTION_MOVE",
        "_NET_WM_ACTION_RESIZE",
        "_NET_WM_ACTION_SHADE",
        "_NET_WM_ACTION_STICK",
        "_NET_WM_ALLOWED_ACTIONS",
        "_NET_WM_BYPASS_COMPOSITOR",
        "_NET_WM_CONTEXT_HELP",
        "_NET_WM_DESKTOP",
        "_NET_WM_FULLSCREEN_MONITORS",
        "_NET_WM_ICON",
        "_NET_WM_ICON_GEOMETRY",
        "_NET_WM_ICON_NAME",
        "_NET_WM_MOVERESIZE",
        "_NET_WM_NAME",
        "_NET_WM_OPAQUE_REGION",
        "_NET_WM_PID",
        "_NET_WM_PING",
        "_NET_WM_WINDOW_OPACITY",
        "_NET_WM_WINDOW_OPACITY_LOCKED",
        "_NET_WM_STATE",
        "_NET_WM_STATE_ABOVE",
        "_NET_WM_STATE_BELOW",
        "_NET_WM_STATE_DEMANDS_ATTENTION",
        "_NET_WM_STATE_FOCUSED",
        "_NET_WM_STATE_FULLSCREEN",
        "_NET_WM_STATE_HIDDEN",
        "_NET_WM_STATE_MAXIMIZED_HORZ",
        "_NET_WM_STATE_MAXIMIZED_VERT",
        "_NET_WM_STATE_MODAL",
        "_NET_WM_STATE_SHADED",
        "_NET_WM_STATE_SKIP_PAGER",
        "_NET_WM_STATE_SKIP_TASKBAR",
        "_NET_WM_STATE_STICKY",
        "_NET_WM_STRUT",
        "_NET_WM_STRUT_PARTIAL",
        "_NET_WM_SYNC_REQUEST",
        "_NET_WM_SYNC_REQUEST_COUNTER",
        "_NET_WM_USER_TIME",
        "_NET_WM_USER_TIME_WINDOW",
        "_NET_WM_WINDOW_TYPE",
        "_NET_WM_WINDOW_TYPE_DESKTOP",
        "_NET_WM_WINDOW_TYPE_DIALOG",
        "_NET_WM_WINDOW_TYPE_DOCK",
        "_NET_WM_WINDOW_TYPE_MENU",
        "_NET_WM_WINDOW_TYPE_NORMAL",
        "_NET_WM_WINDOW_TYPE_SPLASH",
        "_NET_WM_WINDOW_TYPE_TOOLBAR",
        "_NET_WM_WINDOW_TYPE_UTILITY",
        "_NET_WM_WINDOW_TYPE_NOTIFICATION",
        "_NET_WORKAREA",
        "MANAGER",
        "PIXMAP",
        "SM_CLIENT_ID",
        "UTF8_STRING",
        "WM_CHANGE_STATE",
        "WM_CLIENT_LEADER",
        "WM_CLIENT_MACHINE",
        "WM_COLORMAP_WINDOWS",
        "WM_DELETE_WINDOW",
        "WM_HINTS",
        "WM_PROTOCOLS",
        "WM_STATE",
        "WM_TAKE_FOCUS",
        "WM_TRANSIENT_FOR",
        "WM_WINDOW_ROLE",
        "XFWM4_COMPOSITING_MANAGER",
        "XFWM4_TIMESTAMP_PROP",
        "_XROOTPMAP_ID",
        "_XSETROOT_ID",
        "_GTK_READ_RCFILES"
    };
    size_t array_length = sizeof(atom_names) / sizeof(atom_names[0]);
    // g_assert (ATOM_COUNT == G_N_ELEMENTS (atom_names));
    return (XInternAtoms (display_info->dpy,
                          (char **) atom_names,
                          array_length,
                          FALSE, display_info->atoms) != 0);
}

static void
myDisplayCreateTimestampWin (DisplayInfo *display_info)
{
    XSetWindowAttributes attributes;

    attributes.event_mask = PropertyChangeMask;
    attributes.override_redirect = TRUE;
    display_info->timestamp_win =
        XCreateWindow (display_info->dpy, DefaultRootWindow (display_info->dpy),
                       -100, -100, 10, 10, 0, 0, CopyFromParent, CopyFromParent,
                       CWEventMask | CWOverrideRedirect, &attributes);
}


DisplayInfo *
myDisplayInit(Display *dpy)
{
    DisplayInfo *display;
    int major, minor;
    int dummy;
    gchar *hostnametmp;

    

    display = g_new0(DisplayInfo, 1);
    display->dpy = dpy;

    display->quit = FALSE;
    display->reload = FALSE;

    /* Initialize internal atoms */
    if (!myDisplayInitAtoms (display))
    {
        logd ("Some internal atoms were not properly created.");
    }

    for (int j = 0; j < ATOM_COUNT; j++)
    {
        logd ("myDisplayInit: atom[%d] = %s atom = %lu", j, XGetAtomName(display->dpy, display->atoms[j]), display->atoms[j]);
    }  
    
    /* Test XShape extension support */

    display->shape_version = 0;

    display->have_shape = FALSE;

    display->have_render = FALSE;

    display->have_xrandr = FALSE;

    display->have_xres = FALSE;


    myDisplayCreateTimestampWin(display);

    display->screens = NULL;
    display->clients = NULL;
    display->xgrabcount = 0;
    display->double_click_time = 250;
    display->double_click_distance = 5;
    display->nb_screens = 0;
    display->current_time = CurrentTime;

    hostnametmp = g_new0(gchar, (size_t)MAX_HOSTNAME_LENGTH + 1);
    if (gethostname((char *)hostnametmp, MAX_HOSTNAME_LENGTH))
    {
        g_warning("The display's hostname could not be determined.");
        display->hostname = NULL;
    }
    else
    {
        hostnametmp[MAX_HOSTNAME_LENGTH] = '\0';
        display->hostname = g_strdup(hostnametmp);
    }
    g_free(hostnametmp);

    return display;
}

void myDisplayErrorTrapPush(DisplayInfo *display_info)
{
}

gint myDisplayErrorTrapPop(DisplayInfo *display_info)
{
    return 0;
}

void myDisplayErrorTrapPopIgnored(DisplayInfo *display_info)
{
}

ScreenInfo *
myDisplayGetScreenFromRoot (DisplayInfo *display, Window root)
{
    GSList *list;

    g_return_val_if_fail (root != None, NULL);
    g_return_val_if_fail (display != NULL, NULL);

    for (list = display->screens; list; list = g_slist_next (list))
    {
        ScreenInfo *screen = (ScreenInfo *) list->data;
        if (screen->xroot == root)
        {
            return screen;
        }
    }
    logd ("no screen found");

    return NULL;
}

void
myDisplayGrabServer (DisplayInfo *display)
{
    g_return_if_fail (display);

    if (display->xgrabcount == 0)
    {
        XGrabServer (display->dpy);
    }
    display->xgrabcount++;
}

void
myDisplayUngrabServer (DisplayInfo *display)
{
    g_return_if_fail (display);

    display->xgrabcount = display->xgrabcount - 1;
    if (display->xgrabcount < 0)       /* should never happen */
    {
        display->xgrabcount = 0;
    }
    if (display->xgrabcount == 0)
    {
        XUngrabServer (display->dpy);
        XFlush (display->dpy);
    }
}


guint32
myDisplayGetCurrentTime(DisplayInfo *display)
{
    g_return_val_if_fail(display != NULL, (guint32)CurrentTime);

    return display->current_time;
}

guint32
myDisplayGetTime (DisplayInfo * display, guint32 timestamp)
{
    guint32 display_timestamp;

    display_timestamp = timestamp;
    if (display_timestamp == (guint32) CurrentTime)
    {
        display_timestamp = getXServerTime (display);
    }

    logd ("timestamp=%u", (guint32) display_timestamp);
    return display_timestamp;
}

Client *
myDisplayGetClientFromWindow (DisplayInfo *display, Window w, unsigned short mode)
{
    GSList *list;

    g_return_val_if_fail (w != None, NULL);
    g_return_val_if_fail (display != NULL, NULL);

    for (list = display->clients; list; list = g_slist_next (list))
    {
        Client *c = (Client *) list->data;
        if (clientGetFromWindow (c, w, mode))
        {
            return (c);
        }
    }

    return NULL;
}

void
myDisplayAddScreen (DisplayInfo *display, ScreenInfo *screen)
{
    g_return_if_fail (screen != NULL);
    g_return_if_fail (display != NULL);

    display->screens = g_slist_append (display->screens, screen);
    logd ("  display->screens %p" ,  display->screens);

    display->nb_screens = display->nb_screens + 1;
}

void
myDisplayRemoveClient (DisplayInfo *display, Client *c)
{
    g_return_if_fail (c != None);
    g_return_if_fail (display != NULL);

    display->clients = g_slist_remove (display->clients, c);
}

ScreenInfo *
myDisplayGetScreenFromWindow (DisplayInfo *display, Window w)
{
    ScreenInfo *screen;
    Window root;

    g_return_val_if_fail (w != None, NULL);
    g_return_val_if_fail (display != NULL, NULL);

    /* First check if this is a known root window */
    screen = myDisplayGetScreenFromRoot (display, w);
    if (screen)
    {
        return screen;
    }

    /* Else retrieve the window's root window */
    root = myDisplayGetRootFromWindow (display, w);
    if (root != None)
    {
        screen = myDisplayGetScreenFromRoot (display, root);
        if (screen)
        {
            return screen;
        }
    }
    logd ("no screen found for 0x%lx", w);

    return NULL;
}

Window
myDisplayGetRootFromWindow(DisplayInfo *display_info, Window w)
{
    XWindowAttributes attributes;
    int result, status;

    g_return_val_if_fail (w != None, None);
    g_return_val_if_fail (display_info != NULL, None);

    myDisplayErrorTrapPush (display_info);
    status = XGetWindowAttributes(display_info->dpy, w, &attributes);
    result = myDisplayErrorTrapPop (display_info);

    if ((result != Success) || !status)
    {
        logd ("no root found for 0x%lx", w);
        return None;
    }
    return attributes.root;
}