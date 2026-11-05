package com.fde.x11.window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class GeometryTransactionTest {

    private static GeometryTransaction.Bounds bounds(int l, int t, int r, int b) {
        return new GeometryTransaction.Bounds(l, t, r, b);
    }

    @Test
    public void echoMatchesWithinOnePixel() {
        GeometryTransaction tx = new GeometryTransaction();
        tx.begin(bounds(100, 100, 500, 400), GeometryTransaction.SOURCE_REQUESTER, 7L, 1000L);

        assertTrue(tx.match(bounds(100, 100, 500, 400)));
        // Android 的 app bounds 会取整，允许 1px 误差
        assertTrue(tx.match(bounds(101, 99, 501, 401)));
        assertFalse(tx.match(bounds(110, 100, 500, 400)));
    }

    @Test
    public void commitClosesTransaction() {
        GeometryTransaction tx = new GeometryTransaction();
        tx.begin(bounds(0, 0, 200, 100), GeometryTransaction.SOURCE_REQUESTER, 1L, 0L);
        assertTrue(tx.inFlight());

        tx.commit();

        assertFalse(tx.inFlight());
        // 事务结束后同样的 bounds 不再被当成回声，而是一次新的意图
        assertFalse(tx.match(bounds(0, 0, 200, 100)));
    }

    @Test
    public void noTransactionNeverMatches() {
        GeometryTransaction tx = new GeometryTransaction();
        assertFalse(tx.match(bounds(0, 0, 10, 10)));
        assertEquals(0L, tx.epoch());
    }

    @Test
    public void epochIncrementsOnBegin() {
        GeometryTransaction tx = new GeometryTransaction();
        tx.begin(bounds(0, 0, 10, 10), GeometryTransaction.SOURCE_USER_DRAG, 1L, 100L);
        tx.begin(bounds(0, 0, 20, 20), GeometryTransaction.SOURCE_REQUESTER, 2L, 200L);

        assertEquals(2L, tx.epoch());
        assertEquals(2L, tx.token());
        assertEquals(GeometryTransaction.SOURCE_REQUESTER, tx.source());
        assertFalse(tx.match(bounds(0, 0, 10, 10))); // 旧目标已被覆盖
        assertTrue(tx.match(bounds(0, 0, 20, 20)));
    }

    @Test
    public void elapsedUsesBeginTime() {
        GeometryTransaction tx = new GeometryTransaction();
        tx.begin(bounds(0, 0, 10, 10), GeometryTransaction.SOURCE_REQUESTER, 0L, 1000L);
        assertEquals(250L, tx.elapsedMs(1250L));
        tx.commit();
        assertEquals(0L, tx.elapsedMs(2000L));
    }
}
