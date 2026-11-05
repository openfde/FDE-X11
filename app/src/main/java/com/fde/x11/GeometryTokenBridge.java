package com.fde.x11;

import android.app.ActivityTaskManager;
import android.graphics.Rect;
import android.util.Log;

import java.lang.reflect.Method;

/**
 * 带几何事务 token 的 {@code resizeTask} 调用桥。
 *
 * <p>framework（`D:\android17\base`）里新增了
 * {@code ActivityTaskManager.resizeTask(int, Rect, long)}，但 FDE-X11 目前是按公开 SDK
 * 编译的，编译期的 {@code android.jar} 里没有这个方法，直接写会编译失败。
 *
 * <p>所以这里用反射调用：
 * <ul>
 *   <li>运行时 framework 有该方法 → 带 token 调用，Activity 的
 *       {@code onBoundsChanged(...)} 能拿回同一个 token；</li>
 *   <li>运行在没打过补丁的镜像上（方法不存在）→ 自动退回两参数版本，行为与之前一致。</li>
 * </ul>
 *
 * <p>等 FDE-X11 改为编到打过补丁的 framework jar 之后，这个类可以直接删掉，改成直接调用。
 */
public final class GeometryTokenBridge {

    private static final String TAG = "GeometryToken";

    private static Method sResizeWithToken;
    private static boolean sResolved;

    private GeometryTokenBridge() {
    }

    private static Method resolve() {
        if (!sResolved) {
            sResolved = true;
            try {
                sResizeWithToken = ActivityTaskManager.class.getMethod("resizeTask",
                        int.class, Rect.class, long.class);
                Log.i(TAG, "token-aware resizeTask available");
            } catch (NoSuchMethodException e) {
                sResizeWithToken = null;
                Log.i(TAG, "token-aware resizeTask NOT available, falling back to 2-arg");
            }
        }
        return sResizeWithToken;
    }

    /**
     * Resize a task, tagging the request with {@code token} when the framework supports it.
     *
     * @param token opaque, monotonically increasing id; use {@code 0} for "untagged"
     */
    public static void resizeTask(ActivityTaskManager atm, int taskId, Rect rect, long token) {
        Method m = resolve();
        if (m != null) {
            try {
                m.invoke(atm, taskId, rect, token);
                return;
            } catch (Throwable t) {
                Log.w(TAG, "token-aware resizeTask failed, falling back", t);
            }
        }
        atm.resizeTask(taskId, rect);
    }
}
