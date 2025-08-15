#ifndef INC_SCREEN_H
#define INC_SCREEN_H

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/render.h>
#include <X11/cursorfont.h>
#include "glib.h"

#include "hints.h"
#include "client.h"
#include "display.h"

// #include <EGL/egl.h> // requires ndk r5 or newer
// #include <GLES/gl.h>
// #include <EGL/eglext.h>
// #include <GLES2/gl2.h>
// #include <GLES2/gl2ext.h>
// #include <GLES3/gl32.h>

#define MODIFIER_MASK           (ShiftMask | \
                                 ControlMask | \
                                 AltMask | \
                                 MetaMask | \
                                 SuperMask | \
                                 HyperMask)


#define N_BUFFERS 2



struct _gaussian_conv {
    int     size;
    double  *data;
};
typedef struct _gaussian_conv gaussian_conv;


typedef enum
{
    VBLANK_OFF = 0,
    VBLANK_AUTO,
    VBLANK_XPRESENT,
    VBLANK_GLX,
    VBLANK_ERROR,
} vblankMode;

struct _ScreenInfo
{
    /* The display this screen belongs to */
    DisplayInfo *display_info;

    /* Window stacking, per screen */
    GList *windows_stack;
    Client *last_raise;
    GList *windows;
    Client *clients;
    guint client_count;
    unsigned long client_serial;
    gint key_grabs;
    gint pointer_grabs;

    /* Theme pixmaps and other params, per screen */
    // GdkRGBA title_colors[2];
    // GdkRGBA title_shadow_colors[2];
    xfwmPixmap buttons[BUTTON_COUNT][STATE_COUNT];
    xfwmPixmap corners[CORNER_COUNT][2];
    xfwmPixmap sides[SIDE_COUNT][2];
    xfwmPixmap title[TITLE_COUNT][2];
    xfwmPixmap top[TITLE_COUNT][2];

    /* Per screen graphic contexts */
    GC box_gc;

    /* Title font */
    // PangoFontDescription *font_desc;
    // PangoAttrList *pango_attr_list;

    /* Screen data */
    Colormap cmap;
    // GdkScreen *gscr;
    Screen *xscreen;
    gint depth;
    gint width;  /* Size of all output combined */
    gint height; /* Size of all output combined */
    Visual *visual;

    Window xfwm4_win;
    Window xroot;
    Window shape_win;
    gint gnome_margins[4];
    gint margins[4];
    gint screen;
    guint current_ws;
    guint previous_ws;
    gint num_monitors;
    GArray *monitors_index;

    /* Workspace definitions */
    guint workspace_count;
    gchar **workspace_names;
    int workspace_names_items;
    NetWmDesktopLayout desktop_layout;

    /* Button handler for GTK */
    gulong button_handler_id;


    /* show desktop flag */
    gboolean show_desktop;

    /* tabwin css provider */
    gboolean tabwin_provider_ready;



    Window overlay;
    Window root_overlay;
    GList *cwindows;
    // GHashTable *cwindow_hash;
    Window output;

    gaussian_conv *gaussianMap;
    gint gaussianSize;
    guchar *shadowCorner;
    guchar *shadowTop;

    gushort current_buffer;
    gushort use_n_buffers;
    Pixmap rootPixmap[N_BUFFERS];
    Picture rootBuffer[N_BUFFERS];
    Picture zoomBuffer;
    Picture rootPicture;
    Picture blackPicture;
    Picture rootTile;
    XID screenRegion;
    XserverRegion prevDamage;
    XserverRegion allDamage;
    unsigned long cursorSerial;
    Picture cursorPicture;
    gint cursorOffsetX;
    gint cursorOffsetY;
    XRectangle cursorLocation;
    gboolean cursor_is_zoomed;

    guint wins_unredirected;
    gboolean compositor_active;
    gboolean clipChanged;

    gboolean damages_pending;

    guint compositor_timeout_id;

    // XTransform transform;
    gboolean zoomed;
    guint zoom_timeout_id;
    gboolean use_glx;
    gboolean use_present;

    vblankMode vblank_mode;

    gboolean texture_inverted;
    gboolean has_mesa_swap_control;
    gboolean has_ext_swap_control;
    gboolean has_ext_swap_control_tear;
    gboolean has_ext_arb_sync;
    // GLuint rootTexture;
    // GLenum texture_format;
    // GLenum texture_target;
    // GLenum texture_type;
    // GLfloat texture_filter;
    // GLXDrawable glx_drawable[N_BUFFERS];
    // GLXFBConfig glx_fbconfig;
    // GLXContext glx_context;
    // GLXWindow glx_window;
    // GLsync gl_sync;
    // XSyncFence fence[N_BUFFERS];
    gboolean present_pending;
};

gboolean                 myScreenCheckWMAtom                    (ScreenInfo *,
                                                                 Atom atom);
Display                 *myScreenGetXDisplay                    (ScreenInfo *);

ScreenInfo              *myScreenInit                           (DisplayInfo *,
                                                                 unsigned long,
                                                                 int,
                                                                 Window,
                                                                 Window);

#endif /* INC_SCREEN_H */
