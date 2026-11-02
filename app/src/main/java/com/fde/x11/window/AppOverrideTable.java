package com.fde.x11.window;

import java.util.ArrayList;
import java.util.List;

/**
 * 应用级覆盖表：给"不守规范"的应用强行指定角色。
 *
 * <p>现实里总会有应用报错类型或干脆不报类型（WPS 的放映窗、FDE 自己的合成窗等）。
 * 这类特例不应该散落在 native 的 if 里，而应该是一张可维护、可扩展、可运行时追加的表。
 *
 * <p>规则按加入顺序匹配，第一个命中的生效。匹配键是"包含"语义，因为
 * WM_CLASS 常见形式是 {@code "instance\0ClassName\0"}，实际取到的可能只是一部分。
 */
public final class AppOverrideTable {

    public static final class Rule {
        public final String matchClass;
        public final String matchName;
        public final WindowRole.Identity identity;
        public final WindowRole.Containment containment;
        public final WindowRole.Decoration decoration;
        public final String reason;

        Rule(String matchClass, String matchName,
             WindowRole.Identity identity, WindowRole.Containment containment,
             WindowRole.Decoration decoration, String reason) {
            this.matchClass = matchClass;
            this.matchName = matchName;
            this.identity = identity;
            this.containment = containment;
            this.decoration = decoration;
            this.reason = reason;
        }

        boolean matches(WindowFacts f) {
            if (matchClass != null && !matchClass.isEmpty()) {
                String cls = f.wmClass == null ? "" : f.wmClass;
                if (!cls.toLowerCase().contains(matchClass.toLowerCase())) {
                    return false;
                }
            }
            if (matchName != null && !matchName.isEmpty()) {
                String name = f.displayName();
                if (name == null || !name.contains(matchName)) {
                    return false;
                }
            }
            return true;
        }
    }

    private static final List<Rule> RULES = new ArrayList<>();

    static {
        // WPS 放映窗口不按规范设置装饰提示，native 里曾经用
        // net_wm_name == "WPS文字" 硬编码 support_motif=1。这里把它收进表里。
        addRule(new Rule(null, "WPS文字",
                WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.NONE, "override:wps"));

        // FDE 桌面自己的合成外壳窗口，必须永远作为独立 Task。
        addRule(new Rule(null, "android_frame",
                WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.CAPTION, "override:android-frame"));
    }

    private AppOverrideTable() {
    }

    public static synchronized void addRule(Rule rule) {
        if (rule != null) {
            RULES.add(rule);
        }
    }

    static synchronized WindowRole lookup(WindowFacts f, WindowFacts parent) {
        for (Rule rule : RULES) {
            if (!rule.matches(f)) {
                continue;
            }
            WindowRole.Containment containment = rule.containment;
            if (containment == WindowRole.Containment.IN_PARENT && parent == null) {
                containment = WindowRole.Containment.FREE;
            }
            return new WindowRole(rule.identity, containment, rule.decoration,
                    false, rule.reason);
        }
        return null;
    }
}
