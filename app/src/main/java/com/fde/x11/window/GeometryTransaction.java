package com.fde.x11.window;

/**
 * 一个窗口的几何事务：记录"我们期望的最终 bounds"，用来把回声和新的意图区分开。
 *
 * <p>背景：几何变化有两个真正的主人——Android（用户拖拽、最大化、吸附直接改 Task bounds）
 * 和 X 客户端（ICCCM 的内容尺寸，可以拒绝或量化）。两边都会产生变化通知，而协议里都没有
 * "这次是我引起的回声"这条信息，历史实现只能用 {@code isMoving} + {@code postDelayed}
 * 按时间猜。
 *
 * <p>现在 Android 侧通过 {@code Activity.onBoundsChanged(bounds, appBounds, source, token)}
 * 把"来源"和"请求方 token"带回来（见 framework 改动），于是可以用"预期回声"匹配：
 *
 * <ul>
 *   <li>{@link #begin}：我们主动发起一次几何变更，epoch+1，记下目标；</li>
 *   <li>{@link #match}：收到变化时先和期望值比对，匹配就是回声（吸收掉，不回推）；</li>
 *   <li>{@link #commit}：Surface 尺寸落地或超时兜底后关闭事务。</li>
 * </ul>
 *
 * <p>这个类不持有 Android 对象，纯函数，方便单测。
 */
public final class GeometryTransaction {

    /** 期望的最终外框（屏幕坐标）。 */
    public static final class Bounds {
        public final int left;
        public final int top;
        public final int right;
        public final int bottom;

        public Bounds(int left, int top, int right, int bottom) {
            this.left = left;
            this.top = top;
            this.right = right;
            this.bottom = bottom;
        }

        public int width() {
            return right - left;
        }

        public int height() {
            return bottom - top;
        }

        /** 允许 1px 误差：Android 的 app bounds 会做取整。 */
        public boolean matches(Bounds other, int tolerance) {
            if (other == null) {
                return false;
            }
            return Math.abs(left - other.left) <= tolerance
                    && Math.abs(top - other.top) <= tolerance
                    && Math.abs(right - other.right) <= tolerance
                    && Math.abs(bottom - other.bottom) <= tolerance;
        }

        @Override
        public String toString() {
            return "(" + left + "," + top + ")-(" + right + "," + bottom + ") "
                    + width() + "x" + height();
        }
    }

    /** 事务来源，与 framework 的 BOUNDS_CHANGE_SOURCE_* 对齐。 */
    public static final int SOURCE_UNKNOWN = 0;
    public static final int SOURCE_USER_DRAG = 1;
    public static final int SOURCE_REQUESTER = 2;
    public static final int SOURCE_DISPLAY = 3;

    private static final int TOLERANCE_PX = 1;

    private long epoch;
    private Bounds target;
    private int source = SOURCE_UNKNOWN;
    private long token;
    private long beginUptimeMs;

    /** 主动发起一次几何变更。token 由调用方生成（可对应 framework 的 geometryToken）。 */
    public void begin(Bounds target, int source, long token, long uptimeMs) {
        this.epoch++;
        this.target = target;
        this.source = source;
        this.token = token;
        this.beginUptimeMs = uptimeMs;
    }

    /**
     * 尝试把一次外部变化识别为"回声"。
     *
     * @return true 表示这就是我们自己请求的回声，调用方应当吸收、不要再回推
     */
    public boolean match(Bounds observed) {
        if (target == null) {
            return false;
        }
        return target.matches(observed, TOLERANCE_PX);
    }

    /** 事务结束（Surface 尺寸落地，或超时兜底）。 */
    public void commit() {
        target = null;
        token = 0;
        beginUptimeMs = 0;
    }

    public boolean inFlight() {
        return target != null;
    }

    public long epoch() {
        return epoch;
    }

    public Bounds target() {
        return target;
    }

    public int source() {
        return source;
    }

    public long token() {
        return token;
    }

    public long elapsedMs(long uptimeMs) {
        return beginUptimeMs == 0 ? 0 : uptimeMs - beginUptimeMs;
    }
}
