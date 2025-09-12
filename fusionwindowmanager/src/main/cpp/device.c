

#include "device.h"
#include "display.h"
#include "native_log.h"

#define xfwm_device_fill_meta(evtype, evwindow, evdevice) \
{ \
    if (event == NULL) \
    { \
        event = g_new0 (XfwmEvent, 1); \
    } \
    event->meta.type = evtype; \
    event->meta.window = evwindow; \
    event->meta.device = evdevice; \
    event->meta.xevent = xevent; \
}

static XfwmEvent *
xfwm_device_translate_event_key_core (XEvent *xevent, XfwmEvent *event)
{
    xfwm_device_fill_meta (XFWM_EVENT_KEY, xevent->xany.window, None);

    event->key.root = xevent->xkey.root;
    event->key.pressed = xevent->type == KeyPress;
    event->key.keycode = xevent->xkey.keycode;
    event->key.state = xevent->xkey.state;
    event->key.time = xevent->xkey.time;

    return (XfwmEvent *)event;
}

static XfwmEvent *
xfwm_device_translate_event_button_core (XEvent *xevent, XfwmEvent *event)
{
    xfwm_device_fill_meta (XFWM_EVENT_BUTTON, xevent->xany.window, None);

    event->button.root = xevent->xbutton.root;
    event->button.subwindow = xevent->xbutton.subwindow;
    event->button.pressed = xevent->type == ButtonPress;
    event->button.button = xevent->xbutton.button;
    event->button.state = xevent->xbutton.state;
    event->button.x = xevent->xbutton.x;
    event->button.y = xevent->xbutton.y;
    event->button.x_root = xevent->xbutton.x_root;
    event->button.y_root = xevent->xbutton.y_root;
    event->button.time = xevent->xbutton.time;

    return (XfwmEvent *)event;
}

static XfwmEvent *
xfwm_device_translate_event_motion_core (XEvent *xevent, XfwmEvent *event)
{
    xfwm_device_fill_meta (XFWM_EVENT_MOTION, xevent->xany.window, None);

    event->motion.x = xevent->xbutton.x;
    event->motion.y = xevent->xbutton.y;
    event->motion.x_root = xevent->xbutton.x_root;
    event->motion.y_root = xevent->xbutton.y_root;
    event->motion.time = xevent->xbutton.time;

    return (XfwmEvent *)event;
}

static XfwmEvent *
xfwm_device_translate_event_crossing_core (XEvent *xevent, XfwmEvent *event)
{
    xfwm_device_fill_meta (XFWM_EVENT_CROSSING, xevent->xany.window, None);

    event->crossing.root = xevent->xcrossing.root;
    event->crossing.enter = xevent->type == EnterNotify;
    event->crossing.mode = xevent->xcrossing.mode;
    event->crossing.detail = xevent->xcrossing.detail;
    event->crossing.x_root = xevent->xcrossing.x_root;
    event->crossing.y_root = xevent->xcrossing.y_root;
    event->crossing.time = xevent->xcrossing.time;

    return (XfwmEvent *)event;
}

static XfwmEvent *
xfwm_device_translate_event_common (XEvent *xevent, XfwmEvent *event)
{
    xfwm_device_fill_meta (XFWM_EVENT_XEVENT, xevent->xany.window, None);

    return event;
}

XfwmEvent *
xfwm_device_translate_event (XfwmDevices *devices, XEvent *xevent, XfwmEvent *event) {
    switch (xevent->type) {
        case KeyPress:
        case KeyRelease:
            return xfwm_device_translate_event_key_core(xevent, event);
        case ButtonPress:
        case ButtonRelease:
            return xfwm_device_translate_event_button_core(xevent, event);
        case MotionNotify:
            return xfwm_device_translate_event_motion_core(xevent, event);
        case EnterNotify:
        case LeaveNotify:
            return xfwm_device_translate_event_crossing_core(xevent, event);

            return xfwm_device_translate_event_common(xevent, event);
    }
}

gboolean
xfwm_device_grab (XfwmDevices *devices, XfwmDevice *device, Display *display,
                  Window grab_window, gboolean owner_events, guint event_mask,
                  gint grab_mode, Window confine_to, Cursor cursor, Time time)
{
    gboolean result;
    Status status;
    if (device->keyboard)
    {
        status = XGrabKeyboard (display, grab_window, owner_events,
                                grab_mode, grab_mode, time);
        result = (status == GrabSuccess);
    }
    else
    {
        status = XGrabPointer (display, grab_window, owner_events, event_mask,
                               grab_mode, grab_mode, confine_to, cursor, time);
        result = (status == GrabSuccess);
    }
    return result;
}

void
xfwm_device_ungrab (XfwmDevices *devices, XfwmDevice *device, Display *display, Time time)
{
    if (device->keyboard)
    {
        XUngrabKeyboard (display, time);
    }
    else
    {
        XUngrabPointer (display, time);
    }
}


void
xfwm_device_free_event (XfwmEvent *event)
{
    g_free (event);
}


