#include "moveresize.h"
#include "client.h"
#include "device.h"
#include "display.h"
#include "netwm.h"
#include "transients.h"
#include "native_log.h"


#define MOVERESIZE_POINTER_EVENT_MASK \
    PointerMotionMask | \
    ButtonMotionMask | \
    ButtonReleaseMask | \
    LeaveWindowMask

#define MOVERESIZE_KEYBOARD_EVENT_MASK \
    KeyPressMask

// typedef struct _MoveResizeData MoveResizeData;
// struct _MoveResizeData
// {
//     Client *c;
// //    WireFrame *wireframe;
//     gboolean use_keys;
//     gboolean grab;
//     gboolean is_transient;
//     gboolean move_resized;
//     gboolean released;
//     gboolean client_gone;
//     guint button;
//     gint cancel_x, cancel_y;
//     gint cancel_w, cancel_h;
//     unsigned long cancel_flags;
//     unsigned long configure_flags;
//     guint cancel_workspace;
//     gint mx, my;
//     double pxratio, pyratio; /* pointer relative position ratio */
//     gint ox, oy;
//     gint ow, oh;
//     gint oldw, oldh;
//     gint handle;
// //    Poswin *poswin;
// };



void
clientMove (Client * c, XfwmEventButton *event)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    XWindowChanges wc;
    MoveResizeData passdata;
    int changes;
    gboolean g1, g2;

    g_return_if_fail (c != NULL);
    logd("client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING) ||
        !FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MOVE))
    {
        return;
    }

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        return;
    }

    logd("client \"%s\" (0x%lx)", c->name, c->window);
    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    changes = CWX | CWY;

    passdata.c = c;
    passdata.cancel_x = passdata.ox = c->x;
    passdata.cancel_y = passdata.oy = c->y;
    passdata.cancel_w = c->width;
    passdata.cancel_h = c->height;
    passdata.cancel_flags = c->flags;
    passdata.configure_flags = NO_CFG_FLAG;
    passdata.cancel_workspace = c->win_workspace;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    passdata.released = FALSE;
    passdata.client_gone = FALSE;
    passdata.button = AnyButton;
    passdata.is_transient = clientIsValidTransientOrModal (c);
    passdata.move_resized = FALSE;
//    passdata.wireframe = NULL;

//    clientSaveSizePos (c);

    if (event && event->pressed)
    {
        passdata.button = event->button;
        passdata.mx = event->x_root;
        passdata.my = event->y_root;
        passdata.pxratio = 1;//(passdata.mx - 0) / (double) frameExtentWidth (c);
        passdata.pyratio = 1;//(passdata.my - 0) / (double) frameExtentHeight (c);
    }
    else
    {
//        clientSetHandle(&passdata, NO_HANDLE);
        passdata.released = passdata.use_keys = TRUE;
    }
//
    Cursor move_cursor = XCreateFontCursor(display_info->dpy, XC_fleur);
    XGrabPointer(display_info->dpy, c->window, False,
                 PointerMotionMask | ButtonReleaseMask | ButtonPressMask,
                 GrabModeAsync, GrabModeAsync,
                 None, move_cursor, CurrentTime);
   g1 = myScreenGrabKeyboard (screen_info, MOVERESIZE_KEYBOARD_EVENT_MASK,
                              myDisplayGetCurrentTime (display_info));
   g2 = myScreenGrabPointer (screen_info, FALSE, MOVERESIZE_POINTER_EVENT_MASK,
                             myDisplayGetCursorMove (display_info),
                             myDisplayGetCurrentTime (display_info));
   if (!g1 || !g2)
   {
       logd("grab failed in clientMove");
//
//        myDisplayBeep (display_info);
       myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
       myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));
//
       return;
   }

//    if (screen_info->params->box_move && compositorIsActive (screen_info))
//    {
//        passdata.wireframe = wireframeCreate (c);
//    }

//    passdata.poswin = NULL;


    /* Set window translucent while moving */
//    if ((screen_info->params->move_opacity < 100) &&
//        !(screen_info->params->box_move) &&
//        !FLAG_TEST (c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED))
//    {
//        clientSetOpacity (c, c->opacity, OPACITY_MOVE, OPACITY_MOVE);
//    }

    /*
     * Need to remove the sidewalk windows while moving otherwise
     * the motion events aren't reported on screen edges
     */
//    placeSidewalks(screen_info, FALSE);

    /* Clear any previously saved pos flag from screen resize */
    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_SAVED_POS);

    FLAG_SET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);
//    TRACE ("entering move loop");
//    eventFilterPush (display_info->xfilter, clientMoveEventFilter, &passdata);
//    gtk_main ();
//    eventFilterPop (display_info->xfilter);
//    TRACE ("leaving move loop");
    if (passdata.client_gone)
    {
//        goto move_cleanup;
    }
    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);

//    if (passdata.grab && screen_info->params->box_move)
//    {
//        clientDrawOutline (c);
//    }

    /* Set window opacity to its original value */
//    clientSetOpacity (c, c->opacity, OPACITY_MOVE, 0);

    clientSetNetState (c);

    wc.x = c->x;
    wc.y = c->y;
    if (passdata.move_resized)
    {
        wc.width = c->width;
        wc.height = c->height;
        changes |= CWWidth | CWHeight;
    }
    clientConfigure (c, &wc, changes, passdata.configure_flags);

    if (passdata.button != AnyButton && !passdata.released)
    {
        /* If this is a drag-move, wait for the button to be released.
         * If we don't, we might get release events in the wrong place.
         */
//        eventFilterPush (display_info->xfilter, clientButtonReleaseFilter, &passdata);
//        gtk_main ();
//        eventFilterPop (display_info->xfilter);
    }

//    move_cleanup:
    /* Put back the sidewalks as they ought to be */
//    placeSidewalks (screen_info, screen_info->params->wrap_workspaces);

#ifdef SHOW_POSITION
    if (passdata.poswin)
    {
        poswinDestroy (passdata.poswin);
    }
#endif /* SHOW_POSITION */

//    if (passdata.wireframe)
//    {
//        wireframeDelete (passdata.wireframe);
//    }
//
//    myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
//    myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));
//
//    if (passdata.grab && screen_info->params->box_move)
//    {
//        myDisplayUngrabServer (display_info);
//    }
    screen_info->passdata = passdata;
}


void
clientResize (Client * c, int handle, XfwmEventButton *event)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    MoveResizeData passdata;
    int w_orig, h_orig;
    Cursor cursor;
    gboolean g1, g2;
#ifndef SHOW_POSITION
    gint scale;
#endif

    g_return_if_fail (c != NULL);
    logd("client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING))
    {
        return;
    }

    if (!FLAG_TEST_ALL (c->xfwm_flags, XFWM_FLAG_HAS_RESIZE | XFWM_FLAG_IS_RESIZABLE))
    {
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MOVE))
        {
            clientMove (c, event);
        }
        return;
    }

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        return;
    }

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    logd("client \"%s\" (0x%lx)", c->name, c->window);

    passdata.c = c;
    passdata.cancel_x = passdata.ox = c->x;
    passdata.cancel_y = passdata.oy = c->y;
    passdata.cancel_w = passdata.ow = c->width;
    passdata.cancel_h = passdata.oh = c->height;
    passdata.configure_flags = NO_CFG_FLAG;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    passdata.released = FALSE;
    passdata.client_gone = FALSE;
    passdata.button = AnyButton;
    passdata.handle = handle;
//    passdata.wireframe = NULL;
    w_orig = c->width;
    h_orig = c->height;

    if (event && event->pressed)
    {
        passdata.button = event->button;
        passdata.mx = event->x_root;
        passdata.my = event->y_root;
    }
    else
    {
//        clientSetHandle (&passdata, handle);
        passdata.released = passdata.use_keys = TRUE;
    }
    if ((handle > NO_HANDLE) && (handle <= HANDLES_COUNT))
    {
//        cursor = myDisplayGetCursorResize (display_info, passdata.handle);
    }
    else
    {
//        cursor = myDisplayGetCursorMove (display_info);
    }

//    g1 = myScreenGrabKeyboard (screen_info, MOVERESIZE_KEYBOARD_EVENT_MASK,
//                               myDisplayGetCurrentTime (display_info));
//    g2 = myScreenGrabPointer (screen_info, FALSE, MOVERESIZE_POINTER_EVENT_MASK,
//                              cursor, myDisplayGetCurrentTime (display_info));

//    if (!g1 || !g2)
//    {
//        TRACE ("grab failed in clientResize");
//
//        myDisplayBeep (display_info);
//        myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
//        myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));
//
//        return;
//    }

//    if (screen_info->params->box_resize && compositorIsActive (screen_info))
//    {
//        passdata.wireframe = wireframeCreate (c);
//    }

//    passdata.poswin = NULL;
//#ifndef SHOW_POSITION
//    scale = gdk_window_get_scale_factor (myScreenGetGdkWindow (screen_info));
//    if ((c->size->width_inc > scale) || (c->size->height_inc > scale))
//#endif /* SHOW_POSITION */
//    {
//        passdata.poswin = poswinCreate(screen_info->gscr);
//        poswinSetPosition (passdata.poswin, c);
//        poswinShow (passdata.poswin);
//    }

    /* Set window translucent while resizing */
//    if ((screen_info->params->resize_opacity < 100) &&
//        !(screen_info->params->box_resize) &&
//        !FLAG_TEST (c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED))
//    {
//        clientSetOpacity (c, c->opacity, OPACITY_RESIZE, OPACITY_RESIZE);
//    }

    /* Clear any previously saved pos flag from screen resize */
    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_SAVED_POS);

    FLAG_SET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);
//    TRACE ("entering resize loop");
//    eventFilterPush (display_info->xfilter, clientResizeEventFilter, &passdata);
//    gtk_main ();
//    eventFilterPop (display_info->xfilter);
//    TRACE ("leaving resize loop");
    if (passdata.client_gone)
    {
//        goto resize_cleanup;
    }
    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);

//    if (passdata.grab && screen_info->params->box_resize)
//    {
//        clientDrawOutline (c);
//    }

    /* Set window opacity to its original value */
//    clientSetOpacity (c, c->opacity, OPACITY_RESIZE, 0);

    if ((w_orig != c->width) || (h_orig != c->height))
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
        {
//            clientRemoveMaximizeFlag (c);
            passdata.configure_flags = CFG_FORCE_REDRAW;
        }
        if (FLAG_TEST (c->flags, CLIENT_FLAG_RESTORE_SIZE_POS))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_RESTORE_SIZE_POS);
        }
    }
    clientReconfigure (c, passdata.configure_flags);

    if (passdata.button != AnyButton && !passdata.released)
    {
        /* If this is a drag-resize, wait for the button to be released.
         * If we don't, we might get release events in the wrong place.
         */
//        eventFilterPush (display_info->xfilter, clientButtonReleaseFilter, &passdata);
//        gtk_main ();
//        eventFilterPop (display_info->xfilter);
    }

//    resize_cleanup:
//    if (passdata.poswin)
//    {
//        poswinDestroy (passdata.poswin);
//    }
//    if (passdata.wireframe)
//    {
//        wireframeDelete (passdata.wireframe);
//    }

//    myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
//    myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));

//    if (passdata.grab && screen_info->params->box_resize)
//    {
//        myDisplayUngrabServer (display_info);
//    }
}
