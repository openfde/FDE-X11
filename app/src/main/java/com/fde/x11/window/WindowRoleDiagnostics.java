package com.fde.x11.window;

import android.util.Log;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * 窗口角色的"影子判定"诊断。
 *
 * <p>这一版**不改变任何行为**：把 native 采集到的证据跑一遍
 * {@link WindowRolePolicy}，和实际选用的宿主做对比，只打日志。
 *
 * <p>为什么要先做影子判定：现在的判定规则是长期试错堆出来的（尺寸阈值、WPS 特例、
 * overrideRedirect 改写类型……），直接替换风险太大。先用真实窗口数据把"当前实现和
 * 新规则分歧在哪"量出来，再决定哪些规则可以直接换、哪些要保留成覆盖项。
 *
 * <p>日志刻意不走 {@code FLog}：FLog 的开关是 {@code BuildConfig.DEBUG}，设备上装的
 * 多半是 release 包，诊断设施必须在 release 下也能用。
 *
 * <p>触发方式：
 * <pre>adb shell am broadcast -a com.fde.x11.DUMP_WINDOW_ROLES -p com.fde.x11</pre>
 */
public final class WindowRoleDiagnostics {

    /** 与 XWindowService.ROLE_DUMP_TAG 保持一致。 */
    private static final String TAG = "winrole";

    private static final String MATCH = "MATCH";
    private static final String IDENTITY_DIFF = "IDENTITY_DIFF";
    private static final String DECOR_DIFF = "DECOR_DIFF";
    private static final String CONTAINMENT_DIFF = "CONTAINMENT_DIFF";
    private static final String UNKNOWN = "ACTUAL_UNKNOWN";

    private WindowRoleDiagnostics() {
    }

    private static void log(String message) {
        Log.i(TAG, message);
    }

    /**
     * @param encoded     native 编码的每窗口一行（包含已消失的窗口）
     * @param actualHosts xid -> 当前实际选用的宿主；Activity 的细分类以这里为准，
     *                    内嵌 widget 不会出现在这张表里，回退到 native 记录的 kind
     */
    public static void dump(String[] encoded, Map<Long, String> actualHosts) {
        if (encoded == null) {
            log("dump: nativeDumpWindowFacts() 返回 null（库未加载或符号缺失）");
            return;
        }
        if (encoded.length == 0) {
            log("dump: 登记表为空 —— 还没有窗口被 trace，请先打开一个 Linux 应用");
            return;
        }

        List<WindowFacts> factsList = new ArrayList<>(encoded.length);
        Map<Long, WindowFacts> byXid = new HashMap<>(encoded.length * 2);
        // android_frame 这类外壳窗口，属性挂在应用自己的 toplevel 上。
        // 子窗口的 WM_TRANSIENT_FOR 指向的是 toplevel（如 0x600006），
        // 而登记表里的键是外壳窗口（如 0x200004），所以再建一张 propSource 反查表。
        Map<Long, WindowFacts> byPropSource = new HashMap<>(encoded.length * 2);
        for (String line : encoded) {
            WindowFacts f = WindowFacts.parse(line);
            if (f == null) {
                log("dump: 无法解析一行: " + line);
                continue;
            }
            factsList.add(f);
            byXid.put(f.xid, f);
            if (f.propSource != 0 && f.propSource != f.xid) {
                byPropSource.put(f.propSource, f);
            }
        }

        int match = 0;
        int identityDiff = 0;
        int decorDiff = 0;
        int containmentDiff = 0;
        int unknown = 0;
        Map<String, Integer> reasons = new HashMap<>();

        log("dump: " + factsList.size() + " window(s)（含已消失）");
        for (WindowFacts f : factsList) {
            WindowFacts parent = resolveParent(f, byXid, byPropSource);
            WindowRole role = WindowRolePolicy.decide(f, parent);

            String actual = actualHosts == null ? null : actualHosts.get(f.xid);
            if (actual == null) {
                actual = f.actualKindName();
            }

            String verdict;
            if (actual == null) {
                verdict = UNKNOWN;
                unknown++;
                bump(reasons, "? <- " + role.identity + " : " + role.reason);
            } else if (!identityMatches(role.identity, actual)) {
                verdict = IDENTITY_DIFF;
                identityDiff++;
                bump(reasons, "identity " + role.identity + " <- " + actual + " : " + role.reason);
            } else if (!decorMatches(role.decoration, actual)) {
                verdict = DECOR_DIFF;
                decorDiff++;
                bump(reasons, "decor " + role.decoration + " <- " + actual + " : " + role.reason);
            } else if (!containmentMatches(role, actual)) {
                // 归属维度：策略认为该内嵌、实际却是独立浮窗，或者反过来。
                // 以前不比较这一维，所以像"结构性判据提前"这类重排在这里是看不见的。
                verdict = CONTAINMENT_DIFF;
                containmentDiff++;
                bump(reasons, "containment " + role.containment + " <- " + actual
                        + " : " + role.reason);
            } else {
                verdict = MATCH;
                match++;
            }

            log(String.format(
                    "  xid=0x%x class=%s name=%s type=%d[%s] state=[%s] or=%b input=%d flags=0x%x "
                            + "geom=%dx%d@%d,%d transient=0x%x leader=0x%x present=0x%x "
                            + "motif=%d/%d/%d age=%s | policy=%s | actual=%s | %s",
                    f.xid, f.wmClass, f.displayName(), f.typeCode, f.typeNames,
                    f.stateNames, f.overrideRedirect, f.hintInput, f.flags,
                    f.w, f.h, f.x, f.y, f.transientFor, f.leader, f.present,
                    f.motifPresent, f.motifFlags, f.motifDecorations,
                    ageText(f),
                    role, actual, verdict));
        }

        log("summary: total=" + factsList.size() + " match=" + match
                + " identityDiff=" + identityDiff + " decorDiff=" + decorDiff
                + " containmentDiff=" + containmentDiff + " unknown=" + unknown);
        for (Map.Entry<String, Integer> e : reasons.entrySet()) {
            log("  diff[" + e.getValue() + "] " + e.getKey());
        }
    }

    /**
     * 找父窗口。子窗口的 WM_TRANSIENT_FOR（或 X 层的 parent）指向应用的 toplevel，
     * 而登记表里的键是 WM 外壳窗口，所以要同时按 xid 和 propSource 反查。
     */
    private static WindowFacts resolveParent(WindowFacts f,
                                             Map<Long, WindowFacts> byXid,
                                             Map<Long, WindowFacts> byPropSource) {
        long[] candidates = {f.transientFor, f.parent};
        for (long candidate : candidates) {
            if (candidate == 0) {
                continue;
            }
            WindowFacts found = byXid.get(candidate);
            if (found == null) {
                found = byPropSource.get(candidate);
            }
            if (found != null && found.xid != f.xid) {
                return found;
            }
        }
        return null;
    }

    private static String ageText(WindowFacts f) {
        if (f.isGone()) {
            return "GONE(" + f.goneMs + "ms ago)";
        }
        return f.ageMs + "ms";
    }

    private static void bump(Map<String, Integer> map, String key) {
        Integer c = map.get(key);
        map.put(key, c == null ? 1 : c + 1);
    }

    /** 身份维度：Task / Overlay / Systray 是否一致。 */
    private static boolean identityMatches(WindowRole.Identity identity, String actual) {
        switch (identity) {
            case TASK:
                return actual.startsWith("ACTIVITY");
            case SYSTRAY:
                return "SYSTRAY".equals(actual);
            case OVERLAY:
                return "VIEW".equals(actual) || "WIDGET".equals(actual);
            default:
                return false;
        }
    }

    /** 装饰维度：只有 Task 才有"要不要 Shell 标题栏"这个问题。 */

    /**
     * 归属维度：策略说要内嵌（IN_PARENT）的窗口，实际必须是以 WIDGET 形式承载；
     * 说要独立浮窗（FREE）的，实际应该是 VIEW / 独立任务窗口。
     * 只有 OVERLAY 身份有意义，任务窗口和任务栏图标恒为 FREE。
     */
    private static boolean containmentMatches(WindowRole role, String actual) {
        if (role.identity != WindowRole.Identity.OVERLAY) {
            return true;
        }
        boolean actualContained = "WIDGET".equals(actual);
        return role.containment == WindowRole.Containment.IN_PARENT ? actualContained : !actualContained;
    }

    /**
     * 实时模式：单个窗口产生时立刻打一行，由 Xserver.startOrUpdateWindow 调用。
     * 和 dump() 用同一套判定，只是只输出一条、不打 summary。
     */
    public static void dumpOne(String[] encoded, long xid, Map<Long, String> actualHosts) {
        if (encoded == null || encoded.length == 0) {
            return;
        }
        WindowFacts target = null;
        for (String line : encoded) {
            WindowFacts f = WindowFacts.parse(line);
            if (f != null && f.xid == xid) {
                target = f;
                break;
            }
        }
        if (target == null) {
            // 窗口还没进登记表（例如内嵌 widget 没走 android_create_attr）
            log("[live] xid=0x" + Long.toHexString(xid) + " 尚未登记");
            return;
        }
        WindowFacts parent = null;
        for (String line : encoded) {
            WindowFacts f = WindowFacts.parse(line);
            if (f == null) {
                continue;
            }
            if (f.xid == target.transientFor || f.propSource == target.transientFor) {
                parent = f;
                break;
            }
        }
        WindowRole role = WindowRolePolicy.decide(target, parent);
        String actual = actualHosts == null ? null : actualHosts.get(xid);
        if (actual == null) {
            actual = target.actualKindName();
        }
        String verdict;
        if (actual == null) {
            verdict = UNKNOWN;
        } else if (!identityMatches(role.identity, actual)) {
            verdict = IDENTITY_DIFF;
        } else if (!decorMatches(role.decoration, actual)) {
            verdict = DECOR_DIFF;
        } else if (!containmentMatches(role, actual)) {
            verdict = CONTAINMENT_DIFF;
        } else {
            verdict = MATCH;
        }
        log(String.format("[live] xid=0x%x class=%s type=%d[%s] input=%d or=%b flags=0x%x "
                        + "| policy=%s | actual=%s | %s",
                target.xid, target.wmClass, target.typeCode, target.typeNames,
                target.hintInput, target.overrideRedirect, target.flags, role, actual, verdict));
    }

    private static boolean decorMatches(WindowRole.Decoration decoration, String actual) {
        if (!actual.startsWith("ACTIVITY")) {
            return true;
        }
        return decoration == WindowRole.Decoration.CAPTION
                ? "ACTIVITY_DECOR".equals(actual)
                : "ACTIVITY_NODECOR".equals(actual);
    }
}
