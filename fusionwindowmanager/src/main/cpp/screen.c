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

    logd ("atom %lu", atom);

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
    display_name = "FDE-XDISPLAY";
    wm_name = "FDE-XWM";
    wm_sn_atom = XInternAtom (display_info->dpy, "WM_S1001", FALSE);

    if (!setXAtomManagerOwner (display_info, wm_sn_atom, screen_info->xroot, screen_info->xfwm4_win))
    {
        logd ("Cannot acquire window manager selection on screen %s", display_name);
        g_free (display_name);

        return FALSE;
    }
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
    screen_info->show_desktop = TRUE;

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

    width = 1920;
    height = 1080;

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

Client *
myScreenGetClientFromWindow (ScreenInfo *screen_info, Window w, unsigned short mode)
{
    Client *c;
    guint i;

    g_return_val_if_fail (w != None, NULL);
    logd ("looking for (0x%lx)", w);

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, i++)
    {
        if (clientGetFromWindow (c, w, mode))
        {
            return (c);
        }
    }
    logd ("no client found");

    return NULL;
}

gboolean
myScreenGrabKeyboard (ScreenInfo *screen_info, guint event_mask, guint32 timestamp)
{
    gboolean grab;

    g_return_val_if_fail (screen_info, FALSE);

    logd("timestamp %u", (unsigned int) timestamp);

    grab = TRUE;
    if (screen_info->key_grabs == 0)
    {
        myDisplayErrorTrapPush (screen_info->display_info);
        grab = xfwm_device_grab (screen_info->display_info->devices,
                                 &screen_info->display_info->devices->keyboard,
                                 myScreenGetXDisplay (screen_info), screen_info->xroot,
                                 TRUE, event_mask, GrabModeAsync, screen_info->xroot,
                                 None, (Time) timestamp);
        myDisplayErrorTrapPopIgnored (screen_info->display_info);
    }
    screen_info->key_grabs++;
    logd("global key grabs %i", screen_info->key_grabs);

    return grab;
}


gboolean
myScreenGrabPointer (ScreenInfo *screen_info, gboolean owner_events,
                     guint event_mask, Cursor cursor, guint32 timestamp)
{
    gboolean grab;

    g_return_val_if_fail (screen_info, FALSE);
    logd("timestamp %u", (unsigned int) timestamp);

    grab = TRUE;
    if (screen_info->pointer_grabs == 0)
    {
        myDisplayErrorTrapPush (screen_info->display_info);
        grab = xfwm_device_grab (screen_info->display_info->devices,
                                 &screen_info->display_info->devices->pointer,
                                 myScreenGetXDisplay (screen_info), screen_info->xroot,
                                 owner_events, event_mask, GrabModeAsync, screen_info->xroot,
                                 cursor, (Time) timestamp);
        myDisplayErrorTrapPopIgnored (screen_info->display_info);
    }
    screen_info->pointer_grabs++;
    logd("global pointer grabs %i", screen_info->pointer_grabs);

    return grab;
}

unsigned int
myScreenUngrabKeyboard (ScreenInfo *screen_info, guint32 timestamp)
{
    g_return_val_if_fail (screen_info, 0);
    logd("timestamp %u", (unsigned int) timestamp);

    screen_info->key_grabs--;
    if (screen_info->key_grabs < 0)
    {
        screen_info->key_grabs = 0;
    }
    if (screen_info->key_grabs == 0)
    {
        myDisplayErrorTrapPush (screen_info->display_info);
        xfwm_device_ungrab (screen_info->display_info->devices,
                            &screen_info->display_info->devices->keyboard,
                            myScreenGetXDisplay (screen_info), (Time) timestamp);
        myDisplayErrorTrapPopIgnored (screen_info->display_info);
    }
    logd("global key grabs %i", screen_info->key_grabs);

    return screen_info->key_grabs;
}


unsigned int
myScreenUngrabPointer (ScreenInfo *screen_info, guint32 timestamp)
{
    g_return_val_if_fail (screen_info, 0);
    logd("timestamp %u", (unsigned int) timestamp);

    screen_info->pointer_grabs--;
    if (screen_info->pointer_grabs < 0)
    {
        screen_info->pointer_grabs = 0;
    }
    if (screen_info->pointer_grabs == 0)
    {
        myDisplayErrorTrapPush (screen_info->display_info);
        xfwm_device_ungrab (screen_info->display_info->devices,
                            &screen_info->display_info->devices->pointer,
                            myScreenGetXDisplay (screen_info), (Time) timestamp);
        myDisplayErrorTrapPopIgnored (screen_info->display_info);
    }
    logd("global pointer grabs %i", screen_info->pointer_grabs);

    return screen_info->pointer_grabs;
}