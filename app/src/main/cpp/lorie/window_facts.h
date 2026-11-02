//
// X 窗口"事实"采集层。
//
// 这一层只负责回答"这个窗口身上到底有哪些元数据"，不做任何
// "它是 Task 还是 Overlay" 的判断。判断放在 Java 的
// com.fde.x11.window.WindowRolePolicy 里，因为：
//   1. 纯函数可单测，不依赖 X server 和 Android；
//   2. 规则需要频繁调整（WPS 这类应用不守规范），改 Java 不用重编 native。
//
// 之所以要单独抽一层，是因为 X 的元数据是多套协议叠加的：
//   L1 ICCCM : WM_TRANSIENT_FOR / WM_HINTS / WM_NORMAL_HINTS / WM_CLASS
//   L2 Motif : _MOTIF_WM_HINTS
//   L3 EWMH  : _NET_WM_WINDOW_TYPE / _NET_WM_STATE / _NET_WM_PID
//   L4 空白   : 裸 Xlib 应用，什么提示都不给
// "某一层协议存在与否"本身就是最强的分类信号，所以这里用 present 位显式记录。
//

#ifndef FDE_X11_WINDOW_FACTS_H
#define FDE_X11_WINDOW_FACTS_H

#include <X11/X.h>
#include <jni.h>
#include <stdbool.h>
#include <sys/types.h>
#include <window.h>

#ifdef __cplusplus
extern "C" {
#endif

// 哪些元数据存在（存在与否本身是判据）
#define WF_PRESENT_EWMH_TYPE       (1u << 0)
#define WF_PRESENT_EWMH_STATE      (1u << 1)
#define WF_PRESENT_ICCCM_TRANSIENT (1u << 2)
#define WF_PRESENT_ICCCM_WM_HINTS  (1u << 3)
#define WF_PRESENT_ICCCM_NORMAL    (1u << 4)
#define WF_PRESENT_MOTIF           (1u << 5)
#define WF_PRESENT_LEADER          (1u << 6)
#define WF_PRESENT_GROUP           (1u << 7)
#define WF_PRESENT_PID             (1u << 8)
#define WF_PRESENT_CLASS           (1u << 9)
#define WF_PRESENT_NAME            (1u << 10)

// 布尔事实
#define WF_FLAG_OVERRIDE_REDIRECT  (1u << 0)
#define WF_FLAG_ROOT_CHILD         (1u << 1)
#define WF_FLAG_INPUT_FALSE        (1u << 2)  // WM_HINTS.input == False
#define WF_FLAG_MODAL              (1u << 3)  // _NET_WM_STATE_MODAL
#define WF_FLAG_SKIP_TASKBAR       (1u << 4)
#define WF_FLAG_SKIP_PAGER         (1u << 5)
#define WF_FLAG_ABOVE              (1u << 6)
#define WF_FLAG_BELOW              (1u << 7)
#define WF_FLAG_STICKY             (1u << 8)
#define WF_FLAG_FULLSCREEN         (1u << 9)
#define WF_FLAG_MAXIMIZED          (1u << 10)
#define WF_FLAG_RESIZE_INC         (1u << 11) // WM_NORMAL_HINTS 带 PResizeInc（终端/编辑器）

// 窗口类型：沿用 android.h 里的内部枚举，0 表示客户端没有声明
#define WF_TYPE_NONE 0

// 状态位（_NET_WM_STATE 的解析结果，与 flags 冗余但便于日志阅读）
#define WF_STATE_MODAL        (1u << 0)
#define WF_STATE_SKIP_TASKBAR (1u << 1)
#define WF_STATE_SKIP_PAGER   (1u << 2)

#define WF_TEXT_MAX 128

// 实际选用的宿主种类。native 只能区分到"是不是内嵌 widget"，
// Activity 的 decor/overlay/systray 细分类由 Java 侧补齐。
#define WF_ACTUAL_UNKNOWN        0
#define WF_ACTUAL_WIDGET         1   // 合进父窗口 Surface 的子窗口
#define WF_ACTUAL_VIEW           2   // 独立浮窗
#define WF_ACTUAL_SYSTRAY        3
#define WF_ACTUAL_ACTIVITY_DECOR 4
#define WF_ACTUAL_ACTIVITY_NODECOR 5

typedef struct {
    unsigned int present;
    unsigned int flags;

    Window window;
    Window parent;
    Window transient_for;
    Window leader;
    Window group;
    // 属性实际读取的窗口。android_frame 这类"外壳窗口"的属性挂在第一个子窗口上，
    // 记录来源便于排查"为什么这个窗口看起来什么提示都没有"。
    Window prop_source;

    int type_code;                  // _NET_WM_WINDOW_TYPE 解析结果（内部枚举）
    unsigned int state_mask;

    int x, y, w, h;
    int border_width;

    int hint_input;                 // -1 未设置 / 0 False / 1 True
    int hint_initial_state;         // -1 未设置

    int motif_present;
    int motif_flags;
    int motif_decorations;

    long pid;

    // 采集时刻（毫秒，CLOCK_MONOTONIC），用于计算窗口存活时长
    long long captured_ms;
    // 窗口消失时刻（毫秒），0 表示还活着。
    // 保留消失的窗口是必须的：菜单/工具提示只活几百毫秒，手动 dump 抓不到，
    // 只能靠"最近出现过"的历史来找它们。
    long long gone_ms;
    // 实际选用的宿主，见 WF_ACTUAL_*
    int actual_kind;

    char wm_class[WF_TEXT_MAX];
    char wm_name[WF_TEXT_MAX];
    char net_wm_name[WF_TEXT_MAX];
    char type_names[WF_TEXT_MAX * 2];
    char state_names[WF_TEXT_MAX * 2];
} WindowFacts;

// 在 android_create_attr 里调用：此时持有 X server 线程和 pWin，可以安全读属性。
// pWin 是真正被重定向的窗口（几何/键），pPropWin 是属性实际来源
// （android_frame 的外壳窗口，提示属性挂在它的第一个子窗口上）。
void window_facts_capture(WindowPtr pWin, WindowPtr pPropWin, WindowFacts *out);

// 把采集结果登记下来（按 xid 覆盖），供任意线程快照读取。
void window_facts_record(const WindowFacts *facts);

// 窗口销毁/解绑时清理。
void window_facts_forget(Window window);

// 清空全部（服务重启时）。
void window_facts_clear(void);

// 当前登记窗口数量。
int window_facts_count(void);

// 把第 index 个窗口的 facts 编码成 "k=v|k=v|..." 文本。
// 返回实际写入长度；文本超长时截断（诊断用，不追求完整性）。
int window_facts_encode(int index, char *buf, int buf_size);

// 按 xid 查一条 facts 文本，找不到返回 0。
int window_facts_encode_by_xid(Window window, char *buf, int buf_size);

// 某个窗口登记了哪些事实（present 位），未登记返回 0。
int window_facts_present_mask(Window window);

#ifdef __cplusplus
}
#endif

#endif // FDE_X11_WINDOW_FACTS_H
