//
// X 窗口"事实"采集层的实现。
//
// 设计约束（很重要，改动前先读）：
//   1. 采集只在 X server 线程上发生（android_redirect_window 里），
//      所以可以安全地读 pWin 和 userProps。
//   2. 编码/快照可以在任意线程发生，所以登记表必须自己加锁，
//      并且**不能**在编码时回读 X server 的数据结构。
//   3. 这里不做任何判断。加判断请去 Java 的 WindowRolePolicy。
//

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include <X11/X.h>
#include <X11/Xatom.h>
#include <propertyst.h>
#include <windowstr.h>
#include <window.h>

#include "window_facts.h"

// 复用 android.c 里的类型枚举值（267..276），保持两边一致
#define TYPE_NORMAL        273
#define TYPE_DIALOG        269
#define TYPE_UTILITY       276
#define TYPE_MENU          272
#define TYPE_POPUP_MENU    274
#define TYPE_DROPDOWN_MENU 271
#define TYPE_TOOLTIP       275
#define TYPE_COMBO         268
#define TYPE_DND           270
#define TYPE_SYSTRAY       1000
#define TYPE_SYSTIP        1001
// FDE 补充：EWMH 里有、老的映射表没接的类型
#define TYPE_SPLASH        277

#define WF_MAX_WINDOWS 128
#define WF_MAX_ELEMENTS 32

typedef struct {
    bool used;
    Window window;
    WindowFacts facts;
} WfEntry;

static WfEntry wf_entries[WF_MAX_WINDOWS];
static int wf_used = 0;
static pthread_mutex_t wf_lock = PTHREAD_MUTEX_INITIALIZER;

// _WM_WINDOW_TYPE_SYSTRAY / SYSTIP 是 FDE 自定义的类型名，必须按名字匹配。
// 旧代码用 `atoms[i] == 1000` 拿真实 atom 号和内部枚举比，永远不成立，
// 这里按字符串比较，并在编码里带上 matched 标志便于观察。
static const char *WF_NAME_SYSTRAY = "_WM_WINDOW_TYPE_SYSTRAY";
static const char *WF_NAME_SYSTIP = "_WM_WINDOW_TYPE_SYSTIP";

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------

static long long now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (long long) ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static bool atom_name_equal(Atom atom, const char *name) {
    const char *atom_name = NameForAtom(atom);
    return atom_name != NULL && strcmp(atom_name, name) == 0;
}

static void copy_text(char *dst, int dst_size, const char *src) {
    if (!dst || dst_size <= 0) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        return;
    }
    strncpy(dst, src, (size_t) dst_size - 1);
    dst[dst_size - 1] = '\0';
}

// 从 format=8 的属性里取字符串，截断到 dst_size。
static void copy_prop_text(WindowFacts *f, char *dst, int dst_size,
                           unsigned char *data, unsigned long size) {
    if (!data || size == 0) {
        return;
    }
    unsigned long limit = size;
    if (limit > (unsigned long) dst_size - 1) {
        limit = (unsigned long) dst_size - 1;
    }
    memcpy(dst, data, limit);
    dst[limit] = '\0';
    (void) f;
}

// 把类型 atom 名映射成内部枚举。识别不了的返回 WF_TYPE_NONE，
// 但原始类型名仍然会写进 type_names，方便后续加规则。
static int type_name_to_code(const char *name) {
    if (!name) {
        return WF_TYPE_NONE;
    }
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_NORMAL") == 0) return TYPE_NORMAL;
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_DIALOG") == 0) return TYPE_DIALOG;
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_UTILITY") == 0) return TYPE_UTILITY;
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_MENU") == 0) return TYPE_MENU;
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_POPUP_MENU") == 0) return TYPE_POPUP_MENU;
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU") == 0) return TYPE_DROPDOWN_MENU;
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_TOOLTIP") == 0) return TYPE_TOOLTIP;
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_COMBO") == 0) return TYPE_COMBO;
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_DND") == 0) return TYPE_DND;
    if (strcmp(name, "_NET_WM_WINDOW_TYPE_SPLASH") == 0) return TYPE_SPLASH;
    if (strcmp(name, WF_NAME_SYSTRAY) == 0) return TYPE_SYSTRAY;
    if (strcmp(name, WF_NAME_SYSTIP) == 0) return TYPE_SYSTIP;
    return WF_TYPE_NONE;
}

static void append_name(char *dst, int dst_size, const char *name, const char *prefix) {
    if (!dst || dst_size <= 0 || !name) {
        return;
    }
    int used = (int) strlen(dst);
    if (used >= dst_size - 2) {
        return;
    }
    int remain = dst_size - used - 1;
    if (prefix && prefix[0] && used > 0) {
        strncat(dst, ",", (size_t) remain);
        used++;
        remain--;
    }
    strncat(dst, name, (size_t) (remain > 0 ? remain : 0));
}

// ---------------------------------------------------------------------------
// 属性解析
// ---------------------------------------------------------------------------

static void parse_window_type(WindowFacts *f, unsigned char *data, unsigned long size) {
    if (!data || size == 0) {
        return;
    }
    ATOM *atoms = (ATOM *) data;
    // 客户端可以列多个类型（EWMH 允许），但"第一个能识别的"不等于"最具体的"：
    //   [_NET_WM_WINDOW_TYPE_NORMAL, _NET_WM_WINDOW_TYPE_TOOLTIP]
    // 按顺序取会拿到 NORMAL，把 TOOLTIP 的意图丢掉。NORMAL 的语义是"没有更具体
    // 的信息"，所以它只做兜底，具体类型先扫一遍。
    // 名字仍然全部记录下来，规则调整时不用重新采集。
    int specific = WF_TYPE_NONE;
    int normal = WF_TYPE_NONE;
    for (unsigned long i = 0; i < size; i++) {
        const char *name = NameForAtom(atoms[i]);
        if (!name) {
            continue;
        }
        append_name(f->type_names, (int) sizeof(f->type_names), name, ",");
        if (strcmp(name, "_KDE_NET_WM_WINDOW_TYPE_OVERRIDE") == 0) {
            // KDE 的"绕过 WM"类型：窗口可以不是 override-redirect，但仍然要求
            // 不进任务栏、跟随浮层。它在 _NET_WM_WINDOW_TYPE 里而不是 _NET_WM_STATE
            // 里，所以单独记成布尔事实，交给 Java 侧判定。
            f->flags |= WF_FLAG_KDE_OVERRIDE;
            continue;
        }
        int code = type_name_to_code(name);
        if (code == WF_TYPE_NONE) {
            continue;
        }
        if (code == TYPE_NORMAL) {
            if (normal == WF_TYPE_NONE) {
                normal = code;
            }
        } else if (specific == WF_TYPE_NONE) {
            specific = code;
        }
    }
    f->type_code = (specific != WF_TYPE_NONE) ? specific : normal;
    if (f->type_names[0]) {
        f->present |= WF_PRESENT_EWMH_TYPE;
    }
}

static void parse_window_state(WindowFacts *f, unsigned char *data, unsigned long size) {
    if (!data || size == 0) {
        return;
    }
    ATOM *atoms = (ATOM *) data;
    for (unsigned long i = 0; i < size; i++) {
        const char *name = NameForAtom(atoms[i]);
        if (!name) {
            continue;
        }
        append_name(f->state_names, (int) sizeof(f->state_names), name, ",");
        if (strcmp(name, "_NET_WM_STATE_MODAL") == 0) {
            f->flags |= WF_FLAG_MODAL;
            f->state_mask |= WF_STATE_MODAL;
        } else if (strcmp(name, "_NET_WM_STATE_SKIP_TASKBAR") == 0) {
            f->flags |= WF_FLAG_SKIP_TASKBAR;
            f->state_mask |= WF_STATE_SKIP_TASKBAR;
        } else if (strcmp(name, "_NET_WM_STATE_SKIP_PAGER") == 0) {
            f->flags |= WF_FLAG_SKIP_PAGER;
            f->state_mask |= WF_STATE_SKIP_PAGER;
        } else if (strcmp(name, "_NET_WM_STATE_ABOVE") == 0) {
            f->flags |= WF_FLAG_ABOVE;
        } else if (strcmp(name, "_NET_WM_STATE_BELOW") == 0) {
            f->flags |= WF_FLAG_BELOW;
        } else if (strcmp(name, "_NET_WM_STATE_STICKY") == 0) {
            f->flags |= WF_FLAG_STICKY;
        } else if (strcmp(name, "_NET_WM_STATE_FULLSCREEN") == 0) {
            f->flags |= WF_FLAG_FULLSCREEN;
        } else if (strcmp(name, "_NET_WM_STATE_MAXIMIZED_VERT") == 0
                   || strcmp(name, "_NET_WM_STATE_MAXIMIZED_HORZ") == 0) {
            f->flags |= WF_FLAG_MAXIMIZED;
        }
    }
    if (f->state_names[0]) {
        f->present |= WF_PRESENT_EWMH_STATE;
    }
}

// ICCCM WM_HINTS：flags, input, initial_state, ..., window_group(第 9 个)
static void parse_wm_hints(WindowFacts *f, PropertyPtr pProp) {
    if (!pProp || pProp->format != 32 || !pProp->data || pProp->size < 3) {
        return;
    }
    CARD32 *v = (CARD32 *) pProp->data;
    CARD32 flags = v[0];
    f->present |= WF_PRESENT_ICCCM_WM_HINTS;

    if (flags & (1u << 0)) { // InputHint
        f->hint_input = v[1] ? 1 : 0;
        if (!v[1]) {
            f->flags |= WF_FLAG_INPUT_FALSE;
        }
    }
    if (flags & (1u << 1)) { // StateHint
        f->hint_initial_state = (int) v[2];
    }
    if ((flags & (1u << 6)) && pProp->size >= 9) { // WindowGroupHint
        f->group = (Window) v[8];
        if (f->group) {
            f->present |= WF_PRESENT_GROUP;
        }
    }
}

// ICCCM WM_NORMAL_HINTS：flags, x, y, w, h, min_w, min_h, max_w, max_h,
// width_inc, height_inc, ...   PResizeInc = 1<<4
static void parse_wm_normal_hints(WindowFacts *f, PropertyPtr pProp) {
    if (!pProp || pProp->format != 32 || !pProp->data || pProp->size < 1) {
        return;
    }
    CARD32 *v = (CARD32 *) pProp->data;
    f->present |= WF_PRESENT_ICCCM_NORMAL;
    if ((v[0] & (1u << 4)) && pProp->size >= 11) {
        // width_inc/height_inc 非 1 表示按字符格布局（终端、编辑器）
        if (v[9] > 1 || v[10] > 1) {
            f->flags |= WF_FLAG_RESIZE_INC;
        }
    }
}

static void parse_motif(WindowFacts *f, PropertyPtr pProp) {
    if (!pProp || pProp->format != 32 || !pProp->data || pProp->size < 3) {
        return;
    }
    CARD32 *v = (CARD32 *) pProp->data;
    f->present |= WF_PRESENT_MOTIF;
    f->motif_present = 1;
    f->motif_flags = (int) v[0];
    f->motif_decorations = (int) v[2];
}

static void parse_pid(WindowFacts *f, PropertyPtr pProp) {
    if (!pProp || pProp->format != 32 || !pProp->data || pProp->size < 1) {
        return;
    }
    f->pid = (long) (((CARD32 *) pProp->data)[0]);
    f->present |= WF_PRESENT_PID;
}

// ---------------------------------------------------------------------------
// 采集入口
// ---------------------------------------------------------------------------

void window_facts_capture(WindowPtr pWin, WindowPtr pPropWin, WindowFacts *out) {
    if (!pWin || !out) {
        return;
    }
    if (!pPropWin) {
        pPropWin = pWin;
    }
    memset(out, 0, sizeof(*out));

    // 键和几何取自真正被重定向的窗口
    out->window = pWin->drawable.id;
    out->prop_source = pPropWin->drawable.id;
    out->parent = pWin->parent ? pWin->parent->drawable.id : 0;
    out->x = pWin->drawable.x;
    out->y = pWin->drawable.y;
    out->w = pWin->drawable.width;
    out->h = pWin->drawable.height;
    out->border_width = pWin->borderWidth;
    out->hint_input = -1;
    out->hint_initial_state = -1;
    out->captured_ms = now_ms();

    if (pWin->overrideRedirect) {
        out->flags |= WF_FLAG_OVERRIDE_REDIRECT;
    }
    if (pWin->parent && pWin->parent->parent == NULL) {
        out->flags |= WF_FLAG_ROOT_CHILD;
    }

    // 属性取自属性来源窗口
    if (!pPropWin->optional || !pPropWin->optional->userProps) {
        return;
    }

    for (PropertyPtr pProp = pPropWin->optional->userProps; pProp; pProp = pProp->next) {
        const char *name = NameForAtom(pProp->propertyName);
        if (!name) {
            continue;
        }
        unsigned char *data = pProp->data;
        unsigned long size = pProp->size;

        if (strcmp(name, "_NET_WM_WINDOW_TYPE") == 0) {
            parse_window_type(out, data, size);
        } else if (strcmp(name, "_NET_WM_STATE") == 0) {
            parse_window_state(out, data, size);
        } else if (strcmp(name, "WM_HINTS") == 0) {
            parse_wm_hints(out, pProp);
        } else if (strcmp(name, "WM_NORMAL_HINTS") == 0) {
            parse_wm_normal_hints(out, pProp);
        } else if (strcmp(name, "_MOTIF_WM_HINTS") == 0) {
            parse_motif(out, pProp);
        } else if (strcmp(name, "WM_TRANSIENT_FOR") == 0) {
            if (pProp->format == 32 && size >= 1) {
                out->transient_for = (Window) ((CARD32 *) data)[0];
                out->present |= WF_PRESENT_ICCCM_TRANSIENT;
            }
        } else if (strcmp(name, "WM_CLIENT_LEADER") == 0) {
            if (pProp->format == 32 && size >= 1) {
                out->leader = (Window) ((CARD32 *) data)[0];
                if (out->leader) {
                    out->present |= WF_PRESENT_LEADER;
                }
            }
        } else if (strcmp(name, "_NET_WM_PID") == 0) {
            parse_pid(out, pProp);
        } else if (strcmp(name, "WM_CLASS") == 0) {
            copy_prop_text(out, out->wm_class, (int) sizeof(out->wm_class), data, size);
            if (out->wm_class[0]) {
                out->present |= WF_PRESENT_CLASS;
            }
        } else if (strcmp(name, "_NET_WM_NAME") == 0) {
            copy_prop_text(out, out->net_wm_name, (int) sizeof(out->net_wm_name), data, size);
            if (out->net_wm_name[0]) {
                out->present |= WF_PRESENT_NAME;
            }
        } else if (strcmp(name, "WM_NAME") == 0) {
            copy_prop_text(out, out->wm_name, (int) sizeof(out->wm_name), data, size);
            if (out->wm_name[0]) {
                out->present |= WF_PRESENT_NAME;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 登记表
// ---------------------------------------------------------------------------

void window_facts_record(const WindowFacts *facts) {
    if (!facts || facts->window == 0) {
        return;
    }
    pthread_mutex_lock(&wf_lock);
    for (int i = 0; i < WF_MAX_WINDOWS; i++) {
        if (wf_entries[i].used && wf_entries[i].window == facts->window) {
            // 保留首次采集时刻，让 age 表示"这个窗口存在了多久"而不是"距最后一次属性变化多久"
            long long first_seen = wf_entries[i].facts.captured_ms;
            wf_entries[i].facts = *facts;
            if (first_seen > 0) {
                wf_entries[i].facts.captured_ms = first_seen;
            }
            wf_entries[i].facts.gone_ms = 0;
            pthread_mutex_unlock(&wf_lock);
            return;
        }
    }
    for (int i = 0; i < WF_MAX_WINDOWS; i++) {
        if (!wf_entries[i].used) {
            wf_entries[i].used = true;
            wf_entries[i].window = facts->window;
            wf_entries[i].facts = *facts;
            wf_entries[i].facts.gone_ms = 0;
            wf_used++;
            pthread_mutex_unlock(&wf_lock);
            return;
        }
    }
    // 表满：优先淘汰"已经消失并且最久"的那条，保住长生命周期窗口
    int victim = -1;
    long long oldest_gone = 0;
    for (int i = 0; i < WF_MAX_WINDOWS; i++) {
        if (!wf_entries[i].used || wf_entries[i].facts.gone_ms == 0) {
            continue;
        }
        if (victim < 0 || wf_entries[i].facts.gone_ms < oldest_gone) {
            victim = i;
            oldest_gone = wf_entries[i].facts.gone_ms;
        }
    }
    if (victim >= 0) {
        wf_entries[victim].window = facts->window;
        wf_entries[victim].facts = *facts;
        wf_entries[victim].facts.gone_ms = 0;
    }
    pthread_mutex_unlock(&wf_lock);
}

void window_facts_forget(Window window) {
    pthread_mutex_lock(&wf_lock);
    for (int i = 0; i < WF_MAX_WINDOWS; i++) {
        if (wf_entries[i].used && wf_entries[i].window == window) {
            // 不删除：菜单/工具提示只活几百毫秒，删了就再也看不到
            if (wf_entries[i].facts.gone_ms == 0) {
                wf_entries[i].facts.gone_ms = now_ms();
            }
            break;
        }
    }
    pthread_mutex_unlock(&wf_lock);
}

void window_facts_clear(void) {
    pthread_mutex_lock(&wf_lock);
    memset(wf_entries, 0, sizeof(wf_entries));
    wf_used = 0;
    pthread_mutex_unlock(&wf_lock);
}

int window_facts_count(void) {
    pthread_mutex_lock(&wf_lock);
    int n = wf_used;
    pthread_mutex_unlock(&wf_lock);
    return n;
}

// 按 xid 取 flags。Java 侧用它判断"这个窗口要不要上 dock"，
// 比把 skip-taskbar 一路加进 JNI 签名要小。
int window_facts_flags(Window window) {
    int flags = 0;
    pthread_mutex_lock(&wf_lock);
    for (int i = 0; i < WF_MAX_WINDOWS; i++) {
        if (wf_entries[i].used && wf_entries[i].window == window) {
            flags = (int) wf_entries[i].facts.flags;
            break;
        }
    }
    pthread_mutex_unlock(&wf_lock);
    return flags;
}

// ---------------------------------------------------------------------------
// 编码
// ---------------------------------------------------------------------------

// 值里不能出现分隔符，否则 Java 侧解析会错位
static void sanitize_into(char *dst, int dst_size, const char *src) {
    if (!dst || dst_size <= 0) {
        return;
    }
    int j = 0;
    for (int i = 0; src && src[i] && j < dst_size - 1; i++) {
        char c = src[i];
        if (c == '|' || c == '=' || c == '\n' || c == '\r' || c == '\t') {
            c = '_';
        }
        dst[j++] = c;
    }
    dst[j] = '\0';
}

static int encode_one(const WindowFacts *f, char *buf, int buf_size, long long now) {
    if (!f || !buf || buf_size <= 0) {
        return 0;
    }
    char cls[WF_TEXT_MAX * 2];
    char name[WF_TEXT_MAX * 2];
    char netname[WF_TEXT_MAX * 2];
    char types[WF_TEXT_MAX * 2];
    char states[WF_TEXT_MAX * 2];

    sanitize_into(cls, (int) sizeof(cls), f->wm_class);
    sanitize_into(name, (int) sizeof(name), f->wm_name);
    sanitize_into(netname, (int) sizeof(netname), f->net_wm_name);
    sanitize_into(types, (int) sizeof(types), f->type_names);
    sanitize_into(states, (int) sizeof(states), f->state_names);

    long long age = (now > f->captured_ms && f->captured_ms > 0) ? (now - f->captured_ms) : 0;
    long long gone = (f->gone_ms > 0 && now > f->gone_ms) ? (now - f->gone_ms) : -1;

    int n = snprintf(buf, (size_t) buf_size,
                     "xid=%lx|parent=%lx|src=%lx|or=%d|root=%d|type=%d|typenames=%s|state=%u|"
                     "statenames=%s|present=%u|flags=%u|input=%d|init=%d|"
                     "motif=%d/%d/%d|geom=%d,%d,%d,%d|border=%d|"
                     "transient=%lx|leader=%lx|group=%lx|pid=%ld|age=%lld|gone=%lld|kind=%d|"
                     "class=%s|name=%s|netname=%s",
                     (unsigned long) f->window,
                     (unsigned long) f->parent,
                     (unsigned long) f->prop_source,
                     (f->flags & WF_FLAG_OVERRIDE_REDIRECT) ? 1 : 0,
                     (f->flags & WF_FLAG_ROOT_CHILD) ? 1 : 0,
                     f->type_code,
                     types,
                     f->state_mask,
                     states,
                     f->present,
                     f->flags,
                     f->hint_input,
                     f->hint_initial_state,
                     f->motif_present, f->motif_flags, f->motif_decorations,
                     f->x, f->y, f->w, f->h,
                     f->border_width,
                     (unsigned long) f->transient_for,
                     (unsigned long) f->leader,
                     (unsigned long) f->group,
                     f->pid,
                     age,
                     gone,
                     f->actual_kind,
                     cls, name, netname);
    if (n < 0) {
        return 0;
    }
    return (n >= buf_size) ? buf_size - 1 : n;
}

int window_facts_encode(int index, char *buf, int buf_size) {
    if (!buf || buf_size <= 0) {
        return 0;
    }
    buf[0] = '\0';
    long long now = now_ms();
    pthread_mutex_lock(&wf_lock);
    int seen = 0;
    int written = 0;
    for (int i = 0; i < WF_MAX_WINDOWS; i++) {
        if (!wf_entries[i].used) {
            continue;
        }
        if (seen == index) {
            written = encode_one(&wf_entries[i].facts, buf, buf_size, now);
            break;
        }
        seen++;
    }
    pthread_mutex_unlock(&wf_lock);
    return written;
}

int window_facts_encode_by_xid(Window window, char *buf, int buf_size) {
    if (!buf || buf_size <= 0) {
        return 0;
    }
    buf[0] = '\0';
    long long now = now_ms();
    int written = 0;
    pthread_mutex_lock(&wf_lock);
    for (int i = 0; i < WF_MAX_WINDOWS; i++) {
        if (wf_entries[i].used && wf_entries[i].window == window) {
            written = encode_one(&wf_entries[i].facts, buf, buf_size, now);
            break;
        }
    }
    pthread_mutex_unlock(&wf_lock);
    return written;
}

// 供 java 侧观察"某个窗口登记了哪些事实"的计数，避免只靠字符串判断
int window_facts_present_mask(Window window) {
    int present = 0;
    pthread_mutex_lock(&wf_lock);
    for (int i = 0; i < WF_MAX_WINDOWS; i++) {
        if (wf_entries[i].used && wf_entries[i].window == window) {
            present = (int) wf_entries[i].facts.present;
            break;
        }
    }
    pthread_mutex_unlock(&wf_lock);
    return present;
}

// ---------------------------------------------------------------------------
// JNI：把登记表导成 String[]，每条一个窗口。
// 只读自己的登记表，不碰 X server 数据结构，可以被任意线程调用。
// ---------------------------------------------------------------------------

extern int util_is_valid_utf8(const char *string);

JNIEXPORT jint JNICALL
Java_com_fde_x11_Xserver_nativeWindowFactsFlags(JNIEnv *env, jclass cls, jlong window) {
    (void) env;
    (void) cls;
    return (jint) window_facts_flags((Window) window);
}

JNIEXPORT jobjectArray JNICALL
Java_com_fde_x11_Xserver_nativeDumpWindowFacts(JNIEnv *env, jclass cls) {
    (void) cls;
    int count = window_facts_count();
    jclass stringClass = (*env)->FindClass(env, "java/lang/String");
    if (!stringClass) {
        return NULL;
    }
    jobjectArray result = (*env)->NewObjectArray(env, count, stringClass, NULL);
    if (!result) {
        (*env)->DeleteLocalRef(env, stringClass);
        return NULL;
    }
    char buf[2048];
    for (int i = 0; i < count; i++) {
        int len = window_facts_encode(i, buf, (int) sizeof(buf));
        if (len <= 0) {
            continue;
        }
        // 窗口名是应用给的任意字节，非法 UTF-8 会让 NewStringUTF 直接崩
        const char *text = util_is_valid_utf8(buf) ? buf : "xid=invalid|name=invalid_utf8";
        jstring s = (*env)->NewStringUTF(env, text);
        if (!s) {
            continue;
        }
        (*env)->SetObjectArrayElement(env, result, i, s);
        (*env)->DeleteLocalRef(env, s);
    }
    (*env)->DeleteLocalRef(env, stringClass);
    return result;
}
