
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
    // if (!myDisplayInitAtoms (display))
    // {
    //     g_warning ("Some internal atoms were not properly created.");
    // }

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
    // TRACE ("no screen found");

    return NULL;
}

void
myDisplayGrabServer (DisplayInfo *display)
{
    g_return_if_fail (display);

    logw ("entering myDisplayGrabServer");
    if (display->xgrabcount == 0)
    {
        logw ("grabbing server");
        XGrabServer (display->dpy);
    }
    display->xgrabcount++;
    logw ("grabs : %i", display->xgrabcount);
}

void
myDisplayUngrabServer (DisplayInfo *display)
{
    g_return_if_fail (display);

    // DBG ("entering myDisplayUngrabServer");
    display->xgrabcount = display->xgrabcount - 1;
    if (display->xgrabcount < 0)       /* should never happen */
    {
        display->xgrabcount = 0;
    }
    if (display->xgrabcount == 0)
    {
        // DBG ("ungrabbing server");
        XUngrabServer (display->dpy);
        XFlush (display->dpy);
    }
    // DBG ("grabs : %i", display->xgrabcount);
}


guint32
myDisplayGetCurrentTime(DisplayInfo *display)
{
    g_return_val_if_fail(display != NULL, (guint32)CurrentTime);

    // TRACE ("timestamp=%u", (guint32) display->current_time);
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

    logw ("timestamp=%u", (guint32) display_timestamp);
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
    // TRACE ("no client found");

    return NULL;
}

void
myDisplayAddScreen (DisplayInfo *display, ScreenInfo *screen)
{
    g_return_if_fail (screen != NULL);
    g_return_if_fail (display != NULL);

    display->screens = g_slist_append (display->screens, screen);
    logw ("  display->screens %p" ,  display->screens);

    display->nb_screens = display->nb_screens + 1;
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
    logw ("no screen found for 0x%lx", w);

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
        logw ("no root found for 0x%lx", w);
        return None;
    }
    return attributes.root;
}