package com.fde.x11.window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * {@link WindowRolePolicy} 的行为锁。
 *
 * <p>这些用例的作用不是"证明实现正确"，而是把每条规则的**优先级**和**边界**写死：
 * 后面接入生产路径、加 WPS 之类覆盖项时，先看哪条规则被谁抢先。
 */
public class WindowRolePolicyTest {

    // Motif 位，故意在测试里重新写一遍，避免和实现共享同一个错误常量
    private static final int MWM_HINTS_FUNCTIONS = 1 << 0;
    private static final int MWM_HINTS_DECORATIONS = 1 << 1;
    private static final int MWM_DECOR_ALL = 1 << 0;
    private static final int MWM_DECOR_BORDER = 1 << 1;
    private static final int MWM_DECOR_RESIZEH = 1 << 2;
    private static final int MWM_DECOR_TITLE = 1 << 3;

    /** 一个"正常"的 root 顶层应用窗口。 */
    private static WindowFacts rootToplevel() {
        WindowFacts f = new WindowFacts();
        f.xid = 0x200001L;
        f.parent = 0x1297L;
        f.rootChild = true;
        f.typeCode = WindowFacts.TYPE_NORMAL;
        f.present = WindowFacts.PRESENT_EWMH_TYPE;
        f.w = 910;
        f.h = 580;
        f.x = 505;
        f.y = 44;
        f.hintInput = -1;
        return f;
    }

    private static WindowFacts parent(int x, int y, int w, int h) {
        WindowFacts p = new WindowFacts();
        p.xid = 0x200010L;
        p.rootChild = true;
        p.x = x;
        p.y = y;
        p.w = w;
        p.h = h;
        return p;
    }

    private static void assertRole(WindowRole role, WindowRole.Identity identity,
                                   WindowRole.Containment containment,
                                   WindowRole.Decoration decoration) {
        assertEquals(identity, role.identity);
        assertEquals(containment, role.containment);
        assertEquals(decoration, role.decoration);
    }

    private static WindowFacts motif(WindowFacts f, int flags, int decorations) {
        f.motifPresent = 1;
        f.motifFlags = flags;
        f.motifDecorations = decorations;
        f.present |= WindowFacts.PRESENT_MOTIF;
        return f;
    }

    // ------------------------------------------------------------------
    // 结构性判据：override-redirect / KDE override / client child / 极小窗
    // ------------------------------------------------------------------

    @Test
    public void overrideRedirectIsOverlay() {
        WindowFacts f = rootToplevel();
        f.overrideRedirect = true;

        WindowRole role = WindowRolePolicy.decide(f);

        assertRole(role, WindowRole.Identity.OVERLAY, WindowRole.Containment.FREE,
                WindowRole.Decoration.NONE);
        assertEquals("override-redirect", role.reason);
        assertFalse(role.modal);
    }

    @Test
    public void overrideRedirectInsideParentIsContained() {
        WindowFacts f = rootToplevel();
        f.overrideRedirect = true;
        f.x = 10;
        f.y = 10;
        f.w = 50;
        f.h = 40;

        WindowRole role = WindowRolePolicy.decide(f, parent(0, 0, 200, 200));

        assertRole(role, WindowRole.Identity.OVERLAY, WindowRole.Containment.IN_PARENT,
                WindowRole.Decoration.NONE);
    }

    @Test
    public void kdeOverrideOnTypedWindowKeepsTaskButHidesDock() {
        // kylin-connectivity 的现场：DIALOG + KDE override + MODAL。
        // KDE override 只说明"别装饰我、别上任务栏"，不改变身份。
        WindowFacts f = rootToplevel();
        f.flags |= WindowFacts.FLAG_KDE_OVERRIDE;

        WindowRole role = WindowRolePolicy.decide(f);

        assertRole(role, WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.NONE);
        assertFalse(role.hideFromDock);
    }

    @Test
    public void kdeOverrideDialogKeepsTaskIdentity() {
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_DIALOG;
        f.flags |= WindowFacts.FLAG_KDE_OVERRIDE | WindowFacts.FLAG_MODAL;

        WindowRole role = WindowRolePolicy.decide(f);

        assertRole(role, WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.NONE);
        assertTrue(role.modal);
        assertFalse(role.hideFromDock);
        assertEquals("ewmh:dialog", role.reason);
    }

    @Test
    public void kdeOverrideWithInputFalseIsOverlay() {
        // 明确说不要键盘焦点 → 浮层正合适（View 宿主本来就是 NOT_FOCUSABLE）
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_DIALOG;
        f.flags |= WindowFacts.FLAG_KDE_OVERRIDE | WindowFacts.FLAG_INPUT_FALSE;
        f.hintInput = 0;
        f.present |= WindowFacts.PRESENT_ICCCM_WM_HINTS;

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Identity.OVERLAY, role.identity);
        assertEquals("ewmh-type:kde-override+input-false", role.reason);
    }

    @Test
    public void kdeOverrideWithoutStandardTypeIsOverlay() {
        // 只声明了 KDE 那个 atom（没有标准类型）：当作浮层。
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_NONE;
        f.flags |= WindowFacts.FLAG_KDE_OVERRIDE;

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Identity.OVERLAY, role.identity);
        assertEquals("ewmh-type:kde-override", role.reason);
        assertFalse(role.hideFromDock);
    }

    @Test
    public void kdeOverrideDoesNotBeatClientChildStructure() {
        // 结构判据比"意愿"硬：客户端自己的子窗口还是内嵌，KDE override 只让它不上 dock。
        WindowFacts f = rootToplevel();
        f.rootChild = false;
        f.flags |= WindowFacts.FLAG_KDE_OVERRIDE;

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Identity.OVERLAY, role.identity);
        assertEquals(WindowRole.Containment.IN_PARENT, role.containment);
        assertEquals("structure:client-child", role.reason);
        assertFalse(role.hideFromDock);
    }

    @Test
    public void clientChildIsEmbedded() {
        WindowFacts f = rootToplevel();
        f.rootChild = false;
        f.typeCode = WindowFacts.TYPE_MENU; // 类型意愿不能盖过结构事实

        WindowRole role = WindowRolePolicy.decide(f);

        assertRole(role, WindowRole.Identity.OVERLAY, WindowRole.Containment.IN_PARENT,
                WindowRole.Decoration.NONE);
        assertEquals("structure:client-child", role.reason);
    }

    @Test
    public void tinyRootToplevelIsHelper() {
        WindowFacts f = rootToplevel();
        f.w = 1;
        f.h = 1;

        WindowRole role = WindowRolePolicy.decide(f);

        assertRole(role, WindowRole.Identity.OVERLAY, WindowRole.Containment.FREE,
                WindowRole.Decoration.NONE);
        assertEquals("helper:tiny-window", role.reason);
    }

    @Test
    public void borderlineHelperSizeIsStillHelper() {
        WindowFacts f = rootToplevel();
        f.w = 16;
        f.h = 16;

        assertEquals("helper:tiny-window", WindowRolePolicy.decide(f).reason);
    }

    @Test
    public void unknownGeometryIsNotTreatedAsHelper() {
        // 0x0 是"几何还没定"，不是"窗口很小"：按尺寸判身份会误伤，
        // 这种窗口应该按类型/属性继续判。
        WindowFacts f = rootToplevel();
        f.w = 0;
        f.h = 0;

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Identity.TASK, role.identity);
        assertEquals("ewmh:normal", role.reason);
    }

    // ------------------------------------------------------------------
    // EWMH 类型
    // ------------------------------------------------------------------

    @Test
    public void systrayKeepsSystrayIdentity() {
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_SYSTRAY;

        WindowRole role = WindowRolePolicy.decide(f);

        assertRole(role, WindowRole.Identity.SYSTRAY, WindowRole.Containment.FREE,
                WindowRole.Decoration.NONE);
        assertEquals("ewmh:systray", role.reason);
    }

    @Test
    public void utilityIsOverlay() {
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_UTILITY;
        f.flags |= WindowFacts.FLAG_INPUT_FALSE;
        f.hintInput = 0;
        f.present |= WindowFacts.PRESENT_ICCCM_WM_HINTS;

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Identity.OVERLAY, role.identity);
        assertEquals("ewmh:auxiliary-type=" + WindowFacts.TYPE_UTILITY, role.reason);
    }

    @Test
    public void utilityInsideParentIsContained() {
        // ToDesk 的下拉菜单（UTILITY）：194x120 落在主窗矩形内，应该合进父 Surface，
        // 不要变成一块带自己阴影的独立窗口。
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_UTILITY;
        f.x = 100;
        f.y = 100;
        f.w = 194;
        f.h = 120;
        f.flags |= WindowFacts.FLAG_INPUT_FALSE;
        f.hintInput = 0;
        f.present |= WindowFacts.PRESENT_ICCCM_WM_HINTS;

        WindowRole role = WindowRolePolicy.decide(f, parent(0, 0, 1200, 720));

        assertEquals(WindowRole.Identity.OVERLAY, role.identity);
        assertEquals(WindowRole.Containment.IN_PARENT, role.containment);
    }

    @Test
    public void skipTaskbarOnTypedWindowStaysTaskButHidesDock() {
        // 有类型声明时 SKIP_TASKBAR 不改身份，只让它在 dock 上消失。
        WindowFacts f = rootToplevel();
        f.flags |= WindowFacts.FLAG_SKIP_TASKBAR;
        f.x = 100;
        f.y = 100;
        f.w = 80;
        f.h = 60;

        WindowRole role = WindowRolePolicy.decide(f, parent(0, 0, 400, 400));

        assertRole(role, WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.CAPTION);
        assertTrue(role.hideFromDock);
        assertEquals("ewmh:normal", role.reason);
    }

    @Test
    public void dialogWithSkipTaskbarKeepsTaskIdentity() {
        // glade / filezilla 的现场：DIALOG + MODAL + SKIP_TASKBAR。
        // 身份必须是 Task（浮层拿不到键盘焦点，模态框就废了），只是不上 dock。
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_DIALOG;
        f.flags |= WindowFacts.FLAG_MODAL | WindowFacts.FLAG_SKIP_TASKBAR;
        f.transientFor = 0x200010L;
        f.present |= WindowFacts.PRESENT_ICCCM_TRANSIENT;

        WindowRole role = WindowRolePolicy.decide(f);

        assertRole(role, WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.NONE);
        assertTrue(role.modal);
        assertTrue(role.hideFromDock);
        assertEquals("ewmh:dialog", role.reason);
    }

    @Test
    public void untypedSkipTaskbarIsOverlay() {
        // 没有类型声明、只靠 SKIP_TASKBAR 表达"我不是普通窗口"：面板 / 托盘 / 桌面组件。
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_NONE;
        f.present = WindowFacts.PRESENT_EWMH_STATE;
        f.flags |= WindowFacts.FLAG_SKIP_TASKBAR;

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Identity.OVERLAY, role.identity);
        assertEquals("ewmh-state:skip-taskbar", role.reason);
        assertTrue(role.hideFromDock);
    }

    @Test
    public void inputFalseIsOverlay() {
        WindowFacts f = rootToplevel();
        f.flags |= WindowFacts.FLAG_INPUT_FALSE;

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Identity.OVERLAY, role.identity);
        assertEquals("icccm:input=false", role.reason);
    }

    // ------------------------------------------------------------------
    // DIALOG 与模态
    // ------------------------------------------------------------------

    @Test
    public void dialogIsTaskWithoutCaption() {
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_DIALOG;

        WindowRole role = WindowRolePolicy.decide(f);

        assertRole(role, WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.NONE);
        assertFalse(role.modal);
    }

    @Test
    public void transientDialogIsNotModalByItself() {
        // WM_TRANSIENT_FOR 只表达"依附关系"，模态要显式 _NET_WM_STATE_MODAL。
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_DIALOG;
        f.transientFor = 0x200010L;
        f.present |= WindowFacts.PRESENT_ICCCM_TRANSIENT;

        WindowRole role = WindowRolePolicy.decide(f);

        assertFalse(role.modal);
    }

    @Test
    public void dialogWithEwmhModalIsModal() {
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_DIALOG;
        f.flags |= WindowFacts.FLAG_MODAL;

        assertTrue(WindowRolePolicy.decide(f).modal);
    }

    // ------------------------------------------------------------------
    // Motif 装饰的三态
    // ------------------------------------------------------------------

    @Test
    public void normalWithoutMotifGetsCaption() {
        WindowRole role = WindowRolePolicy.decide(rootToplevel());

        assertRole(role, WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.CAPTION);
        assertEquals("ewmh:normal", role.reason);
    }

    @Test
    public void motifWithoutDecorationHintKeepsCaption() {
        // 只声明 FUNCTIONS、没声明 DECORATIONS：客户端没说装饰的事，保持默认。
        WindowFacts f = motif(rootToplevel(), MWM_HINTS_FUNCTIONS, 0);

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Decoration.CAPTION, role.decoration);
        assertEquals("ewmh:normal", role.reason);
    }

    @Test
    public void motifExplicitEmptyDecorationsDropsCaption() {
        WindowFacts f = motif(rootToplevel(), MWM_HINTS_DECORATIONS, 0);

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Decoration.NONE, role.decoration);
        assertEquals("ewmh:normal+motif-no-decoration", role.reason);
    }

    @Test
    public void motifTitleBitKeepsCaption() {
        WindowFacts f = motif(rootToplevel(), MWM_HINTS_DECORATIONS, MWM_DECOR_TITLE);

        assertEquals(WindowRole.Decoration.CAPTION, WindowRolePolicy.decide(f).decoration);
    }

    @Test
    public void motifAllBitKeepsCaption() {
        WindowFacts f = motif(rootToplevel(), MWM_HINTS_DECORATIONS, MWM_DECOR_ALL);

        assertEquals(WindowRole.Decoration.CAPTION, WindowRolePolicy.decide(f).decoration);
    }

    @Test
    public void motifBorderOnlyDropsCaption() {
        // GTK 的 CSD 窗口常报 BORDER（自己画标题栏）：边框位不等于标题栏位。
        WindowFacts f = motif(rootToplevel(), MWM_HINTS_DECORATIONS, MWM_DECOR_BORDER);

        assertEquals(WindowRole.Decoration.NONE, WindowRolePolicy.decide(f).decoration);
    }

    @Test
    public void motifBorderAndResizehDropsCaption() {
        WindowFacts f = motif(rootToplevel(), MWM_HINTS_DECORATIONS,
                MWM_DECOR_BORDER | MWM_DECOR_RESIZEH);

        assertEquals(WindowRole.Decoration.NONE, WindowRolePolicy.decide(f).decoration);
    }

    // ------------------------------------------------------------------
    // 应用级覆盖与兜底
    // ------------------------------------------------------------------

    @Test
    public void wpsOverrideWins() {
        WindowFacts f = rootToplevel();
        f.netWmName = "WPS文字";
        f.w = 300; // 尺寸启发式不该参与身份判定
        f.h = 200;

        WindowRole role = WindowRolePolicy.decide(f);

        assertRole(role, WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.NONE);
        assertEquals("override:wps", role.reason);
    }

    @Test
    public void androidFrameOverrideWins() {
        WindowFacts f = rootToplevel();
        f.wmName = "android_frame";

        WindowRole role = WindowRolePolicy.decide(f);

        assertRole(role, WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.CAPTION);
        assertEquals("override:android-frame", role.reason);
    }

    @Test
    public void propertylessWindowIsHelper() {
        // ICCCM 要求属性在 map 前设好：map 之后一个属性都没有的顶层窗，是工具包的临时窗。
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_NONE;
        f.present = 0;
        f.w = 716;
        f.h = 36;

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Identity.OVERLAY, role.identity);
        assertEquals("helper:no-properties", role.reason);
    }

    @Test
    public void unknownTypeFallsBackToImplicitNormal() {
        WindowFacts f = rootToplevel();
        f.typeCode = WindowFacts.TYPE_NONE;
        // 有元数据、只是没声明类型（例如裸 Xlib 应用设了 WM_CLASS）
        f.present = WindowFacts.PRESENT_CLASS;

        WindowRole role = WindowRolePolicy.decide(f);

        assertEquals(WindowRole.Identity.TASK, role.identity);
        assertEquals("implicit:no-ewmh-type", role.reason);
    }

    @Test
    public void nullFactsFallBackToTask() {
        WindowRole role = WindowRolePolicy.decide(null);

        assertRole(role, WindowRole.Identity.TASK, WindowRole.Containment.FREE,
                WindowRole.Decoration.CAPTION);
        assertEquals("null-facts", role.reason);
    }

    @Test
    public void hostKindMapsToExistingHosts() {
        assertEquals("ACTIVITY_DECOR", WindowRolePolicy.decide(rootToplevel()).hostKind());

        WindowFacts dialog = rootToplevel();
        dialog.typeCode = WindowFacts.TYPE_DIALOG;
        assertEquals("ACTIVITY_NODECOR", WindowRolePolicy.decide(dialog).hostKind());

        WindowFacts overlay = rootToplevel();
        overlay.overrideRedirect = true;
        assertEquals("OVERLAY", WindowRolePolicy.decide(overlay).hostKind());
    }
}
