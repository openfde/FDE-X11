package com.fde.x11.window;

/**
 * 一个 X 窗口应该用哪种 Android 组件来承载。
 *
 * <p>关键是：这不是"一个类型"，而是三个正交维度的组合。以前的代码把三维压成
 * 一个 {@code window_type} 整数（267..276、1000、1001），所以每加一个场景就得
 * 加一个魔法数，而且这三个维度本来可以自由组合。
 *
 * <ul>
 *   <li>{@link Identity} —— 它在 Android 里有没有"身份"：进不进最近任务/任务栏、
 *       能不能被 Alt-Tab、有没有独立的 Task 生命周期。</li>
 *   <li>{@link Containment} —— 它画在哪：合进父窗口的 Surface，还是独立 Surface。</li>
 *   <li>{@link Decoration} —— 谁画标题栏：Shell 画还是不要。</li>
 * </ul>
 *
 * <p>三个维度都必须判对，因为它们的"交互契约"完全不同：错一个维度，用户 3 秒内
 * 就能感知到"很奇怪"（菜单抢焦点、对话框父窗口不变灰、工具窗多出一个任务栏条目）。
 */
public final class WindowRole {

    public enum Identity {
        /** 独立 Task：进最近任务/任务栏，可拖拽缩放最大化。 */
        TASK,
        /** 浮层：不占任务栏、不抢焦点、跟随父窗口。 */
        OVERLAY,
        /** 系统托盘：常驻系统级 UI，不属于任何应用 Task。 */
        SYSTRAY
    }

    public enum Containment {
        /** 独立 Surface。 */
        FREE,
        /** 合进父窗口的 Surface（子窗口在父可视区内时）。 */
        IN_PARENT
    }

    public enum Decoration {
        /** 由 Shell 画标题栏。 */
        CAPTION,
        /** 不要标题栏（内容自带边框，或根本是浮层）。 */
        NONE
    }

    public final Identity identity;
    public final Containment containment;
    public final Decoration decoration;
    /** 是否让父窗口进入不可交互状态（模态）。 */
    public final boolean modal;
    /** 命中的规则文本，排查用。 */
    public final String reason;

    public WindowRole(Identity identity, Containment containment, Decoration decoration,
                      boolean modal, String reason) {
        this.identity = identity;
        this.containment = containment;
        this.decoration = decoration;
        this.modal = modal;
        this.reason = reason;
    }

    /** 对应到现有实现里的宿主：Activity（带 decor）/ Activity（无 decor）/ View。 */
    public String hostKind() {
        if (identity == Identity.TASK) {
            return decoration == Decoration.CAPTION ? "ACTIVITY_DECOR" : "ACTIVITY_NODECOR";
        }
        if (identity == Identity.SYSTRAY) {
            return "SYSTRAY";
        }
        return containment == Containment.IN_PARENT ? "WIDGET" : "OVERLAY";
    }

    @Override
    public String toString() {
        return identity + "/" + containment + "/" + decoration
                + (modal ? "/modal" : "") + "(" + reason + ")";
    }
}
