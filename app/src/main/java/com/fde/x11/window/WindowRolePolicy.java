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
 *   <li>override-redirect；KDE override 且（没有标准类型 或 input == False）也按浮层</li>
 *   <li>非 root 子窗口：X 结构上就是父窗口的一部分 → 内嵌到父窗口</li>
 *   <li>协议辅助窗口（root 顶层 + 非 override-redirect + 极小尺寸，或**一个属性都没有**）→ 浮层</li>
 *   <li>EWMH 显式类型（SYSTRAY / SYSTIP；受管的 UTILITY、菜单类 → 无标题栏 Task）</li>
 *   <li>未声明类型 + EWMH 状态 SKIP_TASKBAR（面板、托盘这类"不是窗口"的东西）</li>
 *   <li>ICCCM {@code WM_HINTS.input == False}：明确说"不要键盘焦点" → 浮层</li>
 *   <li>EWMH DIALOG → 独立 Task，但不带标题栏</li>
 *   <li>EWMH NORMAL / 未声明 → 独立 Task，装饰看 Motif</li>
 * </ol>
 *
 * <p>{@code SKIP_TASKBAR} 和 {@code _KDE_NET_WM_WINDOW_TYPE_OVERRIDE}（有标准类型时）
 * 一律只落到 {@link WindowRole#hideFromDock}，不影响身份：窗口还是 Task（能拿焦点、
 * 输入法），只是不上 dock。
 *
 * <h3>为什么"尺寸"不参与身份判定</h3>
 * 老实现用 {@code w < 400 || h < 250} 把小尺寸的 NORMAL 窗口改判成 DIALOG。
 * 这混淆了两件事：尺寸是"装饰"的弱信号，不该改变"身份"。一个 300x200 的工具窗口
 * 仍然是独立窗口，把它做成不带标题栏的 Task 会让用户没法关掉它。
 *
 * <p>真实翻车现场：mate-terminal 映射时是 361x76（客户端请求的初始尺寸），几十毫秒后
 * 才被 Android 视图的尺寸覆盖成 654x440。按映射那一刻的尺寸判身份，结果就是这个普通
 * 窗口永久没有标题栏——resize 不会重新判定宿主。该启发式已经从
 * {@code Xserver.startOrUpdateWindow} 里删掉，这里不再有第二个尺寸阈值。
 *
 * <h3>与生产流程的关系（刻意保留两份实现，暂不合并）</h3>
 *
 * <p>本类只读 {@link WindowFacts} 快照、只输出结论，不驱动任何行为（影子模式）。真正的
 * 宿主选择仍然在 {@code Xserver.startOrUpdateWindow} / {@code android.c} 里按同一套语义
 * 独立实现。两边<b>故意不合并</b>，原因是：
 *
 * <ul>
 *   <li>这一层是"独立的第二个判断者"：它能在不改变行为的前提下，把"规则认为应该是什么"
 *       和"生产实际做了什么"逐条对比，{@code IDENTITY_DIFF / DECOR_DIFF / CONTAINMENT_DIFF}
 *       就是这么来的。合并进生产代码后，诊断会退化成"用实现解释实现"，失去参考价值；</li>
 *   <li>排查现实问题时，这份纯函数判定可以随手改写、跑单测验证，不必担心把正在运行的
 *       窗口管理改坏；</li>
 *   <li>代价是两份口径要同步维护——所以两边改动必须成对进行，并在提交说明里注明。</li>
 * </ul>
 *
 * <p>如果将来要合并，前提是先有一套覆盖足够全的现场数据 + 单测，能把生产行为固定下来；
 * 在那之前保持现状。
 */
public final class WindowRolePolicy {

    // Motif WM hints 位
    private static final int MWM_HINTS_DECORATIONS = 1 << 1;
    private static final int MWM_DECOR_ALL = 1 << 0;
    private static final int MWM_DECOR_TITLE = 1 << 3;
    private static final int MWM_DECOR_MENU = 1 << 4;
    private static final int MWM_DECOR_MINIMIZE = 1 << 5;
    private static final int MWM_DECOR_MAXIMIZE = 1 << 6;

    /**
     * 跟"要不要 Shell 标题栏"有关的位。
     *
     * <p>刻意不含 {@code MWM_DECOR_BORDER}(1<<1) 和 {@code MWM_DECOR_RESIZEH}(1<<2)：
     * 那两位说的是"画边框 / 留可拖拽的边"，不是"要标题栏"。GTK 的 CSD 窗口经常报
     * {@code BORDER|RESIZEH}，它自己画标题栏，这里必须判成无装饰。
     */
    private static final int MWM_DECOR_TITLE_BITS =
            MWM_DECOR_ALL | MWM_DECOR_TITLE | MWM_DECOR_MENU
                    | MWM_DECOR_MINIMIZE | MWM_DECOR_MAXIMIZE;

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
        return (f.motifDecorations & MWM_DECOR_TITLE_BITS) == 0;
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
        WindowRole role = decideInner(f, parent);
        // SKIP_TASKBAR 是应用明确表达"别把我放进任务栏"的意愿，只影响呈现，不改变身份
        //（模态对话框仍是 Task，做成浮层就拿不到键盘焦点）。
        // 注意这里不包含 KDE override：WPS 给所有窗口（含主窗）都塞了
        // _KDE_NET_WM_WINDOW_TYPE_OVERRIDE 当"不要装饰"用，把它当"不进任务栏"
        // 会让主窗从 dock 上消失。KDE override 只保留"无装饰"语义。
        boolean hidden = f.flag(WindowFacts.FLAG_SKIP_TASKBAR);
        if (hidden && !role.hideFromDock) {
            return role.withHideFromDock();
        }
        return role;
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

        // ---- 1b. KDE override：只在"不需要键盘"时当浮层 ----
        // 两种情况：
        //   a) 只声明了 KDE atom、没有任何标准类型；
        //   b) 明确说不要键盘焦点（WM_HINTS.input == False）。
        // 需要输入的窗口（kylin-connectivity 的 60x60 模态框，input=1）保持 Task：
        // 浮层宿主带 FLAG_NOT_FOCUSABLE，键盘进不去，模态框会变成"能看不能打"。
        // WM_HINTS 没写（hintInput == -1）时按"可能要输入"处理，不降级成浮层。
        if (f.flag(WindowFacts.FLAG_KDE_OVERRIDE)
                && (f.typeCode == WindowFacts.TYPE_NONE
                    || f.flag(WindowFacts.FLAG_INPUT_FALSE))) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    containmentFor(f, parent),
                    WindowRole.Decoration.NONE,
                    false,
                    f.typeCode == WindowFacts.TYPE_NONE
                            ? "ewmh-type:kde-override"
                            : "ewmh-type:kde-override+input-false");
        }

        // ---- 2. 非 root 子窗口：客户端自己的子窗口，应该内嵌到父窗口 ----
        // 这一条和 override-redirect 一样是 X 的结构性事实，比任何客户端声明的
        // hint 都硬，所以必须排在 EWMH / ICCCM 的类型和状态之前。
        // （原先排在最后，会被 SKIP_TASKBAR、input=false 这些"意愿"抢先生效。）
        if (!f.rootChild && !f.overrideRedirect) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    WindowRole.Containment.IN_PARENT,
                    WindowRole.Decoration.NONE,
                    false,
                    "structure:client-child");
        }

        // ---- 3. 协议辅助窗口：root 顶层 + 非 override-redirect + 极小尺寸 ----
        // Qt / GTK 启动时会建一批 1x1、10x10 的辅助窗口（选择、剪贴板、
        // client leader 探测），通常毫秒级就销毁。以前这类窗口也会被当成正常窗口
        // 重定向，造成多余的 Activity / 任务窗口、屏幕闪一下、甚至抢焦点。
        // 典型现场：点击 peony 时日志里出现 CreateNotify 400001 (10x10)，
        // 紧接着 OnCreateNotify 就为它建了 frame。
        // 0x0 不算小窗：那是几何还没定下来（或被截断）的未知值，不该按尺寸判身份。
        if (f.rootChild && !f.overrideRedirect && f.w > 0 && f.h > 0
                && f.w <= 16 && f.h <= 16) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    WindowRole.Containment.FREE,
                    WindowRole.Decoration.NONE,
                    false,
                    "helper:tiny-window");
        }

        // ---- 3b. 一个属性都没有的顶层窗口：工具包的临时窗 ----
        // ICCCM 要求属性在 map 之前设好；map 之后连 WM_CLASS 都没有的窗口，基本都是
        // Qt/GTK 建了又马上销毁的探测窗。宿主侧也不会给它们建窗口（见 android.c）。
        if (f.present == 0) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    WindowRole.Containment.FREE,
                    WindowRole.Decoration.NONE,
                    false,
                    "helper:no-properties");
        }

        // ---- 4. EWMH 显式类型 ----
        if (f.typeCode == WindowFacts.TYPE_SPLASH) {
            // 启动画面：无装饰的独立窗口（浮在主窗之上、通常带进度动画），不上 dock。
            // 以前不认识 SPLASH，会落回 NORMAL 被当成普通窗口套上标题栏。
            // 启动画面本身就"不该出现在 dock"，所以这里直接把 hideFromDock 置位，
            // 不依赖应用是否顺带设了 SKIP_TASKBAR。
            return new WindowRole(WindowRole.Identity.TASK,
                    WindowRole.Containment.FREE,
                    WindowRole.Decoration.NONE,
                    false,
                    "ewmh:splash").withHideFromDock();
        }
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
            // 小尺寸浮动工具条（WPS 那个 102x40 的黑条）：UTILITY + SKIP_TASKBAR
            // + 小尺寸。语义就是"浮在上层的小面板"，不该变成独立窗口；它只吃鼠标，
            // 所以即使 input=1 也按浮层处理。
            if (f.typeCode == WindowFacts.TYPE_UTILITY
                    && (f.flag(WindowFacts.FLAG_SKIP_TASKBAR)
                        || f.flag(WindowFacts.FLAG_KDE_OVERRIDE))
                    && f.w > 0 && f.w <= 320 && f.h > 0 && f.h <= 120) {
                return new WindowRole(WindowRole.Identity.OVERLAY,
                        containmentFor(f, parent),
                        WindowRole.Decoration.NONE,
                        false,
                        "ewmh:utility-floating-toolbar");
            }
            // 需要键盘的（账号登录这种 UTILITY + input=true）必须是 Task：
            // 浮层宿主是 FLAG_NOT_FOCUSABLE，做成浮层就"能看不能打字"。
            // 不需要键盘的菜单/提示/工具条才是挂在父窗上的浮层。
            if (!f.flag(WindowFacts.FLAG_INPUT_FALSE)) {
                return new WindowRole(WindowRole.Identity.TASK,
                        WindowRole.Containment.FREE,
                        WindowRole.Decoration.NONE,
                        false,
                        "ewmh:auxiliary-type=" + f.typeCode + "+input");
            }
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    containmentFor(f, parent),
                    WindowRole.Decoration.NONE,
                    false,
                    "ewmh:auxiliary-type=" + f.typeCode);
        }

        // ---- 5. 没声明类型 + SKIP_TASKBAR：面板 / 托盘 / 桌面组件 ----
        // 有类型声明的窗口不在这里判身份。典型反例是 glade/filezilla 的模态对话框：
        // DIALOG + MODAL + SKIP_TASKBAR，若按 SKIP_TASKBAR 判成浮层，浮层宿主拿不到
        // 键盘焦点，模态框就没法输入了。这类窗口保持 Task，只在 dock 上隐藏。
        if (f.typeCode == WindowFacts.TYPE_NONE && f.flag(WindowFacts.FLAG_SKIP_TASKBAR)) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    containmentFor(f, parent),
                    WindowRole.Decoration.NONE,
                    false,
                    "ewmh-state:skip-taskbar");
        }

        // ---- 5a. KDE override + SKIP_TASKBAR：不参与任务管理的弹层 ----
        // WPS 的下拉面板（class=wps，280x62 / 220x281，[KDE_OVERRIDE, NORMAL] +
        // SKIP_TASKBAR + input=true）就是这一类。以前这两个标记只当"无装饰 + 不上
        // dock"，于是它被判成 Task、在 Android 侧变成独立窗口——交互从"点一下浮层"
        // 变成"切任务"。登录框（同样 KDE_OVERRIDE，但没有 SKIP_TASKBAR）不受影响。
        if (f.flag(WindowFacts.FLAG_KDE_OVERRIDE) && f.flag(WindowFacts.FLAG_SKIP_TASKBAR)) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    containmentFor(f, parent),
                    WindowRole.Decoration.NONE,
                    false,
                    "ewmh:kde-override+skip-taskbar");
        }

        // ---- 6. ICCCM WM_HINTS.input == False：不想要键盘焦点 ----
        // 菜单、工具提示、装饰窗都会设这个。对话框和主窗口一定不会。
        if (f.flag(WindowFacts.FLAG_INPUT_FALSE)) {
            return new WindowRole(WindowRole.Identity.OVERLAY,
                    containmentFor(f, parent),
                    WindowRole.Decoration.NONE,
                    false,
                    "icccm:input=false");
        }

        // ---- 7. EWMH DIALOG：独立 Task，但内容自带边框 ----
        if (f.typeCode == WindowFacts.TYPE_DIALOG) {
            // 模态只认显式的 _NET_WM_STATE_MODAL。
            // WM_TRANSIENT_FOR 只说明"依附于哪个窗口"（父窗口、层叠顺序），
            // 不说明"父窗口要不要变灰"。以前把任何 transient 都当模态，导致
            // 普通弹窗把父窗口的输入吞掉，这正是"交互很奇怪"的来源之一。
            boolean modal = f.flag(WindowFacts.FLAG_MODAL);
            return new WindowRole(WindowRole.Identity.TASK,
                    WindowRole.Containment.FREE,
                    WindowRole.Decoration.NONE,
                    modal,
                    "ewmh:dialog");
        }

        // ---- 8. EWMH NORMAL / 未声明类型：兜底 ----
        // KDE override 也意味着"不要装饰"，即使 Motif 没表达装饰意愿。
        boolean motifNoDecor = motifWantsNoDecoration(f);
        boolean noDecor = motifNoDecor || f.flag(WindowFacts.FLAG_KDE_OVERRIDE);
        String reason = f.typeCode == WindowFacts.TYPE_NORMAL
                ? "ewmh:normal"
                : "implicit:no-ewmh-type";
        if (noDecor) {
            // 分开标注来源：motif=0/0/0 却写 "+motif-no-decoration" 会误导排查
            reason += motifNoDecor ? "+motif-no-decoration" : "+kde-override-no-decoration";
        }
        return new WindowRole(WindowRole.Identity.TASK,
                WindowRole.Containment.FREE,
                noDecor ? WindowRole.Decoration.NONE : WindowRole.Decoration.CAPTION,
                f.flag(WindowFacts.FLAG_MODAL),
                reason);
    }
}
