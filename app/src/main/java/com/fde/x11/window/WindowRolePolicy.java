package com.fde.x11.window;

/**
 * 判定一个 X 窗口该用哪种 Android 组件承载。
 *
 * <p>纯函数、不依赖 Android / X server，可以直接单测。规则要频繁调整
 * （总有 WPS 这种不守规范的应用），所以刻意放在 Java 而不是 native。
 *
 * <h3>判定顺序</h3>
 * 按 X 的语义层次从"最硬"的证据往"最软"的走，第一个命中的生效：
 * <ol>
 *   <li>override-redirect：客户端明确绕过 WM，一定有身份 → 浮层</li>
 *   <li>EWMH 显式类型（SYSTRAY / 各类弹出窗）</li>
 *   <li>EWMH 状态（SKIP_TASKBAR 明确要求不进任务栏）</li>
 *   <li>ICCCM {@code WM_HINTS.input == False}：明确说"不要键盘焦点" → 浮层</li>
 *   <li>EWMH DIALOG → 独立 Task，但不带标题栏</li>
 *   <li>EWMH NORMAL / 未声明 → 独立 Task，装饰看 Motif</li>
 *   <li>兜底 → 独立 Task</li>
 * </ol>
 *
 * <h3>为什么"尺寸"不参与身份判定</h3>
 * 现有实现用 {@code w < 400 || h < 250} 把小尺寸的 NORMAL 窗口改判成 DIALOG。
 * 这混淆了两件事：尺寸是"装饰"的弱信号，不该改变"身份"。一个 300x200 的工具窗口
 * 仍然是独立窗口，把它做成不带标题栏的 Task 会让用户没法关掉它。这里不采用该启发式，
 * 诊断输出里会把它暴露成差异，便于用真实数据决定要不要保留。
 */
public final class WindowRolePolicy {

    // Motif WM hints 位
    private static final int MWM_HINTS_DECORATIONS = 1 << 1;
    private static final int MWM_DECOR_ALL = 1 << 0;
    private static final int MWM_DECOR_TITLE = 1 << 3;
    private static final int MWM_DECOR_MENU = 1 << 4;
    private static final int MWM_DECOR_MINIMIZE = 1 << 5;
    private static final int MWM_DECOR_MAXIMIZE = 1 << 6;

    /** 声明为"附属窗口"的 EWMH 类型：一定没有独立身份。 */
    private static boolean isAuxiliaryType(int typeCode) {
        switch (typeCode) {
            case WindowFacts.TYPE_UTILITY:
            case WindowFacts.TYPE_MENU:
            case WindowFacts.TYPE_POPUP_MENU:
            case WindowFacts.TYPE_DROPDOWN_MENU:
            case WindowFacts.TYPE_TOOLTIP:
            case WindowFacts.TYPE_COMBO:
            case WindowFacts.TYPE_DND:
                return true;
            default:
                return false;
        }
    }

    /**
     * Motif 装饰判定。
     *
     * <p>注意 {@code return 1} 在 native 里曾经表示"无装饰"，而"客户端根本没表达
     * 装饰意愿"和"客户端要求无装饰"被混成了同一个结果——只设 FUNCTIONS 不设
     * DECORATIONS 的客户端会意外丢掉标题栏。这里把三态分开。
     *
     * @return true 表示客户端明确要求无装饰
     */
    private static boolean motifWantsNoDecoration(WindowFacts f) {
        if (f.motifPresent == 0) {
            return false;
        }
        if ((f.motifFlags & MWM_HINTS_DECORATIONS) == 0) {
            // 没说装饰的事 → 保持默认（有装饰）
            return false;
        }
        int mask = MWM_DECOR_ALL | MWM_DECOR_TITLE | MWM_DECOR_MENU
                | MWM_DECOR_MINIMIZE | MWM_DECOR_MAXIMIZE;
        return (f.motifDecorations & mask) == 0;
    }

    /** 窗口矩形是否完全落在父窗口矩形内（坐标都是相对 root 的绝对值）。 */
    private static boolean isInsideParent(WindowFacts f, WindowFacts parent) {
        if (parent == null) {
            return false;
        }
        return f.x >= parent.x && f.y >= parent.y
                && f.x + f.w <= parent.x + parent.w
                && f.y + f.h <= parent.y + parent.h;
    }

    private static WindowRole.Containment containmentFor(WindowFacts f, WindowFacts parent) {
        // 只有浮层才需要区分"合进父 Surface"还是"独立 Surface"。
        // 独立 Task 永远 Free。
        if (parent == null) {
            return WindowRole.Containment.FREE;
        }
        return isInsideParent(f, parent)
                ? WindowRole.Containment.IN_PARENT
                : WindowRole.Containment.FREE;
    }

    public static WindowRole decide(WindowFacts f) {
        return decide(f, null);
    }

    /**
     * @param f      当前窗口的事实
     * @param parent 父窗口的事实，可为 null（父窗口不在登记表里）
     */
    public static WindowRole decide(WindowFacts f, WindowFacts parent) {
        if (f == null) {
            return new WindowRole(WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                    WindowRole.Decoration.CAPTION, false, "null-facts");
        }
        return decideInner(f, parent);
    }

    private static WindowRole decideInner(WindowFacts f, WindowFacts parent) {
        // ---- 0. 应用级覆盖，优先级最高（WPS 这类不守规范的应用）----
        WindowRole override = AppOverrideTable.lookup(f, parent);
        if (override != null) {
            return override;
        }

        // ---- 1. override-redirect：客户端明确绕过 WM ----
        if (f.overrideRedirect) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    containmentFor(f, parent),
                    WindowRole.Decoration.NONE,
                    false,
                    "override-redirect");
        }

        // ---- 2. EWMH 显式类型 ----
        if (f.typeCode == WindowFacts.TYPE_SYSTRAY) {
            return new WindowRole(WindowRole.Identity.SYSTRAY,
                    WindowRole.Containment.FREE,
                    WindowRole.Decoration.NONE, false, "ewmh:systray");
        }
        if (f.typeCode == WindowFacts.TYPE_SYSTIP) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    WindowRole.Containment.FREE,
                    WindowRole.Decoration.NONE, false, "ewmh:systip");
        }
        if (isAuxiliaryType(f.typeCode)) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    containmentFor(f, parent),
                    WindowRole.Decoration.NONE,
                    false,
                    "ewmh:auxiliary-type=" + f.typeCode);
        }

        // ---- 3. EWMH 状态：明确要求不进任务栏 ----
        if (f.flag(WindowFacts.FLAG_SKIP_TASKBAR)) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    containmentFor(f, parent),
                    WindowRole.Decoration.NONE,
                    false,
                    "ewmh-state:skip-taskbar");
        }

        // ---- 4. ICCCM WM_HINTS.input == False：不想要键盘焦点 ----
        // 菜单、工具提示、装饰窗都会设这个。对话框和主窗口一定不会。
        if (f.flag(WindowFacts.FLAG_INPUT_FALSE)) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    containmentFor(f, parent),
                    WindowRole.Decoration.NONE,
                    false,
                    "icccm:input=false");
        }

        // ---- 5. EWMH DIALOG：独立 Task，但内容自带边框 ----
        if (f.typeCode == WindowFacts.TYPE_DIALOG) {
            // 模态判据：显式 MODAL 状态，或 ICCCM 的 transient（ICCCM 只有这一种"对话框"表达）。
            // 注意浮层（菜单/工具窗）永远不该是模态——现有实现对"任何 transient"都让父窗口
            // 不可聚焦，这正是"交互很奇怪"的来源之一。
            boolean modal = f.flag(WindowFacts.FLAG_MODAL)
                    || (f.transientFor != 0 && f.has(WindowFacts.PRESENT_ICCCM_TRANSIENT));
            return new WindowRole(WindowRole.Identity.TASK,
                    WindowRole.Containment.FREE,
                    WindowRole.Decoration.NONE,
                    modal,
                    "ewmh:dialog");
        }

        // ---- 6. EWMH NORMAL / 未声明类型 ----
        // 非 root 子窗口 + 非 override-redirect：客户端自己的子窗口，应该内嵌
        if (!f.rootChild && !f.overrideRedirect) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    WindowRole.Containment.IN_PARENT,
                    WindowRole.Decoration.NONE,
                    false,
                    "structure:client-child");
        }

        boolean noDecor = motifWantsNoDecoration(f);
        String reason = f.typeCode == WindowFacts.TYPE_NORMAL
                ? "ewmh:normal"
                : "implicit:no-ewmh-type";
        if (noDecor) {
            reason += "+motif-no-decoration";
        }
        return new WindowRole(WindowRole.Identity.TASK,
                WindowRole.Containment.FREE,
                noDecor ? WindowRole.Decoration.NONE : WindowRole.Decoration.CAPTION,
                f.flag(WindowFacts.FLAG_MODAL),
                reason);
    }
}
