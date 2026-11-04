package com.fde.x11.window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * native 编码（{@code k=v|k=v}）与 Java 解析之间的契约。
 *
 * <p>这条线断了不会崩，只会让规则悄悄失效（字段全 0），所以每个字段都要有用例盯着。
 */
public class WindowFactsTest {

    @Test
    public void parsesEncodedRow() {
        String line = "xid=200004|parent=1297|src=200004|or=0|root=1|type=273"
                + "|typenames=_NET_WM_WINDOW_TYPE_NORMAL,_KDE_NET_WM_WINDOW_TYPE_OVERRIDE"
                + "|state=1|statenames=_NET_WM_STATE_MODAL|present=1019|flags=4112"
                + "|input=1|init=-1|motif=1/2/0|geom=505,44,910,580|border=0"
                + "|transient=200010|leader=800001|group=0|pid=1234|age=10|gone=-1|kind=4"
                + "|class=ToDesk|name=|netname=ToDesk";

        WindowFacts f = WindowFacts.parse(line);

        assertNotNull(f);
        assertEquals(0x200004L, f.xid);
        assertEquals(0x1297L, f.parent);
        assertEquals(0x200004L, f.propSource);
        assertFalse(f.overrideRedirect);
        assertTrue(f.rootChild);

        assertEquals(WindowFacts.TYPE_NORMAL, f.typeCode);
        assertEquals("_NET_WM_WINDOW_TYPE_NORMAL,_KDE_NET_WM_WINDOW_TYPE_OVERRIDE",
                f.typeNames);

        assertEquals(1, f.stateMask);
        // 1019 = TYPE(1) | STATE(2) | WM_HINTS(8) | NORMAL(16) | MOTIF(32)
        //        | LEADER(64) | GROUP(128) | PID(256) | CLASS(512)
        assertEquals(WindowFacts.PRESENT_EWMH_TYPE
                | WindowFacts.PRESENT_EWMH_STATE
                | WindowFacts.PRESENT_ICCCM_WM_HINTS
                | WindowFacts.PRESENT_ICCCM_NORMAL
                | WindowFacts.PRESENT_MOTIF
                | WindowFacts.PRESENT_LEADER
                | WindowFacts.PRESENT_GROUP
                | WindowFacts.PRESENT_PID
                | WindowFacts.PRESENT_CLASS, f.present);

        // flags：4112 = SKIP_TASKBAR(16) + KDE_OVERRIDE(4096)
        assertTrue(f.flag(WindowFacts.FLAG_SKIP_TASKBAR));
        assertTrue(f.flag(WindowFacts.FLAG_KDE_OVERRIDE));
        assertFalse(f.flag(WindowFacts.FLAG_RESIZE_INC));

        assertEquals(1, f.hintInput);
        assertEquals(-1, f.hintInitialState);

        assertEquals(1, f.motifPresent);
        assertEquals(2, f.motifFlags);
        assertEquals(0, f.motifDecorations);

        assertEquals(505, f.x);
        assertEquals(44, f.y);
        assertEquals(910, f.w);
        assertEquals(580, f.h);

        assertEquals(0x200010L, f.transientFor);
        assertEquals(0x800001L, f.leader);
        assertEquals(1234L, f.pid);
        assertEquals(10, f.ageMs);
        assertEquals(-1, f.goneMs);
        assertFalse(f.isGone());

        assertEquals(WindowFacts.ACTUAL_ACTIVITY_DECOR, f.actualKind);
        assertEquals("ACTIVITY_DECOR", f.actualKindName());

        assertEquals("ToDesk", f.wmClass);
        assertEquals("", f.wmName);
        assertEquals("ToDesk", f.displayName());
    }

    @Test
    public void missingFieldsKeepSaneDefaults() {
        WindowFacts f = WindowFacts.parse("xid=200015|gone=1336388|kind=0");

        assertNotNull(f);
        assertEquals(0x200015L, f.xid);
        assertTrue(f.isGone());
        assertEquals(1336388, f.goneMs);
        assertEquals(0, f.typeCode);
        assertEquals(-1, f.hintInput);
        assertNull(f.actualKindName());
        // 空 WM_CLASS 不能让 displayName() 抛异常
        assertEquals("", f.displayName());
    }

    @Test
    public void rowWithoutXidIsRejected() {
        assertNull(WindowFacts.parse(""));
        assertNull(WindowFacts.parse(null));
        assertNull(WindowFacts.parse("parent=1297|type=273"));
    }

    @Test
    public void badFieldDoesNotDropTheWholeRow() {
        // 单个字段解析失败（比如 native 传了非法数字）不能连累其它字段
        WindowFacts f = WindowFacts.parse("xid=200001|type=oops|geom=1,2,3,4|class=peony");

        assertNotNull(f);
        assertEquals(0x200001L, f.xid);
        assertEquals(0, f.typeCode);
        assertEquals(3, f.w);
        assertEquals("peony", f.wmClass);
    }
}
