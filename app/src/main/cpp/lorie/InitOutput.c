/*

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma ide diagnostic ignored "cppcoreguidelines-narrowing-conversions"
#pragma ide diagnostic ignored "cert-err34-c"
#pragma ide diagnostic ignored "ConstantConditionsOC"
#pragma ide diagnostic ignored "ConstantFunctionResult"
#pragma ide diagnostic ignored "bugprone-integer-division"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wformat-nonliteral"

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <stdio.h>
#include <sys/timerfd.h>
#include <sys/errno.h>
#include <libxcvt/libxcvt.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/hardware_buffer.h>
#include <sys/wait.h>
#include <selection.h>
#include <X11/Xatom.h>
#include <present.h>
#include <present_priv.h>
#include <dri3.h>
#include <sys/mman.h>
#include <busfault.h>
#include <android/native_window_jni.h>
#include "scrnintstr.h"
#include "servermd.h"
#include "fb.h"
#include "input.h"
#include "mipointer.h"
#include "micmap.h"
#include "dix.h"
#include "miline.h"
#include "glx_extinit.h"
#include "randrstr.h"
#include "damagestr.h"
#include "cursorstr.h"
#include "propertyst.h"
#include "shmint.h"
#include "glxserver.h"
#include "glxutil.h"
#include "fbconfigs.h"

#include "renderer.h"
#include "inpututils.h"
#include "lorie.h"
#include "../xserver/dix/enterleave.h"

#define unused __attribute__((unused))
#define wrap(priv, real, mem, func) { priv->mem = real->mem; real->mem = func; }
#define unwrap(priv, real, mem) { real->mem = priv->mem; }
#define USAGE (AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN)

#define PRINT_LOG 0
#define log(prio, ...) if(PRINT_LOG){ __android_log_print(ANDROID_LOG_ ## prio, "huyang_InitOutput", __VA_ARGS__);}              \

#define logh(...) if(PRINT_LOG){__android_log_print(ANDROID_LOG_ERROR, "huyang_InitOutput", __VA_ARGS__);}               \

extern DeviceIntPtr lorieMouse, lorieMouseRelative, lorieTouch, lorieKeyboard;

typedef struct {
    DestroyPixmapProcPtr DestroyPixmap;
    CloseScreenProcPtr CloseScreen;
    CreateScreenResourcesProcPtr CreateScreenResources;

    DamagePtr damage, damage1;
    OsTimerPtr redrawTimer;
    OsTimerPtr fpsTimer;

    Bool cursorMoved;
    int timerFd;

    struct {
        AHardwareBuffer* buffer, *buffer1, *buffer2;
        Bool locked;
        Bool legacyDrawing;
        uint8_t flip;
        uint32_t width, height;
    } root;

    JavaVM* vm;
    JNIEnv* env;
} lorieScreenInfo, *lorieScreenInfoPtr;

int init_cusor;
ScreenPtr pScreenPtr;
WindowPtr separateWindowPtr1;
WindowPtr separateWindowPtr2;
PixmapPtr tempPixmap1;
PixmapPtr tempPixmap2;
static lorieScreenInfo lorieScreen = { .root.width = 1280, .root.height = 1024 };
static lorieScreenInfoPtr pvfb = &lorieScreen;
static char *xstartup = NULL;
static DevPrivateKeyRec loriePixmapPrivateKeyRec;


static Bool TrueNoop() { return TRUE; }
static Bool FalseNoop() { return FALSE; }
static void VoidNoop() {}
static void lorieInitSelectionCallback();

void
ddxGiveUp(unused enum ExitCode error) {
    logh("ddxGiveUp");
    log(ERROR, "Server stopped (%d)", error);
    CloseWellKnownConnections();
    UnlockServer();
    exit(error);
}

static void* ddxReadyThread(unused void* cookie) {
    if (xstartup && serverGeneration == 1) {
        pid_t pid = fork();
        logh("ddxReadyThread pid:%d", pid);
        if (!pid) {
            char DISPLAY[16] = "";
            sprintf(DISPLAY, ":%s", display);
            setenv("DISPLAY", DISPLAY, 1);
            execlp("sh", "sh", "-c", xstartup, NULL);
            dprintf(2, "Failed to start command `sh -c \"%s\"`: %s\n", xstartup, strerror(errno));
            abort();
        } else {
            int status;
            do {
                pid_t w = waitpid(pid, &status, 0);
                if (w == -1) {
                    perror("waitpid");
                    GiveUp(SIGKILL);
                }

                if (WIFEXITED(status)) {
                    printf("%d exited, status=%d\n", w, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    printf("%d killed by signal %d\n", w, WTERMSIG(status));
                } else if (WIFSTOPPED(status)) {
                    printf("%d stopped by signal %d\n", w, WSTOPSIG(status));
                } else if (WIFCONTINUED(status)) {
                    printf("%d continued\n", w);
                }
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            GiveUp(SIGINT);
        }
    }

    return NULL;
}

void
ddxReady(void) {
    logh("ddxReady");
    pthread_t t;
    pthread_create(&t, NULL, ddxReadyThread, NULL);
}

void
OsVendorInit(void) {
}

void
OsVendorFatalError(unused const char *f, unused va_list args) {
    log(ERROR, f, args);
}

#if defined(DDXBEFORERESET)
void
ddxBeforeReset(void) {
    return;
}
#endif

#if INPUTTHREAD
/** This function is called in Xserver/os/inputthread.c when starting
    the input thread. */
void
ddxInputThreadInit(void) {}
#endif

void ddxUseMsg(void) {
    ErrorF("-xstartup \"command\"    start `command` after server startup\n");
    ErrorF("-legacy-drawing        use legacy drawing, without using AHardwareBuffers\n");
    ErrorF("-force-bgra            force flipping colours (RGBA->BGRA)\n");
}

int ddxProcessArgument(unused int argc, unused char *argv[], unused int i) {
    for(int i =0;i < argc; i++){
        logh("ddxReady argv %s", argv[i]);
    }

    if (strcmp(argv[i], "-xstartup") == 0) {  /* -xstartup "command" */
        CHECK_FOR_REQUIRED_ARGUMENTS(1);
        xstartup = argv[++i];
        return 2;
    }

    if (strcmp(argv[i], "-legacy-drawing") == 0) {
        pvfb->root.legacyDrawing = TRUE;
        return 1;
    }

    if (strcmp(argv[i], "-force-bgra") == 0) {
        pvfb->root.flip = TRUE;
        return 1;
    }

    return 0;
}

static RRModePtr lorieCvt(int width, int height, int framerate) {
    struct libxcvt_mode_info *info;
    char name[128];
    xRRModeInfo modeinfo = {0};
    RRModePtr mode;
//    logh("lorieCvt width:%d height:%d framerate:%d", width, height, framerate);

    info = libxcvt_gen_mode_info(width, height, framerate, 0, 0);

    snprintf(name, sizeof name, "%dx%d", info->hdisplay, info->vdisplay);
    modeinfo.nameLength = strlen(name);
    modeinfo.width      = info->hdisplay;
    modeinfo.height     = info->vdisplay;
    modeinfo.dotClock   = info->dot_clock * 1000.0;
    modeinfo.hSyncStart = info->hsync_start;
    modeinfo.hSyncEnd   = info->hsync_end;
    modeinfo.hTotal     = info->htotal;
    modeinfo.vSyncStart = info->vsync_start;
    modeinfo.vSyncEnd   = info->vsync_end;
    modeinfo.vTotal     = info->vtotal;
    modeinfo.modeFlags  = info->mode_flags;

    mode = RRModeGet(&modeinfo, name);
    free(info);
    return mode;
}

static void lorieMoveCursor(unused DeviceIntPtr pDev, unused ScreenPtr pScr, int x, int y) {
    renderer_set_cursor_coordinates(x, y);
    pvfb->cursorMoved = TRUE;
}

static void lorieConvertCursor(CursorPtr pCurs, CARD32 *data) {
    CursorBitsPtr bits = pCurs->bits;
    if (bits->argb) {
        for (int i = 0; i < bits->width * bits->height; i++) {
            /* Convert bgra to rgba */
            CARD32 p = bits->argb[i];
            data[i] = (p & 0xFF000000) | ((p & 0x00FF0000) >> 16) | (p & 0x0000FF00) | ((p & 0x000000FF) << 16);
        }
    } else {
        CARD32 d, fg, bg, *p;
        int x, y, stride, i, bit;

        p = data;
        fg = ((pCurs->foreBlue & 0xff00) << 8) | (pCurs->foreGreen & 0xff00) | (pCurs->foreRed >> 8);
        bg = ((pCurs->backBlue & 0xff00) << 8) | (pCurs->backGreen & 0xff00) | (pCurs->backRed >> 8);
        stride = BitmapBytePad(bits->width);
        for (y = 0; y < bits->height; y++)
            for (x = 0; x < bits->width; x++) {
                i = y * stride + x / 8;
                bit = 1 << (x & 7);
                d = (bits->source[i] & bit) ? fg : bg;
                d = (bits->mask[i] & bit) ? d | 0xff000000 : 0x00000000;
                *p++ = d;
            }
    }
}


static void lorieSetCursor(unused DeviceIntPtr pDev, unused ScreenPtr pScr, CursorPtr pCurs, int x0, int y0) {
    CursorBitsPtr bits = pCurs ? pCurs->bits : NULL;
    if (pCurs && bits) {
        CARD32 data[bits->width * bits->height * 4];

        lorieConvertCursor(pCurs, data);
        renderer_update_cursor(bits->width, bits->height, bits->xhot, bits->yhot, data);
    } else
        renderer_update_cursor(0, 0, 0, 0, NULL);

    if (x0 >= 0 && y0 >= 0) {
        init_cusor++;
        logh("lorieSetCursor x0:%d y0:%d", x0, y0);
        if (init_cusor > 1) {
            lorieMoveCursor(NULL, NULL, x0, y0);
        }
    }
}

static miPointerSpriteFuncRec loriePointerSpriteFuncs = {
        .RealizeCursor = TrueNoop,
        .UnrealizeCursor = TrueNoop,
        .SetCursor = lorieSetCursor,
        .MoveCursor = lorieMoveCursor,
        .DeviceCursorInitialize = TrueNoop,
        .DeviceCursorCleanup = VoidNoop
};

static miPointerScreenFuncRec loriePointerCursorFuncs = {
        .CursorOffScreen = FalseNoop,
        .CrossScreen = VoidNoop,
        .WarpCursor = miPointerWarpCursor
};

static void lorieUpdateBuffer(void) {
//    logh("lorieUpdateBuffer legacydraw %d", pvfb->root.legacyDrawing);
    AHardwareBuffer_Desc d0 = {}, d1 = {};
    AHardwareBuffer *new = NULL, *old = pvfb->root.buffer;
    int status, wasLocked = pvfb->root.locked;
    void *data0 = NULL, *data1 = NULL;

    if (pvfb->root.legacyDrawing) {
        PixmapPtr pixmap = (PixmapPtr) pScreenPtr->devPrivate;
        DrawablePtr draw = &pixmap->drawable;
        data0 = malloc(pScreenPtr->width * pScreenPtr->height * 4);
        data1 = (draw->width && draw->height) ? pixmap->devPrivate.ptr : NULL;
        if (data1)
            pixman_blt(data1, data0, draw->width, pScreenPtr->width, 32, 32, 0, 0, 0, 0,
                       min(draw->width, pScreenPtr->width), min(draw->height, pScreenPtr->height));
        pScreenPtr->ModifyPixmapHeader(pScreenPtr->devPrivate, pScreenPtr->width, pScreenPtr->height, 32, 32, pScreenPtr->width * 4, data0);
        free(data1);
        return;
    }

    if (pScreenPtr->devPrivate) {
        d0.width = pScreenPtr->width;
        d0.height = pScreenPtr->height;
        d0.layers = 1;
        d0.usage = USAGE;
        d0.format = pvfb->root.flip
                ? AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM
                : AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM;

        /* I could use this, but in this case I must swap colours in the shader. */
        // desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;

        status = AHardwareBuffer_allocate(&d0, &new);
        if (status != 0)
            FatalError("Failed to allocate root window pixmap (error %d)", status);

        AHardwareBuffer_describe(new, &d0);
        status = AHardwareBuffer_lock(new, USAGE, -1, NULL, &data0);
        if (status != 0)
            FatalError("Failed to lock root window pixmap (error %d)", status);
        logh("lorieUpdateBuffer pvfb->root.buffer = %p", new);
        pvfb->root.buffer = new;
        pvfb->root.locked = TRUE;

        pScreenPtr->ModifyPixmapHeader(pScreenPtr->devPrivate, d0.width, d0.height, 32, 32, d0.stride * 4, data0);

        renderer_set_buffer(pvfb->env, new);
    }

    if (old) {
        if (wasLocked)
            AHardwareBuffer_unlock(old, NULL);

        if (new && pvfb->root.locked) {
            /*
             * It is pretty easy. If there is old pixmap we should copy it's contents to new pixmap.
             * If it is impossible we should simply request root window exposure.
             */
            AHardwareBuffer_describe(old, &d1);
            status = AHardwareBuffer_lock(old, USAGE, -1, NULL, &data1);
            if (status == 0) {
                pixman_blt(data1, data0, d1.stride, d0.stride,
                           32, 32, 0, 0, 0, 0,
                           min(d1.width, d0.width), min(d1.height, d0.height));
                AHardwareBuffer_unlock(old, NULL);
            } else {
                RegionRec reg;
                BoxRec box = {.x1 = 0, .y1 = 0, .x2 = d0.width, .y2 = d0.height};
                RegionInit(&reg, &box, 1);
                pScreenPtr->WindowExposures(pScreenPtr->root, &reg);
                RegionUninit(&reg);
                AHardwareBuffer_release(old);
                return;
            }
        }
        AHardwareBuffer_release(old);
    }
}

static inline void loriePixmapUnlock(PixmapPtr pixmap) {
//    logh("loriePixmapUnlock");
    if (pvfb->root.legacyDrawing)
        return renderer_update_root(pixmap->drawable.width, pixmap->drawable.height, pixmap->devPrivate.ptr, pvfb->root.flip);

    if (pvfb->root.locked)
        AHardwareBuffer_unlock(pvfb->root.buffer, NULL);

    pvfb->root.locked = FALSE;
    pixmap->drawable.pScreen->ModifyPixmapHeader(pixmap, -1, -1, -1, -1, -1, NULL);
}

static inline Bool loriePixmapLock(PixmapPtr pixmap) {
//    logh("loriePixmapLock");
    AHardwareBuffer_Desc desc = {};
    void *data;
    int status;

    if (pvfb->root.legacyDrawing)
        return TRUE;

    if (!pvfb->root.buffer) {
        pvfb->root.locked = FALSE;
        return FALSE;
    }

    AHardwareBuffer_describe(pvfb->root.buffer, &desc);
    status = AHardwareBuffer_lock(pvfb->root.buffer, desc.usage, -1, NULL, &data);
    pvfb->root.locked = status == 0;
    if (pvfb->root.locked)
        pixmap->drawable.pScreen->ModifyPixmapHeader(pixmap, desc.width, desc.height, -1, -1, desc.stride * 4, data);
    else
        FatalError("Failed to lock surface: %d\n", status);

    return pvfb->root.locked;
}

static void lorieTimerCallback(int fd, unused int r, void *arg) {
//    logh("lorieTimerCallback");
    char dummy[8];
    read(fd, dummy, 8);
    if (renderer_should_redraw() && RegionNotEmpty(DamageRegion(pvfb->damage))) {
//        logh("RegionNotEmpty");
        int redrawn = FALSE;
        ScreenPtr pScreen = (ScreenPtr) arg;

        loriePixmapUnlock(pScreen->GetScreenPixmap(pScreen));
        redrawn = renderer_redraw(pvfb->env, pvfb->root.flip);
        if (loriePixmapLock(pScreen->GetScreenPixmap(pScreen)) && redrawn){
//            logh("DamageEmpty");
            DamageEmpty(pvfb->damage);
        }
    } else if (pvfb->cursorMoved)
        renderer_redraw(pvfb->env, pvfb->root.flip);

    pvfb->cursorMoved = FALSE;
}

static CARD32 lorieFramecounter(unused OsTimerPtr timer, unused CARD32 time, unused void *arg) {
    renderer_print_fps(5000);
    return 5000;
}

static Bool lorieCreateScreenResources(ScreenPtr pScreen) {
    Bool ret;
    pScreen->CreateScreenResources = pvfb->CreateScreenResources;
    logh("lorieCreateScreenResources");
    ret = pScreen->CreateScreenResources(pScreen);
    if (!ret)
        return FALSE;

    pScreen->devPrivate = fbCreatePixmap(pScreen, 0, 0, pScreen->rootDepth, CREATE_PIXMAP_USAGE_BACKING_PIXMAP);

    pvfb->damage = DamageCreate(NULL, NULL, DamageReportNone, TRUE, pScreen, NULL);
    if (!pvfb->damage)
        FatalError("Couldn't setup damage\n");

    DamageRegister(&(*pScreen->GetScreenPixmap)(pScreen)->drawable, pvfb->damage);
    pvfb->fpsTimer = TimerSet(NULL, 0, 5000, lorieFramecounter, pScreen);
    lorieUpdateBuffer();
//    lorieUpdateBufferSeparate();

    return TRUE;
}

static Bool
lorieCloseScreen(ScreenPtr pScreen) {
    unwrap(pvfb, pScreen, CloseScreen)
    // No need to call fbDestroyPixmap since AllocatePixmap sets pixmap as PRIVATE_SCREEN so it is destroyed automatically.
    return pScreen->CloseScreen(pScreen);
}

static Bool
lorieDestroyPixmap(PixmapPtr pPixmap) {
    Bool ret;
    void *ptr = NULL;
    size_t size = 0;

    if (pPixmap->refcnt == 1) {
        ptr = dixLookupPrivate(&pPixmap->devPrivates, &loriePixmapPrivateKeyRec);
        size = pPixmap->devKind * pPixmap->drawable.height;
    }

    unwrap(pvfb, pScreenPtr, DestroyPixmap)
    ret = (*pScreenPtr->DestroyPixmap) (pPixmap);
    wrap(pvfb, pScreenPtr, DestroyPixmap, lorieDestroyPixmap)

    if (ptr)
        munmap(ptr, size);
    return ret;
}

static Bool
lorieRRScreenSetSize(ScreenPtr pScreen, CARD16 width, CARD16 height, unused CARD32 mmWidth, unused CARD32 mmHeight) {
    SetRootClip(pScreen, ROOT_CLIP_NONE);
//    logh("lorieRRScreenSetSize width:%d height:%d", width, height);
    pvfb->root.width = pScreen->width = width;
    pvfb->root.height = pScreen->height = height;
    pScreen->mmWidth = ((double) (width)) * 25.4 / monitorResolution;
    pScreen->mmHeight = ((double) (height)) * 25.4 / monitorResolution;
    lorieUpdateBuffer();
//    lorieUpdateBufferSeparate();
    pScreen->ResizeWindow(pScreen->root, 0, 0, width, height, NULL);
    DamageEmpty(pvfb->damage);
    SetRootClip(pScreen, ROOT_CLIP_FULL);

    RRScreenSizeNotify(pScreen);
    update_desktop_dimensions();
    pvfb->cursorMoved = TRUE;

    return TRUE;
}

static Bool
lorieRRCrtcSet(unused ScreenPtr pScreen, RRCrtcPtr crtc, RRModePtr mode, int x, int y,
               Rotation rotation, int numOutput, RROutputPtr *outputs) {
    return RRCrtcNotify(crtc, mode, x, y, rotation, NULL, numOutput, outputs);
}

static Bool
lorieRRGetInfo(unused ScreenPtr pScreen, Rotation *rotations) {
    *rotations = RR_Rotate_0;
    return TRUE;
}

static Bool
lorieRandRInit(ScreenPtr pScreen) {
    rrScrPrivPtr pScrPriv;
    RROutputPtr output;
    RRCrtcPtr crtc;
    RRModePtr mode;
    logh("lorieRandRInit");

    if (!RRScreenInit(pScreen))
        return FALSE;

    pScrPriv = rrGetScrPriv(pScreen);
    pScrPriv->rrGetInfo = lorieRRGetInfo;
    pScrPriv->rrCrtcSet = lorieRRCrtcSet;
    pScrPriv->rrScreenSetSize = lorieRRScreenSetSize;

    RRScreenSetSizeRange(pScreen, 1, 1, 32767, 32767);

    if (FALSE
        || !(mode = lorieCvt(pScreen->width, pScreen->height, 30))
        || !(crtc = RRCrtcCreate(pScreen, NULL))
        || !RRCrtcGammaSetSize(crtc, 256)
        || !(output = RROutputCreate(pScreen, "screen", 6, NULL))
        || !RROutputSetClones(output, NULL, 0)
        || !RROutputSetModes(output, &mode, 1, 0)
        || !RROutputSetCrtcs(output, &crtc, 1)
        || !RROutputSetConnection(output, RR_Connected)
        || !RRCrtcNotify(crtc, mode, 0, 0, RR_Rotate_0, NULL, 1, &output))
        return FALSE;
    return TRUE;
}

static Bool resetRootCursor(unused ClientPtr pClient, unused void *closure) {
    CursorVisible = TRUE;
    pScreenPtr->DisplayCursor(lorieMouse, pScreenPtr, NullCursor);
    pScreenPtr->DisplayCursor(lorieMouse, pScreenPtr, rootCursor);
    return TRUE;
}

static PixmapPtr loriePixmapFromFds(ScreenPtr screen, CARD8 num_fds, const int *fds, CARD16 width, CARD16 height,
                                    const CARD32 *strides, const CARD32 *offsets, CARD8 depth, unused CARD8 bpp, CARD64 modifier) {
    const CARD64 RAW_MMAPPABLE_FD = 1274;
    PixmapPtr pixmap;
    void *addr = NULL;
    if (num_fds != 1 || modifier != RAW_MMAPPABLE_FD) {
        log(ERROR, "DRI3: More than 1 fd or modifier is not RAW_MMAPPABLE_FD");
        return NULL;
    }
    logh("loriePixmapFromFds");

    addr = mmap(NULL, strides[0] * height, PROT_READ, MAP_SHARED, fds[0], offsets[0]);
    if (!addr || addr == MAP_FAILED) {
        log(ERROR, "DRI3: mmap failed");
        return NULL;
    }

    pixmap = fbCreatePixmap(screen, 0, 0, depth, 0);
    if (!pixmap) {
        log(ERROR, "DRI3: failed to create pixmap");
        munmap(addr, strides[0] * height);
        return NULL;
    }

    dixSetPrivate(&pixmap->devPrivates, &loriePixmapPrivateKeyRec, addr);
    screen->ModifyPixmapHeader(pixmap, width, height, 0, 0, strides[0], addr);

    return pixmap;
}

static int lorieGetFormats(unused ScreenPtr screen, CARD32 *num_formats, CARD32 **formats) {
    *num_formats = 0;
    *formats = NULL;
    return TRUE;
}

static int lorieGetModifiers(unused ScreenPtr screen, unused uint32_t format, uint32_t *num_modifiers, uint64_t **modifiers) {
    *num_modifiers = 0;
    *modifiers = NULL;
    return TRUE;
}

static Bool
lorieScreenInit(ScreenPtr pScreen, unused int argc, unused char **argv) {
    static int timerFd = -1;
    pScreenPtr = pScreen;
    logh("lorieScreenInit");

    if (timerFd == -1) {
        struct itimerspec spec = {0};
        timerFd = timerfd_create(CLOCK_MONOTONIC,  0);
        timerfd_settime(timerFd, 0, &spec, NULL);
    }

    pvfb->timerFd = timerFd;
    SetNotifyFd(timerFd, lorieTimerCallback, X_NOTIFY_READ, pScreen);

    miSetZeroLineBias(pScreen, 0);
    pScreen->blackPixel = 0;
    pScreen->whitePixel = 1;
    static dri3_screen_info_rec dri3Info = {
            .version = 2,
            .fds_from_pixmap = FalseNoop,
            .pixmap_from_fds = loriePixmapFromFds,
            .get_formats = lorieGetFormats,
            .get_modifiers = lorieGetModifiers,
            .get_drawable_modifiers = FalseNoop
    };

    if (FALSE
          || !miSetVisualTypesAndMasks(24, ((1 << TrueColor) | (1 << DirectColor)), 8, TrueColor, 0xFF0000, 0x00FF00, 0x0000FF)
          || !miSetPixmapDepths()
          || !fbScreenInit(pScreen, NULL, pvfb->root.width, pvfb->root.height, monitorResolution, monitorResolution, 0, 32)
          || !fbPictureInit(pScreen, 0, 0)
          || !lorieRandRInit(pScreen)
          || !miPointerInitialize(pScreen, &loriePointerSpriteFuncs, &loriePointerCursorFuncs, TRUE)
          || !fbCreateDefColormap(pScreen)
          || !dri3_screen_init(pScreen, &dri3Info)
          || !dixRegisterPrivateKey(&loriePixmapPrivateKeyRec, PRIVATE_PIXMAP, 0))
        return FALSE;

    wrap(pvfb, pScreen, CreateScreenResources, lorieCreateScreenResources)
    wrap(pvfb, pScreen, CloseScreen, lorieCloseScreen)
    wrap(pvfb, pScreen, DestroyPixmap, lorieDestroyPixmap)

    QueueWorkProc(resetRootCursor, NULL, NULL);
    ShmRegisterFbFuncs(pScreen);

    return TRUE;
}                               /* end lorieScreenInit */

// From xfixes/cursor.c
static CursorPtr
CursorForDevice(DeviceIntPtr pDev) {
    if (!CursorVisible || !EnableCursor)
        return NULL;

    if (pDev && pDev->spriteInfo) {
        if (pDev->spriteInfo->anim.pCursor)
            return pDev->spriteInfo->anim.pCursor;
        return pDev->spriteInfo->sprite ? pDev->spriteInfo->sprite->current : NULL;
    }

    return NULL;
}

Bool lorieChangeWindow(unused ClientPtr pClient, void *closure) {
    SurfaceRes *res = (SurfaceRes *) closure;
    jobject surface = res->surface;
    if(res->id == 0){
        res->pWin = pScreenPtr->root;
    }
    init_cusor = 0;
    logh("lorieChangeWindow buffer:%p  id:%d surface:%p ",
         pvfb->root.buffer, res->id, surface);
    renderer_set_window_each(pvfb->env, res, pvfb->root.buffer);
//    renderer_set_window(pvfb->env, surface, pvfb->root.buffer);
    lorieSetCursor(NULL, NULL, CursorForDevice(GetMaster(lorieMouse, MASTER_POINTER)), -1, -1);
    renderer_update_root(pScreenPtr->width, pScreenPtr->height,
                             ((PixmapPtr) pScreenPtr->devPrivate)->devPrivate.ptr,
                             pvfb->root.flip);
    renderer_redraw(pvfb->env, pvfb->root.flip);
    return TRUE;
}

void lorieConfigureNotify(int width, int height, int framerate) {
    ScreenPtr pScreen = pScreenPtr;
    RROutputPtr output = RRFirstOutput(pScreen);
//    logh("lorieConfigureNotify");

    if (output && width && height && (pScreen->width != width || pScreen->height != height)) {
        CARD32 mmWidth, mmHeight;
        RRModePtr mode = lorieCvt(width, height, framerate);
        mmWidth = ((double) (mode->mode.width)) * 25.4 / monitorResolution;
        mmHeight = ((double) (mode->mode.width)) * 25.4 / monitorResolution;
        RROutputSetModes(output, &mode, 1, 0);
        RRCrtcNotify(RRFirstEnabledCrtc(pScreen), mode,0, 0,RR_Rotate_0, NULL, 1, &output);
        RRScreenSizeSet(pScreen, mode->mode.width, mode->mode.height, mmWidth, mmHeight);
    }

    if (framerate > 0) {
        long nsecs = 1000 * 1000 * 1000 / framerate;
        struct itimerspec spec = { { 0, nsecs }, { 0, nsecs } };
        timerfd_settime(lorieScreen.timerFd, 0, &spec, NULL);
//        log(VERBOSE, "New framerate is %d", framerate);

        FakeScreenFps = framerate;
        present_fake_screen_init(pScreen);
    }
}

void
InitOutput(ScreenInfo * screen_info, int argc, char **argv) {
    int depths[] = { 1, 4, 8, 15, 16, 24, 32 };
    int bpp[] =    { 1, 8, 8, 16, 16, 32, 32 };
    int i;
    logh("InitOutput");

    if (monitorResolution == 0)
        monitorResolution = 96;

    for(i = 0; i < ARRAY_SIZE(depths); i++) {
        screen_info->formats[i].depth = depths[i];
        screen_info->formats[i].bitsPerPixel = bpp[i];
        screen_info->formats[i].scanlinePad = BITMAP_SCANLINE_PAD;
    }

    screen_info->imageByteOrder = IMAGE_BYTE_ORDER;
    screen_info->bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
    screen_info->bitmapScanlinePad = BITMAP_SCANLINE_PAD;
    screen_info->bitmapBitOrder = BITMAP_BIT_ORDER;
    screen_info->numPixmapFormats = ARRAY_SIZE(depths);

    renderer_init(pvfb->env, &pvfb->root.legacyDrawing, &pvfb->root.flip);
    xorgGlxCreateVendor();
    lorieInitSelectionCallback();

    if (-1 == AddScreen(lorieScreenInit, argc, argv)) {
        FatalError("Couldn't add screen %d\n", i);
    }
}

void lorieSetVM(JavaVM* vm) {
    pvfb->vm = vm;
    (*vm)->AttachCurrentThread(vm, &pvfb->env, NULL);
}

static GLboolean drawableSwapBuffers(unused ClientPtr client, unused __GLXdrawable * drawable) { return TRUE; }
static void drawableCopySubBuffer(unused __GLXdrawable * basePrivate, unused int x, unused int y, unused int w, unused int h) {}
static __GLXdrawable * createDrawable(unused ClientPtr client, __GLXscreen * screen, DrawablePtr pDraw,
                                      unused XID drawId, int type, XID glxDrawId, __GLXconfig * glxConfig) {
    __GLXdrawable *private = calloc(1, sizeof *private);
    if (private == NULL)
        return NULL;

    if (!__glXDrawableInit(private, screen, pDraw, type, glxDrawId, glxConfig)) {
        free(private);
        return NULL;
    }

    private->destroy = (void (*)(__GLXdrawable *)) free;
    private->swapBuffers = drawableSwapBuffers;
    private->copySubBuffer = drawableCopySubBuffer;

    return private;
}

static void glXDRIscreenDestroy(__GLXscreen *baseScreen) {
    free(baseScreen->GLXextensions);
    free(baseScreen->GLextensions);
    free(baseScreen->visuals);
    free(baseScreen);
}

static __GLXscreen *glXDRIscreenProbe(ScreenPtr pScreen) {
    __GLXscreen *screen;

    screen = calloc(1, sizeof *screen);
    if (screen == NULL)
        return NULL;

    screen->destroy = glXDRIscreenDestroy;
    screen->createDrawable = createDrawable;
    screen->pScreen = pScreen;
    screen->fbconfigs = configs;
    screen->glvnd = "mesa";

    __glXInitExtensionEnableBits(screen->glx_enable_bits);
    /* There is no real GLX support, but anyways swrast reports it. */
    __glXEnableExtension(screen->glx_enable_bits, "GLX_MESA_copy_sub_buffer");
    __glXEnableExtension(screen->glx_enable_bits, "GLX_EXT_no_config_context");
    __glXEnableExtension(screen->glx_enable_bits, "GLX_ARB_create_context");
    __glXEnableExtension(screen->glx_enable_bits, "GLX_ARB_create_context_no_error");
    __glXEnableExtension(screen->glx_enable_bits, "GLX_ARB_create_context_profile");
    __glXEnableExtension(screen->glx_enable_bits, "GLX_EXT_create_context_es_profile");
    __glXEnableExtension(screen->glx_enable_bits, "GLX_EXT_create_context_es2_profile");
    __glXEnableExtension(screen->glx_enable_bits, "GLX_EXT_framebuffer_sRGB");
    __glXEnableExtension(screen->glx_enable_bits, "GLX_ARB_fbconfig_float");
    __glXEnableExtension(screen->glx_enable_bits, "GLX_EXT_fbconfig_packed_float");
    __glXEnableExtension(screen->glx_enable_bits, "GLX_EXT_texture_from_pixmap");
    __glXScreenInit(screen, pScreen);

    return screen;
}

__GLXprovider __glXDRISWRastProvider = {
        glXDRIscreenProbe,
        "DRISWRAST",
        NULL
};

/*################################################################################################*/

static int (*origProcSendEvent)(ClientPtr) = NULL;
static Atom xaCLIPBOARD = 0, xaTARGETS = 0, xaSTRING = 0, xaUTF8_STRING = 0;
static Bool clipboardEnabled = FALSE;

void lorieEnableClipboardSync(Bool enable) {
    clipboardEnabled = enable;
}

static void lorieSelectionRequest(Atom selection, Atom target) {
    Selection *pSel;

    if (clipboardEnabled && dixLookupSelection(&pSel, selection, serverClient, DixGetAttrAccess) == Success) {
        xEvent event = {0};
        event.u.u.type = SelectionRequest;
        event.u.selectionRequest.owner = pSel->window;
        event.u.selectionRequest.time = currentTime.milliseconds;
        event.u.selectionRequest.requestor = pScreenPtr->root->drawable.id;
        event.u.selectionRequest.selection = selection;
        event.u.selectionRequest.target = target;
        event.u.selectionRequest.property = target;
        WriteEventsToClient(pSel->client, 1, &event);
    }
}

static Bool lorieHasAtom(Atom atom, const Atom list[], size_t size) {
    for (size_t i = 0; i < size; i++)
        if (list[i] == atom)
            return TRUE;

    return FALSE;
}

static inline void lorieConvertLF(const char* src, char *dst, size_t bytes) {
    size_t i = 0, j = 0;
    for (; i < bytes; i++)
        if (src[i] != '\r')
            dst[j++] = src[i];
}

static inline void lorieLatin1ToUTF8(unsigned char* out, const unsigned char* in) {
    while (*in)
        if (*in < 128)
            *out++ = *in++;
        else
            *out++ = 0xc2 + (*in > 0xbf), *out++ = (*in++ & 0x3f) + 0x80;
}

static inline int lorieCheckUTF8(const unsigned char *utf, size_t size) {
    int ix;
    unsigned char c;

    for (ix = 0; (c = utf[ix]) && ix < size;) {
        if (c & 0x80) {
            if ((utf[ix + 1] & 0xc0) != 0x80)
                return 0;
            if ((c & 0xe0) == 0xe0) {
                if ((utf[ix + 2] & 0xc0) != 0x80)
                    return 0;
                if ((c & 0xf0) == 0xf0) {
                    if ((c & 0xf8) != 0xf0 || (utf[ix + 3] & 0xc0) != 0x80)
                        return 0;
                    ix += 4;
                    /* 4-byte code */
                } else
                    /* 3-byte code */
                    ix += 3;
            } else
                /* 2-byte code */
                ix += 2;
        } else
            /* 1-byte code */
            ix++;
    }
    return 1;
}

static void lorieHandleSelection(Atom target) {
    PropertyPtr prop;
    if (target != xaTARGETS && target != xaSTRING && target != xaUTF8_STRING)
        return;

    if (dixLookupProperty(&prop, pScreenPtr->root, target, serverClient, DixReadAccess) != Success)
        return;

    log(DEBUG, "Selection notification for CLIPBOARD (target %s, type %s)\n", NameForAtom(target), NameForAtom(prop->type));

    if (target == xaTARGETS && prop->type == XA_ATOM && prop->format == 32) {
        if (lorieHasAtom(xaUTF8_STRING, (const Atom*)prop->data, prop->size))
            lorieSelectionRequest(xaCLIPBOARD, xaUTF8_STRING);
        else if (lorieHasAtom(xaSTRING, (const Atom*)prop->data, prop->size))
            lorieSelectionRequest(xaCLIPBOARD, xaSTRING);
    } else if (target == xaSTRING && prop->type == xaSTRING && prop->format == 8) {
        if (prop->format != 8 || prop->type != xaSTRING)
            return;

        char filtered[prop->size + 1], utf8[(prop->size + 1) * 2];
        memset(filtered, 0, sizeof(filtered));
        memset(utf8, 0, sizeof(utf8));

        lorieConvertLF(prop->data,  filtered, prop->size);
        lorieLatin1ToUTF8((unsigned char*) utf8, (unsigned char*) filtered);
        log(DEBUG, "Sending clipboard to clients (%zu bytes)\n", strlen(utf8));
        lorieSendClipboardData(utf8);
    } else if (target == xaUTF8_STRING && prop->type == xaUTF8_STRING && prop->format == 8) {
        char filtered[prop->size + 1];

        if (!lorieCheckUTF8(prop->data, prop->size)) {
            dprintf(2, "Invalid UTF-8 sequence in clipboard\n");
            return;
        }

        memset(filtered, 0, prop->size + 1);
        lorieConvertLF(prop->data, filtered, prop->size);

        log(DEBUG, "Sending clipboard to clients (%zu bytes)\n", strlen(filtered));
        lorieSendClipboardData(filtered);
    }
}

static int lorieProcSendEvent(ClientPtr client)
{
    REQUEST(xSendEventReq)
    REQUEST_SIZE_MATCH(xSendEventReq);
    __typeof__(stuff->event.u.selectionNotify)* e = &stuff->event.u.selectionNotify;

    if (clipboardEnabled && e->requestor == pScreenPtr->root->drawable.id &&
        stuff->event.u.u.type == SelectionNotify && e->selection == xaCLIPBOARD && e->target == e->property)
        lorieHandleSelection(e->target);

    return origProcSendEvent(client);
}

static void lorieSelectionCallback(maybe_unused CallbackListPtr *callbacks, maybe_unused void * data, void * args) {
    SelectionInfoRec *info = (SelectionInfoRec *) args;

    if (clipboardEnabled && info->selection->selection == xaCLIPBOARD && info->kind == SelectionSetOwner)
        lorieSelectionRequest(xaCLIPBOARD, xaTARGETS);
}

static void lorieInitSelectionCallback() {
#define ATOM(name) xa##name = MakeAtom(#name, strlen(#name), TRUE)
    ATOM(CLIPBOARD); ATOM(TARGETS); ATOM(STRING); ATOM(UTF8_STRING);

    if (!origProcSendEvent) {
        origProcSendEvent = ProcVector[X_SendEvent];
        ProcVector[X_SendEvent] = lorieProcSendEvent;
    }

    if (!AddCallback(&SelectionCallback, lorieSelectionCallback, NULL))
        FatalError("Adding SelectionCallback failed\n");
}