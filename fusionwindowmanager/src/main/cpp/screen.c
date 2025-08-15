#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>
#include "display.h"
#include "screen.h"
#include "native_log.h"

#ifndef WM_EXITING_TIMEOUT
#define WM_EXITING_TIMEOUT 15 /*seconds */
#endif

gboolean
myScreenCheckWMAtom (ScreenInfo *screen_info, Atom atom)
{
    gchar selection[32];
    Atom wm_sn_atom;

    logw ("atom %lu", atom);

    g_snprintf (selection, sizeof (selection), "WM_S%d", screen_info->screen);
    wm_sn_atom = XInternAtom (myScreenGetXDisplay (screen_info), selection, FALSE);

    return (atom == wm_sn_atom);
}

static gboolean
myScreenSetWMAtom (ScreenInfo *screen_info, gboolean replace_wm)
{
    const char *wm_name;
    gchar selection[32];
    gchar *display_name;
    gulong wait, timeout;
    DisplayInfo *display_info;
    XSetWindowAttributes attrs;
    Window current_wm;
    XEvent event;
    Atom wm_sn_atom;


    g_return_val_if_fail (screen_info, FALSE);
    g_return_val_if_fail (screen_info->display_info, FALSE);


    display_info = screen_info->display_info;
    // g_snprintf (selection, sizeof (selection), "WM_S%d", screen_info->screen);
    // wm_sn_atom = XInternAtom (display_info->dpy, selection, FALSE);
    display_name = "FDE-XDISPLAY";
    wm_name = "FDE-XWM";

    // current_wm = XGetSelectionOwner (display_info->dpy, wm_sn_atom);
    // if (current_wm)
    // {
    //     if (!replace_wm)
    //     {
    //         logd ("Another Window Manager (%s) is already running on screen %s\n", wm_name, display_name);
    //         logd ("To replace the current window manager, try \"--replace\"\n");
    //         // g_free (display_name);

    //         return FALSE;
    //     }
    myDisplayErrorTrapPush (display_info);
    attrs.event_mask = StructureNotifyMask;
    XChangeWindowAttributes (display_info->dpy, screen_info->xfwm4_win, CWEventMask, &attrs);
    XSync (display_info->dpy, FALSE);

        // if (myDisplayErrorTrapPop (display_info))
        // {
            // current_wm = None;
        // }
    // }

    // if (!setXAtomManagerOwner (display_info, wm_sn_atom, screen_info->xroot, screen_info->xfwm4_win))
    // {
        // logw ("Cannot acquire window manager selection on screen %s", display_name);
        // g_free (display_name);

        // return FALSE;
    // }

    /* Waiting for previous window manager to exit */
    // if (current_wm)
    // {
    //     logd ("Waiting for current window manager (%s) on screen %s to exit:", wm_name, display_name);
    //     wait = 0;
    //     timeout = WM_EXITING_TIMEOUT * G_USEC_PER_SEC;
    //     while (wait < timeout)
    //     {
    //         if (XCheckWindowEvent (display_info->dpy, current_wm, StructureNotifyMask, &event) && (event.type == DestroyNotify))
    //         {
    //             break;
    //         }
    //         g_usleep(G_USEC_PER_SEC / 10);
    //         wait += G_USEC_PER_SEC / 10;
    //         if (wait % G_USEC_PER_SEC == 0)
    //         {
    //           logd (".");
    //         }
    //     }

    //     if (wait >= timeout)
    //     {
    //         logd(" Failed\n");
    //         logw("Previous window manager (%s) on screen %s is not exiting", wm_name, display_name);
    //         g_free (display_name);

    //         return FALSE;
    //     }
    //     logd(" Done\n");
    // }
    // g_free (display_name);

    return TRUE;
}

ScreenInfo *
myScreenInit (DisplayInfo *display_info, unsigned long event_mask, int index,
     Window back_window, Window root)
{
    ScreenInfo *screen_info;
    long desktop_visible;
    int i, j;

    g_return_val_if_fail (display_info, NULL);

    screen_info = g_new0 (ScreenInfo, 1);

    screen_info->display_info = display_info;
    desktop_visible = 0;


    // screen_info->cmap = DefaultColormapOfScreen (screen_info->xscreen);
    screen_info->xroot = root;
    screen_info->depth = DefaultDepth (display_info->dpy, screen_info->screen);
    screen_info->visual = DefaultVisual (display_info->dpy, screen_info->screen);
    screen_info->shape_win = (Window) None;
    myScreenComputeSize (screen_info);
    screen_info->xfwm4_win = back_window;

    if (!myScreenSetWMAtom (screen_info, 1))
    {
        logd ("init screen failed");
        g_free (screen_info);
        return NULL;
    }

    screen_info->current_ws = 0;
    screen_info->previous_ws = 0;
    screen_info->current_ws = 0;
    screen_info->previous_ws = 0;

    screen_info->margins[STRUTS_TOP] = screen_info->gnome_margins[STRUTS_TOP] = 0;
    screen_info->margins[STRUTS_LEFT] = screen_info->gnome_margins[STRUTS_LEFT] = 0;
    screen_info->margins[STRUTS_RIGHT] = screen_info->gnome_margins[STRUTS_RIGHT] = 0;
    screen_info->margins[STRUTS_BOTTOM] = screen_info->gnome_margins[STRUTS_BOTTOM] = 0;

    screen_info->workspace_count = 0;
    screen_info->workspace_names = NULL;
    screen_info->workspace_names_items = 0;

    screen_info->windows_stack = NULL;
    screen_info->last_raise = NULL;
    screen_info->windows = NULL;
    screen_info->clients = NULL;
    screen_info->client_count = 0;
    screen_info->client_serial = 0L;
    screen_info->button_handler_id = 0L;

    screen_info->key_grabs = 0;
    screen_info->pointer_grabs = 0;

    // getHint (display_info, screen_info->xroot, NET_SHOWING_DESKTOP, &desktop_visible);
    screen_info->show_desktop = (desktop_visible != 0);

    screen_info->box_gc = None;

    screen_info->monitors_index = NULL;

    return (screen_info);
}


gboolean
myScreenComputeSize (ScreenInfo *screen_info)
{
    gint width, height;
    gboolean changed;

    g_return_val_if_fail (screen_info != NULL, FALSE);

    width = 0;
    height = 0;

    changed = ((screen_info->width != width) | (screen_info->height != height));
    screen_info->width = width;
    screen_info->height = height;

    return changed;
}

Display *
myScreenGetXDisplay (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;

    g_return_val_if_fail (screen_info, NULL);
    g_return_val_if_fail (screen_info->display_info, NULL);
    
    display_info = screen_info->display_info;
    return display_info->dpy;
}