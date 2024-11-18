/*
 * Design is pretty simple.
 * We need somehow attach Android's HardwareBuffers and turnip's textures to X11 pixmaps.
 *
 * About turnip.
 * We can not simply import Mesa's texture since we do not use Mesa in project
 * and even in the case it we ever use it we can not mix mesa with Android's libEGL.
 * Xext's shm allows us to attach dmabuf fd with given width, height and offset,
 * but does not let us specify stride of buffer.
 * For this case we use DRI3's pixmap_from_fds request since it allows us specify
 * width, height, stride and even import fd from client.
 *
 * About Android Hardware Buffers.
 * NDK API does not let us simply flatten GraphicBuffer like it is done in regular AOSP code
 * or even simply extract native handle which contains data and file descriptors
 * needed to recreate GraphicBuffer in different process.
 * But we have AHardwareBuffer_sendHandleToUnixSocket and AHardwareBuffer_recvHandleFromUnixSocket
 * which let us send and receive AHardwareBuffer (aka GraphicBuffer) through Unix socket.
 * So X11 client can simply open socketpair, wait for a while until server sends one byte
 * and respond with AhardwareBuffer using AHardwareBuffer_sendHandleToUnixSocket.
 * About attaching AHardwareBuffer to X11 pixmap:
 * We can not simply do AHardwareBuffer_lock after creating pixmap and expect it to work.
 * Some platforms (especially platforms with separate GPU memory which can not be accessed with CPU)
 * explicitly copy contents of video memory to CPU memory on AHardwareBuffer_lock and content of buffer
 * copied this way is not affected by any actions in X11 client process.
 * So we must explicitly call AHardwareBuffer_lock when GC needs to access pixmap
 * and call AHardwareBuffer_unlock when it finishes its work.
 * Also we consider AHardwareBuffer we have to be read-only, write allowed only for X11 client.
 * So all functions except CopyArea applied to pixmap with attached AHardwareBuffer should be no-ops.
 *
 *
 *
 * FDE append some code for real dri3 between linux app and this X11,
 *  step 1. loriePixmapFromFds get drm fd
 *  step 2. generate a texture by drm fd
 *  step 3. dixSetPrivate set texture to pixmap
 *  step 4. dixSetPrivate set texture to window in copy area, because of flip not support, use copy for every chanage
 *  step 5. update texture when damage occurs
 */

#pragma clang diagnostic ignored "-Wstrict-prototypes"
#include <android/log.h>
#include <gcstruct.h>
#include <privates.h>
#include <scrnintstr.h>
#include <dri3.h>
#include <sys/mman.h>
#include <fb.h>
#include <android/hardware_buffer.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "screenint.h"
#include "lorie.h"
#include "renderer.h"
#include "misync.h"
#include "misyncstr.h"
#include "misyncshm.h"
#include "misyncfd.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include "present_priv.h"
#include <present.h>
#include "lorie.h"

#define NSEC_PER_SEC 1000000000ULL
#define DEFAULT_REFRESH_RATE 60  // 默认60Hz
#define REFRESH_INTERVAL (NSEC_PER_SEC / DEFAULT_REFRESH_RATE)
extern Bool LOG_ENABLE;
#define ANDROID_LOG_ENABLE 0
#define PRINT_LOG (ANDROID_LOG_ENABLE && LOG_ENABLE)
#define log(prio, ...) if(PRINT_LOG){__android_log_print(ANDROID_LOG_ ## prio, "huyang_dri3", __VA_ARGS__);}

// 软件模拟的VBlank状态
typedef struct {
    uint64_t last_ust;          // 上次更新时间
    uint64_t last_msc;          // 上次的MSC计数
    uint64_t refresh_interval;  // 刷新间隔（纳秒）
} vblank_state_t;

static vblank_state_t g_vblank_state = {
        .last_ust = 0,
        .last_msc = 0,
        .refresh_interval = REFRESH_INTERVAL
};

// 获取当前时间（纳秒）
static uint64_t get_monotonic_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NSEC_PER_SEC + (uint64_t)ts.tv_nsec;
}


static DevPrivateKeyRec lorieGCPrivateKey;
static DevPrivateKeyRec lorieScrPrivateKey;
static DevPrivateKeyRec lorieAHBPixPrivateKey;
static DevPrivateKeyRec lorieMmappedPixPrivateKey;

DevPrivateKeyRec FDETexturePrivateKey;
DevPrivateKeyRec FDEWindowTexturePrivateKey;


typedef struct {
    const GCOps *ops;
    const GCFuncs *funcs;
} LorieGCPrivRec, *LorieGCPrivPtr;

typedef struct {
    CreateGCProcPtr CreateGC;
    DestroyPixmapProcPtr DestroyPixmap;
} LorieScrPrivRec, *LorieScrPrivPtr;

typedef struct {
    AHardwareBuffer* buffer;
} LorieAHBPixPrivRec, *LorieAHBPixPrivPtr;



static Bool FalseNoop() { return FALSE; }

#define SYNC_FENCE_PRIV(pFence) \
    (SyncShmFencePrivatePtr) dixLookupPrivate(&pFence->devPrivates, &syncShmFencePrivateKey)
static DevPrivateKeyRec syncShmFencePrivateKey;


#define lorieGCPriv(pGC) LorieGCPrivPtr pGCPriv = dixLookupPrivate(&(pGC)->devPrivates, &lorieGCPrivateKey)
#define lorieScrPriv(pScr) LorieScrPrivPtr pScrPriv = ((LorieScrPrivPtr) dixLookupPrivate(&(pScr)->devPrivates, &lorieScrPrivateKey))
#define loriePixFromDrawable(pDrawable, suffix) \
    PixmapPtr pDrawable ## Pix ## suffix = (pDrawable->type == DRAWABLE_PIXMAP) ? \
        (PixmapPtr) (((char*) pDrawable) - offsetof(PixmapRec, drawable)) : 0
#define loriePixPriv(pDrawable, suffix) \
    LorieAHBPixPrivPtr pPixPriv ## suffix = (pDrawable ## Pix ## suffix) ? ((LorieAHBPixPrivPtr) \
        dixLookupPrivate(&(pDrawable ## Pix ## suffix)->devPrivates, &lorieAHBPixPrivateKey)) : 0

#define wrap(priv, real, mem, func) {\
    priv->mem = real->mem; \
    real->mem = func; \
}

#define LORIE_GC_OP_PROLOGUE(pGC) \
    lorieGCPriv(pGC);  \
    const GCFuncs *oldFuncs = pGC->funcs; \
    const GCOps *oldOps = pGC->ops; \
    unwrap(pGCPriv, pGC, funcs);  \
    unwrap(pGCPriv, pGC, ops); \

#define LORIE_GC_OP_EPILOGUE(pGC) \
    wrap(pGCPriv, pGC, funcs, oldFuncs); \
    wrap(pGCPriv, pGC, ops, oldOps)

#define LORIE_GC_FUNC_PROLOGUE(pGC) \
    lorieGCPriv(pGC); \
    unwrap(pGCPriv, pGC, funcs); \
    if (pGCPriv->ops) unwrap(pGCPriv, pGC, ops)

#define LORIE_GC_FUNC_EPILOGUE(pGC) \
    wrap(pGCPriv, pGC, funcs, &lorieGCFuncs);  \
    if (pGCPriv->ops) wrap(pGCPriv, pGC, ops, &lorieGCOps)

#define unwrap(priv, real, mem) {\
    real->mem = priv->mem; \
}

static const GCOps lorieGCOps;
static const GCFuncs lorieGCFuncs;

static void lorieValidateGC(GCPtr pGC, unsigned long stateChanges, DrawablePtr pDrawable) {
    LORIE_GC_FUNC_PROLOGUE(pGC)
    (*pGC->funcs->ValidateGC) (pGC, stateChanges, pDrawable);
    LORIE_GC_FUNC_EPILOGUE(pGC)
}

static void lorieChangeGC(GCPtr pGC, unsigned long mask) {
    LORIE_GC_FUNC_PROLOGUE(pGC)
    (*pGC->funcs->ChangeGC) (pGC, mask);
    LORIE_GC_FUNC_EPILOGUE(pGC)
}

static void lorieCopyGC(GCPtr pGCSrc, unsigned long mask, GCPtr pGCDst) {
    LORIE_GC_FUNC_PROLOGUE(pGCSrc)
    (*pGCSrc->funcs->CopyGC) (pGCSrc, mask, pGCDst);
    LORIE_GC_FUNC_EPILOGUE(pGCSrc)
}

static void lorieDestroyGC(GCPtr pGC) {
    LORIE_GC_FUNC_PROLOGUE(pGC)
    (*pGC->funcs->DestroyGC) (pGC);
    LORIE_GC_FUNC_EPILOGUE(pGC)
}

static void lorieChangeClip(GCPtr pGC, int type, void *pvalue, int nrects) {
    LORIE_GC_FUNC_PROLOGUE(pGC)
    (*pGC->funcs->ChangeClip) (pGC, type, pvalue, nrects);
    LORIE_GC_FUNC_EPILOGUE(pGC)
}

static void lorieDestroyClip(GCPtr pGC) {
    LORIE_GC_FUNC_PROLOGUE(pGC)
    (*pGC->funcs->DestroyClip) (pGC);
    LORIE_GC_FUNC_EPILOGUE(pGC)
}

static void lorieCopyClip(GCPtr pgcDst, GCPtr pgcSrc) {
    LORIE_GC_FUNC_PROLOGUE(pgcDst)
    (*pgcDst->funcs->CopyClip) (pgcDst, pgcSrc);
    LORIE_GC_FUNC_EPILOGUE(pgcDst)
}

static const GCFuncs lorieGCFuncs = {
        lorieValidateGC, lorieChangeGC, lorieCopyGC, lorieDestroyGC,
        lorieChangeClip, lorieDestroyClip, lorieCopyClip
};

static void lorieFillSpans(DrawablePtr pDrawable, GCPtr pGC, int nInit, DDXPointPtr pptInit, int * pwidthInit, int fSorted) {
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->FillSpans) (pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void lorieSetSpans(DrawablePtr pDrawable, GCPtr pGC, char * psrc, DDXPointPtr ppt, int * pwidth, int nspans, int fSorted) {
    log(ERROR, "DRI3: lorieSetSpans");
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->SetSpans) (pDrawable, pGC, psrc, ppt, pwidth, nspans, fSorted);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void loriePutImage(DrawablePtr pDrawable, GCPtr pGC, int depth, int x, int y, int w, int h, int leftPad, int format, char * pBits) {
    log(ERROR, "DRI3: loriePutImage pDrawable id:%x type:%d", pDrawable->id, pDrawable->type);
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->PutImage) (pDrawable, pGC, depth, x, y, w, h, leftPad, format, pBits);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static RegionPtr fde_gc_copy_area(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC, int srcx, int srcy, int w, int h, int dstx, int dsty){
    // LORIE_GC_OP_PROLOGUE(pGC) 展开为:
    LorieGCPrivPtr pGCPriv = dixLookupPrivate(&(pGC)->devPrivates, &lorieGCPrivateKey);
    const GCFuncs *oldFuncs = pGC->funcs;
    const GCOps *oldOps = pGC->ops;
    unwrap(pGCPriv, pGC, funcs);
    unwrap(pGCPriv, pGC, ops);

    // loriePixFromDrawable(pSrc, 0) 展开为:
    PixmapPtr pSrcPix0 = (pSrc->type == DRAWABLE_PIXMAP) ?
                         (PixmapPtr) (((char*) pSrc) - offsetof(PixmapRec, drawable)) : 0;
    LorieAHBPixPrivPtr pPixPriv0 = (pSrcPix0) ? ((LorieAHBPixPrivPtr)
            dixLookupPrivate(&(pSrcPix0)->devPrivates, &lorieAHBPixPrivateKey)) : 0;

    // loriePixFromDrawable(pDst, 1) 展开为:
    PixmapPtr pDstPix1 = (pDst->type == DRAWABLE_PIXMAP) ?
                         (PixmapPtr) (((char*) pDst) - offsetof(PixmapRec, drawable)) : 0;
    LorieAHBPixPrivPtr pPixPriv1 = (pDstPix1) ? ((LorieAHBPixPrivPtr)
            dixLookupPrivate(&(pDstPix1)->devPrivates, &lorieAHBPixPrivateKey)) : 0;

    RegionPtr r = NULL;
    if (!pPixPriv1)
        r = (*pGC->ops->CopyArea) (pSrc, pDst, pGC, srcx, srcy, w, h, dstx, dsty);

    // LORIE_GC_OP_EPILOGUE(pGC) 展开为:
    wrap(pGCPriv, pGC, funcs, oldFuncs);
    wrap(pGCPriv, pGC, ops, oldOps);
    return r;
}

static RegionPtr lorieCopyArea(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC, int srcx, int srcy, int w, int h, int dstx, int dsty) {
    log(ERROR, "DRI3: lorieCopyArea pSrc id:%x type:%d pDst id:%x type:%d srcx:%d, srcy:%d w:%d h:%d dstx:%d dsty:%d",
        pSrc->id, pSrc->type,  pDst->id, pDst->type, srcx, srcy, w, h, dstx, dsty);
    if(pSrc->type == DRAWABLE_PIXMAP && pDst->type == DRAWABLE_WINDOW ){
        PixmapPtr pPixmap = (PixmapPtr)pSrc;
        TexturePrivRecPtr ptr = dixLookupPrivate(&pPixmap->devPrivates, &FDETexturePrivateKey);
        if(ptr){
            WindowPtr pWin = (WindowPtr)pDst;
            PixmapPtr pixmap = (PixmapPtr) (pGC->pScreen->GetWindowPixmap)(pWin);
//            pGC->pScreen->SetWindowPixmap(pWin, pPixmap);

            TexturePrivRecPtr pTexturePriv = calloc(1, sizeof(TexturePrivRec));
            pTexturePriv->texture = ptr->texture;
            dixSetPrivate(&pWin->devPrivates, &FDEWindowTexturePrivateKey, pTexturePriv);

            log(ERROR, "GetWindowPixmap pixmap:%x", pixmap->drawable.id);
            log(ERROR, "dixLookupPrivate pixmap:%x tid:%d window:%x", pPixmap->drawable.id, ptr->texture, pWin->drawable.id);
            return NULL;
        }
        return fde_gc_copy_area(pSrc, pDst, pGC, srcx, srcy, w, h, dstx, dsty);
    } else {
        return fde_gc_copy_area(pSrc, pDst, pGC, srcx, srcy, w, h, dstx, dsty);
    }
    return NULL;
}

static RegionPtr lorieCopyPlane(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable, GCPtr pGC, int srcx, int srcy, int width, int height, int dstx, int dsty, unsigned long bitPlane) {
    log(ERROR, "DRI3: lorieCopyPlane");
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pSrcDrawable, 0);
    loriePixFromDrawable(pDstDrawable, 1);
    loriePixPriv(pSrcDrawable, 0);
    loriePixPriv(pDstDrawable, 1);
    RegionPtr r = NULL;
    if (!pPixPriv0 && !pPixPriv1)
        r = (*pGC->ops->CopyPlane) (pSrcDrawable, pDstDrawable, pGC, srcx, srcy, width, height, dstx, dsty, bitPlane);
    LORIE_GC_OP_EPILOGUE(pGC)
    return r;
}

static void loriePolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode, int npt, DDXPointPtr pptInit) {
    log(ERROR, "DRI3: loriePolyPoint");
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->PolyPoint) (pDrawable, pGC, mode, npt, pptInit);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void loriePolylines(DrawablePtr pDrawable, GCPtr pGC, int mode, int npt, DDXPointPtr pptInit) {
    log(ERROR, "DRI3: loriePolylines");
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->Polylines) (pDrawable, pGC, mode, npt, pptInit);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void loriePolySegment(DrawablePtr pDrawable, GCPtr pGC, int nseg, xSegment * pSegs) {
    log(ERROR, "DRI3: loriePolySegment");
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->PolySegment) (pDrawable, pGC, nseg, pSegs);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void loriePolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nrects, xRectangle * pRects) {
    log(ERROR, "DRI3: loriePolyRectangle");
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->PolyRectangle) (pDrawable, pGC, nrects, pRects);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void loriePolyArc(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc * parcs) {
    log(ERROR, "DRI3: loriePolyArc");
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->PolyArc) (pDrawable, pGC, narcs, parcs);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void lorieFillPolygon(DrawablePtr pDrawable, GCPtr pGC, int shape, int mode, int count, DDXPointPtr pPts) {
    log(ERROR, "DRI3: lorieFillPolygon");
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->FillPolygon) (pDrawable, pGC, shape, mode, count, pPts);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void loriePolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nrectFill, xRectangle * prectInit) {
    log(ERROR, "DRI3: loriePolyFillRect");
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->PolyFillRect) (pDrawable, pGC, nrectFill, prectInit);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void loriePolyFillArc(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc * parcs) {
    log(ERROR, "DRI3: loriePolyFillArc");
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->PolyFillArc) (pDrawable, pGC, narcs, parcs);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static int loriePolyText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, char * chars) {
    log(ERROR, "DRI3: loriePolyText8");
    LORIE_GC_OP_PROLOGUE(pGC)
    int r = x;
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        r = (*pGC->ops->PolyText8) (pDrawable, pGC, x, y, count, chars);
    LORIE_GC_OP_EPILOGUE(pGC)
    return r;
}

static int loriePolyText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, unsigned short * chars) {
    log(ERROR, "DRI3: loriePolyText8");
    LORIE_GC_OP_PROLOGUE(pGC)
    int r = x;
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        r = (*pGC->ops->PolyText16) (pDrawable, pGC, x, y, count, chars);
    LORIE_GC_OP_EPILOGUE(pGC)
    return r;
}

static void lorieImageText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, char * chars) {
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->ImageText8) (pDrawable, pGC, x, y, count, chars);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void lorieImageText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y, int count, unsigned short * chars) {
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->ImageText16) (pDrawable, pGC, x, y, count, chars);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void lorieImageGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x, int y, unsigned int nglyph, CharInfoPtr *ppci, void *pglyphBase) {
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->ImageGlyphBlt) (pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void loriePolyGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x, int y, unsigned int nglyph, CharInfoPtr *ppci, void *pglyphBase) {
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDrawable, 0);
    loriePixPriv(pDrawable, 0);
    if (!pPixPriv0)
        (*pGC->ops->PolyGlyphBlt) (pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static void loriePushPixels(GCPtr pGC, PixmapPtr pBitMapPix0, DrawablePtr pDst, int w, int h, int x, int y) {
    LORIE_GC_OP_PROLOGUE(pGC)
    loriePixFromDrawable(pDst, 1);
    loriePixPriv(pBitMap, 0);
    loriePixPriv(pDst, 1);
    if (!pPixPriv0 && !pPixPriv1)
        (*pGC->ops->PushPixels) (pGC, pBitMapPix0, pDst, w, h, x, y);
    LORIE_GC_OP_EPILOGUE(pGC)
}

static const GCOps lorieGCOps = {
        lorieFillSpans, lorieSetSpans,
        loriePutImage, lorieCopyArea,
        lorieCopyPlane, loriePolyPoint,
        loriePolylines, loriePolySegment,
        loriePolyRectangle, loriePolyArc,
        lorieFillPolygon, loriePolyFillRect,
        loriePolyFillArc, loriePolyText8,
        loriePolyText16, lorieImageText8,
        lorieImageText16, lorieImageGlyphBlt,
        loriePolyGlyphBlt, loriePushPixels,
};

static Bool
lorieCreateGC(GCPtr pGC) {
    log(ERROR, "DRI3: lorieCreateGC");
    ScreenPtr pScreen = pGC->pScreen;

    lorieScrPriv(pScreen);
    lorieGCPriv(pGC);
    Bool ret;

    unwrap(pScrPriv, pScreen, CreateGC)
    if ((ret = (*pScreen->CreateGC) (pGC))) {
        pGCPriv->ops = pGC->ops;
        pGCPriv->funcs = pGC->funcs;
        pGC->funcs = &lorieGCFuncs;
        pGC->ops = &lorieGCOps;
    }
    wrap(pScrPriv, pScreen, CreateGC, lorieCreateGC)

    return ret;
}

static Bool
lorieDestroyPixmap(PixmapPtr pPixmap) {
    Bool ret;
    void *ptr = NULL;
    LorieAHBPixPrivPtr pPixPriv = NULL;
    size_t size = 0;
    ScreenPtr pScreen = pPixmap->drawable.pScreen;
    lorieScrPriv(pScreen);

    if (pPixmap->refcnt == 1 && pPixmap->drawable.width && pPixmap->drawable.height) {
        ptr = dixLookupPrivate(&pPixmap->devPrivates, &lorieMmappedPixPrivateKey);
        pPixPriv = dixLookupPrivate(&pPixmap->devPrivates, &lorieAHBPixPrivateKey);
        size = pPixmap->devKind * pPixmap->drawable.height;
    }

    unwrap(pScrPriv, pScreen, DestroyPixmap)
    ret = (*pScreen->DestroyPixmap) (pPixmap);
    wrap(pScrPriv, pScreen, DestroyPixmap, lorieDestroyPixmap)

    if (ptr)
        munmap(ptr, size);

    if (pPixPriv) {
        if (pPixPriv->buffer)
            AHardwareBuffer_release(pPixPriv->buffer);
        free(pPixPriv);
    }

    return ret;
}

static int FdsFromPixmap(ScreenPtr screen,
                         PixmapPtr pixmap,
                         int *fds,
                         uint32_t *strides,
                         uint32_t *offsets,
                         uint64_t *modifier){
    log(ERROR, "DRI3: FdsFromPixmap pixmap:%d fd:%d stride:%d offset:%d modifier:%d",
        pixmap->drawable.id,
        fds[0],
        strides[0],
        offsets[0],
        modifier[0]);
    return Success;
}

static PixmapPtr loriePixmapFromFds(ScreenPtr screen, CARD8 num_fds, const int *fds, CARD16 width, CARD16 height,
                                    const CARD32 *strides, const CARD32 *offsets, CARD8 depth, __unused CARD8 bpp, CARD64 modifier) {
    log(ERROR, "DRI3: loriePixmapFromFds num_fds:%d modifier:%d fd:%d width:%d height:%d",
        num_fds, modifier, fds[0], width, height);
    if (num_fds > 1) {
        log(ERROR, "DRI3: More than 1 fd");
        return NULL;
    }
    TexturePrivRecPtr pTexturePriv = calloc(1, sizeof(TexturePrivRec));
    GLuint texture = renderer_create_image(fds[0], width, height, strides, offsets, depth, bpp, modifier);
    if (!pTexturePriv) {
        log(ERROR, "DRI3: pTexturePriv: failed to allocate TexturePrivRecPtr");
        return NULL;
    }

    PixmapPtr pixmap = fbCreatePixmap(screen, width, height, depth, 0);
    if (!pixmap) {
        log(ERROR, "DRI3: failed to create pixmap");
        goto fail;
    }
    log(ERROR, "loriePixmapFromFds pixmap:%lx pTexturePriv:%p", pixmap->drawable.id, pTexturePriv);
    dixSetPrivate(&pixmap->devPrivates, &FDETexturePrivateKey, pTexturePriv);
    pTexturePriv->texture = texture;
    TexturePrivRecPtr ptr = dixLookupPrivate(&pixmap->devPrivates, &FDETexturePrivateKey);
    log(ERROR, "loriePixmapFromFds ptr:%p", ptr, pixmap->devPrivate.ptr);

    if (!pTexturePriv->texture) {
        log(ERROR, "DRI3: pTexturePriv: get a texture");
        goto fail;
    }

//    AHardwareBuffer_describe(pPixPriv->buffer, &desc);
//    if (desc.format != AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM
//        && desc.format != AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM
//        && desc.format != AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM) {
//        log(ERROR, "DRI3: AHARDWAREBUFFER_SOCKET_FD: wrong format of AHardwareBuffer. Must be one of: AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM, AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM (stands for 5).");
//        goto fail;
//    }

    pixmap->devPrivate.ptr = NULL;
    screen->ModifyPixmapHeader(pixmap, width, height, depth, 0, strides[0] * 4, NULL);
    return pixmap;

    fail:
    if (pixmap)
        fbDestroyPixmap(pixmap);

    return NULL;
}

static int lorieGetFormats(__unused ScreenPtr screen, CARD32 *num_formats, CARD32 **formats) {
    log(ERROR, "lorieGetFormats");
    *num_formats = 0;
    *formats = NULL;
    return TRUE;
}

static int lorieGetModifiers(__unused ScreenPtr screen, __unused uint32_t format, uint32_t *num_modifiers, uint64_t **modifiers) {
    log(ERROR, "lorieGetModifiers");
    *num_modifiers = 0;
    *modifiers = NULL;
    return TRUE;
}

static int openClient(__unused ClientPtr client,
                      __unused ScreenPtr screen,
                      __unused RRProviderPtr provider,
                      __unused int *fdp) {
    int fd;
    fd = open("/dev/dri/card0", O_RDWR|O_CLOEXEC);
    log(ERROR, "openClient fd %d", fd);
    if (fd < 0) {
        log(ERROR, "openClient fdp %d", &fdp);
        return BadAlloc;
    }
    log(ERROR, "openClient fd %d", fd);
    *fdp = fd;
    return Success;
//    return BadMatch;
}

static int openDri(__unused ScreenPtr screen,
                   __unused RRProviderPtr provider,
                   __unused int *fdp) {
    return openClient(NULL, screen, provider, fdp);
}

static dri3_screen_info_rec dri3Info = {
        .version = 2,
        .fds_from_pixmap = FdsFromPixmap,
        .pixmap_from_fds = loriePixmapFromFds,
        .get_formats = lorieGetFormats,
        .get_modifiers = lorieGetModifiers,
        .get_drawable_modifiers = FalseNoop,
        .open_client = openClient,
        .open = openDri,
};

static RRCrtcPtr
fde_present_get_crtc(WindowPtr window)
{
    if (window == NULL) {
        return NULL;
    }
    rrScrPrivPtr rr_private;

    rr_private = rrGetScrPriv(window->drawable.pScreen);

    if (rr_private->numCrtcs == 0)
        return NULL;

    return rr_private->crtcs[0];
}

static int
fde_present_get_ust_msc(RRCrtcPtr crtc,
                        CARD64 *ust,
                        CARD64 *msc)
{
    uint64_t current_time = get_monotonic_time_ns();

    if (g_vblank_state.last_ust == 0) {
        // 首次初始化
        g_vblank_state.last_ust = current_time;
        g_vblank_state.last_msc = 0;
        *ust = current_time;
        *msc = 0;
        return Success;
    }

    // 计算从上次更新到现在经过了多少个刷新周期
    uint64_t time_delta = current_time - g_vblank_state.last_ust;
    uint64_t frame_count = time_delta / g_vblank_state.refresh_interval;

    *ust = current_time;
    *msc = g_vblank_state.last_msc + frame_count;

    // 更新状态
    g_vblank_state.last_ust = current_time;
    g_vblank_state.last_msc = *msc;

    return Success;
}

static int
fde_present_queue_vblank(RRCrtcPtr crtc,
                         uint64_t event_id,
                         uint64_t msc)
{
    uint64_t current_ust, current_msc;

    if (!fde_present_get_ust_msc(crtc, &current_ust, &current_msc))
        return BadMatch;

    if (msc <= current_msc) {
        // 目标MSC已过，立即通知
        present_event_notify(event_id, current_ust, current_msc);
    } else {
        // 计算到目标MSC的时间
        uint64_t msc_delta = msc - current_msc;
        uint64_t target_ust = current_ust + (msc_delta * g_vblank_state.refresh_interval);

        // 模拟延迟通知
        present_event_notify(event_id, target_ust, msc);
    }

    return Success;
}

static void
fde_present_abort_vblank(RRCrtcPtr crtc,
                         uint64_t event_id,
                         uint64_t msc)
{
    uint64_t current_ust, current_msc;

    if (fde_present_get_ust_msc(crtc, &current_ust, &current_msc)) {
        // 立即发送中止通知
        present_event_notify(event_id, current_ust, current_msc);
    }
}

static void
fde_present_flush(WindowPtr window)
{
    // 在软件模拟模式下，我们只需要确保窗口内容被标记为已更新
    if (window) {
        RegionRec region;
        BoxRec box = {.x1 = window->drawable.x, .y1 = window->drawable.y, .x2 = window->drawable.width, .y2 = window->drawable.height};
        RegionInit(&region, &box, 1);
        DamageDamageRegion(&window->drawable, &region);
        RegionUninit(&region);
    }
}

static Bool
fde_present_check_flip(RRCrtcPtr crtc,
                       WindowPtr window,
                       PixmapPtr pixmap,
                       Bool sync_flip,
                       PresentFlipReason *reason)
{
    // 在Android NDK环境下，我们不支持真正的flip操作
    if (reason)
        *reason = PRESENT_FLIP_REASON_UNKNOWN;
    return FALSE;
}

static Bool
fde_present_flip(RRCrtcPtr crtc,
                 uint64_t event_id,
                 uint64_t target_msc,
                 PixmapPtr pixmap,
                 Bool sync_flip)
{
    // 在Android NDK环境下，我们不支持真正的flip操作
    return FALSE;
}

static void
fde_present_unflip(ScreenPtr screen,
                   uint64_t event_id)
{
    // 由于我们不支持flip，这里不需要做任何事情
    present_screen_priv_ptr screen_priv = present_screen_priv(screen);
    if (screen_priv) {
        screen_priv->flip_pending = NULL;
        screen_priv->flip_window = NULL;
        screen_priv->flip_crtc = NULL;
        screen_priv->flip_pixmap = NULL;
        screen_priv->flip_sync = FALSE;
    }
}

static present_screen_info_rec fde_present_screen_info = {
        .version = PRESENT_SCREEN_INFO_VERSION,
        .get_crtc = fde_present_get_crtc,
        .get_ust_msc = fde_present_get_ust_msc,
        .queue_vblank = fde_present_queue_vblank,
        .abort_vblank = fde_present_abort_vblank,
        .flush = fde_present_flush,
//        .capabilities = PresentAllCapabilities,
        .check_flip = NULL,
//        .check_flip2 = fde_present_check_flip,
//        .flip = fde_present_flip,
//        .unflip = fde_present_unflip,
};

Bool
fde_present_screen_init(ScreenPtr screen)
{
    return present_screen_init(screen, &fde_present_screen_info);;
}


bool syncInit(ScreenPtr pScreen);

Bool lorieInitDri3(ScreenPtr pScreen) {
    LorieScrPrivPtr pScrPriv;
    log(ERROR, "DRI3: lorieInitDri3");

    if (!dixRegisterPrivateKey(&lorieScrPrivateKey, PRIVATE_SCREEN, 0))
        return FALSE;

    if (dixLookupPrivate(&pScreen->devPrivates, &lorieScrPrivateKey))
        return TRUE;

    if (!dixRegisterPrivateKey(&lorieGCPrivateKey, PRIVATE_GC, sizeof(LorieGCPrivRec))
        || !dixRegisterPrivateKey(&lorieAHBPixPrivateKey, PRIVATE_PIXMAP, 0)
        || !dixRegisterPrivateKey(&FDEWindowTexturePrivateKey, PRIVATE_WINDOW, 0)
        || !dixRegisterPrivateKey(&FDETexturePrivateKey, PRIVATE_PIXMAP, 0)
        || !dixRegisterPrivateKey(&lorieMmappedPixPrivateKey, PRIVATE_PIXMAP, 0)
        || !dri3_screen_init(pScreen, &dri3Info))
        return FALSE;

    pScrPriv = malloc(sizeof(LorieScrPrivRec));
    if (!pScrPriv)
        return FALSE;

    wrap(pScrPriv, pScreen, CreateGC, lorieCreateGC)
    wrap(pScrPriv, pScreen, DestroyPixmap, lorieDestroyPixmap)

    dixSetPrivate(&pScreen->devPrivates, &lorieScrPrivateKey, pScrPriv);
    syncInit(pScreen);
//    fde_present_screen_init(pScreen);
    return TRUE;
}

bool syncInit(ScreenPtr pScreen) {
    if (!miSyncShmScreenInit(pScreen))
        return FALSE;
    return FALSE;
}
