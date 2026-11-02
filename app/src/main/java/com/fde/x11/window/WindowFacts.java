package com.fde.x11.window;

/**
 * 一个 X 窗口身上采集到的全部元数据。
 *
 * <p>这个类只做"数据的容器 + 解析"，不含任何判断逻辑。判断在
 * {@link WindowRolePolicy} 里，这样它可以脱离 X server / Android 单测。
 *
 * <p>字段来源（X 的元数据是多套协议叠加的，缺失本身就是信号）：
 * <ul>
 *   <li>ICCCM：WM_TRANSIENT_FOR / WM_HINTS / WM_NORMAL_HINTS / WM_CLASS</li>
 *   <li>Motif：_MOTIF_WM_HINTS</li>
 *   <li>EWMH：_NET_WM_WINDOW_TYPE / _NET_WM_STATE / _NET_WM_PID</li>
 *   <li>空白：裸 Xlib 应用什么都不给</li>
 * </ul>
 *
 * <p>数据由 native 的 {@code window_facts.c} 采集并编码成 {@code k=v|k=v} 文本，
 * 这里 {@link #parse(String)} 还原。
 */
public final class WindowFacts {

    // ---- present：哪些协议/属性存在 ----
    public static final int PRESENT_EWMH_TYPE = 1 << 0;
    public static final int PRESENT_EWMH_STATE = 1 << 1;
    public static final int PRESENT_ICCCM_TRANSIENT = 1 << 2;
    public static final int PRESENT_ICCCM_WM_HINTS = 1 << 3;
    public static final int PRESENT_ICCCM_NORMAL = 1 << 4;
    public static final int PRESENT_MOTIF = 1 << 5;
    public static final int PRESENT_LEADER = 1 << 6;
    public static final int PRESENT_GROUP = 1 << 7;
    public static final int PRESENT_PID = 1 << 8;
    public static final int PRESENT_CLASS = 1 << 9;
    public static final int PRESENT_NAME = 1 << 10;

    // ---- flags：布尔事实 ----
    public static final int FLAG_OVERRIDE_REDIRECT = 1 << 0;
    public static final int FLAG_ROOT_CHILD = 1 << 1;
    public static final int FLAG_INPUT_FALSE = 1 << 2;
    public static final int FLAG_MODAL = 1 << 3;
    public static final int FLAG_SKIP_TASKBAR = 1 << 4;
    public static final int FLAG_SKIP_PAGER = 1 << 5;
    public static final int FLAG_ABOVE = 1 << 6;
    public static final int FLAG_BELOW = 1 << 7;
    public static final int FLAG_STICKY = 1 << 8;
    public static final int FLAG_FULLSCREEN = 1 << 9;
    public static final int FLAG_MAXIMIZED = 1 << 10;
    public static final int FLAG_RESIZE_INC = 1 << 11;

    // ---- 窗口类型：与 native / Xserver 的内部枚举保持一致 ----
    public static final int TYPE_NONE = 0;
    public static final int TYPE_COMBO = 268;
    public static final int TYPE_DIALOG = 269;
    public static final int TYPE_DND = 270;
    public static final int TYPE_DROPDOWN_MENU = 271;
    public static final int TYPE_MENU = 272;
    public static final int TYPE_NORMAL = 273;
    public static final int TYPE_POPUP_MENU = 274;
    public static final int TYPE_TOOLTIP = 275;
    public static final int TYPE_UTILITY = 276;
    public static final int TYPE_SYSTRAY = 1000;
    public static final int TYPE_SYSTIP = 1001;

    public long xid;
    public long parent;
    /** 属性实际读取的窗口（android_frame 的属性挂在第一个子窗口上）。 */
    public long propSource;

    public boolean overrideRedirect;
    public boolean rootChild;

    public int typeCode;
    public String typeNames = "";
    public int stateMask;
    public String stateNames = "";

    public int present;
    public int flags;

    /** -1 表示 WM_HINTS 里没有 InputHint。 */
    public int hintInput = -1;
    /** -1 表示 WM_HINTS 里没有 StateHint。 */
    public int hintInitialState = -1;

    public int motifPresent;
    public int motifFlags;
    public int motifDecorations;

    public int x, y, w, h;
    public int borderWidth;

    public long transientFor;
    public long leader;
    public long group;
    public long pid;

    /** 采集时刻到现在的毫秒数。 */
    public long ageMs;

    /** 窗口消失到现在多少毫秒；-1 表示还活着。 */
    public long goneMs = -1;

    /** native 侧记录的实际宿主，见 ACTUAL_* 常量；0 表示未知（由 Java 侧补齐）。 */
    public int actualKind;

    public static final int ACTUAL_UNKNOWN = 0;
    public static final int ACTUAL_WIDGET = 1;
    public static final int ACTUAL_VIEW = 2;
    public static final int ACTUAL_SYSTRAY = 3;
    public static final int ACTUAL_ACTIVITY_DECOR = 4;
    public static final int ACTUAL_ACTIVITY_NODECOR = 5;

    public boolean isGone() {
        return goneMs >= 0;
    }

    /** 把 native 的 actualKind 折算成和策略侧同一套宿主名字。 */
    public String actualKindName() {
        switch (actualKind) {
            case ACTUAL_WIDGET: return "WIDGET";
            case ACTUAL_VIEW: return "VIEW";
            case ACTUAL_SYSTRAY: return "SYSTRAY";
            case ACTUAL_ACTIVITY_DECOR: return "ACTIVITY_DECOR";
            case ACTUAL_ACTIVITY_NODECOR: return "ACTIVITY_NODECOR";
            default: return null;
        }
    }

    public String wmClass = "";
    public String wmName = "";
    public String netWmName = "";

    public boolean has(int presentBit) {
        return (present & presentBit) != 0;
    }

    public boolean flag(int flagBit) {
        return (flags & flagBit) != 0;
    }

    public String displayName() {
        if (!netWmName.isEmpty()) {
            return netWmName;
        }
        if (!wmName.isEmpty()) {
            return wmName;
        }
        return wmClass;
    }

    /**
     * 解析 native 编码的一行。格式：
     * {@code xid=%lx|parent=%lx|src=%lx|or=%d|root=%d|type=%d|typenames=...|state=%u|...}
     *
     * <p>解析失败返回 null —— 诊断路径，宁可丢一条也不要抛出。
     */
    public static WindowFacts parse(String line) {
        if (line == null || line.isEmpty()) {
            return null;
        }
        WindowFacts f = new WindowFacts();
        for (String part : line.split("\\|")) {
            int eq = part.indexOf('=');
            if (eq <= 0) {
                continue;
            }
            String key = part.substring(0, eq);
            String value = part.substring(eq + 1);
            try {
                switch (key) {
                    case "xid": f.xid = Long.parseUnsignedLong(value, 16); break;
                    case "parent": f.parent = Long.parseUnsignedLong(value, 16); break;
                    case "src": f.propSource = Long.parseUnsignedLong(value, 16); break;
                    case "or": f.overrideRedirect = "1".equals(value); break;
                    case "root": f.rootChild = "1".equals(value); break;
                    case "type": f.typeCode = Integer.parseInt(value); break;
                    case "typenames": f.typeNames = value; break;
                    case "state": f.stateMask = Integer.parseInt(value); break;
                    case "statenames": f.stateNames = value; break;
                    case "present": f.present = Integer.parseInt(value); break;
                    case "flags": f.flags = Integer.parseInt(value); break;
                    case "input": f.hintInput = Integer.parseInt(value); break;
                    case "init": f.hintInitialState = Integer.parseInt(value); break;
                    case "transient": f.transientFor = Long.parseUnsignedLong(value, 16); break;
                    case "leader": f.leader = Long.parseUnsignedLong(value, 16); break;
                    case "group": f.group = Long.parseUnsignedLong(value, 16); break;
                    case "pid": f.pid = Long.parseLong(value); break;
                    case "age": f.ageMs = Long.parseLong(value); break;
                    case "gone": f.goneMs = Long.parseLong(value); break;
                    case "kind": f.actualKind = Integer.parseInt(value); break;
                    case "border": f.borderWidth = Integer.parseInt(value); break;
                    case "class": f.wmClass = value; break;
                    case "name": f.wmName = value; break;
                    case "netname": f.netWmName = value; break;
                    case "motif": {
                        String[] m = value.split("/");
                        if (m.length == 3) {
                            f.motifPresent = Integer.parseInt(m[0]);
                            f.motifFlags = Integer.parseInt(m[1]);
                            f.motifDecorations = Integer.parseInt(m[2]);
                        }
                        break;
                    }
                    case "geom": {
                        String[] g = value.split(",");
                        if (g.length == 4) {
                            f.x = Integer.parseInt(g[0]);
                            f.y = Integer.parseInt(g[1]);
                            f.w = Integer.parseInt(g[2]);
                            f.h = Integer.parseInt(g[3]);
                        }
                        break;
                    }
                    default:
                        break;
                }
            } catch (NumberFormatException ignored) {
                // 单字段解析失败不影响其它字段
            }
        }
        if (f.xid == 0) {
            return null;
        }
        return f;
    }

    @Override
    public String toString() {
        return "WindowFacts{0x" + Long.toHexString(xid)
                + " type=" + typeCode + "[" + typeNames + "]"
                + " state=[" + stateNames + "]"
                + " or=" + overrideRedirect
                + " input=" + hintInput
                + " motif=" + motifPresent + "/" + motifFlags + "/" + motifDecorations
                + " transient=0x" + Long.toHexString(transientFor)
                + " leader=0x" + Long.toHexString(leader)
                + " geom=" + w + "x" + h + "@" + x + "," + y
                + " class=" + wmClass
                + " name=" + displayName()
                + " age=" + ageMs + "ms}";
    }
}
